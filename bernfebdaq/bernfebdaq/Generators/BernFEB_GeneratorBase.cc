#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

#include "art/Utilities/Exception.h"
#include "cetlib/exception.h"
#include "bernfebdaq-core/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

#include "trace_defines.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


bernfebdaq::BernFEB_GeneratorBase::BernFEB_GeneratorBase(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  ps_(ps)
{
  TRACE(TR_LOG,"BernFeb constructor called");  
  Initialize();
  TRACE(TR_LOG,"BernFeb constructor completed");  
}

void bernfebdaq::BernFEB_GeneratorBase::Initialize(){

  TRACE(TR_LOG,"BernFeb::Initialze() called");

  RunNumber_ = ps_.get<uint32_t>("RunNumber",999);
  SubrunTimeWindowSize_ = ps_.get<uint64_t>("SubRunTimeWindowSize",60e9); //one minute
  SequenceTimeWindowSize_ = ps_.get<uint32_t>("SequenceTimeWindowSize",1e6); //one ms
  nADCBits_  = ps_.get<uint8_t>("nADCBits",12);
  nChannels_ = ps_.get<uint32_t>("nChannels",32);
  ReaderID_ = ps_.get<uint8_t>("ReaderID",0x1);
  FEBIDs_ = ps_.get< std::vector<uint64_t> >("FEBIDs");

  FEBBufferCapacity_ = ps_.get<uint32_t>("FEBBufferCapacity",5e3);
  FEBBufferSizeBytes_ = FEBBufferCapacity_*sizeof(BernFEBEvent);

  FEBDTPBufferCapacity_ = ps_.get<uint32_t>("FEBDTPBufferCapacity",1024);
  FEBDTPBufferSizeBytes_ = FEBDTPBufferCapacity_*sizeof(BernFEBEvent);

  throttle_usecs_ = ps_.get<size_t>("throttle_usecs", 100000);
  throttle_usecs_check_ = ps_.get<size_t>("throttle_usecs_check", 10000);

  if(nChannels_!=32)
    throw cet::exception("BernFEB_GeneratorBase::Initialize")
      << "nChannels != 32. This is not supported.";


  if (throttle_usecs_ > 0 && (throttle_usecs_check_ >= throttle_usecs_ ||
			      throttle_usecs_ % throttle_usecs_check_ != 0) ) {
    throw cet::exception("Error in BernFEB: disallowed combination of throttle_usecs and throttle_usecs_check (see BernFEB.hh for rules)");
  }
  
  TRACE(TR_LOG,"BernFeb::Initialize() completed");

  for( auto const& id : FEBIDs_)
    FEBBuffers_[id] = FEBBuffer_t(FEBBufferCapacity_,id);

  FEBDTPBufferUPtr.reset(new BernFEBEvent[FEBDTPBufferCapacity_]);
  TRACE(TR_DEBUG,"\tMade %lu FEBBuffers",FEBIDs_.size());

  TRACE(TR_LOG,"BernFeb::Initialize() ... starting GetData worker thread.");
  ThreadFunctor functor = std::bind(&BernFEB_GeneratorBase::GetData,this);
  auto worker_functor = WorkerThreadFunctorUPtr(new WorkerThreadFunctor(functor,"GetDataWorkerThread"));
  auto getData_worker = WorkerThread::createWorkerThread(worker_functor);
  GetData_thread_.swap(getData_worker);

}

void bernfebdaq::BernFEB_GeneratorBase::start() {

  TRACE(TR_LOG,"BernFeb::start() called");

  current_subrun_ = 0;

  for(auto & buf : FEBBuffers_)
    buf.second.Init();
  
  ConfigureStart();
  GetData_thread_->start();

  TRACE(TR_LOG,"BernFeb::start() completed");
}

void bernfebdaq::BernFEB_GeneratorBase::stop() {
  TRACE(TR_LOG,"BernFeb::stop() called");
  GetData_thread_->stop();
  ConfigureStop();
  TRACE(TR_LOG,"BernFeb::stop() completed");
}

void bernfebdaq::BernFEB_GeneratorBase::Cleanup(){
  TRACE(TR_LOG,"BernFeb::Cleanup() called");
  TRACE(TR_LOG,"BernFeb::Cleanup() completed");
}

bernfebdaq::BernFEB_GeneratorBase::~BernFEB_GeneratorBase(){
  TRACE(TR_LOG,"BernFeb destructor called");  
  Cleanup();
  TRACE(TR_LOG,"BernFeb destructor completed");  
}

std::string bernfebdaq::BernFEB_GeneratorBase::GetFEBIDString(uint64_t const& id) const{
  std::stringstream ss_id;
  ss_id << "0x" << std::hex << std::setw(12) << std::setfill('0') << (id & 0xffffffffffff);
  return ss_id.str();
}

void bernfebdaq::BernFEB_GeneratorBase::UpdateBufferOccupancyMetrics(uint64_t const& id,
								     size_t const& buffer_size) const {

  std::string id_str = GetFEBIDString(id);
  metricMan_->sendMetric("BufferOccupancy_"+id_str,buffer_size,"events",5,true);    
  metricMan_->sendMetric("BufferOccupancyPercent_"+id_str,
			 ((float)(buffer_size) / (float)(FEBBufferCapacity_))*100.,
			 "%",5,true);    
}

size_t bernfebdaq::BernFEB_GeneratorBase::InsertIntoFEBBuffer(FEBBuffer_t & b, 
							      size_t const& nevents){
  std::unique_lock<std::mutex> lock(*(b.mutexptr));
  b.buffer.insert(b.buffer.end(),&(FEBDTPBufferUPtr[0]),&(FEBDTPBufferUPtr[nevents]));
  return b.buffer.size();
}

size_t bernfebdaq::BernFEB_GeneratorBase::EraseFromFEBBuffer(FEBBuffer_t & b,
							     FEBEventBuffer_t::iterator const& it_start,
							     FEBEventBuffer_t::iterator const& it_end) {
  std::unique_lock<std::mutex> lock(*(b.mutexptr));
  b.buffer.erase(it_start,it_end);
  return b.buffer.size();
}

bool bernfebdaq::BernFEB_GeneratorBase::GetData()
{
  TRACE(TR_GD_LOG,"BernFeb::GetData() called");

  if( GetDataSetup()!=1 ) return false;;

  size_t this_n_events=0,n_events=0,new_buffer_size=0;
  for(auto & buf : FEBBuffers_){
    auto & buf_obj = buf.second;
    this_n_events = GetFEBData(buf_obj.id)/sizeof(BernFEBEvent);
    new_buffer_size = InsertIntoFEBBuffer(buf_obj,this_n_events);
    n_events += this_n_events;

    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, n_events=%lu",buf_obj.id,this_n_events);
    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, buffer_size=%lu",buf_obj.id,buf_obj.buffer.size());

    auto id_str = GetFEBIDString(buf_obj.id);

    metricMan_->sendMetric("EventsAdded_"+id_str,this_n_events,"events",5,true);
    UpdateBufferOccupancyMetrics(buf_obj.id,new_buffer_size);
  }
  
  TRACE(TR_GD_LOG,"BernFeb::GetData() completed, n_events=%lu",n_events);

  GetDataComplete();

  metricMan_->sendMetric("TotalEventsAdded",n_events,"events",5,true);

  if(n_events>0) return true;
  return false;
}

bool bernfebdaq::BernFEB_GeneratorBase::FillFragment(uint64_t const& feb_id,
						     artdaq::FragmentPtrs & frags,
						     bool clear_buffer)
{
  TRACE(TR_FF_LOG,"BernFeb::FillFragment(id=0x%lx,frags,clear=%d) called",feb_id,clear_buffer);

  auto & feb = (FEBBuffers_[feb_id]);

  if(!clear_buffer && feb.buffer.size()==0) {
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() completed, buffer empty.");
    return false;
  }

  TRACE(TR_FF_DEBUG,
	"\tBernFeb::FillFragment() : Look for data, id=0x%lx, time=[%ld,%ld]. Buffer size=%lu",
	feb_id,feb.next_time_start,feb.next_time_start+SequenceTimeWindowSize_,feb.buffer.size());
				   
  int64_t time = 0;
  size_t  local_time_resets = 0;
  int32_t local_last_time = feb.last_time_counter;
  bool found_fragment=false;

  auto it_start_fragment = feb.buffer.begin();
  auto it_end_fragment=feb.buffer.end();

  TRACE(TR_FF_LOG,"BernFeb::FillFragment() : Fragment Searching. Total events in buffer=%ld.",
	std::distance(it_start_fragment,it_end_fragment));
  

  //just find the time boundary first
  for(auto it=it_start_fragment; it!=it_end_fragment-1;++it){

    TRACE(TR_FF_DEBUG,"\n\tBernFeb::FillFragment() ... found event: %s",it->c_str());

    if((int64_t)it->time1.Time() <= local_last_time)
      ++local_time_resets;
    local_last_time = it->time1.Time();

    time = it->time1.Time()+(feb.time_resets+local_time_resets)*1e9;

    TRACE(TR_FF_DEBUG,
	  "\n\tBernFEb::FillFragment() ... ev_time is %u, resets=%lu, local_resets=%lu, time=%ld",
	  it->time1.Time(),feb.time_resets,local_time_resets,time);

    if(time<feb.next_time_start)
      TRACE(TR_WARNING,"BernFeb::FillFragment() WARNING! Time out of place! %ld vs %ld",time,feb.next_time_start);
    else if(time>feb.next_time_start+SequenceTimeWindowSize_){
      TRACE(TR_FF_DEBUG,
	    "\n\tBernFeb::FillFragment() : Found a new time!! %ld (>%ld)",
	    time,feb.next_time_start+SequenceTimeWindowSize_);
      it_end_fragment = it;
      found_fragment = true;
      break;
    }
  }

  //didn't get our last event in this time window ... return
  if(!clear_buffer && !found_fragment){
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() completed, buffer not empty, but waiting for more data.");
    return false;
  }

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Data window completed, [%u,%u), events=%ld",
	it_start_fragment->time1.Time(),it_end_fragment->time1.Time(),std::distance(it_start_fragment,it_end_fragment));


  //ok, queue was non-empty, and we saw our last event. Need to loop through and do proper accounting now.

  //make metadata object
  BernFEBFragmentMetadata metadata(feb.next_time_start,
				   feb.next_time_start+SequenceTimeWindowSize_,
				   RunNumber_,
				   feb.next_time_start/SubrunTimeWindowSize_,
				   feb.next_time_start/SequenceTimeWindowSize_ + 1,
				   feb_id, ReaderID_,
				   nChannels_,nADCBits_);

  local_time_resets = 0;
  local_last_time = feb.last_time_counter;
  for(auto it=it_start_fragment; it!=it_end_fragment; ++it){

    if((int32_t)it->time1.Time() <= feb.last_time_counter){
      ++feb.time_resets;
      TRACE(TR_FF_DEBUG,"\n\tBernFeb::FillFragment() : Time reset (evtime=%u,last_time=%d). Total resets=%lu",
	    it->time1.Time(),feb.last_time_counter,feb.time_resets);
    }
    feb.last_time_counter = it->time1.Time();
    
    if(it->flags.overwritten > feb.overwritten_counter)
      metadata.increment(it->flags.missed,it->flags.overwritten-feb.overwritten_counter);
    else
      metadata.increment(it->flags.missed,0);
    feb.overwritten_counter = it->flags.overwritten;

  }
  feb.next_time_start = feb.next_time_start+SequenceTimeWindowSize_;

  TRACE(TR_FF_DEBUG,
	"BernFeb::FillFragment() : Fragment prepared. Metadata:%s",
	metadata.c_str());

  //great, now add the fragment on the end.`<
  frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata.n_events()*sizeof(BernFEBEvent),  
						      metadata.sequence_number(),feb_id,
						      bernfebdaq::detail::FragmentType::BernFEB, metadata) );
  std::copy(it_start_fragment,it_end_fragment,(BernFEBEvent*)(frags.back()->dataBegin()));

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Fragment created. First event in fragment: %s",
	((BernFEBEvent*)(frags.back()->dataBegin()))->c_str() );

  TRACE(TR_FF_LOG,"BernFeb::FillFragment() : Fragment created. Events=%ld. Metadata : %s",
	std::distance(it_start_fragment,it_end_fragment),metadata.c_str());

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size before erase = %lu",feb.buffer.size());
  size_t new_buffer_size = EraseFromFEBBuffer(feb,it_start_fragment,it_end_fragment);
  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size after erase = %lu",feb.buffer.size());

  auto id_str = GetFEBIDString(feb_id);
  metricMan_->sendMetric("FragmentsBuilt_"+id_str,1.0,"events",5,false,true);
  UpdateBufferOccupancyMetrics(feb_id,new_buffer_size);
  SendMetadataMetrics(metadata);

  return true;
}

