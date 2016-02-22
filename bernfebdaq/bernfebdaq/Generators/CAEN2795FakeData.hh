#ifndef icartdaq_Generators_CAEN2795FAKEDATA_hh
#define icartdaq_Generators_CAEN2795FAKEDATA_hh

#include "icartdaq/Generators/CAEN2795_GeneratorBase.hh"

#include <random>

namespace icarus {    

  class CAEN2795FakeData : public icarus::CAEN2795_GeneratorBase {
  public:

    explicit CAEN2795FakeData(fhicl::ParameterSet const & ps);
    virtual ~CAEN2795FakeData();

  private:
    /*
    bool getNext_(artdaq::FragmentPtrs & output) override;
    { return getNext_Base(); }
    void start() override;
    { start_Base(); }
    void stop() override;
    { stop_Base(); }
    */
    void ConfigureStart(); //called in start()
    void ConfigureStop();  //called in stop()

    int GetData(size_t&,uint32_t*);       //called in getNext_()

    std::mt19937 engine_;
    std::unique_ptr<std::uniform_int_distribution<int>> uniform_distn_;


  };
}

#endif /* artdaq_demo_Generators_CAEN2795_FAKEDATA_hh */
