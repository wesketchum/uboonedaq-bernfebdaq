/* sends literal command without parameters to feb */
#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>

#define MAXPACKLEN 1500

class FEBCONF{

 public:

  FEBCONF(){}

  void SetConfs(std::string scrf,std::string pmrf)
  { SetConfs(scrf.c_str(),pmrf.c_str()); }

  void SetConfs(const char* scrf, const char* pmrf)
  { SetSCRConf(scrf); SetPMRConf(pmrf); }

  void SetSCRConf(std::string scrf)
  { SetSCRConf(scrf.c_str()); }

  void SetPMRConf(std::string pmrf)
  { SetPMRConf(pmrf.c_str()); }

  void SetSCRConf(const char*);
  void SetPMRConf(const char*);

  void SendConfs(uint8_t const&);

  bool configOK() const { return (config_SCR_OK_ && config_PMR_OK_); }
  
  uint8_t const* GetSCRConf() const { return bufSCR; }
  uint8_t const* GetPMRConf() const { return bufPMR; }

  uint8_t const* GetConfBuffer(uint8_t const&);

 private:

  int readbitstream(const char*,uint8_t*);

  uint8_t bufSCR[MAXPACKLEN];
  uint8_t bufPMR[MAXPACKLEN];
  uint8_t confbuf[MAXPACKLEN];

  bool config_SCR_OK_;
  bool config_PMR_OK_;
};
