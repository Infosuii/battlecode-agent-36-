// Strategy brain: the draft classifies the map and the enemy lineup, writes them to draft memory, and every
// champion then loads the settings profile for that map class and adapts it each turn to the score race.
// Map class = team size (3/4/5) x size (small: area <= 2000 tiles, large), index (size - 3) * 2 + large.
// Every profile value defaults to the global setting, and every adaptation defaults to off, so with no
// overrides the bot plays exactly as before. tools/search.py tunes the APEX_C<k>_* and adaptation macros.
#pragma once
#include "common.h"
#include "draft.h"

#ifndef APEX_BRAIN
#define APEX_BRAIN 0
#endif

// Per-class profiles (k = 0..5: 3S, 3L, 4S, 4L, 5S, 5L).
#define APEX_PROFILE_DEFAULTS(k)                                                                                       \
    {                                                                                                                  \
        APEX_C##k##_AGGRESSION, APEX_C##k##_TRIBUTE_CARRY, APEX_C##k##_RETREAT_HP, APEX_C##k##_DANGER_TTD,              \
            APEX_C##k##_SHOP_GOLD, APEX_C##k##_HUNT                                                                    \
    }
#ifndef APEX_C0_AGGRESSION
#define APEX_C0_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C1_AGGRESSION
#define APEX_C1_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C2_AGGRESSION
#define APEX_C2_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C3_AGGRESSION
#define APEX_C3_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C4_AGGRESSION
#define APEX_C4_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C5_AGGRESSION
#define APEX_C5_AGGRESSION APEX_AGGRESSION
#endif
#ifndef APEX_C0_TRIBUTE_CARRY
#define APEX_C0_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C1_TRIBUTE_CARRY
#define APEX_C1_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C2_TRIBUTE_CARRY
#define APEX_C2_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C3_TRIBUTE_CARRY
#define APEX_C3_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C4_TRIBUTE_CARRY
#define APEX_C4_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C5_TRIBUTE_CARRY
#define APEX_C5_TRIBUTE_CARRY APEX_TRIBUTE_CARRY
#endif
#ifndef APEX_C0_RETREAT_HP
#define APEX_C0_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C1_RETREAT_HP
#define APEX_C1_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C2_RETREAT_HP
#define APEX_C2_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C3_RETREAT_HP
#define APEX_C3_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C4_RETREAT_HP
#define APEX_C4_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C5_RETREAT_HP
#define APEX_C5_RETREAT_HP APEX_RETREAT_HP
#endif
#ifndef APEX_C0_DANGER_TTD
#define APEX_C0_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C1_DANGER_TTD
#define APEX_C1_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C2_DANGER_TTD
#define APEX_C2_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C3_DANGER_TTD
#define APEX_C3_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C4_DANGER_TTD
#define APEX_C4_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C5_DANGER_TTD
#define APEX_C5_DANGER_TTD APEX_DANGER_TTD
#endif
#ifndef APEX_C0_SHOP_GOLD
#define APEX_C0_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C1_SHOP_GOLD
#define APEX_C1_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C2_SHOP_GOLD
#define APEX_C2_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C3_SHOP_GOLD
#define APEX_C3_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C4_SHOP_GOLD
#define APEX_C4_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C5_SHOP_GOLD
#define APEX_C5_SHOP_GOLD APEX_SHOP_GOLD
#endif
#ifndef APEX_C0_HUNT
#define APEX_C0_HUNT APEX_HUNT
#endif
#ifndef APEX_C1_HUNT
#define APEX_C1_HUNT APEX_HUNT
#endif
#ifndef APEX_C2_HUNT
#define APEX_C2_HUNT APEX_HUNT
#endif
#ifndef APEX_C3_HUNT
#define APEX_C3_HUNT APEX_HUNT
#endif
#ifndef APEX_C4_HUNT
#define APEX_C4_HUNT APEX_HUNT
#endif
#ifndef APEX_C5_HUNT
#define APEX_C5_HUNT APEX_HUNT
#endif

