#include "bernfebdaq/Generators/BernFEBDataZMQ.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "trace_defines.h"

#include <zmq.h>

bernfebdaq::BernFEBDataZMQ::BernFEBDataZMQ(fhicl::ParameterSet const & ps)
  :
  BernFEB_GeneratorBase(ps)  
{
  TRACE(TR_LOG,"BernFEBDataZMQ constructor called");  

  zmq_data_pub_port_           = ps_.get<std::string>("data_pub_port");
  zmq_data_receive_timeout_ms_ = ps_.get<int>("data_receive_timeout_ms",500);

  TRACE(TR_LOG,"BernFEBDataZMQ constructor : Calling zmq subscriber with port %s",zmq_data_pub_port_.c_str());  
  
  zmq_context_    = zmq_ctx_new();

  TRACE(TR_LOG,"BernFEBDataZMQ constructor completed");  
}

bernfebdaq::BernFEBDataZMQ::~BernFEBDataZMQ(){
  TRACE(TR_LOG,"BernFEBDataZMQ destructor called");  
  zmq_ctx_destroy(zmq_context_);
  Cleanup();
  TRACE(TR_LOG,"BernFEBDataZMQ destructor completed");  
}



void bernfebdaq::BernFEBDataZMQ::ConfigureStart(){
  TRACE(TR_LOG,"BernFEBDataZMQ::ConfigureStart() called");  

  zmq_subscriber_ = zmq_socket(zmq_context_,ZMQ_SUB);

  int res=0;

  res = zmq_connect(zmq_subscriber_,zmq_data_pub_port_.c_str());
  if(res!=0)
    TRACE(TR_ERROR,"BernFEBDataZMQZMQ::ConfigureStart() failed to connect.");

  res = zmq_setsockopt(zmq_subscriber_,ZMQ_SUBSCRIBE,NULL,0);
  res = zmq_setsockopt(zmq_subscriber_,ZMQ_RCVTIMEO,&zmq_data_receive_timeout_ms_,2);

  if(res!=0)
    TRACE(TR_ERROR,"BernFEBDataZMQZMQ::ConfigureStart() socket options failed.");

  TRACE(TR_LOG,"BernFEBDataZMQ::ConfigureStart() completed");  
}

void bernfebdaq::BernFEBDataZMQ::ConfigureStop(){
  TRACE(TR_LOG,"BernFEBDataZMQ::ConfigureStop() called");

  zmq_close(zmq_subscriber_);

  TRACE(TR_LOG,"BernFEBDataZMQ::ConfigureStop() completed");  
}

int bernfebdaq::BernFEBDataZMQ::GetDataSetup(){
  return 1;
}

int bernfebdaq::BernFEBDataZMQ::GetDataComplete(){
  return 1;
}

size_t bernfebdaq::BernFEBDataZMQ::GetFEBData(uint64_t const& feb_id){
  TRACE(TR_GD_LOG,"BernFEBDataZMQ::GetFEBData(0x%lx) called",feb_id);  

  size_t data_size=0;
  size_t events=0;

  zmq_msg_t feb_data_msg;
  int res = zmq_msg_recv(&feb_data_msg,zmq_subscriber_,0);

  if(res==0 && zmq_msg_size(&feb_data_msg)>0){
    std::copy((uint8_t*)zmq_msg_data(&feb_data_msg),
	      (uint8_t*)zmq_msg_data(&feb_data_msg)+zmq_msg_size(&feb_data_msg)-sizeof(BernFEBEvent), //last event contains time info
	      reinterpret_cast<uint8_t*>(&FEBDTPBufferUPtr[events]));

    events += (zmq_msg_size(&feb_data_msg)-sizeof(BernFEBEvent))/sizeof(BernFEBEvent);
    data_size += zmq_msg_size(&feb_data_msg)-sizeof(BernFEBEvent);

    //check : is this too much data for the buffer?
    if( events>FEBDTPBufferCapacity_ ){
      TRACE(TR_ERROR,"BernFEBDataZMQ::GetFEBData(0x%lx) : Too many events in FEB buffer! %lu",feb_id,events);
      throw cet::exception("In BernFEBDataZMQ::GetFebData, Too many events in FEB buffer!");
    }
  }

  return data_size;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(bernfebdaq::BernFEBDataZMQ) 
