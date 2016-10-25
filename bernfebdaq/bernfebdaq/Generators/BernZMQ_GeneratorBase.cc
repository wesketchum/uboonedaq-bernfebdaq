#include "bernfebdaq/Generators/BernZMQ_GeneratorBase.hh"

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
#include <string>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

bernfebdaq::BernZMQ_GeneratorBase::BernZMQ_GeneratorBase(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  ps_(ps)
{
  TRACE(TR_LOG,"BernFeb constructor called");  
  Initialize();
  TRACE(TR_LOG,"BernFeb constructor completed");  
}

void bernfebdaq::BernZMQ_GeneratorBase::Initialize(){
  
  TRACE(TR_LOG,"BernFeb::Initialze() called");

  RunNumber_ = ps_.get<uint32_t>("RunNumber",999);
  SubrunTimeWindowSize_ = ps_.get<uint64_t>("SubRunTimeWindowSize",60e9); //one minute
  SequenceTimeWindowSize_ = ps_.get<uint32_t>("SequenceTimeWindowSize",5e6); //5 ms
  nADCBits_  = ps_.get<uint8_t>("nADCBits",12);
  nChannels_ = ps_.get<uint32_t>("nChannels",32);
  ReaderID_ = ps_.get<uint8_t>("ReaderID",0x1);
  FEBIDs_ = ps_.get< std::vector<uint64_t> >("FEBIDs");

  if(SequenceTimeWindowSize_<1e6)
    throw cet::exception("BernZMQ_GeneratorBase::Initialize")
      << "Sequence Time Window size is less than 1 ms (1e6 ns). This is not supported.";

  FEBBufferCapacity_ = ps_.get<uint32_t>("FEBBufferCapacity",5e3);
  FEBBufferSizeBytes_ = FEBBufferCapacity_*sizeof(BernZMQEvent);

  ZMQBufferCapacity_ = ps_.get<uint32_t>("ZMQBufferCapacity",1024*30);
  ZMQBufferSizeBytes_ = ZMQBufferCapacity_*sizeof(BernZMQEvent);

  MaxTimeDiffs_   = ps_.get< std::vector<uint32_t> >("MaxTimeDiffs",std::vector<uint32_t>(FEBIDs_.size(),1e7));

  if(MaxTimeDiffs_.size()!=FEBIDs_.size()){
    if(MaxTimeDiffs_.size()==1){
      auto size = MaxTimeDiffs_.at(0);
      MaxTimeDiffs_ = std::vector<uint32_t>(FEBIDs_.size(),size);
    }
    else{
      throw cet::exception("BernZMQ_GeneratorBase::Iniitalize")
	<< "MaxTimeDiffs must be same size as ZMQIDs in config!";
    }
  }

  throttle_usecs_ = ps_.get<size_t>("throttle_usecs", 100000);
  throttle_usecs_check_ = ps_.get<size_t>("throttle_usecs_check", 10000);
  
  if(nChannels_!=32)
    throw cet::exception("BernZMQ_GeneratorBase::Initialize")
      << "nChannels != 32. This is not supported.";


  if (throttle_usecs_ > 0 && (throttle_usecs_check_ >= throttle_usecs_ ||
			      throttle_usecs_ % throttle_usecs_check_ != 0) ) {
    throw cet::exception("Error in BernZMQ: disallowed combination of throttle_usecs and throttle_usecs_check (see BernZMQ.hh for rules)");
  }
  
  TRACE(TR_LOG,"BernFeb::Initialize() completed");

						

  for( size_t i_id=0; i_id<FEBIDs_.size(); ++i_id){
    auto const& id = FEBIDs_[i_id];
    FEBBuffers_[id] = FEBBuffer_t(FEBBufferCapacity_,MaxTimeDiffs_[i_id],id);
  }
  ZMQBufferUPtr.reset(new BernZMQEvent[ZMQBufferCapacity_]);

  TRACE(TR_DEBUG,"\tMade %lu ZMQBuffers",FEBIDs_.size());

  TRACE(TR_LOG,"BernFeb::Initialize() ... starting GetData worker thread.");
  ThreadFunctor functor = std::bind(&BernZMQ_GeneratorBase::GetData,this);
  auto worker_functor = WorkerThreadFunctorUPtr(new WorkerThreadFunctor(functor,"GetDataWorkerThread"));
  auto getData_worker = WorkerThread::createWorkerThread(worker_functor);
  GetData_thread_.swap(getData_worker);

  SeqIDMinimumSec_ = 0;
}

void bernfebdaq::BernZMQ_GeneratorBase::start() {

  TRACE(TR_LOG,"BernFeb::start() called");

  current_subrun_ = 0;

  for(auto & buf : FEBBuffers_)
    buf.second.Init();
  
  ConfigureStart();
  GetData_thread_->start();

  TRACE(TR_LOG,"BernFeb::start() completed");
}

void bernfebdaq::BernZMQ_GeneratorBase::stop() {
  TRACE(TR_LOG,"BernFeb::stop() called");
  GetData_thread_->stop();
  ConfigureStop();
  TRACE(TR_LOG,"BernFeb::stop() completed");
}

void bernfebdaq::BernZMQ_GeneratorBase::stopNoMutex() {
  TRACE(TR_LOG,"BernFeb::stopNoMutex() called");
  GetData_thread_->stop();
  ConfigureStop();
  TRACE(TR_LOG,"BernFeb::stopNoMutex() completed");
}

void bernfebdaq::BernZMQ_GeneratorBase::Cleanup(){
  TRACE(TR_LOG,"BernFeb::Cleanup() called");
  TRACE(TR_LOG,"BernFeb::Cleanup() completed");
}

bernfebdaq::BernZMQ_GeneratorBase::~BernZMQ_GeneratorBase(){
  TRACE(TR_LOG,"BernFeb destructor called");  
  Cleanup();
  TRACE(TR_LOG,"BernFeb destructor completed");  
}

std::string bernfebdaq::BernZMQ_GeneratorBase::GetFEBIDString(uint64_t const& id) const{
  std::stringstream ss_id;
  ss_id << "0x" << std::hex << std::setw(12) << std::setfill('0') << (id & 0xffffffffffff);
  return ss_id.str();
}

void bernfebdaq::BernZMQ_GeneratorBase::UpdateBufferOccupancyMetrics(uint64_t const& id,
								     size_t const& buffer_size) const {

  std::string id_str = GetFEBIDString(id);
  metricMan_->sendMetric("BufferOccupancy_"+id_str,buffer_size,"events",5,true,"BernZMQGenerator");    
  metricMan_->sendMetric("BufferOccupancyPercent_"+id_str,
			 ((float)(buffer_size) / (float)(FEBBufferCapacity_))*100.,
			 "%",5,true,"BernZMQGenerator");    
}

size_t bernfebdaq::BernZMQ_GeneratorBase::InsertIntoFEBBuffer(FEBBuffer_t & b,
							      size_t begin_index,
							      size_t nevents,
							      size_t total_events){
  
  TRACE(TR_DEBUG,"FEB ID 0x%lx. Current buffer size %lu / %lu. Want to add %lu events.",
	b.id,b.buffer.size(),b.buffer.capacity(),nevents);

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
	b.buffer.front().Time_TS0(),
	b.buffer.back().Time_TS0());
  TRACE(TR_DEBUG,"Want to add %lu events with times [%u,%u].",nevents,
	ZMQBufferUPtr[begin_index].Time_TS0(),
	ZMQBufferUPtr[begin_index+nevents-1].Time_TS0());
	
  TRACE(TR_DEBUG,"Before sort, here's contents of buffer:");
  TRACE(TR_DEBUG,"============================================");
  for(size_t i_e=0; i_e<b.buffer.size(); ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u",i_e,b.buffer.at(i_e).Time_TS0());
  TRACE(TR_DEBUG,"--------------------------------------------");
  for(size_t i_e=begin_index; i_e<begin_index+nevents; ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u",i_e,ZMQBufferUPtr[i_e].Time_TS0());
  TRACE(TR_DEBUG,"============================================");

  BernZMQEvent* last_event_ptr = &(ZMQBufferUPtr[begin_index]);
  BernZMQEvent* this_event_ptr = last_event_ptr;

  size_t good_events=1;
  while(good_events<nevents){
    this_event_ptr = &(ZMQBufferUPtr[good_events+begin_index]);

    //if times not in order ...
    if(this_event_ptr->Time_TS0() <= last_event_ptr->Time_TS0())
      {

	// ... and the current is not an overflow or prev is not reference
	// then we need to break out of this.
	if( !(last_event_ptr->IsReference_TS0() || this_event_ptr->IsOverflow_TS0()) )
	  break;
      }

    //if time difference is too large
    else
      if( (this_event_ptr->Time_TS0()-last_event_ptr->Time_TS0())>b.max_time_diff )
	break;
    

    last_event_ptr = this_event_ptr;
    ++good_events;
  }

  //note, the order here is important. the buffer with events needs to be last, as that's
  //what is used later for the filling process to determing number of events. 
  //determining number of events is an unlocked procedure

  timeb time_poll_finished = *((timeb*)((char*)(ZMQBufferUPtr[total_events-1].adc)+sizeof(int)+sizeof(struct timeb)));

  TRACE(TR_DEBUG,"Last event looks like \n %s",ZMQBufferUPtr[total_events-1].c_str());
  TRACE(TR_DEBUG,"Time is %ld, %hu",time_poll_finished.time,time_poll_finished.millitm);

  timenow.tv_sec = time_poll_finished.time;
  timenow.tv_usec = time_poll_finished.millitm*1000;

  b.timebuffer.insert(b.timebuffer.end(),good_events,timenow);
  b.droppedbuffer.insert(b.droppedbuffer.end(),good_events-1,0);
  b.droppedbuffer.insert(b.droppedbuffer.end(),1,nevents-good_events);
  b.buffer.insert(b.buffer.end(),&(ZMQBufferUPtr[begin_index]),&(ZMQBufferUPtr[good_events+begin_index]));
  
  TRACE(TR_DEBUG,"After insert, here's contents of buffer:");
  TRACE(TR_DEBUG,"============================================");
  for(size_t i_e=0; i_e<b.buffer.size(); ++i_e)
    TRACE(TR_DEBUG,"\t\t %lu : %u",i_e,b.buffer.at(i_e).Time_TS0());
  if(good_events!=nevents)
    TRACE(TR_DEBUG,"\tWE DROPPED %lu EVENTS.",nevents-good_events);
  TRACE(TR_DEBUG,"============================================");
  
  return b.buffer.size();
}

size_t bernfebdaq::BernZMQ_GeneratorBase::EraseFromFEBBuffer(FEBBuffer_t & b,
							     size_t const& nevents){
  std::unique_lock<std::mutex> lock(*(b.mutexptr));
  b.droppedbuffer.erase_begin(nevents);
  b.timebuffer.erase_begin(nevents);
  b.correctedtimebuffer.erase_begin(nevents);
  b.buffer.erase_begin(nevents);
  return b.buffer.size();
}

bool bernfebdaq::BernZMQ_GeneratorBase::GetData()
{
  TRACE(TR_GD_LOG,"BernFeb::GetData() called");

  if( GetDataSetup()!=1 ) return false;;

  
  //this fills the data from the ZMQ buffer
  size_t total_events = GetZMQData()/sizeof(BernZMQEvent);

  TRACE(TR_GD_DEBUG,"\tBernZMQ::GetData() got %lu total events",total_events);

  if(total_events>0){
    
    size_t i_e=0;
    size_t this_n_events=0;
    uint64_t prev_mac = (FEBIDs_[0] & 0xffffffffffffff00) + ZMQBufferUPtr[0].MAC5();
    size_t new_buffer_size = 0;

    TRACE(TR_GD_DEBUG,"\tBernZMQ::GetData() start sorting with mac=0x%lx",prev_mac);

    while(i_e<total_events){
      
      auto const& this_event = ZMQBufferUPtr[i_e];
      if((prev_mac&0xff)!=this_event.MAC5()){

	TRACE(TR_GD_DEBUG,"\tBernZMQ::GetData() found new MAC (0x%x)! prev_mac=0x%lx, iterator=%lu this_events=%lu",
	      this_event.MAC5(),(prev_mac&0xff),i_e,this_n_events);

	new_buffer_size = InsertIntoFEBBuffer(FEBBuffers_[prev_mac],i_e-this_n_events,this_n_events,total_events);

	TRACE(TR_GD_DEBUG,"\tBernZMQ::GetData() ... id=0x%lx, n_events=%lu, buffer_size=%lu",
	      FEBBuffers_[prev_mac].id,this_n_events,FEBBuffers_[prev_mac].buffer.size());

	auto id_str = GetFEBIDString(prev_mac);	
	metricMan_->sendMetric("EventsAdded_"+id_str,this_n_events,"events",5,true,"BernZMQGenerator");
	UpdateBufferOccupancyMetrics(prev_mac,new_buffer_size);

	this_n_events=0;
      }

      prev_mac = (prev_mac & 0xffffffffffffff00) + this_event.MAC5();
      ++i_e; ++this_n_events;
    }

  }

  metricMan_->sendMetric("TotalEventsAdded",total_events-1,"events",5,true,"BernZMQGenerator");

  if(total_events>0) return true;
  return false;
}

bool bernfebdaq::BernZMQ_GeneratorBase::FillFragment(uint64_t const& feb_id,
						     artdaq::FragmentPtrs & frags,
						     bool clear_buffer)
{
  TRACE(TR_FF_LOG,"BernZMQ::FillFragment(id=0x%lx,frags,clear=%d) called",feb_id,clear_buffer);

  auto & feb = (FEBBuffers_[feb_id]);

  if(!clear_buffer && feb.buffer.size()<3) {
    TRACE(TR_FF_LOG,"BernZMQ::FillFragment() completed, buffer (mostly) empty.");
    return false;
  }

  size_t buffer_end = feb.buffer.size();

  TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : Fragment Searching. Total events in buffer=%lu.",
	buffer_end);

  double time_correction = -1.0;

  //find GPS pulse and count wraparounds
  size_t i_gps=buffer_end;
  size_t n_wraparounds=0;
  uint32_t last_time =0;
  size_t i_e;
  for(i_e=0; i_e<buffer_end; ++i_e){

    auto const& this_event = feb.buffer[i_e];
    

    if(this_event.IsReference_TS0()){
      TRACE(TR_FF_LOG,"BernZMQ::FillFragment() Found reference pulse at i_e=%lu, time=%u",
	    i_e,this_event.Time_TS0());
      i_gps=i_e;
    }
    else if(this_event.Time_TS0()<last_time && this_event.IsOverflow_TS0()){
      n_wraparounds++;
      TRACE(TR_FF_LOG,"BernZMQ::FillFragment() Found wraparound pulse at i_e=%lu, time=%u (last_time=%u), n=%lu",
	    i_e,this_event.Time_TS0(),last_time,n_wraparounds);
    }
    feb.correctedtimebuffer[i_e] = (this_event.Time_TS0()+(uint64_t)n_wraparounds*0x40000000);
    last_time = this_event.Time_TS0();

    if(i_gps!=buffer_end) break;
  }

  if(i_gps==buffer_end){
    TRACE(TR_FF_LOG,"BernZMQ::FillFragment() completed, buffer not empty, but waiting for GPS pulse.");
    return false;
  }
    
  //determine how much time passed. should be close to corrected time.
  //then determine the correction.
  uint32_t elapsed_secs = (feb.correctedtimebuffer[i_gps]/1e9);
  if(feb.correctedtimebuffer[i_gps]%1000000000 > 500000000)
    elapsed_secs+=1;
  time_correction = 1.0e9*(double)elapsed_secs / (double)(feb.correctedtimebuffer[i_gps]);
  
  TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : time correction is %lf, with elapsed_sec=%u and corrected_time=%lu",
	time_correction,elapsed_secs,feb.correctedtimebuffer[i_gps]);

  //ok, so now, determine the nearest second for the last event (closest to one second), based on ntp time
  //get the usecs from the timeval
  auto gps_timeval = feb.timebuffer.at(i_gps);
  
  TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : GPS time was at %ld sec and %ld usec",
	gps_timeval.tv_sec,gps_timeval.tv_usec);

  //report remainder as a metric to watch for
  std::string id_str = GetFEBIDString(feb_id);
  metricMan_->sendMetric("BoundaryTimeRemainder_"+id_str,(float)(gps_timeval.tv_usec),"microseconds",false,"BernZMQGenerator");
  
  //round the boundary time to the nearest second.
  //ROUND DOWN!!
  //if(gps_timeval.tv_usec > 5e5)
  //gps_timeval.tv_sec+=1;
  gps_timeval.tv_usec = 0;



  uint32_t frag_begin_time_s = gps_timeval.tv_sec-elapsed_secs;
  uint32_t frag_begin_time_ns = 0;
  uint32_t frag_end_time_s = gps_timeval.tv_sec-elapsed_secs;
  uint32_t frag_end_time_ns = SequenceTimeWindowSize_;
  
  if(SeqIDMinimumSec_==0){
    SeqIDMinimumSec_ = frag_begin_time_s - 10;
  }

  i_e=0;
  last_time=0;
  uint64_t time_offset=0;
  size_t i_b=0;
  while(frag_end_time_s < gps_timeval.tv_sec){

    if(frag_end_time_ns>=1000000000){
      frag_end_time_ns = frag_end_time_ns%1000000000;
      frag_end_time_s+=1;
    }

    if(frag_begin_time_ns>=1000000000){
      frag_begin_time_ns = frag_begin_time_ns%1000000000;
      frag_begin_time_s+=1;
    }

    //ms since Jan 1 2015
    uint32_t seq_id = (frag_begin_time_s-SeqIDMinimumSec_)*1000 + (frag_begin_time_ns/1000000);

    //make metadata object
    BernZMQFragmentMetadata metadata(frag_begin_time_s,frag_begin_time_ns,
				     frag_end_time_s,frag_end_time_ns,
				     time_correction,time_offset,
				     RunNumber_,
				     seq_id,
				     feb_id, ReaderID_,
				     nChannels_,nADCBits_);

    TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : looking for events in frag %u,%u (%u ms)",
	  frag_begin_time_s,frag_begin_time_ns,seq_id);
    
    while(i_e<i_gps+1){
      
      auto const& this_corrected_time = feb.correctedtimebuffer[i_e];
      auto const& this_event = feb.buffer[i_e];
      
      TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : event %lu, time=%u, corrected_time=%ld",
	    i_e,this_event.Time_TS0(),this_corrected_time);
      
      if( (double)this_corrected_time*time_correction > (frag_end_time_s-(gps_timeval.tv_sec-elapsed_secs))*1e9+frag_end_time_ns )
	break;


      if(this_event.Time_TS0()<last_time)
	time_offset += 0x40000000;

      /*
      if(this_event.flags.overwritten > feb.overwritten_counter)
	metadata.increment(this_event.flags.missed,this_event.flags.overwritten-feb.overwritten_counter,feb.droppedbuffer[i_e]);
      else
	metadata.increment(this_event.flags.missed,0,feb.droppedbuffer[i_e]);
      feb.overwritten_counter = this_event.flags.overwritten;
      */
      metadata.increment(0,0,feb.droppedbuffer[i_e]);

      ++i_e;
    }
    
    //great, now add the fragment on the end.`<
    frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata.n_events()*sizeof(BernZMQEvent),  
							metadata.sequence_number(),feb_id,
							bernfebdaq::detail::FragmentType::BernZMQ, metadata) );
    std::copy(feb.buffer.begin()+i_b,feb.buffer.begin()+i_e,(BernZMQEvent*)(frags.back()->dataBegin()));

    TRACE(TR_FF_DEBUG,"BernZMQ::FillFragment() : Fragment created. First event in fragment: %s",
	  ((BernZMQEvent*)(frags.back()->dataBegin()))->c_str() );
    
    TRACE(TR_FF_LOG,"BernZMQ::FillFragment() : Fragment created. Events=%u. Metadata : %s",
	  metadata.n_events(),metadata.c_str());
        
    SendMetadataMetrics(metadata);


    i_b=i_e; //new beginning...
    frag_begin_time_ns += SequenceTimeWindowSize_;
    frag_end_time_ns += SequenceTimeWindowSize_;
  }

  TRACE(TR_FF_DEBUG,"BernZMQ::FillFragment() : Buffer size before erase = %lu",feb.buffer.size());
  size_t new_buffer_size = EraseFromFEBBuffer(feb,i_gps+1);
  TRACE(TR_FF_DEBUG,"BernZMQ::FillFragment() : Buffer size after erase = %lu",feb.buffer.size());

  metricMan_->sendMetric("FragmentsBuilt_"+id_str,1.0,"events",5,true,"BernZMQGenerator");
  UpdateBufferOccupancyMetrics(feb_id,new_buffer_size);

  return true;
}