void bernfebdaq::BernFEB_GeneratorBase::SendMetadataMetrics(BernFEBFragmentMetadata const& m) {
  auto id_str = GetFEBIDString(m.feb_id());
  metricMan_->sendMetric("FragmentLastTime_"+id_str,(uint64_t)(m.time_end()),"ns",5,true,false);
  metricMan_->sendMetric("EventsInFragment_"+id_str,(float)(m.n_events()),"events",5,true);
  metricMan_->sendMetric("MissedEvents_"+id_str,     (float)(m.missed_events()),     "events",5);
  metricMan_->sendMetric("OverwrittenEvents_"+id_str,(float)(m.overwritten_events()),"events",5);
  float eff=1.0;
  if((m.n_events()+m.missed_events()+m.overwritten_events())!=0)
    eff = (float)(m.n_events()) / (float)(m.n_events()+m.missed_events()+m.overwritten_events());

  metricMan_->sendMetric("Efficiency_"+id_str,eff*100.,"%",5);
}

bool bernfebdaq::BernFEB_GeneratorBase::getNext_(artdaq::FragmentPtrs & frags) {

  TRACE(TR_FF_LOG,"BernFeb::getNext_(frags) called");

  //throttling...
  if (throttle_usecs_ > 0) {
    size_t nchecks = throttle_usecs_ / throttle_usecs_check_;
    
    for (size_t i_c = 0; i_c < nchecks; ++i_c) {
      usleep( throttle_usecs_check_ );
      
      if (should_stop()) {
	return false;
      }
    }
  } else {
    if (should_stop()) {
      return false;
    }
  }
  
  for(auto const& id : FEBIDs_){
    while(true){
      if(!FillFragment(id,frags)) break;
    }
  }

  TRACE(TR_FF_LOG,"BernFeb::getNext_(frags) completed");

  //BernFEBFragment bb(*frags.back());
  if(frags.size()!=0) TRACE(TR_FF_DEBUG,"BernFEB::geNext_() : last fragment is: %s",(BernFEBFragment(*frags.back())).c_str());

  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
//DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(demo::BernFEB) 
