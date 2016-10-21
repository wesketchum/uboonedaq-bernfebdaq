#ifndef bernfebdaq_Generators_BernFEBDataZMQ_hh
#define bernfebdaq_Generators_BernFEBDataZMQ_hh

#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

#include "zmq.h"

namespace bernfebdaq {    

  class BernFEBDataZMQ : public bernfebdaq::BernFEB_GeneratorBase {
  public:

    explicit BernFEBDataZMQ(fhicl::ParameterSet const & ps);
    virtual ~BernFEBDataZMQ();

  private:

    void ConfigureStart(); //called in start()
    void ConfigureStop();  //called in stop()

    size_t GetFEBData(uint64_t const&);
    int    GetDataSetup();
    int    GetDataComplete();

    int    zmq_data_receive_timeout_ms_;
    std::string zmq_data_pub_port_;
    void*  zmq_context_;
    void*  zmq_subscriber_;
    


  };
}

#endif /* artdaq_demo_Generators_BernFEBData_hh */
