// Materialized settings for combined owner integration; validation candidate.
#ifndef A2_BUSH_FAST
#define A2_BUSH_FAST 1
#endif
#ifndef A2_FAST_AMBUSH
#define A2_FAST_AMBUSH 1
#endif
#ifndef A2_CMICRO_FAST
#define A2_CMICRO_FAST 1
#endif
#ifndef A2_GOV
#define A2_GOV 1
#endif
#ifndef A2_SNAP
#define A2_SNAP 4
#endif
#ifndef A2_TD_AUTH
#define A2_TD_AUTH 1
#endif
#ifndef A2_FLEE_HIT
#define A2_FLEE_HIT 1
#endif
#ifndef NOVA_V36_DRAFT
#define NOVA_V36_DRAFT 1
#endif
// Owner experiment selection; the lead source path remains an immutable fallback.
#pragma once
#ifndef NOVA_V36_DRAFT
#define NOVA_V36_DRAFT 0
#endif
#define V36_OWNER_FEATURES (A2_BUSH_FAST || A2_FAST_AMBUSH || A2_CMICRO_FAST || A2_GOV || A2_GOV_WALK || A2_GOV_WARD || A2_SNAP || A2_TD_AUTH || A2_FLEE_HIT || A2_TEL || A2_CHECK || NOVA_V36_DRAFT || NOVA_MAPID_STRICT)
