#include "bernfebdaq/Generators/BernZMQData.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "trace_defines.h"

#include <zmq.h>

bernfebdaq::BernZMQData::BernZMQData(fhicl::ParameterSet const & ps)
  :
  BernZMQ_GeneratorBase(ps)  
{
  TRACE(TR_LOG,"BernZMQData constructor called");  

  zmq_data_pub_port_           = ps_.get<std::string>("zmq_data_pub_port");
  zmq_data_receive_timeout_ms_ = ps_.get<int>("zmq_data_receive_timeout_ms",500);

  TRACE(TR_LOG,"BernZMQData constructor : Calling zmq subscriber with port %s",zmq_data_pub_port_.c_str());  
  
  zmq_context_    = zmq_ctx_new();

  TRACE(TR_LOG,"BernZMQData constructor completed");  
}

bernfebdaq::BernZMQData::~BernZMQData(){
  TRACE(TR_LOG,"BernZMQData destructor called");  
  zmq_ctx_destroy(zmq_context_);
  Cleanup();
  TRACE(TR_LOG,"BernZMQData destructor completed");  
}



void bernfebdaq::BernZMQData::ConfigureStart(){
  TRACE(TR_LOG,"BernZMQData::ConfigureStart() called");  

  zmq_subscriber_ = zmq_socket(zmq_context_,ZMQ_SUB);

  int res=0;

  res = zmq_connect(zmq_subscriber_,zmq_data_pub_port_.c_str());
  if(res!=0)
    TRACE(TR_ERROR,"BernZMQDataZMQ::ConfigureStart() failed to connect.");

  res = zmq_setsockopt(zmq_subscriber_,ZMQ_SUBSCRIBE,NULL,0);
  //res = zmq_setsockopt(zmq_subscriber_,ZMQ_RCVTIMEO,&zmq_data_receive_timeout_ms_,2);

  if(res!=0)
    TRACE(TR_ERROR,"BernZMQDataZMQ::ConfigureStart() socket options failed.");

  TRACE(TR_LOG,"BernZMQData::ConfigureStart() completed");  
}

void bernfebdaq::BernZMQData::ConfigureStop(){
  TRACE(TR_LOG,"BernZMQData::ConfigureStop() called");

  zmq_close(zmq_subscriber_);

  TRACE(TR_LOG,"BernZMQData::ConfigureStop() completed");  
}

int bernfebdaq::BernZMQData::GetDataSetup(){
  return 1;
}

int bernfebdaq::BernZMQData::GetDataComplete(){
  return 1;
}

size_t bernfebdaq::BernZMQData::GetZMQData(){
  TRACE(TR_GD_LOG,"BernZMQData::GetZMQData called");

  size_t data_size=0;
  size_t events=0;

  zmq_msg_t feb_data_msg;
  zmq_msg_init(&feb_data_msg);
  while(zmq_msg_recv(&feb_data_msg,zmq_subscriber_,ZMQ_DONTWAIT)<0){
    //TRACE(TR_GD_DEBUG,"BernZMQData::GetFEBData() called and no data/error.");
    usleep(1000);
    //return 0;
  }
  
  if(zmq_msg_size(&feb_data_msg)>0){

    TRACE(TR_GD_DEBUG,"BernZMQData::GetZMQData() about to copy");
    
    std::copy((uint8_t*)zmq_msg_data(&feb_data_msg),
	      (uint8_t*)zmq_msg_data(&feb_data_msg)+zmq_msg_size(&feb_data_msg), //last event contains time info
	      reinterpret_cast<uint8_t*>(&ZMQBufferUPtr[events]));

    TRACE(TR_GD_DEBUG,"BernZMQData::GetZMQData() copied!");

    events += zmq_msg_size(&feb_data_msg)/sizeof(BernZMQEvent);
    data_size += zmq_msg_size(&feb_data_msg);

    TRACE(TR_GD_DEBUG,"BernZMQData::GetZMQData() copied %lu events (%lu size)",events,data_size);

    //check : is this too much data for the buffer?
    if( events>ZMQBufferCapacity_ ){
      TRACE(TR_ERROR,"BernZMQData::GetZMQData : Too many events in ZMQ buffer! %lu",events);
      throw cet::exception("In BernZMQData::GetZMQData, Too many events in ZMQ buffer!");
    }
    
  }

  zmq_msg_close(&feb_data_msg);

  TRACE(TR_GD_LOG,"BernZMQData::GetZMQData() size returned was %lu",data_size);

  return data_size;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(bernfebdaq::BernZMQData) 
