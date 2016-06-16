#include <zmq.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <sys/timeb.h>

#include "febdrv.h"
#include <curl/curl.h>

#include <functional>

int      FEBDRV::evnum[NBUFS]; //number of good events in the buffer fields
int      FEBDRV::evbufstat[NBUFS]; //flag, showing current status of sub-buffer: 0= empty, 1= being filled, 2= full, 3=being sent out   
uint8_t  FEBDRV::bufPMR[256][1500];
uint8_t  FEBDRV::bufSCR[256][1500];
uint8_t  FEBDRV::FEB_present[256];
time_t   FEBDRV::FEB_lastheard[256]; //number of seconds since the board is heard, -1= never
uint8_t  FEBDRV::FEB_configured[256];
uint8_t  FEBDRV::FEB_daqon[256];
uint8_t  FEBDRV::FEB_evrate[256];
uint8_t  FEBDRV::FEB_biason[256];
uint16_t FEBDRV::FEB_VCXO[256];
uint8_t  FEBDRV::buf[MAXPACKLEN];


FEBDRV::FEBDRV() : 
  GLOB_daqon(0),
  nclients(0),
  sockfd_w(-1), 
  sockfd_r(-1),
  driver_state(DRV_OK),
  dstmac{0x00,0x60,0x37,0x12,0x34,0x00}, //base mac for FEBs, last byte 0->255
  brcmac{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},
  overwritten(0),
  lostinfpga(0),
  total_lost(0),
  total_acquired(0)
  { printf("Constructed FEBDRV\n"); }

void FEBDRV::Init(const char* iface){



  printf("Calling driver Init with iface=%s.\n",iface);

  sleep(2);

  int rv;  
  memset(evbuf,0, sizeof(evbuf));
  memset(evnum,0, sizeof(evnum));
  memset(evbufstat,0, sizeof(evbufstat));
  
  memset(FEB_configured,0,256);
  memset(FEB_daqon,0,256);
  memset(FEB_evrate,0,256);
  memset(FEB_biason,0,256);
  memset(FEB_present,0,256);
  memset(FEB_lastheard,0,256); //number of seconds since the board is heard
  memset(FEB_VCXO,0,256);

  printf("Memsets finished..\n");

  // network interface to febs
  rv=initif(iface);
  if(rv==0) {printdate(); printf("Can't initialize network interface %s! Exiting.\n",iface);}
  context = zmq_ctx_new();

  printf("FEB network interface initialized.\n");
  
  //  Socket to respond to clients
  responder = zmq_socket (context, ZMQ_REP);
  rv=zmq_bind (responder, "tcp://*:5555");
  if(rv<0) {printdate(); printf("Can't bind tcp socket for command! Exiting.\n");}
  rv=zmq_bind (responder, "ipc://command");
  if(rv<0) {printdate(); printf("Can't bind ipc socket for command! Exiting.\n");}
  printdate(); printf ("febdrv: listening at tcp://5555\n");
  printdate(); printf ("febdrv: listening at ipc://command\n");
  
  //  Socket to send data to clients
  publisher = zmq_socket (context, ZMQ_PUB);
  rv = zmq_bind (publisher, "tcp://*:5556");
  if(rv<0) {printdate(); printf("Can't bind tcp socket for data! Exiting.\n");}
  rv = zmq_bind (publisher, "ipc://data");
  if(rv<0) {printdate(); printf("Can't bind ipc socket for data! Exiting.\n");}
  printdate(); printf ("febdrv: data publisher at tcp://5556\n");
  printdate(); printf ("febdrv: data publisher at ipc://data\n");
  
  //  Socket to send statistics to clients
  pubstats = zmq_socket (context, ZMQ_PUB);
  rv = zmq_bind (pubstats, "tcp://*:5557");
  if(rv<0) {printdate(); printf("Can't bind tcp socket for stats! Exiting.\n");}
  rv = zmq_bind (pubstats, "ipc://stats");
  if(rv<0) {printdate(); printf("Can't bind ipc socket for stats! Exiting.\n");}
  printdate(); printf ("febdrv: stats publisher at tcp://5557\n");
  printdate(); printf ("febdrv: stats publisher at ipc://stats\n");
  
  //  Socket to send binary packed statistics to clients
  pubstats2 = zmq_socket (context, ZMQ_PUB);
  rv = zmq_bind (pubstats2, "tcp://*:5558");
  if(rv<0) {printdate(); printf("Can't bind tcp socket for stats2! Exiting.\n");}
  rv = zmq_bind (pubstats2, "ipc://stats2");
  if(rv<0) {printdate(); printf("Can't bind ipc socket for stats2! Exiting.\n");}
  printdate(); printf ("febdrv: stats2 publisher at tcp://5558\n");
  printdate(); printf ("febdrv: stats2 publisher at ipc://stats2\n");
  
  //initialising FEB daisy chain
  //scanclients();
  pingclients();
  
  driver_state=DRV_OK;
}

