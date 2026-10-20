#define APEX_LOWHP_AGGR 0.6f
#define NOVA_BURST_K 1.0f
#define APEX_BUILD 12
#define NOVA_DUO 5
#define NOVA_DUO_MAPS 1
#define NOVA_GOLD_UNSEEN 0.10f
#define NOVA_WARD_AHEAD 6
#define NOVA_DENY_RYTH 1
#define APEX_DET_TS_BUDGET 70000
#define APEX_ROUTE_DEPTH 2
#define NOVA_BIGJOB 0.6f
#define NOVA_BIGJOB_FROM 60
#define NOVA_BIGJOB_MULT 6.0f
#define APEX_KITE_CAST 3
// calsearch variant of apex_c9dev
// c8c: c1dev with #define APEX_GOLD_WEIGHT 0.10f; #define APEX_COHESION 0.05f; #define APEX_COHESION_DIST 8; #define APEX_CHANNEL_GUARD 1.0f; #define APEX_RANGED_FLEE 1; #define APEX_TS_DECIDE 1; #define APEX_TS_DECIDE_MARGIN 2.0f; #define APEX_DET 1; #define APEX_SIEGE 1; #define APEX_SHARE_GOLD 1; #define APEX_FLEE_HOME 150; #define APEX_THREAT_RETREAT 0.4f; #define APEX_CMICRO 1; #define APEX_CMICRO_RISK 1.5f; #define APEX_CMICRO_CASSI 2; #define APEX_MAPDRAFT 1; #define APEX_MD_LANES 2; #define APEX_MD_ELKRING 4
#define APEX_MAPDRAFT 1
#define APEX_MD_LANES 2
#define APEX_MD_ELKRING 4
#define APEX_END_BANK 600
#define APEX_TRIB_DENY 3
#define APEX_CLASSDRAFT 1
#define APEX_BRAIN 1
#define APEX_L5 8
#define APEX_L4 8
#define APEX_L3 8
#define APEX_L2 8
#define APEX_L1 8
#define APEX_L0 8
#define APEX_CASTER_SELF_TTD 1.2325f
#define APEX_CI_RISK 1.2779f
#define APEX_KITE 3
#define APEX_NEAR_DIST 6
#define APEX_BOOT_BONUS 0.3329f
#define APEX_BOOT_ITEMS 15
#define APEX_EVENTS 2
#define APEX_CASTER_TTD 1.0379f
#define APEX_AHEAD_AGGR 1.0063f
#define APEX_AHEAD_GAP 4
#define APEX_BEHIND_CARRY 0.9080f
#define APEX_BEHIND_AGGR 1.1235f
#define APEX_BEHIND_GAP 0
#define APEX_C5_HUNT 0
#define APEX_C5_DANGER_TTD 5.7688f
#define APEX_C5_AGGRESSION 1.6816f
#define APEX_C4_HUNT 0
#define APEX_C4_DANGER_TTD 5.5131f
#define APEX_C4_AGGRESSION 1.3366f
#define APEX_C3_HUNT 0
#define APEX_C3_DANGER_TTD 6.3794f
#define APEX_C3_AGGRESSION 1.8115f
#define APEX_C2_HUNT 0
#define APEX_C2_DANGER_TTD 6.1961f
#define APEX_C2_AGGRESSION 1.6969f
#define APEX_C1_HUNT 0
#define APEX_C1_DANGER_TTD 5.8849f
#define APEX_C1_AGGRESSION 1.2856f
#define APEX_C0_HUNT 0
#define APEX_C0_DANGER_TTD 7.6922f
#define APEX_C0_AGGRESSION 1.5356f
#define APEX_HUNT_RANGE 8
#define APEX_HUNT_GEMS 15
#define APEX_DANGER_HP 0.1916f
#define APEX_CHASE 6
#define APEX_STICKY 1.5230f
#define APEX_NEED_POOL 1.0176f
#define APEX_WAIT_WEIGHT 3.2398f
#define APEX_STAGE_TIMEOUT 7
#define APEX_GOLD_WEIGHT 0.18f
#define APEX_WALK_DIST 7
#define APEX_SHOP_GOLD 126
#define APEX_RETREAT_HP 0.1288f
#define APEX_TRIBUTE_CARRY 72
// apex_v18 = apex_v17 + ward-aware hiding (a bush with an enemy ward in it is no cover; +73 vs v17)
// apex_v12 = bots/apex at 07a252d7 with: SPSA thresholds (frozen by tools/autopilot.py 2026-10-14 12:32)
// Shared includes, small geometry helpers and logging for the apex bot.
#pragma once
#include <cambc.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace bc = cambc;
using bc::team;
using bc::vec2;

