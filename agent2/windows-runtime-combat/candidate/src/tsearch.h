// Tactical search: a local fight played forward in our head. The model follows GameRules.md: one move,
// one basic attack (melee range 1; ranged range 3, and ranged champions move or attack) and one spell
// per turn; spells deal modifier x focus and then wait 8 owner turn ends; stuns cost the target its next
// turn; Varg splashes, Ren retaliates, Marcella ignores stuns, Vendetta's kills reset her cooldowns; the
// killer takes the victim's carried gems; Cassi and Issac die together. Units alternate by team.
// Everything is fixed-size and allocation-free: one rollout costs a few thousand fuel.
#pragma once
#include "common.h"

// What the last fight/run snapshot left out (Agent 2: A2_SNAP selection, A2_TD_AUTH verdict authority, telemetry).
namespace a2ts {
struct SnapInfo {
    int scene = 0;               // 1 + allies within 7 + live enemies within 8, before any selection
    int n = 0;                   // units in the model
    int dropped_enemies = 0, dropped_allies = 0;
    bool omit_reach = false;     // a dropped enemy could hit us next turn (within max(range + 2, 5), as the formula)
    bool link_partial = false;   // a Cassi/Issac in the model whose visible partner is not
    int status = -1;             // tactical_decide: 0 no enemy, 1 safety, 2 budget, 3 incomplete, 4 margin, 5 fight, 6 run
};
inline SnapInfo snap;
} // namespace a2ts