void FEBDRV::printdate()
{
    char str[64];
    time_t result=time(NULL);
    sprintf(str,"%s", asctime(gmtime(&result))); 
    str[strlen(str)-1]=0; //remove CR simbol
    printf("%s ", str); 
}

void FEBDRV::printmac( uint8_t *mac)
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

uint32_t FEBDRV::GrayToBin(uint32_t n)
{
uint32_t res=0;
 int a[32],b[32],i=0;//,c=0;
for(i=0; i<32; i++){ if((n & 0x80000000)>0) a[i]=1; else a[i]=0; 
n=n<<1; }
b[0]=a[0];
for(i=1; i<32; i++){ if(a[i]>0) if(b[i-1]==0) b[i]=1; else b[i]=0;
else b[i]=b[i-1]; }
for(i=0; i<32; i++){
res=(res<<1);
res=(res | b[i]); }
  return res;
}


void FEBDRV::ConfigSetBit(uint8_t *buffer, uint16_t bitlen, uint16_t bit_index, bool value)
{
  uint8_t byte;
  uint8_t mask;
  byte=buffer[(bitlen-1-bit_index)/8];
  mask= 1 << (7-bit_index%8);
  byte=byte & (~mask);
  if(value) byte=byte | mask;
  buffer[(bitlen-1-bit_index)/8]=byte;
}

bool FEBDRV::ConfigGetBit(uint8_t *buffer, uint16_t bitlen, uint16_t bit_index)
{
  uint8_t byte;
  uint8_t mask;
  byte=buffer[(bitlen-1-bit_index)/8];
  mask= 1 << (7-bit_index%8);
  byte=byte & mask;
  if(byte!=0) return true; else return false; 
}


int FEBDRV::initif(const char *iface)
{
  //spkt.iptype=0x0108; // IP type 0x0801
	tv.tv_sec = 0;  /* 0 Secs Timeout */
	tv.tv_usec = 500000;  // 500ms
        sprintf(ifName,"%s",iface); 

 		/* Open RAW socket to send on */
	if ((sockfd_w = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
	    perror("sender: socket");
            return 0;
	}
	/* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
	//	if ((sockfd_r = socket(PF_PACKET, SOCK_RAW, spkt.iptype)) == -1) {
	if ((sockfd_r = socket(PF_PACKET, SOCK_RAW, 0x0108)) == -1) {
		perror("listener: socket");	
		return 0;
	}

        if (setsockopt(sockfd_r, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))== -1){
		perror("SO_SETTIMEOUT");
		return 0;
	}
        	/* Bind listener to device */
	if (setsockopt(sockfd_r, SOL_SOCKET, SO_BINDTODEVICE, ifName, IFNAMSIZ-1) == -1){
		perror("SO_BINDTODEVICE");
		return 0;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFINDEX, &if_idx) < 0)
	    {perror("SIOCGIFINDEX"); return 0;}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFHWADDR, &if_mac) < 0)
	    {perror("SIOCGIFHWADDR"); return 0;}
        memcpy(&hostmac,((uint8_t *)&if_mac.ifr_hwaddr.sa_data),6);
        printdate(); printf("febdrv: initialized %s with MAC  ", ifName);
        printmac(hostmac);
        printf("\n");
        return 1;
}


int FEBDRV::sendtofeb(int len, FEBDTP_PKT_t const& spkt)  //sending spkt
{
	struct ifreq if_idx;
	struct ifreq if_mac;
        uint8_t   thisdstmac[6];
        memcpy(thisdstmac,spkt.dst_mac,6);
	struct sockaddr_ll socket_address;
	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFINDEX, &if_idx) < 0)
	    {perror("SIOCGIFINDEX"); return 0;}
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd_w, SIOCGIFHWADDR, &if_mac) < 0)
	    {perror("SIOCGIFHWADDR");return 0;}

	/* Index of the network device */
	socket_address.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	socket_address.sll_halen = ETH_ALEN;
	/* Destination MAC */
	memcpy(socket_address.sll_addr,spkt.dst_mac,6);
	/* Send packet */
	 if (sendto(sockfd_w, (char*)&spkt, len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0)
	    {driver_state=DRV_SENDERROR; return 0;}
//	printf("host->feb: sent packet %d bytes from  %02x:%02x:%02x:%02x:%02x:%02x ", len,spkt.src_mac[0],spkt.src_mac[1],spkt.src_mac[2],spkt.src_mac[3],spkt.src_mac[4],spkt.src_mac[5]);
//	printf("to %02x:%02x:%02x:%02x:%02x:%02x.\n", spkt.dst_mac[0],spkt.dst_mac[1],spkt.dst_mac[2],spkt.dst_mac[3],spkt.dst_mac[4],spkt.dst_mac[5]);
        return 1; 
}

