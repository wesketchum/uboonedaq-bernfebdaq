#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

//#include "art/Utilities/Exception.h"
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
#include <string>
#include <time.h>
#include <sys/time.h>

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
  SequenceTimeWindowSize_ = ps_.get<uint32_t>("SequenceTimeWindowSize",5e6); //5 ms
  nADCBits_  = ps_.get<uint8_t>("nADCBits",12);
  nChannels_ = ps_.get<uint32_t>("nChannels",32);
  ReaderID_ = ps_.get<uint8_t>("ReaderID",0x1);
  FEBIDs_ = ps_.get< std::vector<uint64_t> >("FEBIDs");

  if(SequenceTimeWindowSize_<1e6)
    throw cet::exception("BernFEB_GeneratorBase::Initialize")
      << "Sequence Time Window size is less than 1 ms (1e6 ns). This is not supported.";

  FEBBufferCapacity_ = ps_.get<uint32_t>("FEBBufferCapacity",5e3);
  FEBBufferSizeBytes_ = FEBBufferCapacity_*sizeof(BernFEBEvent);

  FEBDTPBufferCapacity_ = ps_.get<uint32_t>("FEBDTPBufferCapacity",1024);
  FEBDTPBufferSizeBytes_ = FEBDTPBufferCapacity_*sizeof(BernFEBEvent);

  MaxTimeDiffs_   = ps_.get< std::vector<uint32_t> >("MaxTimeDiffs",std::vector<uint32_t>(FEBIDs_.size(),1e7));
  MaxDTPSleep_    = ps_.get<uint32_t>("MaxDTPSleep",5e8);
  TargetDTPSizes_ = ps_.get< std::vector<size_t> >("TargetDTPSizes",std::vector<size_t>(FEBIDs_.size(),FEBDTPBufferCapacity_/2));

  if(TargetDTPSizes_.size()!=FEBIDs_.size()){
    if(TargetDTPSizes_.size()==1){
      auto size = TargetDTPSizes_.at(0);
      TargetDTPSizes_ = std::vector<size_t>(FEBIDs_.size(),size);
    }
    else{
      throw cet::exception("BernFEB_GeneratorBase::Iniitalize")
	<< "TargetDTPSizes must be same size as FEBIDs in config!";
    }
  }

  for(auto const& s : TargetDTPSizes_)
    if(s > FEBDTPBufferCapacity_)
      throw cet::exception("BernFEB_GeneratorBase::Iniitalize")
	<< "TargetDTPSize " << s << " must be <= FEBDTPBufferCapacity " << FEBDTPBufferCapacity_;


  if(MaxTimeDiffs_.size()!=FEBIDs_.size()){
    if(MaxTimeDiffs_.size()==1){
      auto size = MaxTimeDiffs_.at(0);
      MaxTimeDiffs_ = std::vector<uint32_t>(FEBIDs_.size(),size);
    }
    else{
      throw cet::exception("BernFEB_GeneratorBase::Iniitalize")
	<< "MaxTimeDiffs must be same size as FEBIDs in config!";
    }
  }

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

						

  for( size_t i_id=0; i_id<FEBIDs_.size(); ++i_id){
    auto const& id = FEBIDs_[i_id];
    FEBBuffers_[id] = FEBBuffer_t(FEBBufferCapacity_,MaxTimeDiffs_[i_id],TargetDTPSizes_[i_id],id);
  }
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

void bernfebdaq::BernFEB_GeneratorBase::stopNoMutex() {
  TRACE(TR_LOG,"BernFeb::stopNoMutex() called");
  GetData_thread_->stop();
  ConfigureStop();
  TRACE(TR_LOG,"BernFeb::stopNoMutex() completed");
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
  metricMan_->sendMetric("BufferOccupancy_"+id_str,buffer_size,"events",5,true,"BernFEBGenerator");    
  metricMan_->sendMetric("BufferOccupancyPercent_"+id_str,
			 ((float)(buffer_size) / (float)(FEBBufferCapacity_))*100.,
			 "%",5,true,"BernFEBGenerator");    
}