namespace ts {

inline constexpr int kMaxUnits = 8;

// Spell kinds (bc::ability_kind values) and flags.
enum : std::uint8_t { K_NONE = 0, K_RESET = 1, K_DAMAGE = 2, K_AOE = 3, K_CC = 4, K_HEAL = 5, K_DASH = 6, K_SHIELD = 7 };
enum : std::uint8_t { SF_EXCL_WALK = 1, SF_STUN = 2, SF_ENEMY = 4, SF_ALLY = 8, SF_ENH = 16 };
// Unit flags.
enum : std::uint8_t { U_ALIVE = 1, U_ACTED = 2, U_SPLASH = 4, U_STUN_IMMUNE = 8, U_RESET_ON_KILL = 16, U_ENHANCED = 32 };

struct SSpell {
    float value = 0;     // damage, heal or shield amount (modifier x focus)
    std::int8_t kind = K_NONE, cd = 99, range = 0, radius = 0;
    std::uint8_t flags = 0;
};

struct Unit {
    float hp = 0, max_hp = 1, str = 0, shield = 0, retaliate = 0;
    SSpell sp[2];
    std::int16_t x = 0, y = 0, gems = 0;
    std::int8_t team = 0, range = 1, stunned = 0, link = -1, shield_turns = 0;
    std::uint8_t flags = 0;
    bool alive() const { return (flags & U_ALIVE) != 0; }
};

// A camp near the fight: at each round start it hits the nearest champion within its range (GameRules:
// camp actions come before champion turns; a camp kill destroys the carried gems).
struct Camp {
    float str = 0;
    std::int16_t x = 0, y = 0;
    std::int8_t range = 3;
};
inline constexpr int kMaxCamps = 6;

struct State {
    Unit u[kMaxUnits];
    int n = 0;
    Camp camps[kMaxCamps];
    int nc = 0;
};

struct Map {
    const std::uint64_t* blocked = nullptr; // engine-chunked bitboard: walls, holes, altars, spawns
    int w = 0, h = 0, stride = 0;
    std::int16_t home_x[2] = {0, 0}, home_y[2] = {0, 0}; // spawn centres per team (flight direction)
    bool is_blocked(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return true;
        return (blocked[(static_cast<std::uint32_t>(y) >> 3) * stride + (static_cast<std::uint32_t>(x) >> 3)] >>
                   (((y & 7) << 3) | (x & 7))) & 1u;
    }
};

struct Policy {
    float flee[2] = {0.2f, 0.2f}; // health fraction below which a team's units run when an opponent is close
    int root_mode = 0;           // unit 0: 0 = the policy, 1 = always fight, 2 = always run (plan comparison)
    // Engagement (calibrated against the engine, tools/simcheck.py): a unit closes in and trades only when it wins the
    // race (time to kill < aggr x time to die, capped at 8); otherwise it backs off when threatened and holds when not.
    // 0 = the old rule (everyone fights).
    float aggr[2] = {0.0f, 0.0f};
    bool use_reset = true;       // second hit with an attack reset
    int first = 1;               // team that acts first in the current round (the starting team swaps every round)
    int team_mode[2] = {0, 0};   // team plan: 0 the policy, 1 everyone fights, 2 everyone runs (team commander)
    int focus[2] = {-1, -1};     // a team's focus target (unit index) when it fights as one
};

struct Weights {
    float alive = 25.0f, hp = 12.0f, threat = 0.5f, gem = 1.0f;
};

// The searching unit's root action (unit 0). Indices refer to State::u.
struct Action {
    std::int8_t pre_slot = -1, pre_target = -1; // spell cast first (unit target, or self when -1 and kind allows)
    std::int16_t pre_x = -1, pre_y = -1;        // ground target for an area pre-cast
    std::int8_t dash_slot = -1;
    std::int16_t dash_x = -1, dash_y = -1;      // dash landing
    std::int16_t move_x = -1, move_y = -1;      // one ordinary step
    std::int8_t attack = -1, reset_slot = -1;   // basic attack target (after moving), and a reset for a second hit
    float prior = 0;
};

inline int cheb(int ax, int ay, int bx, int by) { return std::max(std::abs(ax - bx), std::abs(ay - by)); }
inline int sgn(int v) { return (v > 0) - (v < 0); }

inline bool occupied(const State& s, int x, int y) {
    for (int i = 0; i < s.n; ++i)
        if (s.u[i].alive() && s.u[i].x == x && s.u[i].y == y) return true;
    return false;
}
inline bool free_tile(const State& s, const Map& m, int x, int y) { return !m.is_blocked(x, y) && !occupied(s, x, y); }

inline void kill(State& s, int killer, int victim) {
    Unit& v = s.u[victim];
    if (!v.alive()) return;
    v.flags &= static_cast<std::uint8_t>(~U_ALIVE);
    v.hp = 0;
    if (killer >= 0 && s.u[killer].team != v.team) {
        s.u[killer].gems = static_cast<std::int16_t>(s.u[killer].gems + v.gems);
        if (s.u[killer].flags & U_RESET_ON_KILL) s.u[killer].sp[0].cd = s.u[killer].sp[1].cd = 0;
    }
    v.gems = 0;
    if (v.link >= 0 && v.link < s.n && s.u[v.link].alive()) kill(s, killer, v.link); // Cassi and Issac die together
}

// Damage after shields; Ren hits back (no chains).
inline void deal(State& s, int src, int dst, float dmg, bool can_retaliate = true) {
    Unit& t = s.u[dst];
    if (!t.alive() || dmg <= 0) return;
    if (t.shield > 0) {
        const float a = std::min(t.shield, dmg);
        t.shield -= a;
        dmg -= a;
    }
    t.hp -= dmg;
    if (can_retaliate && t.retaliate > 0 && src >= 0 && s.u[src].alive()) deal(s, dst, src, t.retaliate, false);
    if (t.hp <= 0) kill(s, src, dst);
}

inline void stun(State& s, int dst) {
    Unit& t = s.u[dst];
    if (t.alive() && !(t.flags & U_STUN_IMMUNE)) t.stunned = 1;
}

inline void basic_attack(State& s, int a, int t) {
    Unit& u = s.u[a];
    const float dmg = u.str;
    const int tx = s.u[t].x, ty = s.u[t].y;
    if (u.flags & U_ENHANCED) {
        u.flags &= static_cast<std::uint8_t>(~U_ENHANCED);
        stun(s, t);
    }
    deal(s, a, t, dmg);
    if (u.flags & U_SPLASH)
        for (int j = 0; j < s.n; ++j)
            if (j != t && s.u[j].alive() && s.u[j].team != u.team && cheb(s.u[j].x, s.u[j].y, tx, ty) <= 1) deal(s, a, j, dmg);
}

// One spell cast; returns false if it cannot be cast. Target: unit index, or (px, py) for area spells.
inline bool cast(State& s, int a, int slot, int target, int px, int py) {
    Unit& u = s.u[a];
    SSpell& sp = u.sp[slot];
    if (sp.cd != 0 || sp.kind == K_NONE) return false;
    switch (sp.kind) {
    case K_DAMAGE:
    case K_CC:
        if (target < 0 || !s.u[target].alive() || cheb(u.x, u.y, s.u[target].x, s.u[target].y) > sp.range) return false;
        if (sp.flags & SF_STUN) stun(s, target);
        deal(s, a, target, sp.value);
        break;
    case K_AOE:
        if (cheb(u.x, u.y, px, py) > sp.range) return false;
        for (int j = 0; j < s.n; ++j)
            if (s.u[j].alive() && s.u[j].team != u.team && cheb(s.u[j].x, s.u[j].y, px, py) <= sp.radius) {
                if (sp.flags & SF_STUN) stun(s, j);
                deal(s, a, j, sp.value);
            }
        break;
    case K_HEAL:
        if (target < 0 || target == a || !s.u[target].alive() || cheb(u.x, u.y, s.u[target].x, s.u[target].y) > sp.range)
            return false;
        s.u[target].hp = std::min(s.u[target].max_hp, s.u[target].hp + sp.value);
        break;
    case K_SHIELD:
        u.shield = std::max(u.shield, sp.value);
        u.shield_turns = 5;
        break;
    case K_RESET:
        if (sp.flags & SF_ENH) u.flags |= U_ENHANCED;
        break;
    default:
        return false;
    }
    sp.cd = 8;
    return true;
}

// Greedy step: toward (tx, ty) when `toward`, else away from every opponent (and toward home).
inline bool step(State& s, const Map& m, int a, int tx, int ty, bool toward) {
    Unit& u = s.u[a];
    int best_x = u.x, best_y = u.y;
    if (toward) {
        const int dx = sgn(tx - u.x), dy = sgn(ty - u.y);
        const int cand[3][2] = {{dx, dy}, {dx, dy == 0 ? 1 : 0}, {dx == 0 ? 1 : 0, dy}};
        int best_d = cheb(u.x, u.y, tx, ty);
        for (const auto& c : cand) {
            if (c[0] == 0 && c[1] == 0) continue;
            const int nx = u.x + c[0], ny = u.y + c[1];
            const int d = cheb(nx, ny, tx, ty);
            if (d < best_d && free_tile(s, m, nx, ny)) {
                best_d = d;
                best_x = nx;
                best_y = ny;
                break;
            }
        }
    } else {
        // Away: maximise the distance to the nearest opponent, then toward our spawn. Living units are gathered
        // once (opponents for the distance, everyone for occupancy) so each of the 9 tiles costs a short scan.
        int ox[kMaxUnits], oy[kMaxUnits], px[kMaxUnits], py[kMaxUnits], no = 0, np = 0;
        for (int j = 0; j < s.n; ++j) {
            const Unit& o = s.u[j];
            if (!o.alive()) continue;
            px[np] = o.x, py[np] = o.y, ++np;
            if (o.team != u.team) ox[no] = o.x, oy[no] = o.y, ++no;
        }
        const int hx = m.home_x[u.team], hy = m.home_y[u.team];
        auto score = [&](int x, int y) {
            int dmin = 99;
            for (int j = 0; j < no; ++j) dmin = std::min(dmin, cheb(x, y, ox[j], oy[j]));
            return dmin * 64 - cheb(x, y, hx, hy);
        };
        int best = score(u.x, u.y);
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                if (!dx && !dy) continue;
                const int nx = u.x + dx, ny = u.y + dy;
                if (m.is_blocked(nx, ny)) continue;
                bool taken = false;
                for (int j = 0; j < np; ++j) taken |= px[j] == nx && py[j] == ny;
                if (taken) continue;
                const int sc = score(nx, ny);
                if (sc > best) {
                    best = sc;
                    best_x = nx;
                    best_y = ny;
                }
            }
    }
    if (best_x == u.x && best_y == u.y) return false;
    u.x = static_cast<std::int16_t>(best_x);
    u.y = static_cast<std::int16_t>(best_y);
    return true;
}