int FEBDRV::recvfromfeb(int timeout_us, FEBDTP_PKT_t & rcvrpkt) //result is in rpkt
{
  //	int numbytes;
  // set receive timeout
  tv.tv_usec=timeout_us;
  if (setsockopt(sockfd_r, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))== -1){
    perror("SO_SETTIMEOUT");
    printf("ERROR here in setsockopt: timeout!\n");
    return 0;
  }
  numbytes = recvfrom(sockfd_r, &rcvrpkt, MAXPACKLEN, 0, NULL, NULL);
  
  if (numbytes<=0) {
    driver_state=DRV_RECVERROR;
    //printf("feb->host: received packet %d bytes from  %02x:%02x:%02x:%02x:%02x:%02x. errno=%d\n", numbytes,rpkt.src_mac[0],rpkt.src_mac[1],rpkt.src_mac[2],rpkt.src_mac[3],rpkt.src_mac[4],rpkt.src_mac[5],err_code);
    //numbytes = recvfrom(sockfd_r, &rcvrpkt, MAXPACKLEN, 0, NULL, NULL);}
    return 0;} //timeout
  FEB_lastheard[rcvrpkt.src_mac[5]]=time(NULL);
  return numbytes;
}

int FEBDRV::flushlink()
{
          // set short timeout and Flush input buffer 
  while(recvfromfeb(1000,rpkt)>0) {}
      driver_state=DRV_OK; 
      return 1;
}

int FEBDRV::sendcommand(const uint8_t *mac, uint16_t cmd, uint16_t reg, uint8_t * buf)
{

  FEBDTP_PKT_t spkt;
  //int numbytes=1;
  //int retval=0;
  int packlen=64;
  //int tout=50000;
  memcpy(&spkt.dst_mac,mac,6);        
  memcpy(&spkt.src_mac,hostmac,6);
  spkt.iptype = 0x0108;
  memcpy(&spkt.CMD,&cmd,2);
  memcpy(&spkt.REG,&reg,2);
  switch (cmd) {
  case FEB_GET_RATE :
  case FEB_GEN_INIT : 
  case FEB_GEN_HVON : 
  case FEB_GEN_HVOF : 
    break;
  case FEB_SET_RECV : 
    memcpy(&spkt.Data,buf,6); // Copy source mac to the data buffer
    break;
  case FEB_WR_SR : 
    memcpy(&spkt.Data,buf,1); // Copy register value to the data buffer 
    break;
  case FEB_WR_SRFF : 
  case FEB_WR_SCR  : 
  case FEB_WR_PMR  : 
  case FEB_WR_CDR : 
    memcpy(&spkt.Data,buf,256); // Copy 256 register values to the data buffer
    packlen=256+18; 
    break;
  case FEB_RD_CDR : 
    break;
  case FEB_RD_FW : 
  case FEB_WR_FW : 
    memcpy(&spkt.Data,buf,5); // Copy address and numblocks
    break;
  case FEB_DATA_FW : 
    memcpy(&spkt.Data,buf,1024); // Copy 1kB  data buffer
    packlen=1024+18;
    break;	  
  }
  flushlink();
  packlen=sendtofeb(packlen,spkt);
  if(packlen<=0) return 0; 
  return packlen;
}

uint64_t FEBDRV::MACAddress(int client) const{
  if(client >= nclients)
    return 0;

  uint64_t thismac=0,tmp;
  for(int i=5; i>=0; --i){
    tmp = macs[client][i];
    thismac = thismac + ( tmp << (8*(5-i)) );
  }
  return thismac;

}

