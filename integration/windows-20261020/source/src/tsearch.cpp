#include "owner_switches.h"
#if V36_OWNER_FEATURES
#include "owners/tsearch.cpp.inc"
#else
#include "lead/tsearch.cpp.inc"
#endif