// Fast policy for every unit after the root action: run when hurt and threatened, otherwise spell,
// close in and hit the opponent that dies fastest for what it is worth.
inline void act(State& s, const Map& m, int a, const Policy& p) {
    Unit& u = s.u[a];
    if (!u.alive() || (u.flags & U_ACTED)) return;
    u.flags |= U_ACTED;
    auto end_turn = [&] {
        for (auto& sp : u.sp)
            if (sp.cd > 0 && sp.cd < 99) --sp.cd;
        if (u.shield_turns > 0 && --u.shield_turns == 0) u.shield = 0;
    };
    if (u.stunned > 0) {
        u.stunned = 0;
        end_turn();
        return;
    }
    int near = -1, near_d = 99, tgt = -1;
    float best_key = 1e30f;
    for (int j = 0; j < s.n; ++j) {
        const Unit& o = s.u[j];
        if (!o.alive() || o.team == u.team) continue;
        const int d = cheb(u.x, u.y, o.x, o.y);
        if (d < near_d) near_d = d, near = j;
        const float key = (o.hp + o.shield) / std::max(u.str, 1.0f) / (1.0f + o.gems / 30.0f) + 2.0f * std::max(0, d - u.range);
        if (key < best_key) best_key = key, tgt = j;
    }
    if (near < 0) {
        end_turn();
        return;
    }
    // Flight (unit 0 follows the plan under comparison).
    const int mode = a == 0 && p.root_mode ? p.root_mode : p.team_mode[u.team];
    const bool run = mode == 2 ? near_d <= 6 : mode == 1 ? false : (u.hp < p.flee[u.team] * u.max_hp && near_d <= 4);
    if (run) {
        for (int k = 0; k < 2; ++k) {
            SSpell& sp = u.sp[k];
            if (sp.cd != 0) continue;
            if ((sp.kind == K_CC || (sp.kind == K_DAMAGE && (sp.flags & SF_STUN))) && !(sp.flags & SF_EXCL_WALK) &&
                near_d <= sp.range) {
                cast(s, a, k, near, 0, 0);
                break;
            }
            if (sp.kind == K_SHIELD) {
                cast(s, a, k, -1, 0, 0);
                break;
            }
            if (sp.kind == K_DASH) {
                // Two steps away.
                step(s, m, a, 0, 0, false);
                step(s, m, a, 0, 0, false);
                sp.cd = 8;
                break;
            }
        }
        step(s, m, a, 0, 0, false);
        end_turn();
        return;
    }
    if (mode == 1 && p.focus[u.team] >= 0 && s.u[p.focus[u.team]].alive() && cheb(u.x, u.y, s.u[p.focus[u.team]].x, s.u[p.focus[u.team]].y) <= u.range + 4)
        tgt = p.focus[u.team]; // the team fights as one: everyone on the focus target
    Unit& t = s.u[tgt];
    if (p.aggr[u.team] > 0.0f && mode != 1) {
        // Engage only when winning the race; otherwise retreat if threatened, hold if not.
        float incoming = 0;
        for (int j = 0; j < s.n; ++j) {
            const Unit& o = s.u[j];
            if (!o.alive() || o.team == u.team || o.stunned) continue;
            if (cheb(u.x, u.y, o.x, o.y) <= o.range + 2) incoming += o.str;
        }
        const float ttk = (t.hp + t.shield) / std::max(u.str, 1.0f);
        const float ttd = incoming > 0 ? (u.hp + u.shield) / incoming : 1e9f;
        if (!(ttk < std::min(ttd, 8.0f) * p.aggr[u.team])) {
            const int dt0 = cheb(u.x, u.y, t.x, t.y);
            if (incoming > 0 && ttd < 6.0f) step(s, m, a, 0, 0, false);
            else if (dt0 <= u.range) basic_attack(s, a, tgt); // in reach anyway: take the free hit
            end_turn();
            return;
        }
    }
    bool walked_ban = false;
    // Spells first: damage and stuns on the target, heals on a hurt ally, shields when threatened.
    for (int k = 0; k < 2; ++k) {
        SSpell& sp = u.sp[k];
        if (sp.cd != 0) continue;
        const int dt = cheb(u.x, u.y, t.x, t.y);
        if ((sp.kind == K_DAMAGE || sp.kind == K_CC) && dt <= sp.range && (sp.value > 0 || (sp.flags & SF_STUN))) {
            if (cast(s, a, k, tgt, 0, 0)) {
                walked_ban = (sp.flags & SF_EXCL_WALK) != 0;
                break;
            }
        } else if (sp.kind == K_AOE && dt <= sp.range) {
            if (cast(s, a, k, -1, t.x, t.y)) {
                walked_ban = (sp.flags & SF_EXCL_WALK) != 0;
                break;
            }
        } else if (sp.kind == K_HEAL) {
            int best = -1;
            float miss = sp.value * 0.5f;
            for (int j = 0; j < s.n; ++j) {
                const Unit& o = s.u[j];
                if (j == a || !o.alive() || o.team != u.team || cheb(u.x, u.y, o.x, o.y) > sp.range) continue;
                if (o.max_hp - o.hp > miss) miss = o.max_hp - o.hp, best = j;
            }
            if (best >= 0 && cast(s, a, k, best, 0, 0)) break;
        } else if (sp.kind == K_SHIELD && near_d <= 2) {
            if (cast(s, a, k, -1, 0, 0)) break;
        }
    }
    if (!t.alive()) {
        end_turn();
        return;
    }
    const bool ranged = u.range > 1;
    int dt = cheb(u.x, u.y, t.x, t.y);
    if (ranged) {
        if (dt <= u.range && !walked_ban) basic_attack(s, a, tgt);
        else if (!walked_ban) step(s, m, a, t.x, t.y, true);
    } else {
        if (dt > 1 && !walked_ban) {
            // Dash to close a gap of three or more.
            for (int k = 0; k < 2 && dt >= 3; ++k)
                if (u.sp[k].kind == K_DASH && u.sp[k].cd == 0) {
                    step(s, m, a, t.x, t.y, true);
                    step(s, m, a, t.x, t.y, true);
                    u.sp[k].cd = 8;
                    break;
                }
            step(s, m, a, t.x, t.y, true);
            dt = cheb(u.x, u.y, t.x, t.y);
        }
        if (dt <= 1 && t.alive()) {
            basic_attack(s, a, tgt);
            for (int k = 0; k < 2; ++k)
                if (p.use_reset && u.sp[k].kind == K_RESET && u.sp[k].cd == 0 && t.alive()) {
                    cast(s, a, k, -1, 0, 0);
                    basic_attack(s, a, tgt);
                    break;
                }
        }
    }
    end_turn();
}

