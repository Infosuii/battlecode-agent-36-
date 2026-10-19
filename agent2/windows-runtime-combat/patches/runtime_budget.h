// Fuel governor (A2_GOV, Agent 2): cheap deterministic admission for optional stages, distance-field searches capped
// at the depth the remaining fuel can pay for, and per-stage counters.
//
// Rules: a check costs a few instructions and one host call; no unsigned underflow; admission compares a stage's
// worst-case cost (not an average) plus the reserve the rest of the turn needs against the fuel left. A refused or
// capped stage must leave coherent state: the caller takes its documented fallback (no move, a greedy step, the
// formula verdict) and never publishes a half-built result.
#pragma once

#include <algorithm>
#include <cstdint>

#include "common.h"

namespace rb {

enum Stage : int { kWalk, kWalkTerrain, kThreatPath, kWardAhead, kStages };
inline constexpr const char* kStageName[kStages] = {"walk", "walk_terrain", "threat_path", "ward_ahead"};

// Counters: ran = admitted with room to spare, capped = admitted with a binding depth cap, skipped = refused.
inline std::uint32_t ran[kStages], capped[kStages], skipped[kStages];

// Fuel the rest of a turn needs after a movement stage: everything after farm or fight movement (ward drop, altar
// prelock, priority, guarded field building) measured under 1k; the margin covers one more small helper.
inline constexpr std::uint64_t kTailReserve = 6000;
// Engine search cost model: (levels + 16) x chunks plus a fixed overhead (world.cpp, measured to 0.3%).
inline constexpr std::uint64_t kSearchFixed = 3000;

inline std::uint64_t left() { return bc::fuel_remaining(); }

// True when `cost` can start and still leave `reserve`.
inline bool fits(std::uint64_t cost, std::uint64_t reserve) {
    const std::uint64_t l = left();
    return l >= reserve && l - reserve >= cost;
}

inline bool admit(Stage s, std::uint64_t cost, std::uint64_t reserve) {
    if (fits(cost, reserve)) {
        ++ran[s];
        return true;
    }
    ++skipped[s];
    return false;
}

// Deepest search the remaining fuel can pay for while keeping `reserve`; 0 when not even a short one fits.
inline std::uint16_t search_cap(std::uint64_t reserve) {
    const std::uint64_t chunks = std::max<std::uint64_t>(field_chunks(), 1);
    const std::uint64_t l = left();
    if (l <= reserve + kSearchFixed) return 0;
    const std::uint64_t levels = (l - reserve - kSearchFixed) / chunks;
    return levels <= 16 ? 0 : static_cast<std::uint16_t>(std::min<std::uint64_t>(levels - 16, 65534));
}

} // namespace rb
