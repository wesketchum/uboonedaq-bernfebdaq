#include "bernfebdaq-core/Overlays/BernFEBFragment.hh"
#include <iostream>

#include "cetlib/exception.h"

void bernfebdaq::BernFEBFragmentMetadata::CheckTimeWindow() const {
  if(_time_end < _time_start)
    throw cet::exception("BernFragmentMetadata::CheckTimeWindow") 
      << "Failed. time_start (" << _time_start << ") is after time_end (" << _time_end << ")";
}


std::ostream & bernfebdaq::operator << (std::ostream & os, BernFEBFragmentMetadata const& m){
  os << "\nBernFEBFragmentMetadata:"
     << "\n\tTime window start: " << m.time_start()
     << "\n\tTime window end  : " << m.time_end()
     << "\n\tRun number: " << m.run_number()
     << "\n\tSubrun number: " << m.subrun_number()
     << "\n\tSequence number: " << m.sequence_number()
     << std::hex
     << "\n\tFEB ID: " << m.feb_id() 
     << "\n\tReader ID: " << m.reader_id()
     << std::dec
     << "\n\tNumber of channels: " << m.n_channels()
     << "\n\tNumber of adc bits: " << m.n_adc_bits()
     << "\n\tNumber of missed events: " << m.missed_events()
     << "\n\tNumber of overwritten events: " << m.overwritten_events()
     << "\n\tNumber of events recorded: " << m.n_events()
     << "\n\tNumber of datagrams: " << m.n_datagrams();
  os << std::endl;
  return os;
}

std::ostream & bernfebdaq::operator << (std::ostream & os, BernFEBEvent const & e){
  os << "\nBernFEBEvent"
     << "\n\tOverwritten events, Missed events: " 
     << e.flags.overwritten << ", " << e.flags.missed << ", " << std::hex << "(0x" << e.flags_word << std::dec << ")"
     << "\n\tTime1: " << e.time1 << "(" << std::hex << e.time1 << ")" << std::dec
     << "\n\tTime2: " << e.time2 << "(" << std::hex << e.time2 << ")" << std::dec;
  os << std::hex;
  for(size_t i_c=0; i_c<32; ++i_c){
    if(i_c%8==0) os << "\n\t\t";
    os << "0x" << e.adc[i_c] << " ";
  }
  os << std::endl;
  return os;
}

std::ostream & bernfebdaq::operator << (std::ostream & os, BernFEBFragment const & f) {
  os << "BernFEBFragment: "
     << "\n" << *(f.metadata());
  for(size_t i_b=0; i_b<f.metadata()->n_events(); ++i_b)
    os << "\nEvent " << i_b
       << *(f.eventdata(i_b));
  os << std::endl;
  return os;
}

bool bernfebdaq::BernFEBFragment::Verify() const {
  bool verified=true;

  if(metadata()->n_events()*sizeof(BernFEBEvent) != DataPayloadSize() )
    verified = false;

  return verified;
    
}