inline void camps_act(State& s) {
    for (int c = 0; c < s.nc; ++c) {
        const Camp& cp = s.camps[c];
        int best = -1, bd = 99;
        for (int i = 0; i < s.n; ++i) {
            if (!s.u[i].alive()) continue;
            const int d = cheb(cp.x, cp.y, s.u[i].x, s.u[i].y);
            if (d <= cp.range && d < bd) bd = d, best = i;
        }
        if (best >= 0) deal(s, -1, best, cp.str, false);
    }
}

// The rest of this round (units that have not acted, enemies first), then `rounds` full rounds (camps act at
// each new round's start).
inline void rollout(State& s, const Map& m, const Policy& p, int rounds) {
    for (int r = 0; r <= rounds; ++r) {
        if (r > 0) {
            for (int i = 0; i < s.n; ++i) s.u[i].flags &= static_cast<std::uint8_t>(~U_ACTED);
            camps_act(s);
        }
        int ia = 0, ib = 0;
        for (int turn = 0; turn < 2 * kMaxUnits; ++turn) {
            const int lead = (p.first + r) & 1; // GameRules: the starting team swaps every round
            const int team = turn & 1 ? 1 - lead : lead;
            int& idx = team == 1 ? ib : ia;
            while (idx < s.n && (s.u[idx].team != team || !s.u[idx].alive() || (s.u[idx].flags & U_ACTED))) ++idx;
            if (idx < s.n) act(s, m, idx++, p);
            if (ia >= s.n && ib >= s.n) break;
        }
    }
}

