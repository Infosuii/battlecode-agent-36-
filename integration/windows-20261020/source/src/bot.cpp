#include "owner_switches.h"
#if V36_OWNER_FEATURES
#include "owners/bot.cpp.inc"
#else
#include "lead/bot.cpp.inc"
#endif
