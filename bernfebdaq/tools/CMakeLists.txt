cet_script(setupDemoEnvironment.sh DemoControl.rb 
startSystem.sh manageSystem.sh
start2x2System.sh manage2x2System.sh 
start2x2x2System.sh manage2x2x2System.sh
CreateNewFragmentGenerator.rb  generateEventBuilderMain.rb
generateWFViewer.rb generateEventBuilder.rb demo_utilities.rb 
generateToy.rb generateUDP.rb
generatePattern.rb generateNormal.rb generateAscii.rb generateAggregator.rb 
generateAggregatorMain.rb generateV1720.rb generateCompression.rb generateBoardReaderMain.rb RunDriver.rb
makeConfigs.rb
multi_udp_send_artdaq.py)


art_make_exec(test_driver
  LIBRARIES
  artdaq_Application
  artdaq_DAQrate
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
