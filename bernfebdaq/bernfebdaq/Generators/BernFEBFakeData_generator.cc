#include "bernfebdaq/Generators/BernFEBFakeData.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "trace_defines.h"

bernfebdaq::BernFEBFakeData::BernFEBFakeData(fhicl::ParameterSet const & ps)
  :
  BernFEB_GeneratorBase(ps)
{
  TRACE(TR_LOG,"BernFEBFakeData constructor called");  
  TRACE(TR_LOG,"BernFEBFakeData constructor completed");  
}

bernfebdaq::BernFEBFakeData::~BernFEBFakeData(){
  TRACE(TR_LOG,"BernFEBFakeData destructor called");  
  Cleanup();
  TRACE(TR_LOG,"BernFEBFakeData destructor completed");  
}

void bernfebdaq::BernFEBFakeData::ConfigureStart(){
  TRACE(TR_LOG,"BernFEBFakeData::ConfigureStart() called");  
  engine_ = std::mt19937(ps_.get<int64_t>("random_seed", 314159));
  uniform_distn_.reset(new std::uniform_int_distribution<int>(0, std::pow(2,nADCBits_ - 1 )));
  //double rate_per_ns = (double)ps_.get<unsigned int>("cosmic_rate",2500)/1.0e9;
  //poisson_distn_.reset(new std::poisson_distribution<int>(rate_per_ns*SequenceTimeWindowSize_));

  data_wait_time_ = ps_.get<int>("data_wait_time",5e5);

  for(auto const& id : FEBIDs_)
    time_map_[id] = 0;

  TRACE(TR_LOG,"BernFEBFakeData::ConfigureStart() completed");  
}

void bernfebdaq::BernFEBFakeData::ConfigureStop(){
  TRACE(TR_LOG,"BernFEBFakeData::ConfigureStop() called");  
  TRACE(TR_LOG,"BernFEBFakeData::ConfigureStop() completed");  
}

size_t bernfebdaq::BernFEBFakeData::GetFEBData(uint64_t const& feb_id){
  TRACE(TR_GD_LOG,"BernFEBFakeData::GetFEBData(0x%lx) called",feb_id);  

  usleep(data_wait_time_);

  size_t n_events = 5;
  
  BernFEBEvent feb_data;
  for(size_t i_e=0; i_e<n_events; ++i_e){
    feb_data.flags_word = 0xdeadbeef;
    feb_data.time1 = time_map_[feb_id] & 0xffffffff;
    feb_data.time2 = time_map_[feb_id] & 0xffff;
    time_map_[feb_id] += 9999;
    std::generate_n(feb_data.adc,
		    nChannels_,
		    [&](){ return static_cast<uint16_t>((*uniform_distn_)( engine_)); });
    FEBDTPBufferUPtr[i_e] = feb_data;

    TRACE(TR_GD_DEBUG,"\n\tBernFEBFakeData::GetData() : Generated event %s",
	  feb_data.c_str());
  }

  TRACE(TR_GD_LOG,"BernFEBFakeData::GetFEBData() completed, events=%lu, data_size=%lu",
	n_events,n_events*sizeof(BernFEBEvent));  

  return n_events*sizeof(BernFEBEvent);
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(bernfebdaq::BernFEBFakeData) 