void bernfebdaq::BernZMQ_GeneratorBase::SendMetadataMetrics(BernZMQFragmentMetadata const& m) {
  std::string id_str = GetFEBIDString(m.feb_id());
  metricMan_->sendMetric("FragmentLastTime_"+id_str,(uint64_t)(m.time_end_seconds()*1000000000+m.time_end_nanosec()),"ns",5,false,"BernZMQGenerator");
  metricMan_->sendMetric("EventsInFragment_"+id_str,(float)(m.n_events()),"events",5,true,"BernZMQGenerator");
  metricMan_->sendMetric("MissedEvents_"+id_str,     (float)(m.missed_events()),     "events",5,true,"BernZMQGenerator");
  metricMan_->sendMetric("OverwrittenEvents_"+id_str,(float)(m.overwritten_events()),"events",5,true,"BernZMQGenerator");
  float eff=1.0;
  if((m.n_events()+m.missed_events()+m.overwritten_events())!=0)
    eff = (float)(m.n_events()) / (float)(m.n_events()+m.missed_events()+m.overwritten_events());
  
  metricMan_->sendMetric("Efficiency_"+id_str,eff*100.,"%",5,true,"BernZMQGenerator");
}

bool bernfebdaq::BernZMQ_GeneratorBase::getNext_(artdaq::FragmentPtrs & frags) {
  
  TRACE(TR_FF_LOG,"BernZMQ::getNext_(frags) called");
  
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

  TRACE(TR_FF_LOG,"BernZMQ::getNext_(frags) completed");

  //BernZMQFragment bb(*frags.back());
  if(frags.size()!=0) TRACE(TR_FF_DEBUG,"BernZMQ::geNext_() : last fragment is: %s",(BernZMQFragment(*frags.back())).c_str());

  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
//DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(demo::BernZMQ) 
