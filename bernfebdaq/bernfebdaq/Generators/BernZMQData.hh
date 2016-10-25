#ifndef bernfebdaq_Generators_BernZMQData_hh
#define bernfebdaq_Generators_BernZMQData_hh

#include "bernfebdaq/Generators/BernZMQ_GeneratorBase.hh"

#include "zmq.h"

namespace bernfebdaq {    

  class BernZMQData : public bernfebdaq::BernZMQ_GeneratorBase {
  public:

    explicit BernZMQData(fhicl::ParameterSet const & ps);
    virtual ~BernZMQData();

  private:

    void ConfigureStart(); //called in start()
    void ConfigureStop();  //called in stop()

    size_t GetZMQData();
    int    GetDataSetup();
    int    GetDataComplete();

    int    zmq_data_receive_timeout_ms_;
    std::string zmq_data_pub_port_;
    void*  zmq_context_;
    void*  zmq_subscriber_;
    


  };
}

#endif /* artdaq_demo_Generators_BernZMQData_hh */
