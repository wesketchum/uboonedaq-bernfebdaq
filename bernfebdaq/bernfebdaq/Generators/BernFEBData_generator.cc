#include "bernfebdaq/Generators/BernFEBData.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "trace_defines.h"

bernfebdaq::BernFEBData::BernFEBData(fhicl::ParameterSet const & ps)
  :
  BernFEB_GeneratorBase(ps)
{
  TRACE(TR_LOG,"BernFEBData constructor called");  

  auto eth_interface = ps_.get<std::string>("eth_interface","eth0");
  TRACE(TR_LOG,"BernFEBData constructor : Calling driver init with interface %s",eth_interface.c_str());  
  
  febdrv.Init( eth_interface.c_str() );

  TRACE(TR_LOG,"BernFEBData constructor : driver initialized");
  
  runBiasOn = ps_.get<bool>("BiasON",true);

  sleep(2);
  febdrv.sendstats();
  febdrv.sendstats2();
  sleep(2);

  febconf.SetSCRConf(ps_.get<std::string>("SCRCONF"));
  febconf.SetPMRConf(ps_.get<std::string>("PMRCONF"));
  if(!febconf.configOK()){
    TRACE(TR_ERROR,"BernFEBData constructor : Bad config file(s) SCR=%s, PMR=%s",
	  ps_.get<std::string>("SCRCONF").c_str(),
	  ps_.get<std::string>("PMRCONF").c_str());
    throw cet::exception("BernFEBData: Bad config files!");
  }
    

  PingFEBs();
  febdrv.SetDriverState(DRV_OK);

  sleep(1);
  febdrv.sendstats();
  febdrv.sendstats2();

  for(auto const& febid : FEBIDs_){
    febdrv.configu((febid & 0xff),febconf.GetConfBuffer(),(1144+224)/8);
    sleep(1);
  }

  for(int i=0; i<10; ++i){
    sleep(1);
    febdrv.sendstats();
    febdrv.sendstats2();
  }
  if(runBiasOn) febdrv.biasON(0xFF);
  else febdrv.biasOFF(0xFF);

  for(int i=0; i<10; ++i){
    sleep(1);
    febdrv.sendstats();
    febdrv.sendstats2();
  }

  TRACE(TR_LOG,"BernFEBData constructor completed");  
}

bernfebdaq::BernFEBData::~BernFEBData(){
  TRACE(TR_LOG,"BernFEBData destructor called");  
  Cleanup();
  TRACE(TR_LOG,"BernFEBData destructor completed");  
}

void bernfebdaq::BernFEBData::PingFEBs(){
  TRACE(TR_LOG,"BernFEBData::PingFEBs called");  

  int n_active_febs = febdrv.pingclients();

  TRACE(TR_LOG,"BernFEBData::PingFEBs : Driver claims %d active FEBs.",n_active_febs);  


  if( n_active_febs != (int)FEBIDs_.size()){
    TRACE(TR_ERROR,"BernFEBData::ConfigureStart() : Wrong FEB size. Expected %lu, detected %d",
	  FEBIDs_.size(),n_active_febs);
    throw cet::exception("ERROR in BernFEBData. Wrong FEBs");
  }  

  TRACE(TR_LOG,"BernFEBData::ConfigureStart() : Detected %d FEBs",n_active_febs);

  for(int ifeb=0; ifeb<n_active_febs; ++ifeb){
    if(std::find(FEBIDs_.begin(),FEBIDs_.end(),febdrv.MACAddress(ifeb))==FEBIDs_.end()){
      TRACE(TR_ERROR,"BernFEBData::ConfigureStart() : Wrong FEB MAC Address, FEB %d. Expected %#06lX, detected %#06lX",
	    ifeb,FEBIDs_[ifeb],febdrv.MACAddress(ifeb));
      throw cet::exception("ERROR in BernFEBData. Wrong FEBs");
    }

    TRACE(TR_LOG,"BernFEBData::ConfigureStart() : Detected FEB %d, MAC Address %#06lX",
	  ifeb,febdrv.MACAddress(ifeb));
  }

  TRACE(TR_LOG,"BernFEBData::PingFEBs finished");  

}


void bernfebdaq::BernFEBData::ConfigureStart(){
  TRACE(TR_LOG,"BernFEBData::ConfigureStart() called");  

  febdrv.startDAQ(0xFF);

  TRACE(TR_LOG,"BernFEBData::ConfigureStart() completed");  
}