// In-game adaptation (all off by default).
#ifndef APEX_BEHIND_GAP        // enemy score lead (gems) that counts as "behind"; 0 = off
#define APEX_BEHIND_GAP 0
#endif
#ifndef APEX_BEHIND_AGGR       // aggression multiplier while behind
#define APEX_BEHIND_AGGR 1.0f
#endif
#ifndef APEX_BEHIND_CARRY      // tribute-carry multiplier while behind (bank sooner < 1)
#define APEX_BEHIND_CARRY 1.0f
#endif
#ifndef APEX_AHEAD_GAP         // our score lead that counts as "ahead"; 0 = off
#define APEX_AHEAD_GAP 0
#endif
#ifndef APEX_AHEAD_AGGR        // aggression multiplier while ahead (play safe < 1)
#define APEX_AHEAD_AGGR 1.0f
#endif
#ifndef APEX_CASTER_TTD        // danger-threshold multiplier against 2+ enemy casters (flee earlier > 1)
#define APEX_CASTER_TTD 1.0f
#endif

// Per-map multipliers on top of the class profile (official maps only, identified by size and altar count; the
// test variants are flips and transposes, which keep both). M<id>: 0 collision, 1 corners, 2 elkring, 3 expanse,
// 4 interlock, 5 lanes, 6 monolith, 7 packs, 8 rows. MALL applies to every official map (screening).
// Absolute per-map values (default: the global setting): gold weight, cohesion, harmless-group bonus, territory.
#define APEX_MAP_MULS(k)                                                                                               \
    { APEX_M##k##_AGGR, APEX_M##k##_TTD, APEX_M##k##_CARRY, APEX_M##k##_GOLD, APEX_M##k##_COH, APEX_M##k##_BOOT,     \
        APEX_M##k##_TERR }
#ifndef APEX_MALL_GOLD
#define APEX_MALL_GOLD APEX_GOLD_WEIGHT
#endif
#ifndef APEX_M0_GOLD
#define APEX_M0_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M1_GOLD
#define APEX_M1_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M2_GOLD
#define APEX_M2_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M3_GOLD
#define APEX_M3_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M4_GOLD
#define APEX_M4_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M5_GOLD
#define APEX_M5_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M6_GOLD
#define APEX_M6_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M7_GOLD
#define APEX_M7_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_M8_GOLD
#define APEX_M8_GOLD APEX_MALL_GOLD
#endif
#ifndef APEX_MALL_COH
#define APEX_MALL_COH APEX_COHESION
#endif
#ifndef APEX_M0_COH
#define APEX_M0_COH APEX_MALL_COH
#endif
#ifndef APEX_M1_COH
#define APEX_M1_COH APEX_MALL_COH
#endif
#ifndef APEX_M2_COH
#define APEX_M2_COH APEX_MALL_COH
#endif
#ifndef APEX_M3_COH
#define APEX_M3_COH APEX_MALL_COH
#endif
#ifndef APEX_M4_COH
#define APEX_M4_COH APEX_MALL_COH
#endif
#ifndef APEX_M5_COH
#define APEX_M5_COH APEX_MALL_COH
#endif
#ifndef APEX_M6_COH
#define APEX_M6_COH APEX_MALL_COH
#endif
#ifndef APEX_M7_COH
#define APEX_M7_COH APEX_MALL_COH
#endif
#ifndef APEX_M8_COH
#define APEX_M8_COH APEX_MALL_COH
#endif
#ifndef APEX_MALL_BOOT
#define APEX_MALL_BOOT APEX_BOOT_BONUS
#endif
#ifndef APEX_M0_BOOT
#define APEX_M0_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M1_BOOT
#define APEX_M1_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M2_BOOT
#define APEX_M2_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M3_BOOT
#define APEX_M3_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M4_BOOT
#define APEX_M4_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M5_BOOT
#define APEX_M5_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M6_BOOT
#define APEX_M6_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M7_BOOT
#define APEX_M7_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_M8_BOOT
#define APEX_M8_BOOT APEX_MALL_BOOT
#endif
#ifndef APEX_MALL_TERR
#define APEX_MALL_TERR APEX_TERRITORY
#endif
#ifndef APEX_M0_TERR
#define APEX_M0_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M1_TERR
#define APEX_M1_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M2_TERR
#define APEX_M2_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M3_TERR
#define APEX_M3_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M4_TERR
#define APEX_M4_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M5_TERR
#define APEX_M5_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M6_TERR
#define APEX_M6_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M7_TERR
#define APEX_M7_TERR APEX_MALL_TERR
#endif
#ifndef APEX_M8_TERR
#define APEX_M8_TERR APEX_MALL_TERR
#endif

#ifndef APEX_MALL_AGGR
#define APEX_MALL_AGGR 1.0f
#endif
#ifndef APEX_MALL_TTD
#define APEX_MALL_TTD 1.0f
#endif
#ifndef APEX_MALL_CARRY
#define APEX_MALL_CARRY 1.0f
#endif
#ifndef APEX_M0_AGGR
#define APEX_M0_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M1_AGGR
#define APEX_M1_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M2_AGGR
#define APEX_M2_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M3_AGGR
#define APEX_M3_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M4_AGGR
#define APEX_M4_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M5_AGGR
#define APEX_M5_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M6_AGGR
#define APEX_M6_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M7_AGGR
#define APEX_M7_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M8_AGGR
#define APEX_M8_AGGR APEX_MALL_AGGR
#endif
#ifndef APEX_M0_TTD
#define APEX_M0_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M1_TTD
#define APEX_M1_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M2_TTD
#define APEX_M2_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M3_TTD
#define APEX_M3_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M4_TTD
#define APEX_M4_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M5_TTD
#define APEX_M5_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M6_TTD
#define APEX_M6_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M7_TTD
#define APEX_M7_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M8_TTD
#define APEX_M8_TTD APEX_MALL_TTD
#endif
#ifndef APEX_M0_CARRY
#define APEX_M0_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M1_CARRY
#define APEX_M1_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M2_CARRY
#define APEX_M2_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M3_CARRY
#define APEX_M3_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M4_CARRY
#define APEX_M4_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M5_CARRY
#define APEX_M5_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M6_CARRY
#define APEX_M6_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M7_CARRY
#define APEX_M7_CARRY APEX_MALL_CARRY
#endif
#ifndef APEX_M8_CARRY
#define APEX_M8_CARRY APEX_MALL_CARRY
#endif

namespace brain {
struct MapMuls {
    float aggr, ttd, carry, gold, coh, boot, terr;
};
inline constexpr MapMuls kMapMuls[9] = {APEX_MAP_MULS(0), APEX_MAP_MULS(1), APEX_MAP_MULS(2), APEX_MAP_MULS(3),
    APEX_MAP_MULS(4), APEX_MAP_MULS(5), APEX_MAP_MULS(6), APEX_MAP_MULS(7), APEX_MAP_MULS(8)};

// Official map from its size and altar count (-1 for anything else).
inline int map_id(int w, int h, int altars) {
    if (w == 41 && h == 41) return 0;
    if (w == 71 && h == 71) return 1;
    if (w == 30 && h == 30) return 2;
    if (w == 80 && h == 80) return 3;
    if (w == 40 && h == 40) return 4;
    if ((w == 90 && h == 30) || (w == 30 && h == 90)) return 5;
    if (w == 50 && h == 50) return altars >= 4 ? 6 : 8;
    if (w == 31 && h == 31) return 7;
    return -1;
}

struct Profile {
    float aggression;
    int tribute_carry;
    float retreat_hp, danger_ttd;
    int shop_gold, hunt;
};
inline constexpr Profile kProfiles[6] = {APEX_PROFILE_DEFAULTS(0), APEX_PROFILE_DEFAULTS(1), APEX_PROFILE_DEFAULTS(2),
    APEX_PROFILE_DEFAULTS(3), APEX_PROFILE_DEFAULTS(4), APEX_PROFILE_DEFAULTS(5)};

// Draft memory layout: [0] magic, [1] team size, [2] enemy caster picks, [3] map class.
inline constexpr std::uint8_t kMagic = 0xA5;

inline int map_class(int w, int h, int team_size) {
    const int t = std::clamp(team_size, 3, 5);
    return (t - 3) * 2 + (w * h > 2000 ? 1 : 0);
}
inline bool is_caster(bc::champion_draft c) {
    return c == bc::champion_draft::cassi_and_issac || c == bc::champion_draft::stellar ||
        c == bc::champion_draft::prism || c == bc::champion_draft::tavi || c == bc::champion_draft::willow;
}

// Draft side: record what the champions need to know (called on every draft turn).
// max_group = gem value of the biggest camp group on the map (from map facts; 0 = unknown), so
// champions can tell strip-carry maps from sparse bank-early maps (NOVA_BANK_FIX).
// flags: bit0 = race map (dense pool: total gems/area >= NOVA_RACE_DENS), bit1 = monolith map.
inline void record_draft(unsigned team_size, int max_group = 0, unsigned flags = 0) {
    if (!APEX_BRAIN) return;
    int casters = 0;
    for (auto c : bc::draft::current_draft(team::enemy)) casters += is_caster(c);
    const int cls = map_class(bc::map::width(), bc::map::height(), static_cast<int>(team_size));
    const std::uint8_t cl8 = static_cast<std::uint8_t>(cls), ts8 = static_cast<std::uint8_t>(team_size),
        cs8 = static_cast<std::uint8_t>(std::min(casters, 255)), mg8 = static_cast<std::uint8_t>(std::min(std::max(max_group, 0), 255)),
        fl8 = static_cast<std::uint8_t>(flags & 3);
#if NOVA_STRICT_MAP_CACHE && NOVA_STRICT_MAP_ID
    if (bc::constants::draft_memory_bytes >= 16) {
        // Versioned identity extension; the existing six strategy bytes retain
        // their exact layout and meaning. Short SDK writes leave old bytes, so
        // failed facts must explicitly invalidate a previous cache extension.
        std::array<std::uint8_t, 16> mem{kMagic, ts8, cs8, cl8, mg8, fl8};
        if (!nova_record_map_cache(mem, team_size)) std::fill(mem.begin() + 6, mem.end(), 0);
        bc::draft_memory = mem;
        return;
    }
#endif
    if (bc::constants::draft_memory_bytes >= 6) {
        const std::array<std::uint8_t, 6> mem{kMagic, ts8, cs8, cl8, mg8, fl8};
        bc::draft_memory = mem;
    } else if (bc::constants::draft_memory_bytes >= 5) {
        const std::array<std::uint8_t, 5> mem{kMagic, ts8, cs8, cl8, mg8};
        bc::draft_memory = mem;
    } else {
        const std::array<std::uint8_t, 4> mem{kMagic, ts8, cs8, cl8};
        bc::draft_memory = mem;
    }
}

struct State {
    bool ready = false;
    int cls = 0, enemy_casters = 0, map = -1;
    int max_group = 0; // biggest camp-group gem value seen at draft (0 = unknown/absent)
    bool race = false, mono = false; // draft-memory bit flags (NOVA_RACE_BANK / NOVA_MONO_BANK)
};
inline State state;

// Game side: load the class profile once (fallback: classify from the map and our champion count).
inline void load(int ally_count) {
    if (!APEX_BRAIN || state.ready) return;
    state.ready = true;
    int team_size = ally_count, cls = -1;
    try {
        if (bc::draft_memory.size() >= 4 && static_cast<std::uint8_t>(bc::draft_memory[0]) == kMagic) {
            team_size = static_cast<int>(bc::draft_memory[1]);
            state.enemy_casters = static_cast<int>(bc::draft_memory[2]);
            cls = static_cast<int>(bc::draft_memory[3]);
            if (bc::draft_memory.size() >= 5) state.max_group = static_cast<int>(bc::draft_memory[4]);
            if (bc::draft_memory.size() >= 6) { // flags: bit0 race map, bit1 monolith map
                state.race = (static_cast<int>(bc::draft_memory[5]) & 1) != 0;
                state.mono = (static_cast<int>(bc::draft_memory[5]) & 2) != 0;
            }
        }
    } catch (...) {
    }
    if (cls < 0 || cls > 5) cls = map_class(bc::map::width(), bc::map::height(), team_size);
    state.cls = cls;
    try {
        bool approved = true;
        if (NOVA_STRICT_MAP_ID) {
            std::uint32_t groups = 0;
            approved = bc::detail::sdk_map_num_camp_groups(&groups) == 0 &&
                nova_official_map_id(bc::map::width(), bc::map::height(), bc::draft::team_size(), static_cast<int>(groups)) >= 0;
        }
        // The canonical classifier only approves the layout. The existing profile
        // index remains in its separate 0..8 namespace, including legacy aliases.
        state.map = approved ? map_id(bc::map::width(), bc::map::height(), static_cast<int>(bc::map::altars().positions().size())) : -1;
    } catch (...) {
        state.map = -1;
    }
#ifdef APEX_BRAINLOG
    std::printf("BRAIN class %d team %d enemy_casters %d from_memory %d\n", cls, team_size, state.enemy_casters,
        bc::draft_memory.size() >= 4 ? 1 : 0);
#endif
}

// Every turn: set the live settings from the class profile and the current situation.
inline void adapt(int ally_score, int enemy_score) {
    if (!APEX_BRAIN || !state.ready) return;
    const Profile& p = kProfiles[state.cls];
    const MapMuls mm = state.map >= 0 ? kMapMuls[state.map]
                                      : MapMuls{1.0f, 1.0f, 1.0f, APEX_GOLD_WEIGHT, APEX_COHESION, APEX_BOOT_BONUS, APEX_TERRITORY};
    tune::kGoldWeight = mm.gold;
    tune::kCohesion = mm.coh;
    tune::kBootBonus = mm.boot;
    tune::kTerritory = mm.terr;
    float aggr = p.aggression * mm.aggr;
    float carry = static_cast<float>(p.tribute_carry) * mm.carry;
    if (APEX_BEHIND_GAP > 0 && enemy_score - ally_score >= APEX_BEHIND_GAP) {
        aggr *= APEX_BEHIND_AGGR;
        carry *= APEX_BEHIND_CARRY;
    } else if (APEX_AHEAD_GAP > 0 && ally_score - enemy_score >= APEX_AHEAD_GAP) {
        aggr *= APEX_AHEAD_AGGR;
    }
    tune::kAggression = aggr;
    tune::kTributeCarry = std::max(8, static_cast<int>(carry + 0.5f));
    tune::kRetreatHp = p.retreat_hp;
    // NOVA_BLEND_RACE: the per-map-class retreat override fires only on race-classified maps.
    if (blendRace && state.race) tune::kRetreatHp = blendRetreat;
    tune::kDangerTtd = p.danger_ttd * mm.ttd * (state.enemy_casters >= 2 ? APEX_CASTER_TTD : 1.0f);
    tune::kShopGold = p.shop_gold;
    tune::kHunt = p.hunt != 0;
    tune::packOn = NOVA_PACK_C > 0 && state.enemy_casters >= NOVA_PACK_C; // team commander vs caster drafts
}
} // namespace brain