// ---- nova switches (defaults; variant.py overrides by defining first) ----
// NOVA_BUSH_WARD (v25): a camp on a bush tile is invisible from outside its bush, and basic attacks
// need a visible target, so its group could never be finished: champions stood beside it for the
// rest of the game (41% of all champion-turns vs an idle opponent on fresh maps). A ward placed on
// the camp's own tile reveals it (lab-tested), so walk within ward range and drop the ward there.
#ifndef NOVA_BUSH_WARD
#define NOVA_BUSH_WARD 1
#endif
// NOVA_UNBOX (v25): boxed in by live camps (they respawned around us), attack one to get out.
#ifndef NOVA_UNBOX
#define NOVA_UNBOX 1
#endif
// NOVA_CAMP_SAFE = K (0 = off): leave a camp group's reach while K x the damage of every camp in
// reach would kill us, even if a teammate is tanking right now (see fight_camps).
#ifndef NOVA_CAMP_SAFE
#define NOVA_CAMP_SAFE 0.0f
#endif
// NOVA_CASSI_BUILD (0 = off): Cassi buys health before focus (see build_for).
#ifndef NOVA_CASSI_BUILD
#define NOVA_CASSI_BUILD 0
#endif
// NOVA_AMBUSH_SAFE = f (v26: 0.6, 0 = off): no bush ambush below f x max health (see bush_ambush):
// deaths 3.57 vs 3.85 per game, 53.2% vs v25 on clean dev maps.
#ifndef NOVA_AMBUSH_SAFE
#define NOVA_AMBUSH_SAFE 0.6f
#endif
// NOVA_SIEGE_LOS (v26, 0 = off): siege only from ring tiles with a clear line to a live camp
// (contest: 0 gems in 2000 rounds vs an idle team without it, 1000 by round ~1100 with it).
#ifndef NOVA_SIEGE_LOS
#define NOVA_SIEGE_LOS 1
#endif
// NOVA_CI_RETREAT = m (1 = off): Cassi and Issac go home below m x the usual retreat health.
#ifndef NOVA_CI_RETREAT
#define NOVA_CI_RETREAT 1.0f
#endif
// NOVA_BIGJOB (0 = off): a camp group holding at least this share of the map's gems may be opened as a team job
// needing up to the whole team (otherwise jobs needing 3+ champions are never started). Stampede's only gem camp
// (360 of 360) was left to the enemy all game: we banked 0 against a weaker bot; alone we first took it at r300.
#ifndef NOVA_BIGJOB
#define NOVA_BIGJOB 0.0f
#endif
// NOVA_BIGJOB_FROM / NOVA_BIGJOB_MULT: from this round, a dominant group's value is multiplied by MULT so the whole
// team converges on it (stampede opponents commit their whole team to the big camp at rounds 117-182).
#ifndef NOVA_BIGJOB_FROM
#define NOVA_BIGJOB_FROM 100000
#endif
#ifndef NOVA_BIGJOB_MULT
#define NOVA_BIGJOB_MULT 1.0f
#endif
// NOVA_STUN_FOCUS (0 = off): a stunned enemy's target worth (and the micro's damage value on it) is multiplied by
// 1 + this. Top teams finish kills after stuns (Stellar lands 33% of their killing blows, ours 24%; most deaths in
// decoded top-vs-top replays follow a stun).
#ifndef NOVA_STUN_FOCUS
#define NOVA_STUN_FOCUS 0.0f
#endif
// NOVA_DENY_RYTH (0 = off): tribute denial also counts Ryth's area stun (engine kind aoe_damage, not cc) as a stun.
#ifndef NOVA_DENY_RYTH
#define NOVA_DENY_RYTH 0
#endif
// NOVA_CMICRO_MELEE (0 = off): melee champions use the caster micro's tile scoring too, with "step, then hit" options
// (melee can move and attack in one turn); Issac takes Cassi's risk multiplier. No attack option = normal fight logic.
// Platform data: our Varg/Issac take a third more champion damage than top teams' for the same damage dealt.
#ifndef NOVA_CMICRO_MELEE
#define NOVA_CMICRO_MELEE 0
#endif
// NOVA_WARD_AHEAD (0 = off): with no enemy in view and our ward farther than NOVA_WARD_AHEAD tiles, move the ward into
// the nearest bush cluster no ally ward covers, preferring bushes near our target camp. Platform data: top teams
// place 3.7x our wards (92% in bushes); in 26% of our deaths an attacker sat in a bush within 8 tiles beforehand.
#ifndef NOVA_WARD_AHEAD
#define NOVA_WARD_AHEAD 0
#endif
// NOVA_ALTAR_INFER (0 = off): when the enemy score rises and we cannot see the banker, an enemy stands at an altar we
// cannot see: add a sighting there (tribute routes avoid it). 40% of our deaths after round 300 are within 5 tiles
// of an altar (platform summaries).
#ifndef NOVA_ALTAR_INFER
#define NOVA_ALTAR_INFER 0
#endif
// NOVA_DUO (0 = off): a ranged champion works its melee buddy's camp (see buddy()), at full range so the camps hit
// the buddy. Platform summaries (rounds 0-40): top teams take 0.16 camp damage per damage dealt, we 0.33; their
// Cassi/Stellar/Tavi/Prism take 0-115 of it, ours 100-360. Bit 1: follow; bit 2: the melee leader scores camps for
// two; bit 4: the ranged member keeps a camp it has already damaged. NOVA_DUO_DIST: follow only within this many
// steps; NOVA_DUO_UNTIL: last round.
#ifndef NOVA_DUO
#define NOVA_DUO 0
#endif
#ifndef NOVA_DUO_DIST
#define NOVA_DUO_DIST 14
#endif
// NOVA_DUO_MAPS: 0 = every map; 1 = only maps outside the official pool (nova_map_id = -1). Self-play: duo +4 points
// on unseen maps, -3 to -5 on official ones (where the item build alone gains +4.5).
#ifndef NOVA_DUO_MAPS
#define NOVA_DUO_MAPS 0
#endif
// NOVA_GOLD_UNSEEN (< 0 = off): gold weight on maps outside the official pool. 0.18 (the official-map real-game value)
// cost 6.6 points on the short generated suites (maps_test/maps_clean: ~410-round games end before items pay off).
#ifndef NOVA_GOLD_UNSEEN
#define NOVA_GOLD_UNSEEN -1.0f
#endif
#ifndef NOVA_DUO_UNTIL
#define NOVA_DUO_UNTIL 3000
#endif
// NOVA_LATE_CARRY (1 = off): from round NOVA_LATE_CARRY_R the tribute carry is multiplied by this. Platform summaries:
// we lose 88 gems a game on death against the top teams' 52, and bank 66-74 gem bags after round 400 against their 57.
#ifndef NOVA_LATE_CARRY
#define NOVA_LATE_CARRY 1.0f
#endif
#ifndef NOVA_LATE_CARRY_R
#define NOVA_LATE_CARRY_R 400
#endif
// Tactical-decide weights (see tactical_decide): life value from NOVA_TD_LIFE0 at round 0 to NOVA_TD_LIFE1 at
// round NOVA_TD_ROUND; health bar NOVA_TD_HP (or NOVA_TD_HP_SHARE x life when > 0); carried gem NOVA_TD_GEM.
#ifndef NOVA_TD_LIFE0
#define NOVA_TD_LIFE0 8.0f
#endif
#ifndef NOVA_TD_LIFE1
#define NOVA_TD_LIFE1 25.0f
#endif
#ifndef NOVA_TD_ROUND
#define NOVA_TD_ROUND 450
#endif
#ifndef NOVA_TD_HP
#define NOVA_TD_HP 3.0f
#endif
#ifndef NOVA_TD_HP_SHARE
#define NOVA_TD_HP_SHARE 0.0f
#endif
#ifndef NOVA_TD_GEM
#define NOVA_TD_GEM 0.75f
#endif
// NOVA_AOE_AIM (0 = off): aim ground area spells at the best tile (reach range + radius); see cast_on_enemy.
#ifndef NOVA_AOE_AIM
#define NOVA_AOE_AIM 0
#endif
// NOVA_QUARTIC (0 = off): non-linear health value in the tactical search evaluation (see tsearch.h).
#ifndef NOVA_QUARTIC
#define NOVA_QUARTIC 0
#endif
#ifndef NOVA_COMMIT_LATE
#define NOVA_COMMIT_LATE 0.0f
#endif
// NOVA_COMMIT_RETREAT (0 = off): see handle_enemies.
#ifndef NOVA_COMMIT_RETREAT
#define NOVA_COMMIT_RETREAT 0.0f
#endif
// NOVA_SEED (0 = off): evaluation-only jitter of camp-group values (see score_groups).
#ifndef NOVA_SEED
#define NOVA_SEED 0
#endif
#ifndef NOVA_SEED_EPS
#define NOVA_SEED_EPS 0.04f
#endif
// NOVA_SPAWN_HOT (0 = off): stop holding spawn for the heal while an awake camp reaches it.
#ifndef NOVA_SPAWN_HOT
#define NOVA_SPAWN_HOT 0
#endif
// NOVA_FIGHT2 (v29 fight rewrite, bits; 0 = off): 1 strength gate, 2 focus fire on wounded targets,
// 4 regroup toward teammates when fleeing. Tunables below.
#ifndef NOVA_FIGHT2
#define NOVA_FIGHT2 0
#endif
#ifndef NOVA_F2_RADIUS
#define NOVA_F2_RADIUS 6
#endif
#ifndef NOVA_F2_ENGAGE
#define NOVA_F2_ENGAGE 2.0f
#endif
#ifndef NOVA_F2_AVOID
#define NOVA_F2_AVOID 0.6f
#endif
#ifndef NOVA_F2_REACH
#define NOVA_F2_REACH 6
#endif
#ifndef NOVA_F2_FOCUS
#define NOVA_F2_FOCUS 1.5f
#endif
// NOVA_BUDDY = k (0 = off): group value falls by k per tile beyond NOVA_BUDDY_DIST from our buddy's target.
#ifndef NOVA_BUDDY
#define NOVA_BUDDY 0.0f
#endif
#ifndef NOVA_BUDDY_DIST
#define NOVA_BUDDY_DIST 6
#endif
#ifndef NOVA_SPAWN_IGNORE
#define NOVA_SPAWN_IGNORE 2
#endif
#ifndef NOVA_EARLY_SAFE_UNTIL
#define NOVA_EARLY_SAFE_UNTIL 0 // before this round, flee when outnumbered by NOVA_EARLY_SAFE_K
#endif
#ifndef NOVA_EARLY_SAFE_K
#define NOVA_EARLY_SAFE_K 1
#endif
#ifndef NOVA_RUSH_UNTIL
#define NOVA_RUSH_UNTIL 0 // opening rush: the pack hunts at the enemy's nearest camps until this round
#endif
#ifndef NOVA_RUSH_AGGR
#define NOVA_RUSH_AGGR 1.5f
#endif
#ifndef NOVA_RUSH_RADIUS
#define NOVA_RUSH_RADIUS 20 // enemy-side camps within this distance of their spawn
#endif
#ifndef NOVA_RUSH_GEMS
#define NOVA_RUSH_GEMS 30
#endif
#ifndef NOVA_RUSH_MARGIN
#define NOVA_RUSH_MARGIN 0
#endif
#ifndef NOVA_BURST_K
#define NOVA_BURST_K 0.0f // flee when health < K x the burst visible enemies can land next turn (0 = off)
#endif
// Which types get the caster item build: 0 = stellar/cassi/prism, 1 = + tavi, 2 = + willow, marcella,
// 3 = all low-strength-scale types (ryth less).
#ifndef NOVA_CASTER_SET
#define NOVA_CASTER_SET 0
#endif
// One fate: Cassi and Issac battle deaths together, home when either body is low.
#ifndef NOVA_CI_LINK
#define NOVA_CI_LINK 0
#endif
#ifndef NOVA_NO_SIGHT
#define NOVA_NO_SIGHT 0 // test: no shared enemy sightings at all
#endif
// Pack mode (idea 1, top-team counter): when the enemy drafted >= N casters, run the team fight
// commander every turn (focus fire on the lowest-health enemy, all-run retreat orders) with a lower
// verdict margin than the default tuning.
#ifndef NOVA_PACK_C
#define NOVA_PACK_C 0
#endif
#ifndef NOVA_PACK_MARGIN
#define NOVA_PACK_MARGIN 2.0f
#endif
// Big-group scheduling (expace study): 0 = off, else the wait-surge multiplier (x100) for groups
// worth >= 120 gems (35 = 0.35): those respawn on a 40-60 round cycle, so waiting near one is cheap.
#ifndef NOVA_BIGCAMP
#define NOVA_BIGCAMP 0
#endif
// Unknown-map draft (idea 3): 1 = read the map at draft time (monolith presence, camp density,
// size) and pick a proven lineup for the class. 0 = the old behaviour (fallback lineup everywhere).
#ifndef NOVA_DRAFT_AI
#define NOVA_DRAFT_AI 0
#endif
// Lineup choice for unknown maps (NOVA_DRAFT_AI): sparse big maps get the siege-farm comp,
// monolith maps the melee wrecking crew, everything else keeps NOVA_FALLBACK_LINEUP.
#ifndef NOVA_AI_BIG_LINEUP
#define NOVA_AI_BIG_LINEUP 2
#endif
#ifndef NOVA_AI_MONO_LINEUP
#define NOVA_AI_MONO_LINEUP 6
#endif
#ifndef NOVA_AI_BIG_AREA
#define NOVA_AI_BIG_AREA 4000 // w*h at least this for the sparse-big class
#endif
#ifndef NOVA_AI_SPARSE
#define NOVA_AI_SPARSE 250 // tiles per camp group at least this for the sparse-big class
#endif
// Counter-pick (idea 3b): with >= N enemy caster picks already on the board, avoid making more
// caster picks of our own (fight the burst with tanky bodies instead). 0 = off.
#ifndef NOVA_COUNTER
#define NOVA_COUNTER 0
#endif

