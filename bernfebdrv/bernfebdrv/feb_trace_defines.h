#include "trace.h"

/*****
 * TRACE levels used in the code
 * 
 * 0 - ERROR (likely fatal) conditions
 * 1 - WARNING (likely non-fatal but still bad) conditions
 * 2 - LOGGING (general logging/messaging)
 * 3 - DEBUG (general debugging)

 *****/

#define TR_FEBDRV_ERR     32
#define TR_FEBDRV_WARN    33
#define TR_FEBDRV_LOG     34
#define TR_FEBDRV_DEBUG   35

#define TR_FEBCONF_ERR     36
#define TR_FEBCONF_WARN    37
#define TR_FEBCONF_LOG     38
#define TR_FEBCONF_DEBUG   39

