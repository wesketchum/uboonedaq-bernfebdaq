/* sends literal command without parameters to feb */
#include "febconf.h"
#include <cstring>
#include <zmq.h>

void usage()
{
 printf("Usage: ");
 printf("febconf SCRfile.txt PMRfile.txt <FEB S/N>\n");
 printf("SCRfile is a bitstream .txt file for SC register.\n");
 printf("PMRfile is a bitstream .txt file for PM register.\n");
 printf("<FEB S/N> can be 255 or omitted, in this case command is broadcast for all connected FEBs\n");

}

int FEBCONF::readbitstream(const char* fname, uint8_t *buf)
{

  FILE *file = fopen(fname, "r");
  if(!file) return 0;
  char line[128];
  //char bits[128];
  //char comment[128];
  char bit; 
  int ptr, byteptr;
  int bitlen=0;
  char ascii[MAXPACKLEN];
  while (fgets(line, sizeof(line), file)) {
    bit=1; ptr=0; byteptr=0;
    //	  	printf("%d: %s",bitlen,line);

    while(bit!=0x27 && bit!=0 && ptr<(int)sizeof(line) && bitlen<MAXPACKLEN) // ASCII(0x27)= '
      {
	bit=line[ptr];
	    ptr++;
	    if(bit==0x20 || bit==0x27) continue; //ignore spaces and apostrophe
	    //	     printf("%c",bit);
	    ascii[bitlen]=bit;
	    bitlen++;
      }
    //	printf("\n");
  }
  fclose(file);
  memset(buf,0,MAXPACKLEN); //reset buffer
  // Now encode ASCII bitstream into binary
  for(ptr=bitlen-1;ptr>=0;ptr--)
    {
      byteptr=(bitlen-ptr-1)/8;
      if(ascii[ptr]=='1')  buf[byteptr] |= (1 << (7-ptr%8)); 
      //   if((ptr%8)==0) printf("bitpos=%d buf[%d]=%02x\n",ptr,byteptr,buf[byteptr]);
    }
  return bitlen;
}

void FEBCONF::SetSCRConf(const char* scrf){
  int bitlen = readbitstream(scrf,bufSCR);

  if(bitlen==1144){
    printf("FEBDTP::ReadBitStream: %d bits read from SCR config file %s.\n",bitlen,scrf);
    config_SCR_OK_ = true;
  }
  else { 
    printf("FEBDTP::ReadBitStream: %d bits read from unrecognized type file %s. Aborting\n",bitlen,scrf);
    config_SCR_OK_ = false;
  }
  
}

void FEBCONF::SetPMRConf(const char* pmrf){
  int bitlen = readbitstream(pmrf,bufPMR);

  if(bitlen==224){
    printf("FEBDTP::ReadBitStream: %d bits read from PMR config file %s.\n",bitlen,pmrf);
    config_PMR_OK_ = true;
  }
  else { 
    printf("FEBDTP::ReadBitStream: %d bits read from unrecognized type file %s. Aborting\n",bitlen,pmrf);
    config_PMR_OK_ = false;
  }
  
}

void FEBCONF::SendConfs(uint8_t const& mac5){

  char cmd[32];
  uint8_t buf[MAXPACKLEN];

  void * context = zmq_ctx_new ();

  //  Socket to talk to server
  printf ("Connecting to febdrv...\n");
  void *requester = zmq_socket (context, ZMQ_REQ);
  zmq_connect (requester, "tcp://localhost:5555");

  sprintf(cmd,"SETCONF");
  cmd[8]=mac5;
  printf ("Sending command %s...", cmd);
  
  memcpy(buf,cmd,9);
  memcpy(buf+9,bufSCR,1144/8);
  memcpy(buf+9+1144/8,bufPMR,224/8);
  //zmq_send_const ( requester, buf, (1144+224)/8+9, 0);
  zmq_send ( requester, buf, (1144+224)/8+9, 0);
  
  printf ("waiting for reply..\n");
  
  
  zmq_msg_t reply;
  zmq_msg_init (&reply);
  zmq_msg_recv (&reply, requester, 0);
  printf ("Received reply: %s\n", (char*)zmq_msg_data (&reply));
  zmq_msg_close (&reply);
  zmq_close (requester);
  zmq_ctx_destroy (context);

}

int main (int argc, char **argv)
{
  if(argc<3 || argc>4) { usage(); return 0;}

  uint8_t mac5;
  if(argc==4) mac5=atoi(argv[3]); else mac5=255;

  FEBCONF confobj;

  confobj.SetSCRConf(argv[1]);
  confobj.SetPMRConf(argv[2]);

  if(!confobj.configOK())
    return 0;

  confobj.SendConfs(mac5);
  
  return 1;
}