void bernfebdaq::BernFEBData::ConfigureStop(){
  TRACE(TR_LOG,"BernFEBData::ConfigureStop() called");

  febdrv.stopDAQ(0xFF);

  TRACE(TR_LOG,"BernFEBData::ConfigureStop() completed");  
}

int bernfebdaq::BernFEBData::GetDataSetup(){
  return febdrv.polldata_setup();
}

int bernfebdaq::BernFEBData::GetDataComplete(){
  febdrv.polldata_complete();
  febdrv.senddata(); 
  febdrv.sendstats(); 
  febdrv.sendstats2(); 
  return 1;
}

size_t bernfebdaq::BernFEBData::GetFEBData(uint64_t const& feb_id){
  TRACE(TR_GD_LOG,"BernFEBData::GetFEBData(0x%lx) called",feb_id);  

  uint8_t mac_by_byte[6];
  for(size_t i=0; i<6; ++i)
    mac_by_byte[i] = ( (feb_id >> 8*(5-i)) & 0xff);

  //send request for data
  //const uint8_t* febid_ptr = reinterpret_cast<const uint8_t*>(&feb_id);
  //const uint8_t* mac_ptr = febid_ptr +2;
  TRACE(TR_GD_LOG,"BernFEBData::GetFEBData(0x%lx) : Polling feb with mac=%2x:%2x:%2x:%2x:%2x:%2x",
	feb_id,mac_by_byte[0],mac_by_byte[1],mac_by_byte[2],mac_by_byte[3],mac_by_byte[4],mac_by_byte[5]);
  febdrv.pollfeb(mac_by_byte);

  size_t data_size=0;
  size_t thisdata=0;
  size_t events=0;

  //first, let's get some data
  int nbytes = febdrv.recvL2pack();
  TRACE(TR_GD_LOG,"BernFEBData::GetFEBData : recvfromfeb %d bytes",nbytes);

  while(nbytes>0){

    auto const& rpkt = febdrv.ReceivedPkt();

    //check: last data received?
    if(rpkt.CMD==FEB_EOF_CDR){
      febdrv.updateoverwritten();
      TRACE(TR_GD_LOG,"BernFEBData::GetFEBData(0x%lx) finished! Saw %lu events (data size of %lu bytes).",
	    feb_id,events,data_size);  
      return data_size;
    }
    
    //check: is this not a data packet?
    if(rpkt.CMD!=FEB_DATA_CDR){
      TRACE(TR_ERROR,"BernFEBData::GetFEBData(0x%lx) : Bad rpkt CMD! 0x%x",feb_id,rpkt.CMD);
      throw cet::exception("In BernFEBData::GetFebData, Bad rpkt value!");
    }
    
    thisdata = (nbytes-18);

    //check: thisdata is a sensible size
    if( thisdata>sizeof(BernFEBEvent)*1024 || thisdata%sizeof(BernFEBEvent)!=0 ){
      TRACE(TR_ERROR,"BernFEBData::GetFEBData(0x%lx) : Bad data size! %lu",feb_id,thisdata);
      throw cet::exception("In BernFEBData::GetFebData, Bad data size!");
    }

    //copy the data into FEBDTP buffer, and increment counters;
    std::copy(rpkt.Data,rpkt.Data+(thisdata/sizeof(uint8_t)),reinterpret_cast<uint8_t*>(&FEBDTPBufferUPtr[events]));
    events += thisdata/sizeof(BernFEBEvent);
    data_size += thisdata;

    //check : is this too much data for the buffer?
    if( events>FEBDTPBufferCapacity_ ){
      TRACE(TR_ERROR,"BernFEBData::GetFEBData(0x%lx) : Too many events in FEB buffer! %lu",feb_id,events);
      throw cet::exception("In BernFEBData::GetFebData, Too many events in FEB buffer!");
    }

    //process data for Bern monitoring
    febdrv.processL2pack(nbytes-18,reinterpret_cast<const uint8_t*>(&feb_id));

    //call for more data
    nbytes = febdrv.recvL2pack();
  }

  TRACE(TR_ERROR,"BernFEBData::GetFEBData(0x%lx) : Bad nbytes! %d. Data size collected was %lu (%lu events)",
	feb_id,nbytes,data_size,events);


  return 0;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(bernfebdaq::BernFEBData) 
