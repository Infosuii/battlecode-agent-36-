// Agent 3's independently switchable farming experiment. Agent 1 may move these
// constexpr switches into the central registry; no common.h macros are changed.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace farm_score {
inline constexpr bool kCorrectBound = false; // promote only after real runner/fuel and paired gates
inline constexpr bool kDiagnostics = false;
// Diagnostic ONLY: remove work/order caps and route candidate heuristics.
// Toggle kExhaustive to compare exhaustive vs pruned single-group selection.
inline constexpr bool kSingleGroupDiagnostic = false;
inline constexpr bool kExhaustive = false;

enum Counter { considered, math_pruned, work_capped, fuel_capped, order_capped,
    evaluated, scored, bound_bypassed, violations, route_calls, route_skipped,
    route_candidates, route_expansions, route_filtered, stage_attempts,
    stage_skipped, fallback_kept, fallback_empty, count };

template<bool On> struct Stats {
    void add(Counter, std::uint64_t = 1) {}
    std::uint64_t get(Counter) const { return 0; }
    void report(int, int) const {}
};
template<> struct Stats<true> {
    std::array<std::uint64_t, count> n{};
    void add(Counter c, std::uint64_t v = 1) { n[c] += v; }
    std::uint64_t get(Counter c) const { return n[c]; }
    void report(int round, int slot) const {
        std::printf("FARM_LEDGER R%d slot%d considered=%llu math=%llu work_cap=%llu fuel_cap=%llu order_cap=%llu evaluated=%llu scored=%llu bypass=%llu violations=%llu route_calls=%llu route_skips=%llu route_candidates=%llu route_expansions=%llu route_filtered=%llu stage_attempts=%llu stage_skips=%llu fallback_kept=%llu fallback_empty=%llu\n",
            round, slot, (unsigned long long)n[considered], (unsigned long long)n[math_pruned],
            (unsigned long long)n[work_capped], (unsigned long long)n[fuel_capped],
            (unsigned long long)n[order_capped], (unsigned long long)n[evaluated],
            (unsigned long long)n[scored], (unsigned long long)n[bound_bypassed],
            (unsigned long long)n[violations], (unsigned long long)n[route_calls],
            (unsigned long long)n[route_skipped], (unsigned long long)n[route_candidates],
            (unsigned long long)n[route_expansions], (unsigned long long)n[route_filtered],
            (unsigned long long)n[stage_attempts], (unsigned long long)n[stage_skipped],
            (unsigned long long)n[fallback_kept], (unsigned long long)n[fallback_empty]);
    }
};

struct Bound {
    float score = 0;
    bool admissible = false; // false means score the candidate; never compare a sentinel infinity
};

// Base reward and the live multipliers are supplied by the caller. This bounds
// the SINGLE-group objective, not the value of an eventual multi-group route.
// Costs: participants >= 1, kill >= 0, drain >= 0 imply
// cost >= approach + weighted_wait + capture + 4. All omitted value factors
// (claims, contested group, territory, cohesion, buddy, initiation, siege,
// beliefs) must lie in [0,1]. Apply positive factors in scoring order; sticky
// and residual follow division, as they do in the actual scorer.
inline Bound upper_bound(float reward, int approach, int wait, float capture,
    float boot, float dominant, float altar, float numbers, float jitter_abs,
    float sticky, float residual, bool monotone_discounts) {
    if (!monotone_discounts || approach < 0 || wait < 0 || !std::isfinite(reward) || reward < 0 ||
        !std::isfinite(capture) || capture < 0 || !std::isfinite(jitter_abs) || jitter_abs > 1 || jitter_abs < 0)
        return {};
    float value = reward;
    for (float factor : {boot, dominant, altar, numbers, 1.0f + jitter_abs}) {
        if (!std::isfinite(factor) || factor < 0) return {};
        value *= std::max(1.0f, factor);
        if (!std::isfinite(value)) return {};
    }
    // Cast separately to avoid integer overflow in diagnostic/adversarial inputs.
    const float cost = (static_cast<float>(approach) + static_cast<float>(wait)) + capture + 4.0f;
    if (!std::isfinite(cost) || cost <= 0) return {};
    float score = value / cost;
    for (float factor : {sticky, residual}) {
        if (!std::isfinite(factor) || factor < 0) return {};
        score *= std::max(1.0f, factor);
        if (!std::isfinite(score)) return {};
    }
    // Small upward rounding guard, including reassociation of positive costs.
    // Overflow invalidates the bound rather than producing an infinite score.
    score = std::nextafter(score * 1.000002f, std::numeric_limits<float>::infinity());
    if (!std::isfinite(score)) return {};
    return {score, true};
}
} // namespace farm_score
