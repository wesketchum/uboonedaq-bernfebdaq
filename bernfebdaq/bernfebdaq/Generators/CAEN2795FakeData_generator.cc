#include "icartdaq/Generators/CAEN2795FakeData.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


icarus::CAEN2795FakeData::CAEN2795FakeData(fhicl::ParameterSet const & ps)
  :
  CAEN2795_GeneratorBase(ps)
{
  Initialize();
  start();
}

icarus::CAEN2795FakeData::~CAEN2795FakeData(){
  stop();
  Cleanup();
}

void icarus::CAEN2795FakeData::ConfigureStart(){
  engine_ = std::mt19937(ps_.get<int64_t>("random_seed", 314159));
  uniform_distn_.reset(new std::uniform_int_distribution<int>(0, std::pow(2,metadata_.num_adc_bits() - 1 )));
}

void icarus::CAEN2795FakeData::ConfigureStop(){}

int icarus::CAEN2795FakeData::GetData(size_t & data_size, uint32_t* data_loc){

  data_size=0;

  for(size_t i_b=0; i_b<metadata_.num_boards(); ++i_b){
    size_t offset = i_b*(2+metadata_.channels_per_board()*metadata_.samples_per_channel()/2);
    data_loc[offset]   = (metadata_.event_number() & 0xffffff);
    data_loc[offset+1] = metadata_.event_number()+100;
    std::generate_n((uint16_t*)(&(data_loc[offset+2])),
		    metadata_.channels_per_board()*metadata_.samples_per_channel(),
		    [&](){ return static_cast<uint16_t>((*uniform_distn_)( engine_)); });
    data_size += (metadata_.channels_per_board()*metadata_.samples_per_channel()/2 + 2)*sizeof(uint32_t);
  }

  return 0;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(icarus::CAEN2795FakeData) 
