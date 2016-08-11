////////////////////////////////////////////////////////////////////////
// Class:       TimeCoincidence
// Module Type: analyzer
// File:        TimeCoincidence_module.cc
// Description: Find Time Coincidence
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Utilities/Exception.h"
#include "bernfebdaq-core/Overlays/BernFEBFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include "art/Framework/Services/Optional/TFileService.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>

#include "TTree.h"

namespace bernfebdaq {
  class TimeCoincidence;
}

class bernfebdaq::TimeCoincidence : public art::EDAnalyzer {
public:
  explicit TimeCoincidence(fhicl::ParameterSet const & pset);
  virtual ~TimeCoincidence();

  virtual void analyze(art::Event const & evt);

private:
  std::string  raw_data_label_;
  unsigned int max_time_diff_;

  TTree*       my_tree_;
};

//constructor. Initialize configuration parameters.
bernfebdaq::TimeCoincidence::TimeCoincidence(fhicl::ParameterSet const & pset)
  : EDAnalyzer(pset),
    raw_data_label_(pset.get<std::string>("raw_data_label")),
    max_time_diff_(pset.get<unsigned int>("MaxTimeDifference",100))  //matching criteria. default 100
{

  //this is the magic stuff that sets up the tree.
  art::ServiceHandle<art::TFileService> tfs;
  my_tree_ = tfs->make<TTree>("my_tree","CRT Analysis Tree");

}

//destructor. Ain't doing much.
bernfebdaq::TimeCoincidence::~TimeCoincidence()
{
}

//analyze function. This is what gets done once per event.
void bernfebdaq::TimeCoincidence::analyze(art::Event const & evt)
{
  
  // look for raw BernFEB data  
  art::Handle< std::vector<artdaq::Fragment> > rawHandle;
  evt.getByLabel(raw_data_label_, "BernFEB", rawHandle);

  //check to make sure the data we asked for is valid
  if(!rawHandle.isValid()){
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << evt.event() << " has zero"
              << " BernFEB fragments " << " in module " << raw_data_label_ << std::endl;
    std::cout << std::endl;
    return;
  }

  //get better access to the data
  std::vector<artdaq::Fragment> const& rawFragments(*rawHandle);

  //loop over the raw data fragments
  //There should be one fragment per FEB in each event.
  for(auto const& frag : rawFragments){

    //overlay this so it's in the "BernFragment" format. Same data!
    BernFEBFragment bfrag(frag);

    //Grab the metadata.
    //See bernfebdaq-core/bernfebdaq-core/Overlays/BernFEBFragment.hh
    auto bfrag_metadata = bfrag.metadata();

    int64_t  time_begin = bfrag_metadata->time_start(); //start time of this packet of data
    int64_t  time_end   = bfrag_metadata->time_end();   //end time of this packet of data
    uint64_t mac        = bfrag_metadata->feb_id();     //mac addresss of this packet
    size_t   nevents    = bfrag_metadata->n_events();   //number of BernFEBEvents in this packet

    std::cout << "Analyzing " << nevents << " hits from FEB " << std::hex << (mac & 0xff) << std::dec
	      << " from time range [" << time_begin << "," << time_end << ")" << std::endl;

    for(size_t i_e=0; i_e<nevents; ++i_e){
      BernFEBEvent const* this_event = bfrag.eventdata(i_e); //get the single hit/event
      
      BernFEBTimeStamp gps_time = this_event->time1;         //grab the event time

      //let's get channel and value for max adc count
      uint16_t max_adc_value=0; int max_adc_channel=0;
      for(size_t i_chan=0; i_chan<32; ++i_chan)
	if(this_event->adc[i_chan]>max_adc_value) //grab the adc value in each channel
	  { max_adc_value=this_event->adc[i_chan]; max_adc_channel=i_chan; }

      std::cout << "\tEvent " << i_e << " was at time " << gps_time.Time() 
		<< ". Max adc value = " << max_adc_value << " on channel " << max_adc_channel
		<< std::endl;
      
    }//end loop over events for this FEB in this packet

  }//end loop over all fragments/FEBs in the event
  

}//end analyze, run once per event

DEFINE_ART_MODULE(bernfebdaq::TimeCoincidence)
