#ifndef icartdaq_Generators_CAEN2795_FAKEDATA_hh
#define icartdaq_Generators_CAEN2795_FAKEDATA_hh

// CAEN2795 is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, generating all ADC counts with equal probability via
// the std::uniform_int_distribution class

// CAEN2795 is designed to simulate values coming in from one of
// two types of digitizer boards, one called "TOY1" and the other
// called "TOY2"; the only difference between the two boards is the #
// of bits in the ADC values they send. These values are declared as
// FragmentType enum's in artdaq-demo's
// artdaq-core-demo/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "icartdaq-core/Overlays/CAEN2795Fragment.hh"
#include "icartdaq-core/Overlays/FragmentType.hh"
#include "CAENComm.h"
#include "A2795.h"
#include <random>
#include <vector>
#include <atomic>

namespace demo {    

  class CAEN2795FakeData : public artdaq::CommandableFragmentGenerator {
  public:
    explicit CAEN2795FakeData(fhicl::ParameterSet const & ps);

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    // FHiCL-configurable variables. Note that the C++ variable names
    // are the FHiCL variable names with a "_" appended

    uint32_t const  nSamplesPerChannel_;
    uint8_t  const  nADCBits_;
    uint16_t const  nChannelsPerBoard_;
    uint16_t const  nBoards_;
    uint32_t const  RunNumber_;
    std::size_t const  throttle_usecs_;        // Sleep at start of each call to getNext_(), in us
    std::size_t const  throttle_usecs_check_;  // Period between checks for stop/pause during the sleep (must be less than, and an integer divisor of, throttle_usecs_)

    // Members needed to generate the simulated data

    std::mt19937 engine_;
    std::unique_ptr<std::uniform_int_distribution<int>> uniform_distn_;


  };
}

#endif /* artdaq_demo_Generators_CAEN2795_FAKEDATA_hh */
