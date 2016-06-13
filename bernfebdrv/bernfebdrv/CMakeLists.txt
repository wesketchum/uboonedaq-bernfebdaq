cet_make_library( LIBRARY_NAME bernfebdrv_febdrv
		  SOURCE febdrv.cc
		  LIBRARIES
		     zmq
		)

cet_make_library( LIBRARY_NAME bernfebdrv_febconf
		  SOURCE febconf.cc
		  LIBRARIES
		     zmq
		)

install_headers()

