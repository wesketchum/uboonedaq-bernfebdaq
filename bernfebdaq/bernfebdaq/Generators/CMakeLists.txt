#include_directories(/home/icadaq/artdaq-demo-base/artdaq-demo/artdaq-demo)
#set(HEADER_FILES /home/icadaq/artdaq-demo-base/artdaq-demo/artdaq-demo/A2795.h)

art_make_library( LIBRARY_NAME icartdaq-CAEN2795_GeneratorBase
		  SOURCE CAEN2795_GeneratorBase.cc
		  LIBRARIES
			icartdaq-core_Overlays
			artdaq_Application
			artdaq_DAQdata
			artdaq-core_Utilities
			artdaq-core_Data
			${ART_UTILITIES}
			${FHICLCPP}
			${MF_MESSAGELOGGER}
			${MF_UTILITIES}
			${CETLIB}
		)

#simple_plugin(CAEN2795 "generator"
#  icartdaq-core_Overlays
#  artdaq_Application
#  artdaq_DAQdata
#  artdaq-core_Utilities
#  artdaq-core_Data
#  CAENComm
#  ${ART_UTILITIES}
#  ${FHICLCPP}
#  ${MF_MESSAGELOGGER}
#  ${MF_UTILITIES}
#  ${CETLIB}
#  )

simple_plugin(CAEN2795FakeData "generator"
  icartdaq-CAEN2795_GeneratorBase
  icartdaq-core_Overlays
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  )
