// Learned residual corrector for the farm scorer (NOVA_LEARNER): score *= fix(phi).
// The dot product is hand-rolled (no imports, a few hundred float weights); the weights start
// at zero, which reproduces today's policy EXACTLY, so offline training (self-play via bench.py)
// can only move behavior away from the hand rules in measured steps. phi order is frozen:
// [0] gems/64 [1] gold/64 [2] approach/32 [3] wait/64 [4] kill time/32 [5] participants/4
// [6] camp-group state (0 reward..2 alive) [7] harmless [8] lower-claims/4 [9] higher-claims/4
// [10] hand score/3 [11] drain [12] siege [13] initiating [14] solo_job [15] need/4
#pragma once
#ifndef NOVA_LEARNER
#define NOVA_LEARNER 0
#endif
#ifndef NOVA_LEARNER_W
#define NOVA_LEARNER_W 0.0f // trained weight list, comma separated; variant.py replaces it
#endif
namespace learner {
inline constexpr bool on = NOVA_LEARNER > 0;
inline constexpr float w[] = {NOVA_LEARNER_W};
inline constexpr int n = static_cast<int>(sizeof(w) / sizeof(w[0]));
inline constexpr float gain = 0.06f; // a dot of +-1 moves the score by +-6%: bounded, recoverable errors
inline constexpr float lo = 0.5f, hi = 2.0f;

inline float fix(float score, const float* phi) {
    if constexpr (!on) {
        return score;
    } else {
        float d = 0.0f;
        const int k = n < 16 ? n : 16;
        for (int i = 0; i < k; ++i) d += w[i] * phi[i];
        float m = 1.0f + gain * d;
        m = m < lo ? lo : (m > hi ? hi : m);
        return score * m;
    }
}

// Gather from the locals already computed in score_groups: no extra field loads, ~30 FMA per
// evaluated group and only when switched on.
inline void gather(const void* graw, float hp_left, float kill, float drain, int approach, int wait, int dist,
    int participants, int need, int state, bool harmless, int lower, int higher, float value, float score,
    bool siege, bool initiating, bool solo_job, float* phi) {
    (void)graw;
    (void)value;
    (void)hp_left;
    phi[0] = 0.0f; // gems filled by the caller from g (kept out of here to avoid the raw type)
    phi[1] = 0.0f; // gold likewise
    phi[2] = approach / 32.0f > 3.0f ? 3.0f : static_cast<float>(approach) / 32.0f;
    phi[3] = wait / 64.0f > 3.0f ? 3.0f : static_cast<float>(wait) / 64.0f;
    phi[4] = kill / 32.0f > 3.0f ? 3.0f : kill / 32.0f;
    phi[5] = participants / 4.0f > 2.0f ? 2.0f : static_cast<float>(participants) / 4.0f;
    phi[6] = static_cast<float>(state);
    phi[7] = harmless ? 1.0f : 0.0f;
    phi[8] = static_cast<float>(lower) / 4.0f > 3.0f ? 3.0f : static_cast<float>(lower) / 4.0f;
    phi[9] = static_cast<float>(higher) / 4.0f > 3.0f ? 3.0f : static_cast<float>(higher) / 4.0f;
    phi[10] = score / 3.0f > 3.0f ? 3.0f : score / 3.0f;
    phi[11] = drain / 4.0f > 3.0f ? 3.0f : drain / 4.0f;
    phi[12] = siege ? 1.0f : 0.0f;
    phi[13] = initiating ? 1.0f : 0.0f;
    phi[14] = solo_job ? 1.0f : 0.0f;
    phi[15] = static_cast<float>(need) / 4.0f;
    (void)dist;
}
} // namespace learner
