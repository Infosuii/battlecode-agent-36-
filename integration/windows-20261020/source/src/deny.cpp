#include "owner_switches.h"
#if V36_OWNER_FEATURES
#include "owners/deny.cpp.inc"
#else
#include "lead/deny.cpp.inc"
#endif
