art_make_library( LIBRARY_NAME bernfebdaq-WorkerThread
		  SOURCE workerThread.cpp )


art_make_library( LIBRARY_NAME bernfebdaq-BernFEB_GeneratorBase
		  SOURCE BernFEB_GeneratorBase.cc
		  LIBRARIES
			bernfebdaq-core_Overlays
			bernfebdaq-WorkerThread
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

art_make_library( LIBRARY_NAME bernfebdaq-BernZMQ_GeneratorBase
		  SOURCE BernZMQ_GeneratorBase.cc
		  LIBRARIES
			bernfebdaq-core_Overlays
			bernfebdaq-WorkerThread
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

simple_plugin(BernFEBFakeData "generator"
  bernfebdaq-BernFEB_GeneratorBase
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

simple_plugin(BernFEBData "generator"
  bernfebdaq-BernFEB_GeneratorBase
  bernfebdaq-core_Overlays
  bernfebdrv_febdrv
  bernfebdrv_febconf
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

simple_plugin(BernZMQData "generator"
  bernfebdaq-BernZMQ_GeneratorBase
  bernfebdaq-core_Overlays
  zmq
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