// NOVA_BANK_FIX (season-3 structural fix): on sparse maps every bag of NOVA_EDGE_BAG gems or
// more goes to the altar until round NOVA_EDGE_UNTIL. Evidence: the 10-2026-10-16 replay bundle
// showed our bags of 22-58 held 100-200 rounds (corners 0 submissions to r150, rushbait to
// r200; local C-base mirrored both) while ladder opponents banked continuously (their r50-r150
// scores were 22-220) and built the items that closed the mid game. Big-strip maps (whose
// largest camp group is worth NOVA_BIG_MAP_GEMS gems or more, read from map facts at draft)
// keep the strip-camp carry discipline. 0 = off.
#ifndef NOVA_BANK_FIX
#define NOVA_BANK_FIX 1 // adopted 2026-10-17: ladder bundle showed 0-submission openings lose (see common.h note)
#endif
#ifndef NOVA_EDGE_BAG
#define NOVA_EDGE_BAG 22
#endif
#ifndef NOVA_EDGE_UNTIL
#define NOVA_EDGE_UNTIL 250
#endif
#ifndef NOVA_BIG_MAP_GEMS
#define NOVA_BIG_MAP_GEMS 100
#endif
#ifndef NOVA_EDGE_DIST
#define NOVA_EDGE_DIST 14 // longest sparse-map opening bank trip still worth taking
#endif
#ifndef NOVA_DEATH_BASE
#define NOVA_DEATH_BASE 150.0f // value lost when a champion dies (bag, respawn, kill gold)
#endif
#ifndef NOVA_DEATH_BAG
#define NOVA_DEATH_BAG 2.0f // extra value per carried gem
#endif
// NOVA_TGT (structural fight targeting): upgrades the engage target score (ttk / worth).
// Changes: distance discount for out-of-reach targets (they walk while we approach),
// fire-focus (killing high-threat champions denies their dps), and a finishing bonus for
// already-wounded targets. Default 0 = the scoring compiles out exactly as today.
// NOVA_BLEND (structural, v24): per-map-class behavior blends. The draft records each map's
// class in draft memory (bit0 = race/dense map: total gems / area >= NOVA_RACE_DENS; bit1 =
// monolith map); blendRace=1 makes the ret/blend + NOVA_TGT settings fire only on race-class
// maps, keeping the co-adapted baseline on sparse-carry maps.
#ifndef NOVA_CLASSIFY
#define NOVA_CLASSIFY 1 // write the class bits to draft memory (inert unless a consumer reads them)
#endif
#ifndef NOVA_BLEND_RACE
#define NOVA_BLEND_RACE 0
#endif
#ifndef NOVA_BLEND_RETREAT
#define NOVA_BLEND_RETREAT 0.2000f // retreat_hp on race-class maps (v23 default is 0.1288)
#endif
inline constexpr bool classify = NOVA_CLASSIFY > 0;
inline constexpr bool blendRace = NOVA_BLEND_RACE > 0;
inline constexpr float blendRetreat = NOVA_BLEND_RETREAT;
#ifndef NOVA_TGT
#define NOVA_TGT 0
#endif
#ifndef NOVA_TGT_THREAT
#define NOVA_TGT_THREAT 2.0f // worth added per point of enemy sustained dps
#endif
#ifndef NOVA_TGT_WOUNDED
#define NOVA_TGT_WOUNDED 0.75f // worth bonus for targets already below the wound line
#endif
#ifndef NOVA_TGT_WOUNDED_HP
#define NOVA_TGT_WOUNDED_HP 0.45f // the wound fraction line
#endif
inline constexpr bool tgtNew = NOVA_TGT > 0;
inline constexpr float tgtThreat = NOVA_TGT_THREAT;
inline constexpr float tgtWounded = NOVA_TGT_WOUNDED;
inline constexpr float tgtWoundedHp = NOVA_TGT_WOUNDED_HP;
inline constexpr float deathBase = NOVA_DEATH_BASE;
inline constexpr float deathBag = NOVA_DEATH_BAG;
inline constexpr bool bankFix = NOVA_BANK_FIX > 0;

// NOVA_RACE_BANK (v24): dense maps (total gems / tile area >= NOVA_RACE_DENS) play as score races:
// the pool is small relative to the board and respawn cycles repay whoever cashes in early, so the
// sparse-map opening bank applies there too even when a big monolith group exists (collision 108,
// eight 132, swirl 192; density 0.32-1.28). Flag rides byte 6 of the draft memory from map facts.
#ifndef NOVA_RACE_BANK
#define NOVA_RACE_BANK 0 // default off until the version-23 ladder data proves sparse banking safe
#endif
#ifndef NOVA_RACE_DENS
#define NOVA_RACE_DENS 0.25f // collision .32, lanes .32, eight .44, packs .43, swirl 1.28 / expanse .16, corners .18
#endif
#ifndef NOVA_MONO_BANK
#define NOVA_MONO_BANK 0 // bank small bags on shared-monolith maps (monolith/contest) during the opening
#endif
inline constexpr bool raceBank = NOVA_RACE_BANK > 0;
inline constexpr bool monoBank = NOVA_MONO_BANK > 0;
inline constexpr float raceDens = NOVA_RACE_DENS;

// Set to 1 for verbose per-turn logs (costs fuel; stdout is capped near 4KB a turn).
#ifndef APEX_DEATHLOG
#define APEX_DEATHLOG 0
#endif
#ifndef APEX_LOG
#define APEX_LOG 0
#endif
#define LOG(...)                                                                                                       \
    do {                                                                                                               \
        if (APEX_LOG) std::printf(__VA_ARGS__);                                                                        \
    } while (0)

// Fuel-consumption switch (NOVA_FUEL_OPEN): on calm turns (no enemy in view, which is nearly every
// opening turn at 100k budget) build more distance fields per turn instead of the fixed quota,
// finishing the whole map prospecting table several rounds earlier.
#ifndef NOVA_FUEL_OPEN
#define NOVA_FUEL_OPEN 0
#endif
inline constexpr int fuelOpen = NOVA_FUEL_OPEN;

// NOVA_FUEL_FIRST (0 = off): the first turn (200k budget, we used ~100k) and dead turns build distance fields until
// the fuel floor instead of a fixed quota, so the whole map's fields are ready from the opening.
#ifndef NOVA_FUEL_FIRST
#define NOVA_FUEL_FIRST 0
#endif

// Fuel profiler (APEX_PROFILE=1): charges the fuel spent inside each scope to a named section.
#ifndef APEX_TRACE
#define APEX_TRACE 0
#endif
#ifndef APEX_PROFILE
#define APEX_PROFILE 0
#endif
// Fuel probe (APEX_FUEL_PROBE=1, with APEX_PROFILE): print every play turn's fuel spend and
// remainder for the first 120 rounds, to find how much budget actually goes unused.
#ifndef APEX_FUEL_PROBE
#define APEX_FUEL_PROBE 0
#endif
namespace prof {
enum Section { turn, init, refresh, world, groups, champs, dist, enemies, farm, score, move, field, shop, s_order, s_need, s_claims, s_open, route, escape, tsearch, tdecide, tfc, plan, w_vis, w_hot, w_tail, nsections };
inline constexpr const char* kNames[nsections] = {"turn", "init", "refresh", "world", "groups", "champs", "dist", "enemies",
    "farm", "score", "move", "field", "shop", "s_order", "s_need", "s_claims", "s_open", "route", "escape", "tsearch", "tdecide", "tfc", "plan", "w_vis", "w_hot", "w_tail"};
inline std::uint64_t total[nsections];
inline std::uint32_t calls[nsections];
inline std::uint64_t worst_turn = 0;
inline std::uint32_t worst_round = 0, turns_seen = 0, suspended = 0;
// Fuel-starvation counters: farm() entered under kLowFuel, under kRouteFuel, and low-fuel farms that gave up.
inline std::uint32_t farm_low = 0, farm_route_skip = 0, farm_gaveup = 0, farm_calls = 0, td_then_farm = 0;
inline int td_round = -1; // round of the last tactical_decide call
inline std::uint64_t cur[nsections]; // this turn's spend per section
inline std::uint32_t check_round0 = 0;
inline bool check_done = false;
// Checkpoint: the first one reached in a later round than the turn started tells where the turn ran out of fuel.
inline void check(const char* where) {
    if (check_done || bc::round_number() == check_round0) return;
    check_done = true;
    std::printf("SUSPAT R%u before %s\n", check_round0, where);
}
inline void dump_turn(const char* why, std::uint32_t round, std::uint64_t used, std::uint64_t start) {
    std::printf("HEAVY %s R%u start %llu used %llu:", why, round, static_cast<unsigned long long>(start),
        static_cast<unsigned long long>(used));
    for (int i = 1; i < nsections; ++i)
        if (cur[i]) std::printf(" %s=%llu", kNames[i], static_cast<unsigned long long>(cur[i]));
    std::printf("\n");
}
struct Scope {
    int id;
    std::uint64_t start;
    std::uint32_t round0 = 0;
    explicit Scope(int i) : id(i), start(bc::fuel_remaining()) {
        if (id == turn) {
            round0 = bc::round_number();
            for (auto& c : cur) c = 0;
            check_round0 = round0;
            check_done = false;
        }
    }
    ~Scope() {
        const std::uint64_t left = bc::fuel_remaining();
        const std::uint64_t used = start > left ? start - left : 0;
        total[id] += used;
        cur[id] += used;
        ++calls[id];
        if (id != turn) return;
        // A turn that ends in a later round ran out of fuel and was suspended.
        if (bc::round_number() != round0) {
            ++suspended;
            dump_turn("suspended", round0, used, start);
        } else if (turns_seen > 0 && used > 90000)
            dump_turn("over90k", round0, used, start);
        // The first play turn has a doubled budget, so leave it out of the worst case.
        else if (++turns_seen > 1 && used > worst_turn) worst_turn = used, worst_round = round0;
#if APEX_FUEL_PROBE
        if (round0 < 120)
            std::printf("FUELR R%u used %llu left %llu\n", round0, static_cast<unsigned long long>(used),
                static_cast<unsigned long long>(left));
#endif
    }
};
inline void report(int round) {
    std::printf("PROF R%d worst-turn %llu (R%u) suspended %u\n", round, static_cast<unsigned long long>(worst_turn),
        worst_round, suspended);
    std::printf("PROF farmfuel calls %u low %u route_skip %u gaveup %u td_then_farm %u\n", farm_calls, farm_low, farm_route_skip,
        farm_gaveup, td_then_farm);
    for (int i = 0; i < nsections; ++i)
        if (calls[i])
            std::printf("PROF %-8s calls %6u avg %7llu\n", kNames[i], calls[i],
                static_cast<unsigned long long>(total[i] / calls[i]));
}
} // namespace prof
#define PROF_CAT2(a, b) a##b
#define PROF_CAT(a, b) PROF_CAT2(a, b)
#if APEX_PROFILE
#define PROF(section) prof::Scope PROF_CAT(prof_scope_, __LINE__)(prof::section)
#define PROF_CHECK(where) prof::check(where)
#else
#define PROF(section) ((void)0)
#define PROF_CHECK(where) ((void)0)
#endif

