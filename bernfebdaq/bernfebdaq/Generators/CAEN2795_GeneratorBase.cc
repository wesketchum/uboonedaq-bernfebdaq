#include "icartdaq/Generators/CAEN2795_GeneratorBase.hh"

#include "art/Utilities/Exception.h"
//#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "icartdaq-core/Overlays/FragmentType.hh"
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


icarus::CAEN2795_GeneratorBase::CAEN2795_GeneratorBase(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  ps_(ps)
{
  Initialize();
}

void icarus::CAEN2795_GeneratorBase::Initialize(){

  RunNumber_ = ps_.get<uint32_t>("RunNumber",999);
  EventsPerSubrun_ = ps_.get<uint32_t>("EventsPerSubrun",10);
  SamplesPerChannel_ = ps_.get<uint32_t>("SamplesPerChannel",3000);
  nADCBits_ = ps_.get<uint8_t>("nADCBits",12);
  ChannelsPerBoard_ = ps_.get<uint16_t>("ChannelsPerBoard",64);
  nBoards_ = ps_.get<uint16_t>("nBoards",9);
  CrateID_ = ps_.get<uint8_t>("CrateID",0x1);
  BoardIDs_ = ps_.get< std::vector<CAEN2795FragmentMetadata::id_t> >("BoardIDs");

  std::cout << "Board IDs: ";
  for(auto const& id : BoardIDs_)
    std::cout << id  << " ";
  std::cout << std::endl;

  throttle_usecs_ = ps_.get<size_t>("throttle_usecs", 100000);
  throttle_usecs_check_ = ps_.get<size_t>("throttle_usecs_check", 10000);
  metadata_ = CAEN2795FragmentMetadata(RunNumber_,0,0,
				       SamplesPerChannel_,nADCBits_,
				       ChannelsPerBoard_,nBoards_,
				       CrateID_,BoardIDs_);


  if (throttle_usecs_ > 0 && (throttle_usecs_check_ >= throttle_usecs_ ||
			      throttle_usecs_ % throttle_usecs_check_ != 0) ) {
    throw cet::exception("Error in CAEN2795: disallowed combination of throttle_usecs and throttle_usecs_check (see CAEN2795.hh for rules)");
  }
  

}

void icarus::CAEN2795_GeneratorBase::start() {
  current_subrun_ = 0;
  ConfigureStart();
}

void icarus::CAEN2795_GeneratorBase::stop() {
  ConfigureStop();
}

void icarus::CAEN2795_GeneratorBase::Cleanup(){
}

icarus::CAEN2795_GeneratorBase::~CAEN2795_GeneratorBase(){
  Cleanup();
}

bool icarus::CAEN2795_GeneratorBase::getNext_(artdaq::FragmentPtrs & frags) {

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
  
  // Set fragment's metadata

  std::cout << "Starting up ... " << std::endl;

  event_number_ = ev_counter();
  if(event_number_%EventsPerSubrun_==0) ++current_subrun_;

  //CAEN2795FragmentMetadata m = metadata_;

  metadata_.SetSubrunNumber(current_subrun_);
  metadata_.SetEventNumber(event_number_);
  
  std::cout << metadata_ << std::endl;

  std::cout << "Metadata done ... " << metadata_.ExpectedDataSize() << std::endl;

  std::cout << "Current frags size is " << frags.size() << std::endl;

  frags.emplace_back( artdaq::Fragment::FragmentBytes(metadata_.ExpectedDataSize(),  
						      event_number_, fragment_id(),
						      icarus::detail::FragmentType::CAEN2795, metadata_) );

  std::cout << "Initialized data of size " << frags.back()->dataSizeBytes() << std::endl;

  CAEN2795Fragment newfrag(*frags.back());
  /*
  std::cout << "frags.back()->hasMetadata(): " << frags.back()->hasMetadata() << std::endl;
  std::cout << "frags.back()->sizeBytes(): " << frags.back()->sizeBytes() << std::endl;
  std::cout << "frags.back()->dataSizeBytes(): " << frags.back()->dataSizeBytes() << std::endl;
  std::cout << "static_cast<void*>( frags.back()->headerBeginBytes() ): " << static_cast<void*>( frags.back()->headerBeginBytes() ) << std::endl;
  std::cout << "static_cast<void*>( frags.back()->dataBeginBytes() ): " << static_cast<void*>( frags.back()->dataBeginBytes() ) << std::endl;
  if(frags.back()->hasMetadata())
    std::cout << "static_cast<void*>( frags.back()->metadata() ): " << static_cast<void*>( frags.back()->metadata<CAEN2795Fragment::Metadata>() ) << std::endl;
  std::cout << "newfrag.dataTotalBegin(): " << newfrag.dataTotalBegin() << std::endl;
  */

  last_status_ = GetData(last_read_data_size_,(uint32_t*)(frags.back()->dataBeginBytes()));

  std::cout << "Read data status was " << last_status_ << std::endl;
  std::cout << "Read data size was " << last_read_data_size_ << std::endl;


  /*
  if(metricMan_ != nullptr) {
    metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  }
  */
  ev_counter_inc();

  std::cout << "Done." << std::endl;
  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
//DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(demo::CAEN2795) 
