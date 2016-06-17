/* sends literal command without parameters to feb */
#include "FEBCONF.h"
#include "feb_trace_defines.h"
#include <cstring>

int FEBCONF::readbitstream(const char* fname, uint8_t *buf)
{
  TRACE(TR_FEBCONF_LOG,"FEBCONF::readbitstream(%s) called",fname);

  FILE *file = fopen(fname, "r");
  if(!file) {
    TRACE(TR_FEBCONF_ERR,"FEBCONF::readbitstream  Error opening file %s.",fname);
    return 0;
  }

  char line[128];
  char bit; 
  int ptr, byteptr;
  int bitlen=0;
  char ascii[MAXPACKLEN];

  while (fgets(line, sizeof(line), file)) {
    bit=1; ptr=0; byteptr=0;

    while(bit!=0x27 && bit!=0 && ptr<(int)sizeof(line) && bitlen<MAXPACKLEN) // ASCII(0x27)= '
      {
	bit=line[ptr];
	    ptr++;
	    if(bit==0x20 || bit==0x27) continue; //ignore spaces and apostrophe
	    ascii[bitlen]=bit;
	    bitlen++;
      }
  }
  fclose(file);
  memset(buf,0,MAXPACKLEN); //reset buffer

  // Now encode ASCII bitstream into binary
  for(ptr=bitlen-1;ptr>=0;ptr--)
    {
      byteptr=(bitlen-ptr-1)/8;
      if(ascii[ptr]=='1')  buf[byteptr] |= (1 << (7-ptr%8)); 
    }

  TRACE(TR_FEBCONF_LOG,"FEBCONF::readbitstream(%s)  complete. bitlen=%d",fname,bitlen);
  return bitlen;
}

void FEBCONF::SetSCRConf(const char* scrf){
  int bitlen = readbitstream(scrf,bufSCR);

  if(bitlen==1144){
    TRACE(TR_FEBCONF_LOG,"FEBDTP::SetSCRConf  %d bits read from SCR config file %s.\n",bitlen,scrf);
    config_SCR_OK_ = true;
  }
  else { 
    TRACE(TR_FEBCONF_ERR,"FEBDTP::SetSCRConf  %d bits read from unrecognized type file %s. Aborting\n",bitlen,scrf);
    config_SCR_OK_ = false;
  }
  
}

void FEBCONF::SetPMRConf(const char* pmrf){
  int bitlen = readbitstream(pmrf,bufPMR);

  if(bitlen==1144){
    TRACE(TR_FEBCONF_LOG,"FEBDTP::SetPMRConf  %d bits read from PMR config file %s.\n",bitlen,pmrf);
    config_PMR_OK_ = true;
  }
  else { 
    TRACE(TR_FEBCONF_ERR,"FEBDTP::SetPMRConf  %d bits read from unrecognized type file %s. Aborting\n",bitlen,pmrf);
    config_PMR_OK_ = false;
  }
  
}

uint8_t const* FEBCONF::GetConfBuffer(){
  memcpy(confbuf,bufSCR,1144/8);
  memcpy(confbuf+1144/8,bufPMR,224/8);
  return confbuf;
}

void FEBCONF::SendConfs(uint8_t const& mac5,char* dest){

  TRACE(TR_FEBCONF_LOG,"FEBCONF::SendConfs(%02X,%s) called.",mac5,dest);

  void * context = zmq_ctx_new ();

  //  Socket to talk to server
  TRACE(TR_FEBCONF_LOG,"FEBCONF::SendConfs  Connecting to febdrv...");
  void *requester = zmq_socket (context, ZMQ_REQ);
  zmq_connect (requester,dest);

  char cmd[32];
  sprintf(cmd,"SET_CONF");
  cmd[8]=mac5;

  uint8_t mybuf[MAXPACKLEN];
  memcpy(mybuf,cmd,9);
  memcpy(mybuf+9,GetConfBuffer(),(1144+224)/8);

  zmq_send ( requester, mybuf , (1144+224)/8+9, 0);
  TRACE(TR_FEBCONF_LOG,"FEBCONF::SendConfs  waiting for reply...");  
  
  zmq_msg_t reply;
  zmq_msg_init (&reply);
  zmq_msg_recv (&reply, requester, 0);
  TRACE(TR_FEBCONF_LOG,"Received reply: %s", (char*)zmq_msg_data (&reply));

  zmq_msg_close (&reply);
  zmq_close (requester);
  zmq_ctx_destroy (context);

}
