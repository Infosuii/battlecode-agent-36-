// Optional runtime governor. Estimates are admissions, not proofs of worst-case cost.
#pragma once
#include "common.h"

namespace runtime {
enum class Stage : unsigned { Init, Snapshot, Prewarm, TacticalPair, ActionSearch, FarmScore, Route, Fields, TurnTail, Turn, Count };
inline constexpr std::uint64_t kActionReserve = 22000;
// Subtraction after the floor test avoids overflow in cost + reserve and underflow.
inline constexpr bool fits(std::uint64_t left, std::uint64_t cost, std::uint64_t reserve) {
    return left >= reserve && cost <= left - reserve;
}
inline const char* name(Stage s) {
    constexpr const char* names[] = {"init", "snapshot", "prewarm", "tactical_pair", "action_search", "farm_score", "route", "fields", "tail", "turn"};
    return names[static_cast<unsigned>(s)];
}
inline bool admit(Stage s, std::uint64_t cost, std::uint64_t reserve = kActionReserve) {
#if NOVA_RUNTIME_GOVERNOR
    const bool ok = fits(bc::fuel_remaining(), cost, reserve);
#if NOVA_RUNTIME_TELEMETRY
    if (!ok) std::printf("RBSKIP r%u stage%s cost%llu reserve%llu\n", bc::round_number(), name(s),
        static_cast<unsigned long long>(cost), static_cast<unsigned long long>(reserve));
#endif
    return ok;
#else
    (void)s; (void)cost; (void)reserve;
    return true;
#endif
}
// Enable only in an instrumented release build: host fuel counters remain charged.
struct Probe {
#if NOVA_RUNTIME_TELEMETRY
    Stage stage;
    std::uint64_t start;
    unsigned round;
    explicit Probe(Stage s) : stage(s), start(bc::fuel_remaining()), round(bc::round_number()) {}
    ~Probe() {
        const auto end = bc::fuel_remaining();
        const auto now = bc::round_number();
        std::printf("RB r%u stage%s cost%llu left%llu suspended%d\n", round, name(stage),
            static_cast<unsigned long long>(now == round && start >= end ? start - end : 0),
            static_cast<unsigned long long>(end), now != round ? 1 : 0);
    }
#else
    explicit Probe(Stage) {}
#endif
};
} // namespace runtime
