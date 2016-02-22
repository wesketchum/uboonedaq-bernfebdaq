art_make(NO_PLUGINS # WAnt different link lists for modues
 BASENAME_ONLY
 LIBRARY_NAME icartdaq_ArtModules
 LIB_LIBRARIES icartdaq-core_Overlays icartdaq_Products
)

#art_make(NO_PLUGINS # WAnt different link lists for modues
#  LIB_LIBRARIES artdaq-core-demo_Overlays 
#)

simple_plugin(CAEN2795Dump "module"
              icartdaq-core_Overlays)

# Subdirectories.
#add_subdirectory(DQM)