// Value for team 0 in gem units: lives, health and carried gems, minus the enemy's; a unit that the
// opponents can kill next turn counts `threat` of its life and bag as lost.
inline float evaluate(const State& s, const Weights& w) {
    // Threat sources gathered once per team: living, unstunned units with their basic reach and first ready
    // offensive spell (the pair loop below then costs a few instructions per opposing pair).
    struct Src {
        int x, y, reach, sreach[2];
        float str, sval[2];
    };
    Src src[2][kMaxUnits];
    int ns[2] = {0, 0};
    for (int j = 0; j < s.n; ++j) {
        const Unit& o = s.u[j];
        if (!o.alive() || o.stunned) continue;
        Src& r = src[o.team & 1][ns[o.team & 1]++];
        r.x = o.x, r.y = o.y, r.reach = o.range + 1, r.str = o.str;
        for (int k = 0; k < 2; ++k) {
            const SSpell& sp = o.sp[k];
            const bool ready = sp.cd == 0 && (sp.kind == K_DAMAGE || sp.kind == K_CC || sp.kind == K_AOE);
            r.sreach[k] = ready ? sp.range + 1 : -1;
            r.sval[k] = sp.value;
        }
    }
    float v = 0;
    for (int i = 0; i < s.n; ++i) {
        const Unit& u = s.u[i];
        if (!u.alive()) continue;
        // NOVA_QUARTIC (0 = off): OpenAI Five's health utility (x + 1 - (1 - x)^4) / 2 (paper Table 6): chip
        // damage on a healthy unit is cheap, the last third of its health is expensive.
        float hpv = u.hp / u.max_hp;
        if (NOVA_QUARTIC) {
            const float x = hpv < 0.0f ? 0.0f : (hpv > 1.0f ? 1.0f : hpv);
            const float o = 1.0f - x;
            hpv = 0.5f * (x + 1.0f - o * o * o * o);
        }
        float val = w.alive + w.hp * hpv + w.gem * u.gems;
        float incoming = 0;
        const int ot = 1 - (u.team & 1);
        for (int j = 0; j < ns[ot]; ++j) {
            const Src& o = src[ot][j];
            const int d = cheb(u.x, u.y, o.x, o.y);
            if (d <= o.reach) incoming += o.str;
            // The first ready offensive spell that reaches (in slot order), as before.
            if (d <= o.sreach[0]) incoming += o.sval[0];
            else if (d <= o.sreach[1]) incoming += o.sval[1];
        }
        // Risk of dying next turn, smooth in the share of our health the opponents can take.
        const float share = std::min(1.0f, incoming / std::max(u.hp + u.shield, 1.0f));
        val -= w.threat * (w.alive + w.gem * u.gems) * share * share;
        v += u.team == 0 ? val : -val;
    }
    return v;
}

