// Skirmish simulator. Deliberately simple and cheap (walls ignored, one target choice rule):
// it only has to rank "engage" against "retreat", not predict exact health.
#include "fight.h"

#include <cmath>
#include <variant>

SpellKit spell_kit(bc::champion_type type, float focus) {
    // Spell metadata comes from the immutable design; cache modifiers per champion type.
    struct Entry {
        bool loaded = false;
        float mod[2] = {0, 0};
        bool stun[2] = {false, false};
    };
    static std::array<Entry, 12> table{};
    const int t = static_cast<int>(type);
    if (t < 0 || t >= 12) return {};
    auto& e = table[t];
    if (!e.loaded) {
        e.loaded = true;
        try {
            bc::champion design{type};
            const auto abilities = design.abilities();
            for (int s = 0; s < 2; ++s) {
                const auto& info = abilities[s].info();
                if (!info || !abilities[s].can_target_enemies()) continue;
                std::visit(
                    [&](const auto& a) {
                        using A = std::decay_t<decltype(a)>;
                        if constexpr (std::is_same_v<A, bc::ability_damage> || std::is_same_v<A, bc::ability_aoe_damage>)
                            e.mod[s] = a.modifier;
                        if constexpr (std::is_same_v<A, bc::ability_cc>) {
                            e.mod[s] = a.modifier;
                            e.stun[s] = true;
                        }
                    },
                    *info);
            }
        } catch (...) {
        }
    }
    SpellKit kit;
    const float d0 = e.mod[0] * focus, d1 = e.mod[1] * focus;
    kit.nuke = std::max(d0, d1);
    kit.stun = e.stun[0] || e.stun[1];
    kit.two_spells = d0 > 0 && d1 > 0;
    return kit;
}

namespace {
int sgn(int v) { return (v > 0) - (v < 0); }

// Lowest-health living opponent within `reach` of `from`, or -1.
int weakest_in_reach(const std::vector<SimUnit>& us, int team, vec2 from, int reach) {
    int best = -1;
    for (int i = 0; i < static_cast<int>(us.size()); ++i) {
        const auto& o = us[i];
        if (!o.alive || o.team == team || cheb(o.pos, from) > reach) continue;
        if (best < 0 || o.hp < us[best].hp) best = i;
    }
    return best;
}

int nearest_opponent(const std::vector<SimUnit>& us, int team, vec2 from) {
    int best = -1, best_d = 1 << 20;
    for (int i = 0; i < static_cast<int>(us.size()); ++i) {
        const auto& o = us[i];
        if (!o.alive || o.team == team) continue;
        const int dist = cheb(o.pos, from);
        if (dist < best_d) {
            best_d = dist;
            best = i;
        }
    }
    return best;
}
} // namespace

SimResult simulate_fight(std::vector<SimUnit> us, bool ours_retreat, int turns) {
    const int n = static_cast<int>(us.size());
    std::array<float, 16> start_hp{};
    for (int i = 0; i < n && i < 16; ++i) start_hp[i] = us[i].hp;
    for (int t = 0; t < turns; ++t) {
        // Teams alternate; ours first is a fair approximation of the alternating turn order.
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i < n; ++i) {
                auto& u = us[i];
                if (!u.alive || u.team != side) continue;
                if (u.stunned > 0) {
                    --u.stunned;
                    continue;
                }
                const int near = nearest_opponent(us, u.team, u.pos);
                if (near < 0) break;
                if (u.team == 0 && ours_retreat) {
                    const vec2 away = u.pos - us[near].pos;
                    u.pos = u.pos + vec2{sgn(away.x), sgn(away.y)};
                    continue;
                }
                // Spell first (range 4), on the weakest opponent it reaches.
                if (u.cd == 0 && u.nuke > 0) {
                    const int tgt = weakest_in_reach(us, u.team, u.pos, 4);
                    if (tgt >= 0) {
                        us[tgt].hp -= u.nuke;
                        if (u.stun) us[tgt].stunned = std::max(us[tgt].stunned, 1);
                        if (us[tgt].hp <= 0) us[tgt].alive = false;
                        u.cd = u.spell_cd;
                    }
                }
                int tgt = weakest_in_reach(us, u.team, u.pos, u.range);
                if (tgt < 0) {
                    // Step toward the nearest opponent; melee can still hit after the step.
                    const vec2 dir = us[near].pos - u.pos;
                    u.pos = u.pos + vec2{sgn(dir.x), sgn(dir.y)};
                    if (u.range <= 1) tgt = weakest_in_reach(us, u.team, u.pos, u.range);
                }
                if (tgt >= 0) {
                    us[tgt].hp -= u.str;
                    if (us[tgt].hp <= 0) us[tgt].alive = false;
                }
                if (u.cd > 0) --u.cd;
            }
    }
    SimResult r;
    for (int i = 0; i < n && i < 16; ++i) {
        const auto& u = us[i];
        const float lost = (start_hp[i] - std::max(u.hp, 0.0f)) / u.max_hp * 100.0f + (u.alive ? 0.0f : u.death_cost);
        if (u.team == 0) {
            r.score -= lost;
            r.our_deaths += u.alive ? 0 : 1;
        } else {
            r.score += lost;
            r.their_deaths += u.alive ? 0 : 1;
        }
    }
    return r;
}
