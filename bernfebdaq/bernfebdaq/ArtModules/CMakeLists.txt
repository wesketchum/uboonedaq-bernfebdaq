#art_make(NO_PLUGINS # WAnt different link lists for modues
# BASENAME_ONLY
# LIBRARY_NAME icartdaq_ArtModules
# LIB_LIBRARIES icartdaq-core_Overlays icartdaq_Products
#)

#art_make(NO_PLUGINS # WAnt different link lists for modues
#  LIB_LIBRARIES artdaq-core-demo_Overlays 
#)

simple_plugin(BernFEBDump "module"
              bernfebdaq-core_Overlays)

simple_plugin(BernZMQDump "module"
              bernfebdaq-core_Overlays)

simple_plugin(TimeCoincidence "module"
              bernfebdaq-core_Overlays
	      art_Framework_Services_Optional
	      art_Framework_Services_Registry
	      art_Framework_Services_Optional_TFileService_service
	      ${ROOT_BASIC_LIB_LIST} )

# Subdirectories.
#add_subdirectory(DQM)

install_headers()
install_source()