int FEBDRV::pingclients()
{
 if(GLOB_daqon!=0) return 0; //ping only allowed when DAQ is off
 nclients=0;
 int changed=0;
 uint8_t febs[256];
 memset(febs,0,256);
 //printf("Pinging connected FEBs with FEB_SET_RECV..\n");
  dstmac[5]=0xff; 
  sendcommand(dstmac,FEB_SET_RECV,FEB_VCXO[nclients],hostmac);
  
  while(recvfromfeb(10000,rpkt)) if(rpkt.CMD==FEB_OK) 
  {
      memcpy(macs[nclients],rpkt.src_mac,6);
      febs[macs[nclients][5]]=1;
      if(FEB_present[macs[nclients][5]]==0)
      {
      printdate(); printf("Newly connected FEB: ");
      for(int i=0;i<5;i++) printf("%02x:",macs[nclients][i]);
      printf("%02x ",macs[nclients][5]);
      printf("%s\n",rpkt.Data); // new client reply received
      sprintf(verstr[nclients],"%s",rpkt.Data); 
      changed=1;
     // startDAQ(macs[nclients][5]); GLOB_daqon=0;
      }
      nclients++;
  }
  for(int f=0;f<256;f++)
	{
	if(FEB_present[f]==1 && febs[f]==0) {
		      	printdate(); printf("Disconnected FEB: ");
      			for(int i=0;i<5;i++) printf("%02x:",macs[0][i]);
      			printf("%02x \n",f);
			changed=1;
		}
        FEB_present[f]=febs[f];
	}
  if(changed) {printdate();  printf("In total %d FEBs connected.\n",nclients);}
  return nclients;
}

int FEBDRV::sendconfig(uint8_t mac5)
{
  //t->SendCMD(t->dstmac,FEB_SET_RECV,fNumberEntry75->GetNumber(),t->srcmac);
  int nreplies=0;
  int PrevConfigured=FEB_configured[mac5];
  dstmac[5]=mac5;
  FEB_configured[mac5]=0;
  sendcommand(dstmac,FEB_WR_SCR,0x0000,bufSCR[mac5]);
  while(recvfromfeb(50000,rpkt)) { 
    if(rpkt.CMD!=FEB_OK_SCR) return 0;
    nreplies++;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255) return 0;
  
  sendcommand(dstmac,FEB_WR_PMR,0x0000,bufPMR[mac5]);
  if(!recvfromfeb(50000,rpkt))  return 0;
  while(recvfromfeb(50000,rpkt)) { 
    if(rpkt.CMD!=FEB_OK_PMR) return 0;
    nreplies++;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255) return 0;
  
  FEB_configured[mac5]=PrevConfigured+1;
  
  return 1;
}


int FEBDRV::stopDAQ(uint8_t)// mac5)
{
  GLOB_daqon=0;
  return 1;
}

int FEBDRV::startDAQ(uint8_t mac5)
{

  printf("STARTING DAQ...\n");

  int nreplies=0;
  dstmac[5]=mac5; 
  
  memset(evbuf,0, sizeof(evbuf));
  memset(evnum,0, sizeof(evnum));
  memset(evbufstat,0, sizeof(evbufstat));
  
  nreplies=0;
  sendcommand(dstmac,FEB_GEN_INIT,0,buf); //stop DAQ on the FEB
  
  while(recvfromfeb(10000,rpkt)) { 
    if(rpkt.CMD!=FEB_OK) {
      printf("RESPONSE COMMAND NOT OK! %u\n",rpkt.CMD);
      return 0;
    }
    nreplies++;
  }
  //if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255) {
    printf("Received different number of replies than clients, %d!=%d\n",nreplies,nclients);
    return 0;
  }
  nreplies=0;
  sendcommand(dstmac,FEB_GEN_INIT,1,buf); //reset buffer
  
  while(recvfromfeb(10000,rpkt)) { 
    if(rpkt.CMD!=FEB_OK){
      printf("RESPONSE COMMAND NOT OK! %u",rpkt.CMD);
      return 0;
    }
    nreplies++;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255){
    printf("Received different number of replies than clients, %d!=%d\n",nreplies,nclients);
    return 0;
  }

  nreplies=0;
  sendcommand(dstmac,FEB_GEN_INIT,2,buf); //set DAQ_Enable flag on FEB

  while(recvfromfeb(10000,rpkt)) { 
    if(rpkt.CMD!=FEB_OK){
      printf("RESPONSE COMMAND NOT OK! %u",rpkt.CMD);
      return 0;
    }
    FEB_daqon[rpkt.src_mac[5]]=1;
    nreplies++;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255){
    printf("Received different number of replies than clients, %d!=%d\n",nreplies,nclients);
    return 0;
  }
  GLOB_daqon=1;
  return 1;
}


int FEBDRV::biasON(uint8_t mac5)
{
  printf("Calling BIAS_ON with mac5=%hu\n",mac5);

  int nreplies=0;
  dstmac[5]=mac5; 
  sendcommand(dstmac,FEB_GEN_HVON,0,buf); //reset buffer
  
  while(recvfromfeb(10000,rpkt)) { 
  if(rpkt.CMD!=FEB_OK) return 0;
  nreplies++;
  FEB_biason[rpkt.src_mac[5]]=1;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255) return 0;

  return 1;

}
int FEBDRV::biasOFF(uint8_t mac5)
{
  int nreplies=0;
  dstmac[5]=mac5; 
  sendcommand(dstmac,FEB_GEN_HVOF,0,buf); //reset buffer
  
  while(recvfromfeb(10000,rpkt)) { 
  if(rpkt.CMD!=FEB_OK) return 0;
  nreplies++;
  FEB_biason[rpkt.src_mac[5]]=0;
  }
  if(nreplies==0) return 0;
  if(nreplies!=nclients && mac5==255) return 0;
  return 1;
}