// Direct reads of engine-chunked buffers (8x8 tiles per chunk; layout checked by bots/probe8).
// A host call such as is_set()/get() costs ~59 fuel; these cost a handful. No bounds checks:
// callers pass on-map tiles.
// Index of tile p inside an engine-chunked distance buffer (same layout as fdist).
inline std::size_t field_index(vec2 p) {
    const auto& L = bc::detail::layout();
    return ((static_cast<std::uint32_t>(p.y) >> 3) * L.chunked_width + (static_cast<std::uint32_t>(p.x) >> 3)) * 64 +
        static_cast<std::size_t>(((p.y & 7) << 3) + (p.x & 7));
}
inline std::uint16_t fdist(const bc::distance_field& f, vec2 p) {
    const auto& L = bc::detail::layout();
    return f.ptr()[((static_cast<std::uint32_t>(p.y) >> 3) * L.chunked_width + (static_cast<std::uint32_t>(p.x) >> 3)) * 64 +
                   ((p.y & 7) << 3) + (p.x & 7)];
}
inline void set_bit(bc::bitboard& b, vec2 p) {
    const auto& L = bc::detail::layout();
    b.ptr()[(static_cast<std::uint32_t>(p.y) >> 3) * L.chunk_stride + (static_cast<std::uint32_t>(p.x) >> 3)] |=
        std::uint64_t{1} << (((p.y & 7) << 3) | (p.x & 7));
}
inline void clear_bit(bc::bitboard& b, vec2 p) {
    const auto& L = bc::detail::layout();
    b.ptr()[(static_cast<std::uint32_t>(p.y) >> 3) * L.chunk_stride + (static_cast<std::uint32_t>(p.x) >> 3)] &=
        ~(std::uint64_t{1} << (((p.y & 7) << 3) | (p.x & 7)));
}
// A distance-field search costs about (levels + 16) x (number of 8x8 chunks) fuel (measured).
inline std::uint64_t field_chunks() {
    const auto& L = bc::detail::layout();
    return static_cast<std::uint64_t>(L.chunked_width) * L.chunked_height;
}
inline bool bit(const bc::bitboard& b, vec2 p) {
    const auto& L = bc::detail::layout();
    return (b.ptr()[(static_cast<std::uint32_t>(p.y) >> 3) * L.chunk_stride + (static_cast<std::uint32_t>(p.x) >> 3)] >>
               (((p.y & 7) << 3) | (p.x & 7))) & 1u;
}

inline int cheb(vec2 a, vec2 b) { return static_cast<int>(a.chebyshev_distance(b)); }

inline constexpr std::array<vec2, 8> kDirs{
    vec2{0, 1}, vec2{1, 1}, vec2{1, 0}, vec2{1, -1}, vec2{0, -1}, vec2{-1, -1}, vec2{-1, 0}, vec2{-1, 1}};

inline constexpr std::uint16_t kUnreach = bc::distance_field::unreachable;