// Applies the root action for unit 0. Returns false when it is illegal in the model.
inline bool apply_root(State& s, const Map& m, const Action& act) {
    Unit& me = s.u[0];
    me.flags |= U_ACTED;
    if (act.pre_slot >= 0 && !cast(s, 0, act.pre_slot, act.pre_target, act.pre_x, act.pre_y)) return false;
    if (act.dash_slot >= 0) {
        if (me.sp[act.dash_slot].cd != 0 || !free_tile(s, m, act.dash_x, act.dash_y)) return false;
        me.x = act.dash_x;
        me.y = act.dash_y;
        me.sp[act.dash_slot].cd = 8;
    }
    if (act.move_x >= 0) {
        if (cheb(me.x, me.y, act.move_x, act.move_y) != 1 || !free_tile(s, m, act.move_x, act.move_y)) return false;
        me.x = act.move_x;
        me.y = act.move_y;
    }
    if (act.attack >= 0) {
        const Unit& t = s.u[act.attack];
        if (!t.alive() || cheb(me.x, me.y, t.x, t.y) > me.range) return false;
        basic_attack(s, 0, act.attack);
        if (act.reset_slot >= 0 && s.u[act.attack].alive() && cast(s, 0, act.reset_slot, -1, 0, 0)) basic_attack(s, 0, act.attack);
    }
    for (auto& sp : me.sp)
        if (sp.cd > 0 && sp.cd < 99) --sp.cd;
    if (me.shield_turns > 0 && --me.shield_turns == 0) me.shield = 0;
    return true;
}

