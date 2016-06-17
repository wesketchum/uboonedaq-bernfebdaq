cet_make_library( LIBRARY_NAME bernfebdrv_febdrv
		  SOURCE FEBDRV.cc
		  LIBRARIES
		     zmq
		)

cet_make_library( LIBRARY_NAME bernfebdrv_febconf
		  SOURCE febconf.cc
		  LIBRARIES
		     zmq
		)

cet_make_exec(febdrv
		LIBRARIES
		  bernfebdrv_febdrv
		  zmq)

install_headers()