// Tunable strategy constants. Kept together so tests can sweep them.
namespace tune {
#ifndef APEX_TRIBUTE_CARRY
#define APEX_TRIBUTE_CARRY 64
#endif
inline int kTributeCarry = APEX_TRIBUTE_CARRY; // walk to an altar once carrying this many gems
#ifndef NOVA_EDGE_BAG
#define NOVA_EDGE_BAG 22
#endif
#ifndef NOVA_EDGE_UNTIL
#define NOVA_EDGE_UNTIL 250
#endif
#ifndef NOVA_BIG_MAP_GEMS
#define NOVA_BIG_MAP_GEMS 100
#endif
inline constexpr int kEdgeBankBag = NOVA_EDGE_BAG;   // sparse-map opening bank threshold
inline constexpr int kEdgeBankUntil = NOVA_EDGE_UNTIL; // sparse-map opening bank horizon
inline constexpr int kBigMapGems = NOVA_BIG_MAP_GEMS;  // groups at least this big mean strip-carry maps
inline constexpr int kEdgeBankDist = NOVA_EDGE_DIST;   // longest sparse-map opening bank trip worth taking
#ifndef APEX_NEAR_DIST         // tribute any bag when an altar is this close (v11: 6)
#define APEX_NEAR_DIST 3
#endif
inline constexpr int kTributeNearDist = APEX_NEAR_DIST;
#ifndef APEX_STALE_BAG         // a carrier of at least this many gems that flees near an altar marks it watched (0 = off)
#define APEX_STALE_BAG 0
#endif
inline constexpr int kStaleBag = APEX_STALE_BAG;
#ifndef APEX_STALE_WATCH       // rounds an altar stays watched; banking trips meanwhile go to another altar
#define APEX_STALE_WATCH 40
#endif
inline constexpr int kStaleWatch = APEX_STALE_WATCH;
#ifndef APEX_END_BANK          // once either score reaches this, any bag of 10+ banks at an altar within 20 steps (0 = off)
#define APEX_END_BANK 0
#endif
inline constexpr int kEndBank = APEX_END_BANK;
#ifndef APEX_TRIB_DENY         // tribute denial (from apex_v18b): 1 = a tributer is no threat, 2 = deny_tribute times the cancel
#define APEX_TRIB_DENY 0
#endif
inline constexpr int kTribDeny = APEX_TRIB_DENY;
#ifndef APEX_RETREAT_HP
#define APEX_RETREAT_HP 0.1357f
#endif
// Go home below this health fraction (v11: 0.22, was 0.30; with SHOP_GOLD 150: +54 fresh, +101 new maps).
inline float kRetreatHp = APEX_RETREAT_HP;
#ifndef APEX_SHOP_GOLD
#define APEX_SHOP_GOLD 118
#endif
// Go home to shop with this much gold (v11: 150, was 210: +35 tuning, +25 fresh alone; 120 and 180 similar).
inline int kShopGold = APEX_SHOP_GOLD;
#ifndef APEX_BUILD
#define APEX_BUILD 0
#endif
inline constexpr int kBuild = APEX_BUILD;        // item build preset (see build_for)
#ifndef APEX_WALK_DIST
#define APEX_WALK_DIST 6
#endif
inline int kShopWalkDist = APEX_WALK_DIST; // walk instead of recalling within this distance of spawn (v10: 5, was 8; a recall takes 5 turns)
#ifndef APEX_GOLD_WEIGHT
#define APEX_GOLD_WEIGHT 0.10f
#endif
inline float kGoldWeight = APEX_GOLD_WEIGHT; // value of one gold relative to one gem
inline constexpr int kCaptureTime = 5;
#ifndef APEX_STAGE_TIMEOUT
#define APEX_STAGE_TIMEOUT 9
#endif
inline int kStageTimeout = APEX_STAGE_TIMEOUT; // turns to wait for helpers before giving up
#ifndef APEX_WAIT_WEIGHT
#define APEX_WAIT_WEIGHT 2.1847f
#endif
inline float kWaitWeight = APEX_WAIT_WEIGHT; // cost of a turn spent waiting for a respawn
#ifndef APEX_NEED_POOL
#define APEX_NEED_POOL 0.9912f
#endif
inline float kNeedPool = APEX_NEED_POOL; // share of our health a solo camp fight may use
#ifndef APEX_TERRITORY
#define APEX_TERRITORY 1.0f
#endif
#ifndef APEX_TERRITORY_MARGIN
#define APEX_TERRITORY_MARGIN 4
#endif
// Value multiplier for camp groups in enemy territory (nearer their spawn than ours by more than
// the margin, in steps); 1 = off.
inline float kTerritory = APEX_TERRITORY; // runtime: per-map override (strategy.h)
#ifndef APEX_OUTNUMBERED
#define APEX_OUTNUMBERED 0
#endif
// Retreat when enemies within 8 outnumber us (and healthy allies within 5) by at least this; 0 = off.
inline constexpr int kOutnumbered = APEX_OUTNUMBERED;
#ifndef APEX_ENDGAME_MARGIN
#define APEX_ENDGAME_MARGIN 0
#endif
// Bank our bag once score + bag + this margin reaches 1,000 (an altar within 25 steps); 0 = off.
inline constexpr int kEndgameMargin = APEX_ENDGAME_MARGIN;
inline constexpr int kTerritoryMargin = APEX_TERRITORY_MARGIN;
#ifndef APEX_STICKY
#define APEX_STICKY 1.4372f
#endif
inline constexpr float kStickiness = APEX_STICKY; // score bonus for the current target
#ifndef APEX_INITIATE
#define APEX_INITIATE 1
#endif
inline constexpr bool kInitiateJobs = APEX_INITIATE; // open two-champion jobs as calls for help
#ifndef APEX_USE_WAITS
#define APEX_USE_WAITS 1
#endif
inline constexpr bool kUseWaits = APEX_USE_WAITS;    // tribute/shop while a target respawns
#ifndef APEX_LOCKS
#define APEX_LOCKS 1
#endif
inline constexpr bool kAltarLocks = APEX_LOCKS;      // carry an altar lock to cancel enemy tributes
#ifndef APEX_AGGRESSION
#define APEX_AGGRESSION 1.2f
#endif
inline float kAggression = APEX_AGGRESSION; // >1 takes riskier champion fights
#ifndef APEX_CHASE
#define APEX_CHASE 5
#endif
inline int kChaseRange = APEX_CHASE;        // extra tiles we walk to reach a fight
#ifndef APEX_HUNT
#define APEX_HUNT 1
#endif
inline bool kHunt = APEX_HUNT;              // walk toward fresh enemy sightings
#ifndef APEX_HUNT_RANGE
#define APEX_HUNT_RANGE 7
#endif
inline int kHuntRange = APEX_HUNT_RANGE;
#ifndef APEX_HUNT_HP
#define APEX_HUNT_HP 0.7832f
#endif
inline float kHuntHp = APEX_HUNT_HP;       // only hunt above this health fraction
#ifndef APEX_HUNT_GEMS
#define APEX_HUNT_GEMS 13
#endif
inline int kHuntGems = APEX_HUNT_GEMS;     // only hunt while carrying fewer gems
#ifndef APEX_HUNT_FIX
#define APEX_HUNT_FIX 1
#endif
// No hunting right after fleeing, nor toward enemies on (or by) their spawn, where they heal.
inline constexpr bool kHuntFix = APEX_HUNT_FIX;
#ifndef APEX_SAFE_ALTARS
#define APEX_SAFE_ALTARS 1
#endif
inline constexpr bool kSafeAltars = APEX_SAFE_ALTARS; // never tribute within 5 of the enemy spawn (if avoidable)
#ifndef APEX_UNSTICK
#define APEX_UNSTICK 1
#endif
inline constexpr bool kUnstick = APEX_UNSTICK; // replan around units when a static field leads into them
#ifndef APEX_EVENTS            // share group respawn timers through the sighting bits (bit 63 flags an event; v11: 2)
#define APEX_EVENTS 0
#endif
#ifndef APEX_BOOT_ITEMS        // gold bootstrapping: champions with fewer estimated shards than this ... (v11: 10)
#define APEX_BOOT_ITEMS 0
#endif
#ifndef APEX_BOOT_BONUS        // ... value harmless (gem/gold cluster) groups this much more
#define APEX_BOOT_BONUS 0.5f
#endif
inline float kBootBonus = APEX_BOOT_BONUS; // runtime: per-map override (strategy.h)
#ifndef APEX_KITE              // casters keep enemy melee at least this many tiles away before shooting (0 = off)
#define APEX_KITE 0
#endif
inline constexpr int kKite = APEX_KITE;
#ifndef APEX_CI_RISK           // risk multiplier for Cassi & Issac fights (they die together)
#define APEX_CI_RISK 1.0f
#endif
inline constexpr float kCiRisk = APEX_CI_RISK;
#ifndef APEX_CASTER_SELF_TTD   // our casters flee this much earlier (danger threshold multiplier)
#define APEX_CASTER_SELF_TTD 1.0f
#endif
inline constexpr float kCasterSelfTtd = APEX_CASTER_SELF_TTD;
#ifndef APEX_SIEGE
#define APEX_SIEGE 1
#endif
inline bool kSiege = APEX_SIEGE;           // casters wear down dangerous camps from range 4
#ifndef APEX_WARDS
#define APEX_WARDS 0
#endif
inline constexpr bool kWards = APEX_WARDS;           // leave wards at captures and altars
#ifndef APEX_FIGHT_SIM
#define APEX_FIGHT_SIM 0
#endif
inline constexpr bool kFightSim = APEX_FIGHT_SIM;    // decide fights by simulating engage vs retreat
#ifndef APEX_SIM_TURNS
#define APEX_SIM_TURNS 6
#endif
inline constexpr int kSimTurns = APEX_SIM_TURNS;
#ifndef APEX_DANGER_TTD
#define APEX_DANGER_TTD 6.6632f
#endif
#ifndef APEX_DANGER_HP
#define APEX_DANGER_HP 0.22f
#endif
inline float kDangerTtd = APEX_DANGER_TTD; // flee when we would die within this many turns (v10: 7.0, was 5.5: +34 Elo)
inline float kDangerHp = APEX_DANGER_HP;   // ...or when below this share of health
#ifndef APEX_DISTRESS
#define APEX_DISTRESS 0
#endif
#ifndef APEX_DISTRESS_RANGE
#define APEX_DISTRESS_RANGE 14
#endif
inline constexpr bool kDistress = APEX_DISTRESS;          // fleeing champions call for help
inline constexpr int kDistressRange = APEX_DISTRESS_RANGE; // allies this close answer
#ifndef APEX_BUSH_HIDE
#define APEX_BUSH_HIDE 300
#endif
inline constexpr int kBushHide = APEX_BUSH_HIDE; // fleeing prefers tiles hidden from the threats
#ifndef APEX_HIDE_LOS
#define APEX_HIDE_LOS 0
#endif
inline constexpr bool kHideLos = APEX_HIDE_LOS; // hidden = out of enemy line of sight, not only bushes
#ifndef APEX_FLEE_ALLY
#define APEX_FLEE_ALLY 0
#endif
inline constexpr int kFleeAlly = APEX_FLEE_ALLY; // flee score lost per tile away from a healthy ally
#ifndef APEX_BUSH_AMBUSH
#define APEX_BUSH_AMBUSH 2
#endif
// Fight from bushes: a champion inside a bush cluster cannot be seen, so cannot be attacked or
// targeted by spells, by anyone outside that cluster, and attacking from it does not reveal it
// (checked with bots/probe_bush). 1 = attack from a hidden bush tile whenever one reaches a target.
inline constexpr int kBushAmbush = APEX_BUSH_AMBUSH;
#ifndef APEX_BUSH_DEF
#define APEX_BUSH_DEF 1
#endif
// Counter-ambush: health lost that no visible enemy or awake camp explains means an enemy hides in a
// bush that reaches us. 1 = healthy champions step into that bush (which reveals it) and fight; hurt
// ones step out of its reach.
inline constexpr int kBushDef = APEX_BUSH_DEF;
#ifndef APEX_WARD_AWARE
#define APEX_WARD_AWARE 1
#endif
// A bush cluster with a visible enemy ward inside is no cover: their ward shows them everyone in it.
inline constexpr int kWardAware = APEX_WARD_AWARE;
#ifndef APEX_WARD_REVEAL
#define APEX_WARD_REVEAL 2
#endif
// A ward placed inside a bush cluster shows us everyone in it (bots/probe_bush). 1 = ward the bush an
// invisible attacker must be in, or that a just-seen enemy vanished into (within ward range 2).
inline constexpr int kWardReveal = APEX_WARD_REVEAL;
#ifndef APEX_BUSH_RECALL
#define APEX_BUSH_RECALL 1
#endif
// Recall from cover: in a bush no visible enemy shares, nobody can target us, so the 5-turn recall
// is not cancelled by enemy hits. 1 = recall there even with enemies close.
inline constexpr int kBushRecall = APEX_BUSH_RECALL;
#ifndef APEX_PRIORITY
#define APEX_PRIORITY 5
#endif
// Turn order: within a team, higher priority acts first (from the next round on). 1 = champions in
// contact with enemies take the earliest slots, nearest contact and lowest health first.
inline constexpr int kPriority = APEX_PRIORITY;
#ifndef APEX_PRELOCK
#define APEX_PRELOCK 0
#endif
#ifndef APEX_PRELOCK_SCORE
#define APEX_PRELOCK_SCORE 0
#endif
inline constexpr bool kPrelock = APEX_PRELOCK;           // lock every altar we pass
inline constexpr int kPrelockScore = APEX_PRELOCK_SCORE; // ...once the enemy has tributed this much
#ifndef APEX_SHIELD_PVP
#define APEX_SHIELD_PVP 0
#endif
inline constexpr bool kShieldPvp = APEX_SHIELD_PVP; // keep self-shields for champion fights
#ifndef APEX_FLEE_HOME
#define APEX_FLEE_HOME 150
#endif
// Extra flee score per step closer to spawn (heals fully; enemies cannot enter). Measured vs v7.1:
// 30: 0, 80: +18, 150: +33, 300: +38 (+39 on fresh maps), 1000: +18. But 300 (= v7.2) then scored
// 207-211 vs v7.1 on 418 games on brand-new maps, so it stays off until a bigger test confirms it.
inline constexpr int kFleeHome = APEX_FLEE_HOME;
#ifndef APEX_ROUTE_DEPTH
#define APEX_ROUTE_DEPTH 1
#endif
#ifndef APEX_ROUTE_K
#define APEX_ROUTE_K 6
#endif
#ifndef APEX_ROUTE_M
#define APEX_ROUTE_M 3
#endif
// Target choice looks this many camp groups ahead (1 = greedy): the K best first targets are
// extended by their best follow-up groups (M kept per level at depth 3).
inline constexpr int kRouteDepth = APEX_ROUTE_DEPTH;
inline constexpr int kRouteK = APEX_ROUTE_K;
inline constexpr int kRouteM = APEX_ROUTE_M;
inline constexpr std::uint64_t kRouteFuel = 40000; // skip the route search below this much fuel
#ifndef APEX_ROUTE_SLACK
#define APEX_ROUTE_SLACK 0.6f
#endif
inline constexpr float kRouteSlack = APEX_ROUTE_SLACK; // first targets kept: score >= slack x best
#ifndef APEX_BURST
#define APEX_BURST 0
#endif
inline constexpr bool kBurst = APEX_BURST; // take any kill available this turn, even when fleeing
#ifndef APEX_POSITION
#define APEX_POSITION 0
#endif
inline constexpr bool kPosition = APEX_POSITION; // melee picks the least exposed tile to attack from
#ifndef APEX_STAND
#define APEX_STAND 1
#endif
// Caught by an enemy we cannot outrun (no dash, no stun, spawn too far): fight back instead of
// fleeing, which only hands the chaser free hits (87% of our deaths happened while fleeing).
// Bits: 1 = dash out of reach, 2 = stun a lone chaser and step away, 4 = stand and fight.
inline constexpr int kStand = APEX_STAND;
#ifndef APEX_CASTER_FOCUS
#define APEX_CASTER_FOCUS 1
#endif
inline constexpr bool kCasterFocus = APEX_CASTER_FOCUS; // casters buy focus shards first
inline constexpr std::uint64_t kLowFuel = 60000;     // below this, keep the current target instead of rescoring
inline constexpr std::uint64_t kScoreFloor = 25000;   // stop scoring more groups below this
inline constexpr std::uint64_t kMinMoveFuel = 15000;  // below this, skip path searches
inline constexpr std::uint64_t kFieldBuildReserve = 10000; // fuel kept back after precomputing distance fields
#ifndef APEX_ESCORT            // casters follow the camp claim of the nearest healthy melee ally within this many tiles (0 = off)
#define APEX_ESCORT 0
#endif
inline constexpr int kEscort = APEX_ESCORT;
#ifndef APEX_SQUAD_SHARE       // squad followers stay on the region through the capture so the reward is split
#define APEX_SQUAD_SHARE 0
#endif
inline constexpr bool kSquadShare = APEX_SQUAD_SHARE;
#ifndef APEX_ROLE_SQUAD        // 1: casters + Issac farm as one pack (lowest slot leads), melee solo; 2: Ren joins the pack
#define APEX_ROLE_SQUAD 0
#endif
inline constexpr int kRoleSquad = APEX_ROLE_SQUAD;
#ifndef APEX_SQUADS
#define APEX_SQUADS 0
#endif
inline constexpr int kSquads = APEX_SQUADS; // 0: everyone solo; n: n squads following a leader
#ifndef APEX_CAP_STAFF
#define APEX_CAP_STAFF 0
#endif
inline constexpr int kCapStaff = APEX_CAP_STAFF; // skip groups lower slots already staff to need + n - 1 (0 = off)
#ifndef APEX_PAIR
#define APEX_PAIR 0
#endif
inline constexpr int kPair = APEX_PAIR; // the last n claim slots farm as one squad (0 = off)
#ifndef APEX_SOLO_INITIATE
#define APEX_SOLO_INITIATE 1
#endif
inline constexpr bool kSoloInitiate = APEX_SOLO_INITIATE; // solo champions may open help calls in pair mode
#ifndef APEX_CI_FOCUS
#define APEX_CI_FOCUS 1
#endif
inline constexpr bool kCiFocus = APEX_CI_FOCUS; // Cassi/Issac are worth two kills (linked death)
#ifndef APEX_EMBLEM_MODEL
#define APEX_EMBLEM_MODEL 1
#endif
inline constexpr bool kEmblemModel = APEX_EMBLEM_MODEL; // jungle/durability/monster passives in the soak model
#ifndef APEX_CALL_COOLDOWN
#define APEX_CALL_COOLDOWN 0
#endif
inline constexpr int kCallCooldown = APEX_CALL_COOLDOWN; // rounds without new help calls after one times out
#ifndef APEX_WIN_FIRST
#define APEX_WIN_FIRST 1
#endif
inline constexpr bool kWinFirst = APEX_WIN_FIRST; // a bag that finishes the race is banked before shopping
#ifndef APEX_HP_COST
#define APEX_HP_COST 0.0f
#endif
inline constexpr float kHpCost = APEX_HP_COST; // target cost in turns per full health bar a camp drains (0 = off)
#ifndef APEX_GOLD_TAPER
#define APEX_GOLD_TAPER 0
#endif
inline constexpr int kGoldTaper = APEX_GOLD_TAPER; // our score at which gold is worth a fifth as much (0 = off)
#ifndef APEX_INCUMBENT
#define APEX_INCUMBENT 0
#endif
// Small jobs belong to the champion that claimed them first (tracked from the shared claims), not to
// the lowest slot: a later claimant no longer evicts one that already walked there and did the work.
inline constexpr bool kIncumbent = APEX_INCUMBENT;
#ifndef APEX_LOWHP_AGGR
#define APEX_LOWHP_AGGR 0.0f
#endif
// Below this share of health, the aggression margin shrinks linearly toward 1 (an even race is a
// coin flip that costs us 60 rounds and the bag when lost); 0 = off.
inline constexpr float kLowHpAggr = APEX_LOWHP_AGGR;
#ifndef APEX_SHARE_GOLD
#define APEX_SHARE_GOLD 1
#endif
// Rewards go to whoever stands on the region when the capture completes. With this switch the
// champion with fewer items (estimated from stats) captures and richer ones hand over and move on,
// so a third champion no longer stays at base stats; the value is the margin in shards (0 = off).
inline constexpr int kShareGold = APEX_SHARE_GOLD;
#ifndef APEX_KITE_CAST
#define APEX_KITE_CAST 0
#endif
// Casters under APEX_KITE stepped away every turn a melee enemy was near, and spells that exclude walking
// cannot follow a step: in 18 games vs v13b Stellar and Tavi cast 0 spells on champions (115 / 51 on camps).
// Bits: 1 = cast a ready damage/stun spell that reaches the target before kiting (kite on cooldown turns);
// 2 = engage from spell range (4) instead of walking to attack range first; 4 = while fleeing, stun a lone
// melee chaser even with a spell that excludes walking (a stunned chaser loses its next turn).
inline constexpr int kKiteCast = APEX_KITE_CAST;
#ifndef APEX_SPELL_HOLD
#define APEX_SPELL_HOLD 0
#endif
#ifndef APEX_SPELL_HOLD_RANGE
#define APEX_SPELL_HOLD_RANGE 12
#endif
#ifndef APEX_SPELL_HOLD_AGE
#define APEX_SPELL_HOLD_AGE 10
#endif
// Spells on camps leave them on an 8-turn cooldown when an enemy arrives. On the ladder (15 Oct, 110 games vs
// the top 10) the same champions dealt half the top teams' champion damage (Cassi 83 vs 325, Ren 352 vs 725
// per 100 rounds). Bits: 1 = no spell on camps while an enemy is visible or was sighted within RANGE tiles in
// the last AGE rounds; 2 = never spend a stun (cc spells, Ryth's area stun) on camps.
inline constexpr int kSpellHold = APEX_SPELL_HOLD;
inline constexpr int kSpellHoldRange = APEX_SPELL_HOLD_RANGE;
inline constexpr int kSpellHoldAge = APEX_SPELL_HOLD_AGE;
#ifndef APEX_SAVE_DASH
#define APEX_SAVE_DASH 0
#endif
// Dashes cut travel by one tile per 8 turns but sit on cooldown when an escape needs them. 1: no
// travel dashes within 15 tiles of an enemy seen in the last 15 rounds; 2: never for travel (fights,
// hunts and escapes still dash); 0 = off.
inline constexpr int kSaveDash = APEX_SAVE_DASH;
#ifndef APEX_STAND2
#define APEX_STAND2 0
#endif
// A melee enemy next to us follows and hits every turn, so stepping away only hands it free hits
// (56% of flee deaths had an enemy adjacent on each of the last four turns). Bits: 1 = when our
// spawn is reachable before that damage kills us, retreat along the shortest way home, hitting the
// chaser as we go; 2 = otherwise stand and fight with every spell and reset. A ready dash still
// escapes first. 0 = off.
inline constexpr int kStand2 = APEX_STAND2;
#ifndef APEX_AWAKE_ZONE
#define APEX_AWAKE_ZONE 1
#endif
// The no-recall zone covered only damaged camps, so a full-health member of an awake group could keep
// cancelling our recall until it killed us (seen on rows: 16 turns, then dead). With this switch every
// live member of an awake group counts, and a champion that lost health since its last turn with no
// enemy in sight steps out of every camp's reach before recalling.
inline constexpr bool kAwakeZone = APEX_AWAKE_ZONE;
#ifndef APEX_FLEE_MULTI
#define APEX_FLEE_MULTI 1
#endif
// Flee from every visible enemy, not only the nearest: the step score sums each enemy's distance
// (capped at 6) weighted by its damage, so fleeing one chaser does not run into another. 0 = off.
inline constexpr bool kFleeMulti = APEX_FLEE_MULTI;
#ifndef APEX_THREAT_RETREAT
#define APEX_THREAT_RETREAT 0.4f
#endif
// Go home below this health share (instead of APEX_RETREAT_HP) while an enemy is in sight or was seen
// within 10 tiles in the last 6 rounds: a champion caught below half health rarely escapes (fleeing
// champions at <=25% health died within 10 rounds 58% of the time). 0 = off.
inline constexpr float kThreatRetreat = APEX_THREAT_RETREAT;
#ifndef APEX_SIDE_HP
#define APEX_SIDE_HP 0.0f
#endif
// Skip fights (unless the kill is quick: ttk under half our time to die) when our side's health
// within 6 tiles is below this share of the enemies' there. Engagements with our side behind lost
// more champions than they killed (34% vs 28% over the next 10 rounds). 0 = off.
inline constexpr float kSideHp = APEX_SIDE_HP;
#ifndef APEX_NODASH_AGGR
#define APEX_NODASH_AGGR 0.0f
#endif
// Aggression for champions without a dash (they cannot break contact once caught: 57-70% of their
// deaths had an enemy adjacent for the last four turns, against 29-36% for dashers); 0 = off.
inline constexpr float kNoDashAggr = APEX_NODASH_AGGR;
#ifndef APEX_RECALL_TRAVEL
#define APEX_RECALL_TRAVEL 0
#endif
// Recall (5 turns, heals, shops) instead of walking when our target is at least this many steps
// nearer to our spawn than to us, after paying for the recall; 0 = off.
inline constexpr int kRecallTravel = APEX_RECALL_TRAVEL;
#ifndef APEX_ESCAPE_SEARCH
#define APEX_ESCAPE_SEARCH 0
#endif
// Flee by planning the next APEX_ESC_DEPTH turns (steps, plus a dash on the first) against every
// nearby enemy's walking distance, instead of one greedy step away from the nearest; 0 = off.
inline constexpr int kEscapeSearch = APEX_ESCAPE_SEARCH;
#ifndef APEX_ESC_DEPTH
#define APEX_ESC_DEPTH 3
#endif
inline constexpr int kEscDepth = APEX_ESC_DEPTH; // turns planned (1-4)
#ifndef APEX_ESC_HOME
#define APEX_ESC_HOME 2
#endif
inline constexpr int kEscHome = APEX_ESC_HOME; // plan cost (health points) per step from spawn at its end
#ifndef APEX_ESC_DASH_COST
#define APEX_ESC_DASH_COST 0.08f
#endif
// A plan that dashes costs this share of max health extra: the dash then sits on cooldown.
inline constexpr float kEscDashCost = APEX_ESC_DASH_COST;
#ifndef APEX_THREAT_PATH
#define APEX_THREAT_PATH 0
#endif
#ifndef APEX_THREAT_AGE
#define APEX_THREAT_AGE 8
#endif
#ifndef APEX_THREAT_DETOUR
#define APEX_THREAT_DETOUR 6
#endif
#ifndef APEX_THREAT_HP
#define APEX_THREAT_HP 1.0f
#endif
// Travel (not fights, hunts or flight) routes around tiles within APEX_THREAT_PATH (+1 per 2 rounds of
// age) of an enemy seen in the last APEX_THREAT_AGE rounds, if that costs at most APEX_THREAT_DETOUR extra
// steps, while below APEX_THREAT_HP of our health. Champions caught walking die: 72% of deaths started
// below half health four turns earlier, 64% with no ally near. 0 = off.
inline constexpr int kThreatPath = APEX_THREAT_PATH;
inline constexpr int kThreatAge = APEX_THREAT_AGE;
inline constexpr int kThreatDetour = APEX_THREAT_DETOUR;
inline constexpr float kThreatHp = APEX_THREAT_HP;

// Tactical search (tsearch.h, APEX_TSEARCH): with an enemy within APEX_TS_RANGE, our champion plays each
// candidate action forward APEX_TS_ROUNDS rounds against two enemy models (they flee at APEX_TS_FLEE_A and
// at APEX_TS_FLEE_C of their health) and takes the best average outcome. The evaluation counts gems: a life
// is APEX_TS_ALIVE, a full health bar APEX_TS_HP, carried gems at face value (the killer takes them), and a
// unit that dies next turn at the horizon loses APEX_TS_THREAT of its life. Search stops at APEX_TS_RESERVE
// fuel left.
#ifndef APEX_TSEARCH
#define APEX_TSEARCH 0
#endif
#ifndef APEX_TS_RANGE
#define APEX_TS_RANGE 6
#endif
#ifndef APEX_TS_ROUNDS
#define APEX_TS_ROUNDS 2
#endif
#ifndef APEX_TS_RESERVE
#define APEX_TS_RESERVE 22000
#endif
// Budgeted fight search (APEX_TS_BUDGET): predict a playout's cost as APEX_TS_UNIT_COST x units^2 (scaled by the
// horizon), shorten the horizon or skip so a comparison always finishes, and keep APEX_TS_BUDGET_RESERVE back.
#ifndef APEX_TS_BUDGET
#define APEX_TS_BUDGET 0
#endif
#ifndef APEX_TS_BUDGET_RESERVE
#define APEX_TS_BUDGET_RESERVE 30000
#endif
#ifndef APEX_TS_UNIT_COST
#define APEX_TS_UNIT_COST 1000
#endif
#ifndef APEX_TS_ALIVE
#define APEX_TS_ALIVE 25.0f
#endif
#ifndef APEX_TS_HP
#define APEX_TS_HP 12.0f
#endif
#ifndef APEX_TS_THREAT
#define APEX_TS_THREAT 0.5f
#endif
#ifndef APEX_TS_FLEE_A
#define APEX_TS_FLEE_A 0.12f
#endif
#ifndef APEX_TS_FLEE_C
#define APEX_TS_FLEE_C 0.35f
#endif
#ifndef APEX_TS_ALLY_FLEE
#define APEX_TS_ALLY_FLEE 0.2f
#endif
#ifndef APEX_COHESION
#define APEX_COHESION 0.05f
#endif
#ifndef APEX_COHESION_DIST
#define APEX_COHESION_DIST 8
#endif
#ifndef APEX_PHASE_AGGR
#define APEX_PHASE_AGGR 0
#endif
#ifndef APEX_PHASE_EARLY
#define APEX_PHASE_EARLY 0.8f
#endif
#ifndef APEX_PHASE_LATE
#define APEX_PHASE_LATE 1.25f
#endif
#ifndef APEX_PHASE_ROUND
#define APEX_PHASE_ROUND 450
#endif
inline constexpr int kPhaseAggr = APEX_PHASE_AGGR;
inline constexpr float kPhaseEarly = APEX_PHASE_EARLY, kPhaseLate = APEX_PHASE_LATE;
inline constexpr int kPhaseRound = APEX_PHASE_ROUND;
#ifndef APEX_TERRITORY_UNTIL
#define APEX_TERRITORY_UNTIL 100000
#endif
#ifndef APEX_CHANNEL_GUARD
#define APEX_CHANNEL_GUARD 1.0f
#endif
#ifndef APEX_RANGED_FLEE
#define APEX_RANGED_FLEE 1
#endif
inline constexpr int kRangedFlee = APEX_RANGED_FLEE;
#ifndef APEX_CI_OUTNUMBERED
#define APEX_CI_OUTNUMBERED -1
#endif
inline constexpr int kCiOutnumbered = APEX_CI_OUTNUMBERED; // C&I leave when outnumbered by this margin (-1 = off) // fleeing casters stun the chaser and step away
inline constexpr float kChannelGuard = APEX_CHANNEL_GUARD; // multiplier on the kill estimate; 0 = off
inline constexpr int kTerritoryUntil = APEX_TERRITORY_UNTIL; // the territory discount ends at this round
inline float kCohesion = APEX_COHESION; // value lost per tile beyond kCohesionDist from the nearest teammate
inline constexpr int kCohesionDist = APEX_COHESION_DIST;
#ifndef APEX_TS_LEARNED
#define APEX_TS_LEARNED 0
#endif
inline constexpr int kTsLearned = APEX_TS_LEARNED;
#ifndef APEX_TS_DECIDE
#define APEX_TS_DECIDE 1
#endif
#ifndef APEX_TS_DECIDE_ROUNDS
#define APEX_TS_DECIDE_ROUNDS 3
#endif
#ifndef APEX_TS_DECIDE_MARGIN
#define APEX_TS_DECIDE_MARGIN 2.0f
#endif
#ifndef APEX_TS_CAMPS
#define APEX_TS_CAMPS 0
#endif
inline constexpr int kTsCamps = APEX_TS_CAMPS; // camps in the fight model
#ifndef APEX_TS_LOCALNET
#define APEX_TS_LOCALNET 0
#endif
inline constexpr int kTsLocalNet = APEX_TS_LOCALNET; // judge plans with the learned local-fight evaluator
#ifndef APEX_TS_CALIB
#define APEX_TS_CALIB 0
#endif
inline constexpr int kTsCalib = APEX_TS_CALIB;
#ifndef APEX_EARLY_CARRY
#define APEX_EARLY_CARRY 0
#endif
#ifndef APEX_EARLY_UNTIL
#define APEX_EARLY_UNTIL 150
#endif
#ifndef APEX_PLAN
#define APEX_PLAN 0
#endif
#ifndef APEX_PLAN_CARRY
#define APEX_PLAN_CARRY 0.75f
#endif
#ifndef APEX_PLAN_DEPTH
#define APEX_PLAN_DEPTH 3
#endif
#ifndef APEX_PLAN_BANK_BIAS
#define APEX_PLAN_BANK_BIAS 1.0f
#endif
#ifndef APEX_ESC_FUEL
#define APEX_ESC_FUEL 60000
#endif
inline constexpr std::uint64_t kEscFuel = APEX_ESC_FUEL; // fuel the escape planner needs left to start
inline constexpr int kPlan = APEX_PLAN;                    // farming planner (plan_search)
inline constexpr float kPlanCarry = APEX_PLAN_CARRY;       // a carried gem vs a banked one (learned: 0.75)
inline constexpr int kPlanDepth = APEX_PLAN_DEPTH;
inline constexpr float kPlanBankBias = APEX_PLAN_BANK_BIAS;
inline constexpr int kEarlyCarry = APEX_EARLY_CARRY; // bag size that sends us to bank before APEX_EARLY_UNTIL (0 = off)
inline constexpr int kEarlyUntil = APEX_EARLY_UNTIL; // engine-calibrated simulator policy
#ifndef APEX_TFC
#define APEX_TFC 0
#endif
// NOVA_PACK_C runtime flag: pack commander on against caster drafts (set once per turn by brain::adapt).
inline bool packOn = false;
#ifndef APEX_TFC_ROUNDS
#define APEX_TFC_ROUNDS 3
#endif
#ifndef APEX_TFC_MARGIN
#define APEX_TFC_MARGIN 3.0f
#endif
#ifndef APEX_TFC_SOLO
#define APEX_TFC_SOLO 0
#endif
#ifndef APEX_TFC_FOLLOW_HP
#define APEX_TFC_FOLLOW_HP 0.35f
#endif
inline constexpr int kTfc = APEX_TFC;                       // team fight commander
inline constexpr int kTfcRounds = APEX_TFC_ROUNDS;
inline constexpr float kTfcMargin = APEX_TFC_MARGIN;
inline constexpr int kTfcSolo = APEX_TFC_SOLO;              // also decide when no ally is near
inline constexpr float kTfcFollowHp = APEX_TFC_FOLLOW_HP;   // followers join an engage order above this health
inline constexpr int kTsDecide = APEX_TS_DECIDE;             // fight-or-run verdict by search
inline constexpr int kTsDecideRounds = APEX_TS_DECIDE_ROUNDS;
inline constexpr float kTsDecideMargin = APEX_TS_DECIDE_MARGIN; // search weights from the position evaluator
inline constexpr int kTSearch = APEX_TSEARCH;
inline constexpr int kTsRange = APEX_TS_RANGE;
inline constexpr int kTsRounds = APEX_TS_ROUNDS;
inline constexpr int kTsReserve = APEX_TS_RESERVE;
// Enemy beliefs (APEX_BELIEF, world.cpp): farm scoring divides a group's score by 1 + APEX_BELIEF_W x the expected
// enemies that could be there before we finish, beyond APEX_BELIEF_ALLY per friendly champion (us included).
#ifndef APEX_BELIEF
#define APEX_BELIEF 0
#endif
#ifndef APEX_BELIEF_W
#define APEX_BELIEF_W 0.5f
#endif
#ifndef APEX_BELIEF_ALLY
#define APEX_BELIEF_ALLY 0.5f
#endif
inline constexpr int kBelief = APEX_BELIEF;
inline constexpr float kBeliefW = APEX_BELIEF_W;
inline constexpr float kBeliefAlly = APEX_BELIEF_ALLY;
#ifndef APEX_TS_PHANTOM // the fight search adds expected hidden enemies (beliefs) as phantom units
#define APEX_TS_PHANTOM 0
#endif
inline constexpr int kTsPhantom = APEX_TS_PHANTOM;
// Deterministic budgets (APEX_DET): work is bounded by counts and predicted costs instead of the fuel left, so
// decisions depend on the game state only (fuel checks remain as safety floors that should almost never bind).
// Otherwise any code change shifts fuel use, flips threshold decisions and moves strength by +-20-40 Elo.
#ifndef APEX_DET
#define APEX_DET 1
#endif
#ifndef APEX_DET_SAFETY       // fuel floor for optional work
#define APEX_DET_SAFETY 12000
#endif
#ifndef APEX_DET_SCORE_N      // camp groups fully evaluated per farm decision
#define APEX_DET_SCORE_N 16
#endif
#ifndef APEX_DET_TS_BUDGET    // predicted fuel for the fight search's playouts
#define APEX_DET_TS_BUDGET 45000
#endif
#ifndef APEX_DET_UNIT_COST    // playout cost per unit per simulated round (in game: 27k for 3 units, 43-72k for 8, 4 rounds)
#define APEX_DET_UNIT_COST 2200
#endif
#ifndef APEX_DET_FIELDS       // distance fields built per calm turn (no enemy in view); dead turns build 4x
#define APEX_DET_FIELDS 2
#endif
inline constexpr int kDet = APEX_DET;
// Caster micro (APEX_CMICRO, tsearch.cpp): ranged champions within APEX_CMICRO_RANGE of an enemy pick their turn by
// scoring every stay/step option as damage dealt minus APEX_CMICRO_RISK x the damage they can take there before
// their next turn; below APEX_CMICRO_FLEE_HP health while losing, the ordinary flight takes over.
#ifndef APEX_CMICRO
#define APEX_CMICRO 1
#endif
#ifndef APEX_CMICRO_RISK
#define APEX_CMICRO_RISK 1.5f
#endif
#ifndef APEX_CMICRO_RANGE
#define APEX_CMICRO_RANGE 6
#endif
#ifndef APEX_CMICRO_FLEE_HP
#define APEX_CMICRO_FLEE_HP 0.35f
#endif
#ifndef APEX_CMICRO_CASSI // 0: Cassi keeps the old fight logic (she fights beside Issac)
#define APEX_CMICRO_CASSI 2
#endif
inline constexpr int kCMicro = APEX_CMICRO;
inline constexpr int kCMicroCassi = APEX_CMICRO_CASSI;
inline constexpr float kCMicroRisk = APEX_CMICRO_RISK;
inline constexpr int kCMicroRange = APEX_CMICRO_RANGE;
inline constexpr float kCMicroFleeHp = APEX_CMICRO_FLEE_HP;
// Economy mechanics. APEX_BANK_RATIO r: bank any bag of 20+ when the altar is within bag / r steps (0 = off).
// APEX_ALTAR_BONUS b: groups within 6 steps of an altar are worth 1 + b (their gems bank at once; 0 = off).
// APEX_SNIPE: walk in on an enemy channelling a tribute (it cannot move; its bag goes to the killer) when we reach it
// before the channel can end and are healthy (0 = off; value = largest walk in steps).
#ifndef APEX_BANK_RATIO
#define APEX_BANK_RATIO 0.0f
#endif
#ifndef APEX_ALTAR_BONUS
#define APEX_ALTAR_BONUS 0.0f
#endif
#ifndef APEX_SNIPE
#define APEX_SNIPE 0
#endif
inline constexpr float kBankRatio = APEX_BANK_RATIO;
inline constexpr float kAltarBonus = APEX_ALTAR_BONUS;
inline constexpr int kSnipe = APEX_SNIPE;
// Numbers advantage (APEX_NUMBERS = k): with d = our living champions minus theirs, aggression is scaled by
// 1 + k d (0.6-1.6) and, from APEX_NUMBERS_INVADE extra living champions, enemy-side groups gain APEX_NUMBERS_BONUS value.
#ifndef APEX_NUMBERS
#define APEX_NUMBERS 0.0f
#endif
#ifndef APEX_NUMBERS_INVADE
#define APEX_NUMBERS_INVADE 99
#endif
#ifndef APEX_NUMBERS_BONUS
#define APEX_NUMBERS_BONUS 0.5f
#endif
inline constexpr float kNumbers = APEX_NUMBERS;
inline constexpr int kNumbersInvade = APEX_NUMBERS_INVADE;
inline constexpr float kNumbersBonus = APEX_NUMBERS_BONUS;
#ifndef APEX_OPP_MODEL // learn the opponent's back-off health in game and use it in the fight search
#define APEX_OPP_MODEL 0
#endif
inline constexpr int kOppModel = APEX_OPP_MODEL;
inline constexpr int kDetSafety = APEX_DET_SAFETY;
inline constexpr int kDetScoreN = APEX_DET_SCORE_N;
inline constexpr int kDetTsBudget = APEX_DET_TS_BUDGET;
inline constexpr int kDetUnitCost = APEX_DET_UNIT_COST;
inline constexpr int kDetFields = APEX_DET_FIELDS;
inline constexpr int kTsBudget = APEX_TS_BUDGET;
inline constexpr int kTsBudgetReserve = APEX_TS_BUDGET_RESERVE;
inline constexpr int kTsUnitCost = APEX_TS_UNIT_COST;
inline constexpr float kTsAlive = APEX_TS_ALIVE;
inline constexpr float kTsHp = APEX_TS_HP;
inline constexpr float kTsThreat = APEX_TS_THREAT;
inline constexpr float kTsFleeA = APEX_TS_FLEE_A;
inline constexpr float kTsFleeC = APEX_TS_FLEE_C;
inline constexpr float kTsAllyFlee = APEX_TS_ALLY_FLEE;
} // namespace tune
