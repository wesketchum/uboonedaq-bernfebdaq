#include "trace.h"

/*****
 * TRACE levels used in the code
 * 
 * 0 - ERROR (likely fatal) conditions
 * 1 - WARNING (likely non-fatal but still bad) conditions
 * 2 - LOGGING (general logging/messaging)
 * 3 - DEBUG (general debugging)

 *****/

#define TR_FEBDRV_ERR     12
#define TR_FEBDRV_WARN    13
#define TR_FEBDRV_LOG     14
#define TR_FEBDRV_DEBUG   15

#define TR_FEBCONF_ERR     16
#define TR_FEBCONF_WARN    17
#define TR_FEBCONF_LOG     18
#define TR_FEBCONF_DEBUG   19

