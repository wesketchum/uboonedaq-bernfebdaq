////////////////////////////////////////////////////////////////////////
// Class:       BernFEBDump
// Module Type: analyzer
// File:        BernFEBDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Utilities/Exception.h"
#include "bernfebdaq-core/Overlays/BernFEBFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>

namespace bernfebdaq {
  class BernFEBDump;
}

class bernfebdaq::BernFEBDump : public art::EDAnalyzer {
public:
  explicit BernFEBDump(fhicl::ParameterSet const & pset);
  virtual ~BernFEBDump();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
};


bernfebdaq::BernFEBDump::BernFEBDump(fhicl::ParameterSet const & pset)
  : EDAnalyzer(pset),
    raw_data_label_(pset.get<std::string>("raw_data_label"))
{
}

bernfebdaq::BernFEBDump::~BernFEBDump()
{
}

void bernfebdaq::BernFEBDump::analyze(art::Event const & evt)
{
  
  art::EventNumber_t eventNumber = evt.event();
  
  // ***********************
  // *** BernFEB Fragments ***
  // ***********************
  
  // look for raw BernFEB data
  
  art::Handle< std::vector<artdaq::Fragment> > raw;
  evt.getByLabel(raw_data_label_, "BernFEB", raw);
  
  if(!raw.isValid()){
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " BernFEB fragments " << " in module " << raw_data_label_ << std::endl;
    std::cout << std::endl;
    return;
  }
  
  
  std::cout << "######################################################################" << std::endl;
  std::cout << std::endl;
  std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	    << ", event " << eventNumber << " has " << raw->size()
	    << " BernFEB fragment(s)." << std::endl;
  
  for (size_t idx = 0; idx < raw->size(); ++idx) {
    const auto& frag((*raw)[idx]);
    
    BernFEBFragment bb(frag);

    std::cout << bb << std::endl;
  }
}

DEFINE_ART_MODULE(bernfebdaq::BernFEBDump)
