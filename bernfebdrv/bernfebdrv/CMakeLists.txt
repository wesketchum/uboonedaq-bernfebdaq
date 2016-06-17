cet_make_library( LIBRARY_NAME bernfebdrv_febdrv
		  SOURCE FEBDRV.cc
		  LIBRARIES
		     zmq
		)

cet_make_library( LIBRARY_NAME bernfebdrv_febconf
		  SOURCE FEBCONF.cc
		  LIBRARIES
		    zmq
		)

cet_make_exec(febdrv
		LIBRARIES
		  bernfebdrv_febdrv
		  zmq)

cet_make_exec(febconf
		LIBRARIES
		  bernfebdrv_febconf
		  zmq)

cet_make_exec(febmon
		LIBRARIES
		  zmq)

cet_make_exec(febstore
		LIBRARIES
		  zmq)

cet_make_exec(febctl
		LIBRARIES
		  zmq)

install_headers()

