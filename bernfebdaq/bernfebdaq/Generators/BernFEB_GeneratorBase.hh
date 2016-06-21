#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "bernfebdaq-core/Overlays/BernFEBFragment.hh"
#include "bernfebdaq-core/Overlays/FragmentType.hh"

#include <unistd.h>
#include <vector>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <boost/circular_buffer.hpp>

#include "workerThread.h"

namespace bernfebdaq {    

  class BernFEB_GeneratorBase : public artdaq::CommandableFragmentGenerator{
  public:
    explicit BernFEB_GeneratorBase(fhicl::ParameterSet const & ps);
    virtual ~BernFEB_GeneratorBase();

    //private:
  protected:

    bool getNext_(artdaq::FragmentPtrs & output) override;
    void start() override;
    void stop() override;

    uint32_t RunNumber_;
    uint64_t SubrunTimeWindowSize_;
    uint32_t SequenceTimeWindowSize_; //in nanoseconds

    uint32_t ReaderID_;
    uint32_t nChannels_;
    uint32_t nADCBits_;

    std::vector<uint64_t> FEBIDs_;
    size_t nFEBs() { return FEBIDs_.size(); }

    std::size_t throttle_usecs_;        // Sleep at start of each call to getNext_(), in us
    std::size_t throttle_usecs_check_;  // Period between checks for stop/pause during the sleep (must be less than, and an integer divisor of, throttle_usecs_)

    uint32_t current_subrun_;
    size_t event_number_;


    //These functions MUST be defined by the derived classes
    virtual void ConfigureStart() = 0; //called in start()
    virtual void ConfigureStop() = 0;  //called in stop()

    //gets the data. Output is size of data filled. Input is FEM ID.
    virtual size_t GetFEBData(uint64_t const&) = 0;
    virtual int    GetDataSetup() { return 1; }
    virtual int    GetDataComplete() { return 1; }

    size_t last_read_data_size_;
    int    last_status_;

  protected:

    BernFEBFragmentMetadata metadata_;
    fhicl::ParameterSet const ps_;

    //These functions could be overwritten by the derived class
    virtual void Initialize();     //called in constructor
    virtual void Cleanup();        //called in destructor

    std::unique_ptr<BernFEBEvent[]> FEBDTPBufferUPtr;
    uint32_t FEBDTPBufferCapacity_;
    uint32_t FEBDTPBufferSizeBytes_;

    typedef boost::circular_buffer<BernFEBEvent> FEBEventBuffer_t;

  private:
    typedef struct FEBBuffer{
      FEBEventBuffer_t buffer;
      std::mutex       buffer_mutex;
      size_t           time_resets;
      int64_t          next_time_start;
      uint32_t         overwritten_counter;
      int32_t          last_time_counter;
      FEBBuffer(uint32_t capacity)
	: buffer(FEBEventBuffer_t(capacity)),
	  time_resets(0),
	  next_time_start(0),
	  overwritten_counter(0),
	  last_time_counter(-1)
      { buffer.clear(); }
      FEBBuffer() { FEBBuffer(0); }
      FEBBuffer& operator=(const FEBBuffer& b){
	  buffer = b.buffer;
	  time_resets = b.time_resets;
	  next_time_start = b.next_time_start;
	  overwritten_counter = b.overwritten_counter;
	  last_time_counter = b.last_time_counter;
	  return *this;
      }
    } FEBBuffer_t;

    std::unordered_map< uint64_t, FEBBuffer_t > FEBBuffers_;
    uint32_t FEBBufferCapacity_;
    uint32_t FEBBufferSizeBytes_;

    bool GetData();
    bool FillFragment(uint64_t const&, artdaq::FragmentPtrs &,bool clear_buffer=false);

    size_t InsertIntoFEBBuffer(FEBBuffer_t &,size_t const&);
    size_t EraseFromFEBBuffer(FEBBuffer_t &,
			      FEBEventBuffer_t::iterator const&,
			      FEBEventBuffer_t::iterator const&);

    std::string GetFEBIDString(uint64_t const& id) const;
    void SendMetadataMetrics(BernFEBFragmentMetadata const& m);
    void UpdateBufferOccupancyMetrics(uint64_t const& ,size_t const&) const;
    
    WorkerThreadUPtr GetData_thread_;

  };
}