// Root candidates for unit 0: every legal step (and staying) with the best hit from there, the spells
// that fit, and dashes that flee or engage. Returns the count.
inline int gen_root(const State& s, const Map& m, Action* out, int cap) {
    const Unit& me = s.u[0];
    const bool ranged = me.range > 1;
    int n = 0;
    // Best opponent to hit from a tile (lowest effective health within basic range).
    auto best_hit = [&](int x, int y) {
        int best = -1;
        float bh = 1e30f;
        for (int j = 1; j < s.n; ++j) {
            const Unit& o = s.u[j];
            if (!o.alive() || o.team == me.team || cheb(x, y, o.x, o.y) > me.range) continue;
            const float h = (o.hp + o.shield) / (1.0f + o.gems / 30.0f);
            if (h < bh) bh = h, best = j;
        }
        return best;
    };
    int reset = -1, dash = -1, spell = -1, spell_target = -1;
    int ax = -1, ay = -1;
    bool spell_excl = false;
    for (int k = 0; k < 2; ++k) {
        const SSpell& sp = me.sp[k];
        if (sp.cd != 0) continue;
        if (sp.kind == K_RESET) reset = k;
        else if (sp.kind == K_DASH) dash = k;
    }
    // The best offensive spell from where we stand: the target it hurts most for what it is worth.
    for (int k = 0; k < 2; ++k) {
        const SSpell& sp = me.sp[k];
        if (sp.cd != 0 || !(sp.kind == K_DAMAGE || sp.kind == K_CC || sp.kind == K_AOE)) continue;
        float best = -1;
        for (int j = 1; j < s.n; ++j) {
            const Unit& o = s.u[j];
            if (!o.alive() || o.team == me.team || cheb(me.x, me.y, o.x, o.y) > sp.range) continue;
            const float v = std::min(sp.value, o.hp + o.shield) + ((sp.flags & SF_STUN) ? o.str * 2.0f : 0.0f) + o.gems;
            if (v > best) best = v, spell = k, spell_target = j, ax = o.x, ay = o.y, spell_excl = (sp.flags & SF_EXCL_WALK) != 0;
        }
    }
    // Self-only support (shield) is cast whenever it is ready.
    int shield = -1;
    for (int k = 0; k < 2; ++k)
        if (me.sp[k].cd == 0 && me.sp[k].kind == K_SHIELD) shield = k;
    auto add = [&](Action a) {
        if (n < cap) out[n++] = a;
    };
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            const bool stay = !dx && !dy;
            const int x = me.x + dx, y = me.y + dy;
            if (!stay && !free_tile(s, m, x, y)) continue;
            Action a;
            if (!stay) a.move_x = static_cast<std::int16_t>(x), a.move_y = static_cast<std::int16_t>(y);
            if (spell >= 0 && (stay || !spell_excl)) {
                a.pre_slot = static_cast<std::int8_t>(spell);
                a.pre_target = static_cast<std::int8_t>(spell_target);
                a.pre_x = static_cast<std::int16_t>(ax);
                a.pre_y = static_cast<std::int16_t>(ay);
            } else if (shield >= 0) {
                a.pre_slot = static_cast<std::int8_t>(shield);
            }
            if (!ranged || stay) {
                const int t = best_hit(x, y);
                if (t >= 0) {
                    a.attack = static_cast<std::int8_t>(t);
                    if (reset >= 0 && a.pre_slot < 0) a.reset_slot = static_cast<std::int8_t>(reset);
                }
            }
            add(a);
            // The same step without the spell (keep it for later).
            if (a.pre_slot >= 0) {
                Action b = a;
                b.pre_slot = -1;
                b.pre_target = -1;
                if (b.attack >= 0 && reset >= 0) b.reset_slot = static_cast<std::int8_t>(reset);
                add(b);
            }
        }
    if (dash >= 0) {
        // Dash landings two tiles out: the two that flee furthest and the one closest to a target.
        int esc[2] = {-1, -1}, esc_sc[2] = {-1 << 30, -1 << 30}, eng = -1, eng_d = 99;
        int lx[16], ly[16], nl = 0;
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy) {
                if (std::max(std::abs(dx), std::abs(dy)) != 2) continue;
                const int x = me.x + dx, y = me.y + dy;
                if (!free_tile(s, m, x, y) || m.is_blocked(me.x + sgn(dx), me.y + sgn(dy))) continue;
                int dmin = 99;
                for (int j = 1; j < s.n; ++j)
                    if (s.u[j].alive() && s.u[j].team != me.team) dmin = std::min(dmin, cheb(x, y, s.u[j].x, s.u[j].y));
                const int sc = dmin * 64 - cheb(x, y, m.home_x[0], m.home_y[0]);
                lx[nl] = x, ly[nl] = y;
                if (sc > esc_sc[0]) esc_sc[1] = esc_sc[0], esc[1] = esc[0], esc_sc[0] = sc, esc[0] = nl;
                else if (sc > esc_sc[1]) esc_sc[1] = sc, esc[1] = nl;
                if (dmin < eng_d && (!ranged || dmin >= 2)) eng_d = dmin, eng = nl;
                ++nl;
            }
        for (int k = 0; k < 3; ++k) {
            const int li = k < 2 ? esc[k] : eng;
            if (li < 0 || (k == 2 && (li == esc[0] || li == esc[1]))) continue;
            Action a;
            a.dash_slot = static_cast<std::int8_t>(dash);
            a.dash_x = static_cast<std::int16_t>(lx[li]);
            a.dash_y = static_cast<std::int16_t>(ly[li]);
            if (!ranged) {
                const int t = best_hit(lx[li], ly[li]);
                if (t >= 0) a.attack = static_cast<std::int8_t>(t);
            }
            add(a);
        }
    }
    return n;
}

} // namespace ts