size_t bernfebdaq::BernFEB_GeneratorBase::InsertIntoFEBBuffer(FEBBuffer_t & b, 
							      size_t const& nevents){
  
  timeval timenow; gettimeofday(&timenow,NULL);

  //don't fill while we wait for available capacity...
  while( (b.buffer.capacity()-b.buffer.size()) < nevents){ usleep(10); }

  //obtain the lock
  std::unique_lock<std::mutex> lock(*(b.mutexptr));

  //ok, now, we need to do a few things:
  //(1) loop through our new events, and see if things are in order (time1)
  //(2) if they aren't, check if it's a reference or overflow condition
  //(3) if not, we're gonna stop right there and drop the remaining events.
  //(4) insert good events onto the list

  TRACE(TR_DEBUG,"FEB ID 0x%lx. Current buffer size %lu with times [%u,%u].",b.id,b.buffer.size(),
	b.buffer.front().time1.Time(),
	b.buffer.back().time1.Time());
  TRACE(TR_DEBUG,"Want to add %lu events with times [%u,%u].",nevents,
	FEBDTPBufferUPtr[0].time1.Time(),
	FEBDTPBufferUPtr[nevents-1].time1.Time());
	
  TRACE(TR_DEBUG,"Before sort, here's contents of buffer:");
  TRACE(TR_DEBUG,"============================================");
  for(size_t i_e=0; i_e<b.buffer.size(); ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u (%x)",i_e,b.buffer.at(i_e).time1.Time(),b.buffer.at(i_e).time1.rawts);
  TRACE(TR_DEBUG,"--------------------------------------------");
  for(size_t i_e=0; i_e<nevents; ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u (%x)",i_e,FEBDTPBufferUPtr[i_e].time1.Time(),FEBDTPBufferUPtr[i_e].time1.rawts);
  TRACE(TR_DEBUG,"============================================");

  auto last_time = FEBDTPBufferUPtr[0].time1;
  auto this_time = last_time;

  size_t good_events=1;
  while(good_events<nevents){
    this_time = FEBDTPBufferUPtr[good_events].time1;

    //if times not in order ...
    if(this_time.Time() <= last_time.Time())
      {

	// ... and the current is not an overflow or prev is not reference
	// then we need to break out of this.
	if( !(last_time.IsReference() || this_time.IsOverflow()) )
	  break;
      }

    //if time difference is too large
    else
      if( (this_time.Time()-last_time.Time())>b.max_time_diff )
	break;
    

    last_time = this_time;
    ++good_events;
  }

  //note, the order here is important. the buffer with events needs to be last, as that's
  //what is used later for the filling process to determing number of events. 
  //determining number of events is an unlocked procedure
  b.timebuffer.insert(b.timebuffer.end(),good_events,timenow);
  b.droppedbuffer.insert(b.droppedbuffer.end(),good_events-1,0);
  b.droppedbuffer.insert(b.droppedbuffer.end(),1,nevents-good_events);
  b.buffer.insert(b.buffer.end(),&(FEBDTPBufferUPtr[0]),&(FEBDTPBufferUPtr[good_events]));
  
  TRACE(TR_DEBUG,"After insert, here's contents of buffer:");
  TRACE(TR_DEBUG,"============================================");
  for(size_t i_e=0; i_e<b.buffer.size(); ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u (%x)",i_e,b.buffer.at(i_e).time1.Time(),b.buffer.at(i_e).time1.rawts);
  if(good_events!=nevents)
    TRACE(TR_DEBUG,"\tWE DROPPED %lu EVENTS.",nevents-good_events);
  TRACE(TR_DEBUG,"============================================");
  
  return b.buffer.size();
}

size_t bernfebdaq::BernFEB_GeneratorBase::EraseFromFEBBuffer(FEBBuffer_t & b,
							     size_t const& nevents){
  std::unique_lock<std::mutex> lock(*(b.mutexptr));
  b.droppedbuffer.erase_begin(nevents);
  b.timebuffer.erase_begin(nevents);
  b.correctedtimebuffer.erase_begin(nevents);
  b.buffer.erase_begin(nevents);
  return b.buffer.size();
}

bool bernfebdaq::BernFEB_GeneratorBase::GetData()
{
  TRACE(TR_GD_LOG,"BernFeb::GetData() called");

  if( GetDataSetup()!=1 ) return false;;

  size_t this_n_events=0,n_events=0,new_buffer_size=0;
  for(auto & buf : FEBBuffers_){
    auto & buf_obj = buf.second;

    bool condition = buf_obj.timer.check_time();
    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, time=%ld, max_time=%u, pass? %d",
	  buf_obj.id,std::chrono::nanoseconds(buf_obj.timer.time_end-buf_obj.timer.time_start).count(),buf_obj.timer.buffer_sleep,
	  condition);
    if(!condition) continue;

    this_n_events = GetFEBData(buf_obj.id)/sizeof(BernFEBEvent);

    int time = buf_obj.timer.end_and_update(this_n_events,MaxDTPSleep_);

    new_buffer_size = InsertIntoFEBBuffer(buf_obj,this_n_events);
    n_events += this_n_events;

    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, n_events=%lu, time=%d",buf_obj.id,this_n_events,time);
    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, buffer_size=%lu",buf_obj.id,buf_obj.buffer.size());

    auto id_str = GetFEBIDString(buf_obj.id);

    metricMan_->sendMetric("EventsAdded_"+id_str,this_n_events,"events",5,true,"BernFEBGenerator");
    UpdateBufferOccupancyMetrics(buf_obj.id,new_buffer_size);
  }
  
  TRACE(TR_GD_LOG,"BernFeb::GetData() completed, n_events=%lu",n_events);

  GetDataComplete();

  metricMan_->sendMetric("TotalEventsAdded",n_events,"events",5,true,"BernFEBGenerator");

  if(n_events>0) return true;
  return false;
}

bool bernfebdaq::BernFEB_GeneratorBase::FillFragment(uint64_t const& feb_id,
						     artdaq::FragmentPtrs & frags,
						     bool clear_buffer)
{
  TRACE(TR_FF_LOG,"BernFeb::FillFragment(id=0x%lx,frags,clear=%d) called",feb_id,clear_buffer);

  auto & feb = (FEBBuffers_[feb_id]);

  if(!clear_buffer && feb.buffer.size()<3) {
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() completed, buffer (mostly) empty.");
    return false;
  }

  size_t buffer_end = feb.buffer.size();

  TRACE(TR_FF_LOG,"BernFeb::FillFragment() : Fragment Searching. Total events in buffer=%lu.",
	buffer_end);

  double time_correction = -1.0;

  //find GPS pulse and count wraparounds
  size_t i_gps=buffer_end;
  size_t n_wraparounds=0;
  uint32_t last_time =0;
  size_t i_e;
  for(i_e=0; i_e<buffer_end; ++i_e){

    auto const& this_event = feb.buffer[i_e];
    

    if(this_event.time1.IsReference()){
      i_gps=i_e;
    }
    else if(this_event.time1.Time()<last_time && this_event.time1.IsOverflow())
      n_wraparounds++;

    feb.correctedtimebuffer[i_e] = (this_event.time1.Time()+(uint64_t)n_wraparounds*0x40000000);
    last_time = this_event.time1.Time();

    if(i_gps!=buffer_end) break;
  }

  if(i_gps==buffer_end){
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() completed, buffer not empty, but waiting for GPS pulse.");
    return false;
  }
  
  
  //determine how much time passed. should be close to corrected time.
  //then determine the correction.
  uint32_t elapsed_secs = (feb.correctedtimebuffer[i_gps]/1e9);
  if(feb.correctedtimebuffer[i_gps]%1000000000 > 500000000)
    elapsed_secs+=1;
  time_correction = 1.0e9*(double)elapsed_secs / (double)(feb.correctedtimebuffer[i_gps]);
  
  
  //ok, so now, determine the nearest second for the last event (closest to one second), based on ntp time
  //get the usecs from the timeval
  auto gps_timeval = feb.timebuffer.at(i_gps);
  
  //report remainder as a metric to watch for
  std::string id_str = GetFEBIDString(feb_id);
  metricMan_->sendMetric("BoundaryTimeRemainder_"+id_str,(float)(gps_timeval.tv_usec),"microseconds",false,"BernFEBGenerator");
  
  //round the boundary time to the nearest second.
  if(gps_timeval.tv_usec > 5e5)
    gps_timeval.tv_sec+=1;
  gps_timeval.tv_usec = 0;


  uint32_t frag_begin_time_s = gps_timeval.tv_sec-elapsed_secs;
  uint32_t frag_begin_time_ns = 0;
  uint32_t frag_end_time_s = gps_timeval.tv_sec-elapsed_secs;
  uint32_t frag_end_time_ns = SequenceTimeWindowSize_;
  
  i_e=0;
  last_time=0;
  uint64_t time_offset=0;
  while(frag_end_time_s < gps_timeval.tv_sec){

    if(frag_end_time_ns>=1000000000){
      frag_end_time_ns = frag_end_time_ns%1000000000;
      frag_end_time_s+=1;
    }

    if(frag_begin_time_ns>=1000000000){
      frag_begin_time_ns = frag_begin_time_ns%1000000000;
      frag_begin_time_s+=1;
    }

    //make metadata object
    BernFEBFragmentMetadata metadata(frag_begin_time_s,frag_begin_time_ns,
				     frag_end_time_s,frag_end_time_ns,
				     time_correction,time_offset,
				     RunNumber_,
				     (frag_begin_time_s*1000)+(frag_begin_time_ns/1000000), //sequence id is ms since epoch
				     feb_id, ReaderID_,
				     nChannels_,nADCBits_);
    
    while(i_e<i_gps+1){
      
      auto const& this_corrected_time = feb.correctedtimebuffer[i_e];
      
      if( (double)this_corrected_time*time_correction > frag_end_time_s*1e9+frag_end_time_ns )
	break;

      auto const& this_event = feb.buffer[i_e];

      if(this_event.time1.Time()<last_time)
	time_offset += 0x40000000;

      if(this_event.flags.overwritten > feb.overwritten_counter)
	metadata.increment(this_event.flags.missed,this_event.flags.overwritten-feb.overwritten_counter,feb.droppedbuffer[i_e]);
      else
	metadata.increment(this_event.flags.missed,0,feb.droppedbuffer[i_e]);
      feb.overwritten_counter = this_event.flags.overwritten;
    
      ++i_e;
    }
    
    //great, now add the fragment on the end.`<
    frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata.n_events()*sizeof(BernFEBEvent),  
							metadata.sequence_number(),feb_id,
							bernfebdaq::detail::FragmentType::BernFEB, metadata) );
    std::copy(feb.buffer.begin(),feb.buffer.begin()+buffer_end,(BernFEBEvent*)(frags.back()->dataBegin()));
    
    TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Fragment created. First event in fragment: %s",
	  ((BernFEBEvent*)(frags.back()->dataBegin()))->c_str() );
    
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() : Fragment created. Events=%u. Metadata : %s",
	  metadata.n_events(),metadata.c_str());
        
    SendMetadataMetrics(metadata);
  }

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size before erase = %lu",feb.buffer.size());
  size_t new_buffer_size = EraseFromFEBBuffer(feb,i_gps+1);
  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size after erase = %lu",feb.buffer.size());

  metricMan_->sendMetric("FragmentsBuilt_"+id_str,1.0,"events",5,true,"BernFEBGenerator");
  UpdateBufferOccupancyMetrics(feb_id,new_buffer_size);

  return true;
}

void bernfebdaq::BernFEB_GeneratorBase::SendMetadataMetrics(BernFEBFragmentMetadata const& m) {
  std::string id_str = GetFEBIDString(m.feb_id());
  metricMan_->sendMetric("FragmentLastTime_"+id_str,(uint64_t)(m.time_end_seconds()*1000000000+m.time_end_nanosec()),"ns",5,false,"BernFEBGenerator");
  metricMan_->sendMetric("EventsInFragment_"+id_str,(float)(m.n_events()),"events",5,true,"BernFEBGenerator");
  metricMan_->sendMetric("MissedEvents_"+id_str,     (float)(m.missed_events()),     "events",5,true,"BernFEBGenerator");
  metricMan_->sendMetric("OverwrittenEvents_"+id_str,(float)(m.overwritten_events()),"events",5,true,"BernFEBGenerator");
  float eff=1.0;
  if((m.n_events()+m.missed_events()+m.overwritten_events())!=0)
    eff = (float)(m.n_events()) / (float)(m.n_events()+m.missed_events()+m.overwritten_events());

  metricMan_->sendMetric("Efficiency_"+id_str,eff*100.,"%",5,true,"BernFEBGenerator");
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
