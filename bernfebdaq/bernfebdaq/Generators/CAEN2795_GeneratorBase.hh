#ifndef icartdaq_Generators_CAEN2795_hh
#define icartdaq_Generators_CAEN2795_hh

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "icartdaq-core/Overlays/CAEN2795Fragment.hh"
#include "icartdaq-core/Overlays/FragmentType.hh"
//#include "CAENComm.h"
//#include "A2795.h"
#include <unistd.h>
#include <vector>
#include <atomic>

namespace icarus {    

  class CAEN2795_GeneratorBase : public artdaq::CommandableFragmentGenerator{
  public:
    explicit CAEN2795_GeneratorBase(fhicl::ParameterSet const & ps);
    virtual ~CAEN2795_GeneratorBase();

    //private:
  protected:

    bool getNext_(artdaq::FragmentPtrs & output) override;
    void start() override;
    void stop() override;

    uint32_t RunNumber_;
    uint32_t EventsPerSubrun_;
    
    uint32_t SamplesPerChannel_;
    uint8_t  nADCBits_;
    uint16_t ChannelsPerBoard_;
    uint16_t nBoards_;

    uint8_t  CrateID_;
    std::vector<CAEN2795FragmentMetadata::id_t> BoardIDs_;

    std::size_t throttle_usecs_;        // Sleep at start of each call to getNext_(), in us
    std::size_t throttle_usecs_check_;  // Period between checks for stop/pause during the sleep (must be less than, and an integer divisor of, throttle_usecs_)

    uint32_t current_subrun_;
    size_t event_number_;


    //These functions MUST be defined by the derived classes
    virtual void ConfigureStart() = 0; //called in start()
    virtual void ConfigureStop() = 0;  //called in stop()
    virtual int GetData(size_t&,uint32_t*) = 0;       //called in getNext_()

    size_t last_read_data_size_;
    int    last_status_;

  protected:

    CAEN2795FragmentMetadata metadata_;
    fhicl::ParameterSet const ps_;

    //These functions could be overwritten by the derived class
    virtual void Initialize();     //called in constructor
    virtual void Cleanup();        //called in destructor

  };
}

#endif /* artdaq_demo_Generators_CAEN2795_hh */
