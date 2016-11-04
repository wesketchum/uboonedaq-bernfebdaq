#cet_script()


art_make_exec(test_driver
  LIBRARIES
  artdaq_Application
  artdaq_DAQrate
  artdaq-utilities_Plugins
  artdaq-utilities_Plugins_graphite_metric
  artdaq-utilities_Plugins_file_metric
  artdaq-core_Utilities
  artdaq-core_Core
  ${ART_FRAMEWORK_ART}
  ${FHICLCPP}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${MPI_C_LIBRARIES}
  ${CETLIB}
  )

art_make_exec(readfhicl
  LIBRARIES
  ${FHICLCPP}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${CETLIB}
  )  

add_subdirectory(snippets)

# Is this necessary?
#install_source()
