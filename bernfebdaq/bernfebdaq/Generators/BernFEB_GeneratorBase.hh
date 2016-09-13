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
#include <sys/time.h>

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

    std::vector<size_t>   TargetDTPSizes_;
    std::vector<uint32_t> MaxTimeDiffs_;
    unsigned int MaxDTPSleep_;           //nanoseconds

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
    typedef boost::circular_buffer<timeval>      FEBEventTimeBuffer_t;
    typedef boost::circular_buffer<unsigned int> FEBEventsDroppedBuffer_t;
    //typedef std::deque<BernFEBEvent> FEBEventBuffer_t;

    typedef std::chrono::high_resolution_clock hires_clock;

  private:

    typedef struct DTPBufferTimer{

      unsigned int                         target_dtp_size;

      hires_clock                          buffer_clock;
      std::chrono::time_point<hires_clock> time_start;
      std::chrono::time_point<hires_clock> time_end;
      unsigned int                         buffer_sleep;

      DTPBufferTimer() { target_dtp_size=0; buffer_sleep=0; }

      void set_target_size(unsigned int t) { target_dtp_size = t; }

      void t_start()      { time_start = buffer_clock.now(); }
      void t_end()        { time_end = buffer_clock.now(); }
      int  nanosec()    { return std::chrono::nanoseconds(time_end-time_start).count(); }
      bool check_time() { t_end(); if( (unsigned int)(nanosec()) < buffer_sleep) return false; else return true; }

      int end_and_update(size_t nevents, unsigned int max_time)
      {
	t_end(); int ns = nanosec(); t_start();
       
	if(ns>0) { 
	  if(nevents==0){ if(target_dtp_size==0) buffer_sleep=0; else buffer_sleep += 1e6; }
	  else { buffer_sleep = ns * (float)(target_dtp_size)/(float)(nevents); }
	}

	if(buffer_sleep > max_time) buffer_sleep = max_time;

	return ns;
      }

    } DTPBufferTimer_t;

    typedef struct FEBBuffer{

      FEBEventBuffer_t             buffer;
      FEBEventTimeBuffer_t         timebuffer;
      FEBEventsDroppedBuffer_t     droppedbuffer;
      std::unique_ptr<std::mutex>  mutexptr;
      size_t                       time_resets;
      int64_t                      next_time_start;
      uint32_t                     overwritten_counter;
      int32_t                      last_time_counter;
      uint32_t                     max_time_diff;
      DTPBufferTimer_t             timer;
      uint64_t                     id;

      FEBBuffer(uint32_t capacity, uint32_t td, uint32_t target_dtp_size, uint64_t i)
	: buffer(FEBEventBuffer_t(capacity)),
	  timebuffer(FEBEventTimeBuffer_t(capacity)),
	  droppedbuffer(FEBEventsDroppedBuffer_t(capacity)),
	  mutexptr(new std::mutex),
	  time_resets(0),
	  next_time_start(0),
	  overwritten_counter(0),
	  last_time_counter(-1),
	  max_time_diff(td),
	  id(i)
      { Init(); timer.set_target_size(target_dtp_size); timer.t_start(); }
      FEBBuffer() { FEBBuffer(0,10000000,0,0); }
      void Init() {
	buffer.clear();
	timebuffer.clear();
	mutexptr->unlock();
	time_resets = 0;
	next_time_start = 0;
	overwritten_counter = 0;
	last_time_counter = 0;
      }
    } FEBBuffer_t;

    std::chrono::system_clock insertTimer_;

    std::unordered_map< uint64_t, FEBBuffer_t  > FEBBuffers_;
    uint32_t FEBBufferCapacity_;
    uint32_t FEBBufferSizeBytes_;

    bool GetData();
    bool FillFragment(uint64_t const&, artdaq::FragmentPtrs &,bool clear_buffer=false);

    size_t InsertIntoFEBBuffer(FEBBuffer_t &,size_t const&);
    size_t EraseFromFEBBuffer(FEBBuffer_t &, size_t const&);

    std::string GetFEBIDString(uint64_t const& id) const;
    void SendMetadataMetrics(BernFEBFragmentMetadata const& m);
    void UpdateBufferOccupancyMetrics(uint64_t const& ,size_t const&) const;
    
    WorkerThreadUPtr GetData_thread_;

  };
}

