#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

#include "art/Utilities/Exception.h"
//#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "bernfebdaq-core/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

//#include "CAENComm.h"
//#include "keyb.h"
//#include "keyb.c"

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

  FEBDequeBufferCapacity_ = ps_.get<uint32_t>("FEBDequeBufferCapacity",5e3);
  FEBDequeBufferSizeBytes_ = FEBDequeBufferCapacity_*sizeof(BernFEBEvent);

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


}

void bernfebdaq::BernFEB_GeneratorBase::start() {

  TRACE(TR_LOG,"BernFeb::start() called");

  current_subrun_ = 0;

  ConfigureStart();
  for(auto const& id : FEBIDs_)
    FEBDequeBuffers_[id] = FEBBuffer_t();
  TRACE(TR_DEBUG,"\tMade %lu FEBDequeBuffers",FEBIDs_.size());

  FEBDTPBufferUPtr.reset(new BernFEBEvent[FEBDTPBufferCapacity_]);

  TRACE(TR_LOG,"BernFeb::start() ... starting GetData worker thread.");
  ThreadFunctor functor = std::bind(&BernFEB_GeneratorBase::GetData,this);
  auto worker_functor = WorkerThreadFunctorUPtr(new WorkerThreadFunctor(functor,"GetDataWorkerThread"));
  auto getData_worker = WorkerThread::createWorkerThread(worker_functor);
  GetData_thread_.swap(getData_worker);

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

bool bernfebdaq::BernFEB_GeneratorBase::GetData()
{
  TRACE(TR_GD_LOG,"BernFeb::GetData() called");

  size_t this_n_events=0,n_events=0;
  for(auto const& id : FEBIDs_){
    this_n_events = GetFEBData(id)/sizeof(BernFEBEvent);
    FEBDequeBuffers_[id].buffer.insert(FEBDequeBuffers_[id].buffer.end(),&(FEBDTPBufferUPtr[0]),&(FEBDTPBufferUPtr[this_n_events]));
    n_events += this_n_events;

    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, n_events=%lu",id,this_n_events);
    TRACE(TR_GD_DEBUG,"\tBernFeb::GetData() ... id=0x%lx, buffer_size=%lu",id,FEBDequeBuffers_[id].buffer.size());

  }
  
  TRACE(TR_GD_LOG,"BernFeb::GetData() completed, n_events=%lu",n_events);

  if(n_events>0) return true;
  return false;
}

bool bernfebdaq::BernFEB_GeneratorBase::FillFragment(uint64_t const& feb_id,
						     artdaq::FragmentPtrs & frags,
						     bool clear_buffer)
{
  TRACE(TR_FF_LOG,"BernFeb::FillFragment(id=0x%lx,frags,clear=%d) called",feb_id,clear_buffer);

  auto & feb = FEBDequeBuffers_[feb_id];

  if(!clear_buffer && feb.buffer.size()==0) {
    TRACE(TR_FF_LOG,"BernFeb::FillFragment() completed, buffer empty.");
    return false;
  }

  BernFEBFragmentMetadata metadata(feb.next_time_start,
				   feb.next_time_start+SequenceTimeWindowSize_,
				   RunNumber_,
				   feb.next_time_start/SubrunTimeWindowSize_,
				   feb.next_time_start/SequenceTimeWindowSize_ + 1,
				   feb_id, ReaderID_,
				   nChannels_,nADCBits_);
  TRACE(TR_FF_DEBUG,
	"\tBernFeb::FillFragment() : Look for data, id=0x%lx, time=[%ld,%ld]",
	feb_id,feb.next_time_start,feb.next_time_start+SequenceTimeWindowSize_);
  //std::cout << metadata << std::endl;
				   
  int64_t time = 0;
  size_t  local_time_resets = 0;
  int32_t local_last_time = feb.last_time_counter;
  bool found_fragment=false;

  auto it_start_fragment = feb.buffer.cbegin();
  auto it_end_fragment=feb.buffer.cend();

  //just find the time boundary first
  for(auto it=it_start_fragment; it!=it_end_fragment;++it){

    TRACE(TR_FF_DEBUG,"\n\tBernFeb::FillFragment() ... found event: %s",it->c_str());

    if((int64_t)it->time1 <= local_last_time)
      ++local_time_resets;
    local_last_time = it->time1;

    time = it->time1+(feb.time_resets+local_time_resets)*1e9;

    TRACE(TR_FF_DEBUG,
	  "\n\tBernFEb::FillFragment() ... ev_time is %u, resets=%lu, local_resets=%lu, time=%ld",
	  it->time1,feb.time_resets,local_time_resets,time);

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
	it_start_fragment->time1,it_end_fragment->time1,std::distance(it_start_fragment,it_end_fragment));


  //ok, queue was non-empty, and we saw our last event. Need to loop through and do proper accounting now.
  local_time_resets = 0;
  local_last_time = feb.last_time_counter;
  for(auto it=it_start_fragment; it!=it_end_fragment; ++it){

    if((int32_t)it->time1 <= feb.last_time_counter){
      ++feb.time_resets;
      TRACE(TR_FF_DEBUG,"\n\tBernFeb::FillFragment() : Time reset (evtime=%u,last_time=%d). Total resets=%lu",
	    it->time1,feb.last_time_counter,feb.time_resets);
    }
    feb.last_time_counter = it->time1;
    
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

  //great, now add the fragment on the end.
  frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata.n_events()*sizeof(BernFEBEvent),  
						      metadata.sequence_number(),ReaderID_,
						      bernfebdaq::detail::FragmentType::BernFEB, metadata) );
  std::copy(it_start_fragment,it_end_fragment,(BernFEBEvent*)(frags.back()->dataBegin()));

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Fragment created. First event in fragment: %s",
	((BernFEBEvent*)(frags.back()->dataBegin()))->c_str() );

  TRACE(TR_FF_LOG,"BernFeb::FillFragment() : Fragment created. Events=%ld. Metadata : %s",
	std::distance(it_start_fragment,it_end_fragment),metadata.c_str());

  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size before erase = %lu",feb.buffer.size());
  feb.buffer.erase(it_start_fragment,it_end_fragment);
  TRACE(TR_FF_DEBUG,"BernFeb::FillFragment() : Buffer size after erase = %lu",feb.buffer.size());
  //std::cout << "New first fragment has time " << feb.buffer.cbegin()->time1 << std::endl;

  return true;
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
