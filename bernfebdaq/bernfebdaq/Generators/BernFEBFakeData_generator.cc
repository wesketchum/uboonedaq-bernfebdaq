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
  events_per_packet_ = ps_.get<size_t>("events_per_packet",5);
  time_increment_per_event_ = ps_.get<unsigned int>("time_increment_per_event",49999);

  missing_events_mod_ = ps_.get<std::vector<unsigned int>>("missing_events_mod",{3,7,23});

  time1_max_ = ps_.get<uint32_t>("time1_max",1e9);
  time2_max_ = ps_.get<uint32_t>("time1_max",4e9);

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

  BernFEBEvent feb_data;
  for(size_t i_e=0; i_e<events_per_packet_; ++i_e){

    feb_data.flags.overwritten = 0;
    feb_data.flags.missed = 0;

    for(auto const& mod : missing_events_mod_) 
      if(i_e%mod==0) feb_data.flags.missed++;

    feb_data.time1.rawts = time_map_[feb_id] % time1_max_;
    feb_data.time2.rawts = time_map_[feb_id] % time2_max_;
    time_map_[feb_id] += time_increment_per_event_;

    std::generate_n(feb_data.adc,
		    nChannels_,
		    [&](){ return static_cast<uint16_t>((*uniform_distn_)( engine_)); });
    FEBDTPBufferUPtr[i_e] = feb_data;

    TRACE(TR_GD_DEBUG,"\n\tBernFEBFakeData::GetData() : Generated event %s",
	  feb_data.c_str());
  }

  TRACE(TR_GD_LOG,"BernFEBFakeData::GetFEBData() completed, events=%lu, data_size=%lu",
	events_per_packet_,events_per_packet_*sizeof(BernFEBEvent));  

  return events_per_packet_*sizeof(BernFEBEvent);
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(bernfebdaq::BernFEBFakeData) 