int FEBDRV::configu(uint8_t mac5, const uint8_t *buf1, int len)
{
  int rv=1;
  printf("called configu(%02x,buf,%d), %hu %hu %hu\n",mac5,len,buf1[0],buf1[1],buf1[2]);
  if(len != (1144+224)/8) { rv=0; return 0;}
  memcpy(bufSCR[mac5],buf1,1144/8);
  memcpy(bufPMR[mac5],buf1+1144/8,224/8);
  
  rv=sendconfig(mac5);
  return rv;
}

int FEBDRV::getSCR(uint8_t mac5, uint8_t *buf1)
{
int rv=1;
memcpy(buf1,bufSCR[mac5],1144/8);
return rv;
}


EVENT_t * FEBDRV::getnextevent() //flag, showing current status of sub-buffer: 0= empty, 1= being filled, 2= full, 3=being sent out  
{
  EVENT_t * retval=0;
  //EVENT_t evbuf[2][256*EVSPERFEB]; //0MQ backend event buffer, first index-triple-buffering, second - feb, third-event
  //int evnum[2]; //number of good events in the buffer fields
  //int evbufstat[2]; //flag, showing current status of sub-buffer: 0= empty, 1= being filled, 2= ready, 3=being sent out   
  int sbi=0;
  // check for available buffers
  for(sbi=0;sbi<NBUFS;sbi++) if(evbufstat[sbi]==1)  ///check for buffer being filled
			       {
				 retval=&(evbuf[sbi][evnum[sbi]]); 
				 evnum[sbi]++; 
				 if(evnum[sbi]==EVSPERFEB*256)  evbufstat[sbi]=2; //buffer full, set to ready
				 //      printf("%d",sbi);
				 return retval;
			       } //found buffer being filled, return pointer
  for(sbi=0;sbi<NBUFS;sbi++) if(evbufstat[sbi]==0) ///check for empty buffer
			       {
				 retval=&(evbuf[sbi][0]); 
				 evnum[sbi]=1; 
				 evbufstat[sbi]=1; //buffer being filled
				 //        printf("%d",sbi);
				 return retval;
			       } //started new buffer, return pointer
  //if we get here, than no buffers are available!
  return 0;
}


//void free_subbufer (void *data, void *hint) //call back from ZMQ sent function, hint points to subbufer index
void FEBDRV::free_subbufer (void*, void *hint) //call back from ZMQ sent function, hint points to subbufer index
{
  //return;
  int sbi;
  sbi =  (intptr_t)hint; //reset buffer status to empty
  evbufstat[sbi]=0;
  evnum[sbi]=0; 
  // printf("Subbuf %d transmission complete.\n",sbi);
  
}


int FEBDRV::senddata()
{
int sbi=0;
void* bptr=0;
int sbitosend=-1; // check for filled buffers
//printf("bufstat "); for(sbi=0;sbi<NBUFS;sbi++) { printf("%d ",evbufstat[sbi]); if(evbufstat[sbi]==1) printf("found 1 ");}  printf("\n");
for(sbi=0;sbi<NBUFS;sbi++) 
			{
			   if(evbufstat[sbi]==1) sbitosend=sbi;  //the if there is being filled buffer
                           if(evbufstat[sbi]==2) sbitosend=sbi;  //first check for buffer that is full
                        }
if(sbitosend==-1) return 0; //no buffers to send out

evbufstat[sbitosend]=3; //  reset to 0 by the callback only when transmission is done!
//fill buffer trailer in the last event
evbuf[sbitosend][evnum[sbitosend]].flags=0xFFFF;
evbuf[sbitosend][evnum[sbitosend]].mac5=0xFFFF;
evbuf[sbitosend][evnum[sbitosend]].ts0=MAGICWORD32;
evbuf[sbitosend][evnum[sbitosend]].ts1=MAGICWORD32;
bptr=&(evbuf[sbitosend][evnum[sbitosend]].adc[0]); //pointer to start of ADC field
 memcpy(bptr,&(evnum[sbitosend]),sizeof(int)); bptr=(char*)bptr+sizeof(int);
 memcpy(bptr,&mstime0,sizeof(struct timeb)); bptr=(char*)bptr+sizeof(struct timeb); //start of poll time
 memcpy(bptr,&mstime1,sizeof(struct timeb)); bptr=(char*)bptr+sizeof(struct timeb);  //end of poll time
evnum[sbitosend]++;
zmq_msg_t msg;
 zmq_msg_init_data (&msg, evbuf[sbitosend], evnum[sbitosend]*EVLEN , free_subbufer, (void*)(intptr_t)(sbitosend));
//printf("Scheduling subbuf %d for sending..\n",sbitosend);
zmq_msg_send (&msg, publisher, ZMQ_DONTWAIT);
//printf("done..\n");
zmq_msg_close (&msg);
 return sbitosend;
}


