// Admissible upper bound for a single farming group's final score.
// This does not bound route_search's multi-group objective or its candidate heuristics.
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace farming {
inline float score_upper_bound(float reward, float cost_floor, float stickiness,
    float boot, float dominant, float altar, float numbers, float jitter_epsilon,
    float residual_max, bool supported = true) {
    constexpr float unprunable = std::numeric_limits<float>::max();
    if (!supported || !std::isfinite(reward) || reward < 0.0f ||
        !std::isfinite(cost_floor) || cost_floor <= 0.0f) return unprunable;
    // Every omitted operation is a nonnegative discount, or increases the denominator.
    // Runtime boot tuning is supplied by the caller; no compile-time boot assumption.
    const float factors[] = {stickiness, boot, dominant, altar, numbers,
        1.0f + std::abs(jitter_epsilon), residual_max};
    float numerator = reward;
    for (float factor : factors) {
        if (!std::isfinite(factor) || factor < 0.0f) return unprunable;
        factor = std::max(1.0f, factor);
        if (numerator > unprunable / factor) return unprunable;
        numerator *= factor;
    }
    const float bound = numerator / cost_floor;
    // Cover float rounding across the differently ordered production operations.
    constexpr float pad = 1.0f + 32.0f * std::numeric_limits<float>::epsilon();
    if (!std::isfinite(bound) || bound > unprunable / pad) return unprunable;
    return bound * pad;
}
} // namespace farming
