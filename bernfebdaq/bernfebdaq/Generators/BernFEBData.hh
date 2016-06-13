#ifndef bernfebdaq_Generators_BernFEBData_hh
#define bernfebdaq_Generators_BernFEBData_hh

#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

#include "bernfebdrv/febdrv.h"
#include "bernfebdrv/febconf.h"

namespace bernfebdaq {    

  class BernFEBData : public bernfebdaq::BernFEB_GeneratorBase {
  public:

    explicit BernFEBData(fhicl::ParameterSet const & ps);
    virtual ~BernFEBData();

  private:

    void ConfigureStart(); //called in start()
    void ConfigureStop();  //called in stop()

    size_t GetFEBData(uint64_t const&);
    int    GetDataSetup();
    int    GetDataComplete();

    FEBDRV febdrv;
    FEBCONF febconf;

    void PingFEBs();
    bool runBiasOn;

  };
}

#endif /* artdaq_demo_Generators_BernFEBData_hh */