int FEBDRV::sendstats2() //send statistics in binary packet format
{
// float Rate;
 void* ptr;
 DRIVER_STATUS_t ds;
 FEB_STATUS_t fs;

     ds.datime=time(NULL);
     ds.daqon=GLOB_daqon;  
     ds.status=driver_state;
     ds.nfebs=nclients;  
     ds.msperpoll=msperpoll;
 zmq_msg_t msg;
 zmq_msg_init_size (&msg, sizeof(ds)+nclients*sizeof(fs));
 ptr=zmq_msg_data (&msg);
 memcpy(ptr,&ds,sizeof(ds)); ptr=(char*)ptr+sizeof(ds);
// Get event rates
for(int f=0; f<nclients;f++) //loop on all connected febs : macs[f][6]
 {
   sendcommand(macs[f],FEB_GET_RATE,0,buf);
  memset(&fs,0,sizeof(fs));
  fs.connected=1;
  if(recvfromfeb(10000,rpkt)==0) fs.connected=0;  
//   sprintf(str+strlen(str), "Timeout for FEB %02x:%02x:%02x:%02x:%02x:%02x !\n",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
  if(rpkt.CMD!=FEB_OK)   fs.error=1;  
 //  sprintf(str+strlen(str), "no FEB_OK for FEB %02x:%02x:%02x:%02x:%02x:%02x !\n",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
 
  //fs.evtrate=*(reinterpret_cast<float*>(rpkt.Data));
  auto evtrate_ptr = reinterpret_cast<float*>(rpkt.Data);
  fs.evtrate = *evtrate_ptr;
//   sprintf(str+strlen(str), "FEB %02x:%02x:%02x:%02x:%02x:%02x ",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
//   sprintf(str+strlen(str), "%s Conf: %d Bias: %d ",verstr[f],FEB_configured[macs[f][5]],FEB_biason[macs[f][5]]);
   fs.configured=FEB_configured[macs[f][5]];
   fs.biason=FEB_biason[macs[f][5]];
   sprintf(fs.fwcpu,"%s",verstr[f]);
   sprintf(fs.fwfpga,"rev3.1");
   memcpy(fs.mac,macs[f],6);
   
   if(GLOB_daqon)
   {
      fs.evtperpoll=evtsperpoll[macs[f][5]];
      fs.lostcpu=lostperpoll_cpu[macs[f][5]];
      fs.lostfpga=lostperpoll_fpga[macs[f][5]];
//    sprintf(str+strlen(str), "Per poll: %d ",evtsperpoll[macs[f][5]]);
//   sprintf(str+strlen(str), "Lost CPU: %d ",lostperpoll_cpu[macs[f][5]]);
//    sprintf(str+strlen(str), "FPGA: %d ",lostperpoll_fpga[macs[f][5]]);
   }
//   sprintf(str+strlen(str), "rate %5.1f Hz\n",Rate);
   memcpy(ptr,&fs,sizeof(fs)); ptr=(char*)ptr+sizeof(fs);
   
 }
 //if(GLOB_daqon) sprintf(str+strlen(str), "Poll %d ms.\n",msperpoll);

 zmq_msg_send (&msg, pubstats2, ZMQ_DONTWAIT);
//printf("done..\n");
zmq_msg_close (&msg);
 return 1; 
 
}

