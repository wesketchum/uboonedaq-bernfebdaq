#ifndef bernfebdaq_Generators_BernFEBFakeData_hh
#define bernfebdaq_Generators_BernFEBFakeData_hh

#include "bernfebdaq/Generators/BernFEB_GeneratorBase.hh"

#include <random>

namespace bernfebdaq {    

  class BernFEBFakeData : public bernfebdaq::BernFEB_GeneratorBase {
  public:

    explicit BernFEBFakeData(fhicl::ParameterSet const & ps);
    virtual ~BernFEBFakeData();

  private:

    void ConfigureStart(); //called in start()
    void ConfigureStop();  //called in stop()

    size_t GetFEBData(uint64_t const&);

    std::mt19937 engine_;
    std::unique_ptr<std::uniform_int_distribution<int>> uniform_distn_;
    int data_wait_time_;
    size_t events_per_packet_;
    unsigned int time_increment_per_event_;
    std::vector<unsigned int> missing_events_mod_;

    uint32_t time1_max_;
    uint32_t time2_max_;

    std::map<uint64_t,int64_t> time_map_;

  };
}

#endif /* artdaq_demo_Generators_BernFEBFakeData_hh */
