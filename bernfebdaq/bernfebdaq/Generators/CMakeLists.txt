art_make_library( LIBRARY_NAME bernfebdaq-BernFEB_GeneratorBase
		  SOURCE BernFEB_GeneratorBase.cc
		  LIBRARIES
			bernfebdaq-core_Overlays
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

#simple_plugin(CAEN2795FakeData "generator"
#  icartdaq-CAEN2795_GeneratorBase
#  icartdaq-core_Overlays
#  artdaq_Application
#  artdaq_DAQdata
#  artdaq-core_Utilities
#  artdaq-core_Data
#  ${ART_UTILITIES}
#  ${FHICLCPP}
#  ${MF_MESSAGELOGGER}
#  ${MF_UTILITIES}
#  ${CETLIB}
#  )