int FEBDRV::sendstats()
{
 char str[2048]; //stats string
 float Rate;
 memset(str,0,sizeof(str));

     time_t result=time(NULL);
    if(GLOB_daqon) sprintf(str+strlen(str), "\nDAQ ON; System time: %s", asctime(gmtime(&result))); 
    else sprintf(str+strlen(str), "\nDAQ OFF; System time: %s", asctime(gmtime(&result))); 
//for(int mac5=0;mac5<255;mac5++) printf("%02x %d ",mac5,FEB_configured[mac5]);    

// Get event rates
for(int f=0; f<nclients;f++) //loop on all connected febs : macs[f][6]
 {
   sendcommand(macs[f],FEB_GET_RATE,0,buf); 
   if(recvfromfeb(10000,rpkt)==0) 
   sprintf(str+strlen(str), "Timeout for FEB %02x:%02x:%02x:%02x:%02x:%02x !\n",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
  if(rpkt.CMD!=FEB_OK)    
   sprintf(str+strlen(str), "no FEB_OK for FEB %02x:%02x:%02x:%02x:%02x:%02x !\n",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
  
  auto evtrate_ptr = reinterpret_cast<float*>(rpkt.Data);
  Rate = *evtrate_ptr;

  //Rate=*(reinterpret_cast<float*>(rpkt.Data));
   sprintf(str+strlen(str), "FEB %02x:%02x:%02x:%02x:%02x:%02x ",macs[f][0],macs[f][1],macs[f][2],macs[f][3],macs[f][4],macs[f][5]);
   sprintf(str+strlen(str), "%s Conf: %d Bias: %d ",verstr[f],FEB_configured[macs[f][5]],FEB_biason[macs[f][5]]);
   if(GLOB_daqon)
   {
    sprintf(str+strlen(str), "Per poll: %d ",evtsperpoll[macs[f][5]]);
    sprintf(str+strlen(str), "Lost CPU: %d ",lostperpoll_cpu[macs[f][5]]);
    sprintf(str+strlen(str), "FPGA: %d ",lostperpoll_fpga[macs[f][5]]);
   }
   sprintf(str+strlen(str), "rate %5.1f Hz\n",Rate);
 //  printf("%s",str);
 }
 if(GLOB_daqon) sprintf(str+strlen(str), "Poll %d ms.\n",msperpoll);

 zmq_msg_t msg;
 zmq_msg_init_size (&msg, strlen(str)+1);
 memcpy(zmq_msg_data (&msg), str, strlen(str)+1);
 zmq_msg_send (&msg, pubstats, ZMQ_DONTWAIT);
//printf("done..\n");
zmq_msg_close (&msg);
 return 1; 
 
}

int FEBDRV::recvL2pack(){
  printf("Receiving L2 pack ...\n");
  return recvfromfeb(5000,rpkt);
}

void FEBDRV::processL2pack(int datalen,const uint8_t* mac){

  EVENT_t *evt=0;

  int jj=0;
  while(jj<datalen)
    {
      auto ovrwr_ptr = reinterpret_cast<uint16_t*>(&(rpkt).Data[jj]);
      overwritten=*ovrwr_ptr;
      jj=jj+2;
      
      auto lif_ptr = reinterpret_cast<uint16_t*>(&(rpkt).Data[jj]);
      lostinfpga=*lif_ptr;
      jj=jj+2;
      
      total_lost+=lostinfpga;
      lostperpoll_fpga[rpkt.src_mac[5]]+=lostinfpga;
      
      auto ts0_ptr = reinterpret_cast<uint32_t*>(&(rpkt).Data[jj]);
      ts0=*ts0_ptr; jj=jj+4; 
      auto ts1_ptr = reinterpret_cast<uint32_t*>(&(rpkt).Data[jj]);
      ts1=*ts1_ptr; jj=jj+4; 
      
      ls2b0=ts0 & 0x00000003;
      ls2b1=ts1 & 0x00000003;
      tt0=(ts0 & 0x3fffffff) >>2;
      tt1=(ts1 & 0x3fffffff) >>2;
      tt0=(GrayToBin(tt0) << 2) | ls2b0;
      tt1=(GrayToBin(tt1) << 2) | ls2b1;
      tt0=tt0+5;//IK: correction based on phase drift w.r.t GPS
      tt1=tt1+5; //IK: correction based on phase drift w.r.t GPS
      NOts0=((ts0 & 0x40000000)>0); // check overflow bit
      NOts1=((ts1 & 0x40000000)>0);
      //        if((ts0 & 0x80000000)>0) {ts0=0x0; ts0_ref=tt0; ts0_ref_MEM[rpkt.src_mac[5]]=tt0;} 
      //        else { ts0=tt0; ts0_ref=ts0_ref_MEM[rpkt.src_mac[5]]; }
      //        if((ts1 & 0x80000000)>0) {ts1=0x0; ts1_ref=tt1; ts1_ref_MEM[rpkt.src_mac[5]]=tt1;} 
      //        else { ts1=tt1; ts1_ref=ts1_ref_MEM[rpkt.src_mac[5]]; }
      
      if((ts0 & 0x80000000)>0)
	{
	  REFEVTts0=1; 
	  ts0=tt0; 
	  //ts0_ref=tt0; 
	  ts0_ref_MEM[rpkt.src_mac[5]]=tt0;
	} 
      else {
	REFEVTts0=0; 
	ts0=tt0; 
	//ts0_ref=ts0_ref_MEM[rpkt.src_mac[5]]; 
      }
      if((ts1 & 0x80000000)>0) 
	{
	  REFEVTts1=1; 
	  ts1=tt1; 
	  //ts1_ref=tt1; 
	  ts1_ref_MEM[rpkt.src_mac[5]]=tt1;
	} 
      else {
	REFEVTts1=0; 
	ts1=tt1; 
	//ts1_ref=ts1_ref_MEM[rpkt.src_mac[5]]; 
      }
      
      //	 printf("T0=%u ns, T1=%u ns T0_ref=%u ns  T1_ref=%u ns \n",ts0,ts1,ts0_ref,ts1_ref);
      //         tmp=ts0*1e9/(ts0_ref-1); ts0=tmp;
      //         tmp=ts1*1e9/(ts0_ref-1); ts1=tmp;
      //         tmp=ts1_ref*1e9/(ts0_ref-1); ts1_ref=tmp;
      //	 printf("Corrected T0=%u ns, T1=%u ns T0_ref=%u ns  T1_ref=%u ns \n",ts0,ts1,ts0_ref,ts1_ref);
      evt=getnextevent();
      if(evt==0) {driver_state=DRV_BUFOVERRUN; printdate();  printf("Buffer overrun for FEB S/N %d !! Aborting.\n",mac[5]); continue;} 
      evt->ts0=ts0;
      evt->ts1=ts1;
      evt->flags=0;
      evt->mac5=rpkt.src_mac[5];
      if(NOts0==0) evt->flags|=0x0001;    //opposite logic! 1 if TS is present, 0 if not!    
      if(NOts1==0) evt->flags|=0x0002;        
      if(REFEVTts0==1) evt->flags|=0x0004; //bit indicating TS0 reference event        
      if(REFEVTts1==1) evt->flags|=0x0008; //bit indicating TS1 reference event        
      for(int kk=0; kk<32; kk++) { 
	auto adc_ptr = reinterpret_cast<uint16_t*>(&(rpkt).Data[jj]);
	evt->adc[kk]=*adc_ptr; 
	jj++; 
	jj++;
      }  
      total_acquired++;
      evtsperpoll[rpkt.src_mac[5]]++;
    }
  
}

int FEBDRV::recvandprocessL2pack(const uint8_t* mac){

  numbytes = recvL2pack();
  
  if(numbytes<=0) return numbytes;       
  if(rpkt.CMD!=FEB_DATA_CDR) return -1; //should not happen, but just in case..

  processL2pack(numbytes-18,mac);

  return numbytes;
}

int FEBDRV::polldata_setup(){

  ftime(&mstime0);
  
  msperpoll=0;
  overwritten=0;
  lostinfpga=0;
  //evtsperpoll=0;
  memset(lostperpoll_cpu,0,sizeof(lostperpoll_cpu));
  memset(lostperpoll_fpga,0,sizeof(lostperpoll_fpga));
  memset(evtsperpoll,0,sizeof(evtsperpoll));
  
  if(GLOB_daqon==0) {sleep(1); return 0;} //if no DAQ running - just delay for not too fast ping

  return 1;
}

void FEBDRV::pollfeb(const uint8_t* mac)
{ 
  printf("POLLING FEB %hu\n",mac[5]);
  sendcommand(mac,FEB_RD_CDR,0,buf); 
  numbytes=1; rpkt.CMD=0; //clear these out
}

void FEBDRV::updateoverwritten(){
  total_lost+=overwritten;
  lostperpoll_cpu[rpkt.src_mac[5]]+=overwritten;
}

void FEBDRV::polldata_complete(){
  //if(evtsperpoll>0) printf(" %d events per poll.\n",evtsperpoll); 
  ftime(&mstime1);
  msperpoll=(mstime1.time-mstime0.time)*1000+(mstime1.millitm-mstime0.millitm);
}


int FEBDRV::polldata() // poll data from daysy-chain and send it to the publisher socket
{  
  int rv=0;
  if(polldata_setup()==0) return 0;

  for(int f=0; f<nclients;f++) //loop on all connected febs : macs[f][6]
    {
      printf("Preparing to poll FEB %hu\n",macs[f][5]);
      pollfeb(macs[f]);
      
      while(numbytes>0 && rpkt.CMD!=FEB_EOF_CDR) //loop on messages from one FEB
	if( recvandprocessL2pack(macs[f])<=0 ) continue;
      
      updateoverwritten();
      
    } //loop on FEBS
  
  polldata_complete();
  return rv;
  
}
