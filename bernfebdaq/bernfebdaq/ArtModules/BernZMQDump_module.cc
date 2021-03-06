////////////////////////////////////////////////////////////////////////
// Class:       BernZMQDump
// Module Type: analyzer
// File:        BernZMQDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

//#include "art/Utilities/Exception.h"
#include "bernfebdaq-core/Overlays/BernZMQFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>

namespace bernfebdaq {
  class BernZMQDump;
}

class bernfebdaq::BernZMQDump : public art::EDAnalyzer {
public:
  explicit BernZMQDump(fhicl::ParameterSet const & pset);
  virtual ~BernZMQDump();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
  int         verbosity_;
  bool        TestPulseMode_;
};


bernfebdaq::BernZMQDump::BernZMQDump(fhicl::ParameterSet const & pset)
  : EDAnalyzer(pset),
    raw_data_label_(pset.get<std::string>("raw_data_label")),
    verbosity_(pset.get<int>("verbosity",2)),
    TestPulseMode_(pset.get<bool>("TestPulseMode",false))
{
}

bernfebdaq::BernZMQDump::~BernZMQDump()
{
}

void bernfebdaq::BernZMQDump::analyze(art::Event const & evt)
{
  
  art::EventNumber_t eventNumber = evt.event();
  
  // ***********************
  // *** BernZMQ Fragments ***
  // ***********************
  
  // look for raw BernZMQ data
  
  art::Handle< std::vector<artdaq::Fragment> > raw;
  evt.getByLabel(raw_data_label_, "BernZMQ", raw);
  
  if(!raw.isValid()){
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " BernZMQ fragments " << " in module " << raw_data_label_ << std::endl;
    std::cout << std::endl;
    return;
  }
  
  if(verbosity_>0){
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has " << raw->size()
	      << " BernZMQ fragment(s)." << std::endl;
  } 

  size_t n_events=0;
  bool event_mismatch_error = false;
  for (size_t idx = 0; idx < raw->size(); ++idx) {
    const auto& frag((*raw)[idx]);
    
    BernZMQFragment bb(frag);
    auto bbm = bb.metadata();

    if(TestPulseMode_){
      if(idx==0)
	n_events=bbm->n_events()+bbm->dropped_events();
      else
	if((bbm->n_events()+bbm->dropped_events())!=n_events)
	  event_mismatch_error = true;
    }

    if(verbosity_==1)
      std::cout << "\tFragment ID=0x" << std::hex << bbm->feb_id() << std::dec
		<< " Time=[  (" << bbm->time_start_seconds() << "," << bbm->time_start_nanosec() << ") , ("
		<< bbm->time_end_seconds() << "," << bbm->time_end_nanosec() << ")  )"
		<< " (Events,Missed,Overwritten,Dropped)=(" << bbm->n_events() << "," 
		<< bbm->missed_events() << "," << bbm->overwritten_events() << bbm->dropped_events() << ")"
		<< std::endl;
    else if(verbosity_==2)
      std::cout << *bbm << std::endl;
    else if(verbosity_>2)
      std::cout << bb << std::endl;
  }


  if(TestPulseMode_ && event_mismatch_error){
    for (size_t idx = 0; idx < raw->size(); ++idx) {
      const auto& frag((*raw)[idx]);
      
      BernZMQFragment bb(frag);
      std::cout << bb << std::endl;
    }
  }

}

DEFINE_ART_MODULE(bernfebdaq::BernZMQDump)
