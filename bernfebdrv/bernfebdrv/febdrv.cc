#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>

#include "FEBDRV.h"

void usage()
{
  std::cout << "Usage: to init febdrv on eth0 interface type \n"
	    << "febdrv eth0" << std::endl;
}

int main (int argc, char* argv[])
{
  std::cout << "Starting febdrv..." << std::endl;

  if(argc!=2) { usage(); return 0;}

  FEBDRV febdrv;
  febdrv.Init(argv[1]);
  
  zmq_msg_t request;
  char cmd[32]; //command string
  uint8_t buf[MAXPACKLEN];
  
  while (1) {  // main loop

    febdrv.pingclients();
    febdrv.SetDriverState(DRV_OK);

    //Check next request from client
    zmq_msg_init (&request);

    if(zmq_msg_recv (&request, febdrv.GetResponder(), ZMQ_DONTWAIT)==-1) {
      zmq_msg_close (&request);  
      febdrv.polldata(); 
      febdrv.senddata(); 
      febdrv.sendstats(); 
      febdrv.sendstats2(); 
      continue;
    } 
    sprintf(cmd,"%s",(char*)zmq_msg_data(&request));
    febdrv.printdate();
    printf ("Received Command %s %02x  ",cmd, *(uint8_t*)((char*)(zmq_msg_data(&request))+8) );

    int rv=0;
    if(febdrv.NClients()>0)
      {
	if(strcmp(cmd, "BIAS_ON")==0) 
	  rv=febdrv.biasON(*(uint8_t*)((char*)(zmq_msg_data(&request))+8)); //get 8-th byte of message - mac of target board
	else if (strcmp(cmd, "BIAS_OF")==0) 
	  rv=febdrv.biasOFF(*(uint8_t*)((char*)(zmq_msg_data(&request))+8));
	else if (strcmp(cmd, "DAQ_BEG")==0) 
	  rv=febdrv.startDAQ(*(uint8_t*)((char*)(zmq_msg_data(&request))+8));
	else if (strcmp(cmd, "DAQ_END")==0) 
	  rv=febdrv.stopDAQ(*(uint8_t*)((char*)(zmq_msg_data(&request))+8));
	else if (strcmp(cmd, "SETCONF")==0) 
	  rv=febdrv.configu(*(uint8_t*)((char*)(zmq_msg_data(&request))+8), (uint8_t*)((char*)(zmq_msg_data(&request))+9), zmq_msg_size(&request)-9); 
	else if (strcmp(cmd, "GET_SCR")==0) 
	  rv=febdrv.getSCR(*(uint8_t*)((char*)(zmq_msg_data(&request))+8),buf); 
      }

    //  Send reply back to client
    zmq_msg_t reply;
    if (strcmp(cmd, "GET_SCR")==0) 
      {
	zmq_msg_init_size (&reply, 1144/8);
	memcpy(zmq_msg_data (&reply),buf,1144/8);
      }
    else
      { 
	zmq_msg_init_size (&reply, 5);
	if(rv>0) sprintf((char*)(zmq_msg_data (&reply)), "OK");
	else sprintf( (char*)(zmq_msg_data (&reply)), "ERR");
      }
    printf("Sending reply %s\n",(char*)zmq_msg_data (&reply));
    zmq_msg_send (&reply, febdrv.GetResponder(), 0);
    zmq_msg_close (&reply);


  } //end main loop


  //  We never get here but if we did, this would be how we end
  zmq_close (febdrv.GetResponder());
  zmq_ctx_destroy ( febdrv.GetContext() );
  return 1;
}
