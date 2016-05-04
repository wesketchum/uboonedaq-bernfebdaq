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
  Initialize();
}

void bernfebdaq::BernFEB_GeneratorBase::Initialize(){

  std::cout << "INITIALIZING!" << std::endl;

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
  
  std::cout << "DONE BASE INITIALIZING!" << std::endl;


}

void bernfebdaq::BernFEB_GeneratorBase::start() {

  std::cout << "START PROCESS!!!!" << std::endl;

  current_subrun_ = 0;

  ConfigureStart();
  for(auto const& id : FEBIDs_)
    FEBDequeBuffers_[id] = FEBBuffer_t();
  std::cout << "Make buffer?" << std::endl;
  FEBDTPBufferUPtr.reset(new BernFEBEvent[FEBDTPBufferCapacity_]);

  std::cout << "GONNA START THREAD!!!!" << std::endl;
  ThreadFunctor functor = std::bind(&BernFEB_GeneratorBase::GetData,this);
  auto worker_functor = WorkerThreadFunctorUPtr(new WorkerThreadFunctor(functor,"GetDataWorkerThread"));
  auto getData_worker = WorkerThread::createWorkerThread(worker_functor);
  GetData_thread_.swap(getData_worker);

  GetData_thread_->start();

  std::cout << "GONNA START!!!!" << std::endl;
}

void bernfebdaq::BernFEB_GeneratorBase::stop() {
  GetData_thread_->stop();
  ConfigureStop();
}

void bernfebdaq::BernFEB_GeneratorBase::Cleanup(){
}

bernfebdaq::BernFEB_GeneratorBase::~BernFEB_GeneratorBase(){
  Cleanup();
}

bool bernfebdaq::BernFEB_GeneratorBase::GetData()
{
  std::cout << "Running GetData" << std::endl;
  size_t this_n_events=0,n_events=0;
  for(auto const& id : FEBIDs_){
    this_n_events = GetFEBData(id)/sizeof(BernFEBEvent);
    FEBDequeBuffers_[id].buffer.insert(FEBDequeBuffers_[id].buffer.end(),&(FEBDTPBufferUPtr[0]),&(FEBDTPBufferUPtr[this_n_events]));
    n_events += this_n_events;
  }
  
  if(n_events>0) return true;
  return false;
}

bool bernfebdaq::BernFEB_GeneratorBase::FillFragment(uint64_t const& feb_id,
						     artdaq::FragmentPtrs & frags,
						     bool clear_buffer)
{

  auto & feb = FEBDequeBuffers_[feb_id];

  if(!clear_buffer && feb.buffer.size()==0) return false;

  //std::cout << "Running FillFragment 0x" << std::hex << feb_id << std::dec << std::endl;


  BernFEBFragmentMetadata metadata(feb.next_time_start,
				   feb.next_time_start+SequenceTimeWindowSize_,
				   RunNumber_,
				   feb.next_time_start/SubrunTimeWindowSize_,
				   feb.next_time_start/SequenceTimeWindowSize_,
				   feb_id, ReaderID_,
				   nChannels_,nADCBits_);

  std::cout << metadata << std::endl;
				   
  int64_t time = 0;
  size_t  local_time_resets = 0;
  int32_t local_last_time = feb.last_time_counter;
  bool found_fragment=false;

  auto it_start_fragment = feb.buffer.cbegin();
  auto it_end_fragment=feb.buffer.cend();

  //just find the time boundary first
  for(auto it=it_start_fragment; it!=it_end_fragment;++it){

    //std::cout << "Checking time..." << std::endl;
    std::cout << *it << std::endl;

    if((int64_t)it->time1 <= local_last_time)
      ++local_time_resets;
    local_last_time = it->time1;

    //std::cout << "Time is " << it->time1 << " " << feb.time_resets << " " << local_time_resets << std::endl;

    time = it->time1+(feb.time_resets+local_time_resets)*1e9;

    if(time<feb.next_time_start)
      std::cout << "ERROR!!!! THIS IS BAD!!!! " << time << " " << feb.next_time_start << std::endl;
    else if(time>feb.next_time_start+SequenceTimeWindowSize_){
      std::cout << "Found a new time!! " << time << " " << feb.next_time_start+SequenceTimeWindowSize_ << std::endl;
      it_end_fragment = it;
      found_fragment = true;
      break;
    }
  }

  //didn't get our last event in this time window ... return
  //if(!clear_buffer && it_end_fragment==feb.buffer.cend()) {
  if(!clear_buffer && !found_fragment){
    std::cout << "Didn't get all our time!" << std::endl;
    return false;
  }

  std::cout << "WE GOT ALL OUR TIME! " << it_start_fragment->time1 << " " << it_end_fragment->time1 << std::endl;


  //ok, queue was non-empty, and we saw our last event. Need to loop through and do proper accounting now.
  local_time_resets = 0;
  local_last_time = feb.last_time_counter;
  for(auto it=it_start_fragment; it!=it_end_fragment; ++it){

    if((int32_t)it->time1 <= feb.last_time_counter)
      ++feb.time_resets;
    feb.last_time_counter = it->time1;
    
    if(it->flags.overwritten > feb.overwritten_counter)
      metadata.increment(it->flags.missed,it->flags.overwritten-feb.overwritten_counter);
    else
      metadata.increment(it->flags.missed,0);
    feb.overwritten_counter = it->flags.overwritten;

  }
  feb.next_time_start = feb.next_time_start+SequenceTimeWindowSize_;

  std::cout << "Constructed fragment. Current buffer size is " << feb.buffer.size() << std::endl;
  std::cout << metadata << std::endl;
  std::cout << "First fragment has time " << it_start_fragment->time1 << std::endl;

  //great, now add the fragment on the end.
  frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata.n_events()*sizeof(BernFEBEvent),  
						      metadata.sequence_number(),ReaderID_,
						      bernfebdaq::detail::FragmentType::BernFEB, metadata_) );
  std::copy(it_start_fragment,it_end_fragment,(BernFEBEvent*)(frags.back()->dataBegin()));

  std::cout << "First fragment copied has time " << it_start_fragment->time1 << std::endl;
  std::cout << *((BernFEBEvent*)(frags.back()->dataBegin())) << std::endl;

  feb.buffer.erase(it_start_fragment,it_end_fragment);
  std::cout << "New first fragment has time " << feb.buffer.cbegin()->time1 << std::endl;

  std::cout << "Finished fragment. Current buffer size is " << feb.buffer.size() << std::endl;

  return true;
}

bool bernfebdaq::BernFEB_GeneratorBase::getNext_(artdaq::FragmentPtrs & frags) {

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
  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
//DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(demo::BernFEB) 
