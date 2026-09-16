#include "ewaf.h"
#include "ewaf_build_stamp.h"
/* Makes changes to the external Rust archive explicit SwiftPM/Xcode inputs. */
const char *ewaf_link_build_stamp(void) { return EWAF_CORE_BUILD_HASH; }
