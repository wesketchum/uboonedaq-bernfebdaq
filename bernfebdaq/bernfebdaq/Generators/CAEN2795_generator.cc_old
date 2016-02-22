#include "icartdaq/Generators/CAEN2795.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "icartdaq-core/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

#include "CAENComm.h"
//#include "keyb.h"
//#include "keyb.c"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


#define PBLT_SIZE     (61440)                     // in byte
#define RECORD_LENGTH (4*1024)                    // waveform length (in samples)
#define EVENT_SIZE    ((RECORD_LENGTH * 32) + 3)  // Event Size in lwords
#define NEV_READ      (1)                         // num of event to read with one BLT
#define BUFFER_SIZE   (EVENT_SIZE * NEV_READ)     // readout buffer size

demo::CAEN2795::CAEN2795(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  nSamplesPerChannel_(ps.get<uint32_t>("nSamplesPerChannel",3000)),
  nADCBits_(ps.get<uint8_t>("nADCBits",12)),
  nChannelsPerBoard_(ps.get<uint16_t>("nChannelsPerBoard",64)),
  nBoards_(ps.get<uint16_t>("nBoards",9)),
  RunNumber_(ps.get<uint32_t>("RunNumber",999)),
  throttle_usecs_(ps.get<size_t>("throttle_usecs", 100000)),
  throttle_usecs_check_(ps.get<size_t>("throttle_usecs_check", 10000)),
  engine_(ps.get<int64_t>("random_seed", 314159)),
  uniform_distn_(new std::uniform_int_distribution<int>(0, pow(2, 12 ) - 1 ))
{

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
  //int board_num  = 4;
  //uint32_t data;
  CAENComm_ErrorCode ret = CAENComm_Success;
  int i;

  std::cout << "In initialization. We have a positive ret code: " << ret << std::endl;

  // sveglio il MASTER: conet node deve essere 0
  ret = CAENComm_OpenDevice(CAENComm_OpticalLink, 0, 0, 0, &LinkHandle[0]);

  std::cout << "After OpenDevice. We have a ret code: " << ret << std::endl;
  
  if (ret != CAENComm_Success) 
    {
      printf("Error opening the Optical Node (%d). \n", 0);
      return;
    }
  else
    {
      std::cout << "After OpenDevice. Ret was success!  " << ret << std::endl;
      LinkInit[0] = 1;
    }
  // sveglio eventuali SLAVE
  if (nBoards_ > 1)
    {
      for (i = 1; i < nBoards_; i++)
        {
	  ret = CAENComm_OpenDevice(CAENComm_OpticalLink, 0, i, 0, &LinkHandle[i]);
	  if (ret != CAENComm_Success) 
	    {
	      printf("Error opening the Optical Node (%d). \n", i);
	      return;
	    }
	  else {LinkInit[i] = 1;}
	}
    }
  CAENComm_Write32(LinkHandle[0], A_Signals, SIGNALS_TTLINK_ALIGN);
  std::cout<<"LinkInit[0] = "<<LinkInit[0]<<std::endl;
  usleep(100);
//-------------------------------------------------------------------------------------  
//------------------------------------------------------------------------------------- 

  // Check and make sure that the fragment type will be one of the "toy" types
  
  if (throttle_usecs_ > 0 && (throttle_usecs_check_ >= throttle_usecs_ ||
			      throttle_usecs_ % throttle_usecs_check_ != 0) ) {
    throw cet::exception("Error in CAEN2795: disallowed combination of throttle_usecs and throttle_usecs_check (see CAEN2795.hh for rules)");
  }
    

  start();
}

//taken from SW_MULTI::test7
void demo::CAEN2795::start() {

  int loc_conet_node = 0;
  //int loc_conet_handle;
  int ret;
  unsigned int data;
  //unsigned int data_ap;
  
  printf("BOARD CONET NODE: (Default is %d) ", loc_conet_node);
  //scanf("%d", &loc_conet_node);
  
  if (loc_conet_node <8){
    // la scheda non è inizializzata
    if ((!LinkInit[loc_conet_node])) {  
      // inizializzo
      ret = CAENComm_OpenDevice(CAENComm_OpticalLink, 0, loc_conet_node, 0, &LinkHandle[loc_conet_node]);  // init locale
      if (ret != CAENComm_Success)
	printf("Error opening the Conet Node (ret = %d). A2795 not found\n", ret);
      else{
	LinkInit[loc_conet_node] = 1;
      }
    }
    
    // RUN/NOT RUN
    ret = CAENComm_Read32(LinkHandle[loc_conet_node], A_ControlReg, &data);
    if ((data & CTRL_ACQRUN) == 0){ 
      ret = CAENComm_Write32(LinkHandle[loc_conet_node], A_ControlReg_Set, CTRL_ACQRUN);
      printf("\nBOARD RUNNING...\n");
    }
    else{
      ret = CAENComm_Write32(LinkHandle[loc_conet_node], A_ControlReg_Clear, CTRL_ACQRUN);
      printf("\nBOARD NOT RUNNING...\n");
    }		
  }
  else{  // Parlo col MASTER
    // il master non è inizializzato
    if ((!LinkInit[0])) {  
      // inizializzo
      ret = CAENComm_OpenDevice(CAENComm_OpticalLink, 0, 0, 0, &LinkHandle[0]);  // init locale
      if (ret != CAENComm_Success)
	printf("Error opening the Conet Node 0 (ret = %d). A2795 MASTER not found\n", ret);
      else{
	LinkInit[0] = 1;
      }
    }
    
    usleep(100);
    
    // leggo se il master è già in run per decidere se dare SOR o EOR
    CAENComm_Read32(LinkHandle[0], A_StatusReg, &data);
    
    //data_ap = (data & STATUS_RUNNING);
    
    if ((data & STATUS_RUNNING) == 0){
      // Start Acq
      CAENComm_Write32(LinkHandle[0], A_Signals, SIGNALS_TTLINK_SOR);  
      printf("\nTTLink SOR...\n");
    }
    else{
      CAENComm_Write32(LinkHandle[0], A_Signals, SIGNALS_TTLINK_EOR);  
      printf("\nTTLink EOR...\n");
    }
    CAENComm_Read32(LinkHandle[0], A_StatusReg, &data);
    //data_ap = (data & STATUS_RUNNING);
    
  }
  
  usleep(100);
}

void demo::CAEN2795::stop() {
  start();
}

// ---------------------------------------------------------------------------
// Read a block of data (32 events???) and get the waveforms
// ---------------------------------------------------------------------------
int ReadEvent(int handle,int *nb,uint32_t* buff)
{
  //uint32_t* buff;   // Max ev size = 4K sample per channel = 4*1024*32+3
  int ret, nw, nword, s;
  
  //int32_t OneSample[32];
  //unsigned short CHeSample;
  //unsigned short CHoSample;
  int record_length=4096;
  
  
  
  // malloc BLT buffer 
  //buff = (uint32_t*)malloc(BUFFER_SIZE*4);
  
  
  // Execute Readout
  nword=0;
  
  ret = CAENComm_BLTRead(handle, A_OutputBuffer, buff, BUFFER_SIZE, &nw);
  if ((ret != CAENComm_Success) && (ret != CAENComm_Terminated)){
    printf("BLTReadCycle Error on Module (ret = %d)\n", ret);
    //getch();
    //return 2;
  }
  nword=nword+(nw);
  auto Evnum = buff[0] & 0x00FFFFFF;
  auto Timestamp = buff[1];

  std::cout << "EventNumber and Timestamp are " << Evnum << ", " << Timestamp << std::endl;
  
  for(size_t i_p=0; i_p<10*8; i_p++){
    std::cout << "0x" << std::hex << buff[i_p] << " " << std::dec;
    if(i_p%8==7) std::cout << std::endl;
  }

  for(s=0; s<record_length; s++){  // HACK: iterare su pi� eventi
    // 1 sample 32 word
    for(nw=0; nw<32; nw++){
      //OneSample[nw] = buff[(32*s)+nw+2];
    }
    /*
    for(ch=0;ch<64;ch++){
      if ((ch%2)==0){ // ch even
	CHeSample = (unsigned short)(0x0000FFFF & OneSample[(ch/2)]);
	acq.wave[ch][s] = CHeSample;
      }
      else {          // ch odd
	CHoSample = (unsigned short)((0xFFFF0000 & OneSample[(ch/2)]) >> 16);
	acq.wave[ch][s] = CHoSample;
      }
    }
    */
  }
  *nb=nword*4;
  
  //free(buff);
  
  return 0;
}

bool demo::CAEN2795::getNext_(artdaq::FragmentPtrs & frags) {

    if (throttle_usecs_ > 0) {
    size_t nchecks = throttle_usecs_ / throttle_usecs_check_;

    for (size_t i_c = 0; i_c < nchecks; ++i_c) {
      usleep( throttle_usecs_check_ );

      if (should_stop()) {
	return false;
      }
    }
  } else {
    if (should_stop()) {
      return false;
    }
  }

  // Set fragment's metadata

  CAEN2795Fragment::Metadata metadata;
  metadata.samples_per_channel = nSamplesPerChannel_;
  metadata.num_adc_bits = nADCBits_;
  metadata.channels_per_board = nChannelsPerBoard_;
  metadata.num_boards = nBoards_;

  std::size_t initial_payload_size = 0;

  frags.emplace_back( artdaq::Fragment::FragmentBytes(initial_payload_size,  
						      ev_counter(), fragment_id(),
						      demo::detail::FragmentType::CAEN2795, metadata) );

  std::cout << "Initialized data of size " << frags.back()->dataSize() << std::endl;

  CAEN2795Fragment newfrag(*frags.back());

  unsigned int data;
  CAENComm_Read32(LinkHandle[0], A_StatusReg, &data);
  data = (data & STATUS_DRDY);

  frags.back()->resizeBytes(BUFFER_SIZE*4);

  std::cout << "Initialized data of size " << frags.back()->dataSize() << std::endl;



  std::cout << "frags.back()->hasMetadata(): " << frags.back()->hasMetadata() << std::endl;
  std::cout << "frags.back()->sizeBytes(): " << frags.back()->sizeBytes() << std::endl;
  std::cout << "frags.back()->dataSizeBytes(): " << frags.back()->dataSizeBytes() << std::endl;
  std::cout << "static_cast<void*>( frags.back()->headerBeginBytes() ): " << static_cast<void*>( frags.back()->headerBeginBytes() ) << std::endl;
  std::cout << "static_cast<void*>( frags.back()->dataBeginBytes() ): " << static_cast<void*>( frags.back()->dataBeginBytes() ) << std::endl;
  if(frags.back()->hasMetadata())
    std::cout << "static_cast<void*>( frags.back()->metadata() ): " << static_cast<void*>( frags.back()->metadata<CAEN2795Fragment::Metadata>() ) << std::endl;
  std::cout << "newfrag.dataTotalBegin(): " << newfrag.dataTotalBegin() << std::endl;

  int nb;
  if ((data & STATUS_DRDY) != 0)
    ReadEvent(LinkHandle[0],&nb,(uint32_t*)newfrag.dataTotalBegin());

  std::cout << "nb val is " << nb << std::endl;

  std::cout << "Sending data of size " << frags.back()->dataSize() << std::endl;

  for(size_t i_p=0; i_p<10*8; i_p++){
    std::cout << "0x" << std::hex << *((uint32_t*)frags.back()->headerBeginBytes() + i_p) << " " << std::dec;
    if(i_p%8==7) std::cout << std::endl;
  }

  if(metricMan_ != nullptr) {
    metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  }

  ev_counter_inc();

  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(demo::CAEN2795) 
