#ifndef artdaq_demo_BuildInfo_GetPackageBuildInfo_hh
#define artdaq_demo_BuildInfo_GetPackageBuildInfo_hh

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <string>

namespace demo {

  struct GetPackageBuildInfo {

    static artdaq::PackageBuildInfo getPackageBuildInfo();
  };

}

#endif /* artdaq_demo_BuildInfo_GetPackageBuildInfo_hh */
