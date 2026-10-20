// Tactical search, bot side: snapshot the local fight into a ts::State, play every root candidate forward
// against two enemy models while fuel lasts, and carry out the best one (APEX_TSEARCH).
#include "bot.h"
#include "tsearch.h"
#include "local_model.h"
#include "runtime_budget.h"

#include <variant>

namespace d = cambc::detail;

namespace {
// Champion kits from the immutable designs, cached per type.
struct KitSpell {
    std::int8_t kind = 0, range = 0, radius = 0;
    std::uint8_t flags = 0;
    float mod = 0;
};
struct Kit {
    bool loaded = false;
    KitSpell sp[2];
};

const Kit& kit_of(bc::champion_type type) {
    static std::array<Kit, 12> table{};
    static Kit none;
    const int t = static_cast<int>(type);
    if (t < 0 || t >= 12) return none;
    Kit& k = table[t];
    if (k.loaded) return k;
    k.loaded = true;
    try {
        bc::champion design{type};
        const auto abilities = design.abilities();
        for (int s = 0; s < 2; ++s) {
            const auto& info = abilities[s].info();
            if (!info) continue;
            KitSpell& ks = k.sp[s];
            std::visit(
                [&](const auto& a) {
                    using A = std::decay_t<decltype(a)>;
                    ks.kind = static_cast<std::int8_t>(A::kind);
                    if (a.exclusive_with_movement) ks.flags |= ts::SF_EXCL_WALK;
                    if (a.valid_targets.contains(bc::ability_target_mask::enemies)) ks.flags |= ts::SF_ENEMY;
                    if (a.valid_targets.contains(bc::ability_target_mask::allies)) ks.flags |= ts::SF_ALLY;
                    if constexpr (requires { a.range; }) ks.range = static_cast<std::int8_t>(a.range);
                    if constexpr (requires { a.modifier; }) ks.mod = a.modifier;
                    if constexpr (requires { a.radius; }) ks.radius = static_cast<std::int8_t>(a.radius);
                },
                *info);
            if (ks.kind == ts::K_CC) ks.flags |= ts::SF_STUN;
            if (ks.kind == ts::K_AOE && type == bc::champion_type::ryth) ks.flags |= ts::SF_STUN; // Ryth's area stun
            if (ks.kind == ts::K_RESET && type == bc::champion_type::marcella) ks.flags |= ts::SF_ENH;
            if (ks.kind == ts::K_DASH && ks.range <= 0) ks.range = 2;
        }
    } catch (...) {
    }
    return k;
}

// Fills a unit from live numbers; cooldowns we cannot read count as ready (the cautious guess).
void fill_unit(ts::Unit& u, int team, bc::champion_type type, vec2 p, float hp, float max_hp, float str, float focus,
    int range, int gems, std::uint32_t handle, bool acted, const int* known_cd) {
    u = ts::Unit{};
    u.team = static_cast<std::int8_t>(team);
    u.x = static_cast<std::int16_t>(p.x);
    u.y = static_cast<std::int16_t>(p.y);
    u.hp = hp;
    u.max_hp = std::max(max_hp, 1.0f);
    u.str = str;
    u.range = static_cast<std::int8_t>(range);
    u.gems = static_cast<std::int16_t>(gems);
    u.flags = ts::U_ALIVE | (acted ? ts::U_ACTED : 0);
    if (type == bc::champion_type::varg) u.flags |= ts::U_SPLASH;
    if (type == bc::champion_type::marcella) u.flags |= ts::U_STUN_IMMUNE;
    if (type == bc::champion_type::vendetta) u.flags |= ts::U_RESET_ON_KILL;
    if (type == bc::champion_type::ren) u.retaliate = 0.2f * focus;
    const Kit& k = kit_of(type);
    for (int s = 0; s < 2; ++s) {
        const KitSpell& ks = k.sp[s];
        ts::SSpell& sp = u.sp[s];
        sp.kind = ks.kind;
        sp.range = ks.range;
        sp.radius = ks.radius;
        sp.flags = ks.flags;
        sp.value = ks.mod * focus;
        int cd = 0;
        if (known_cd) {
            cd = known_cd[s];
        } else {
            std::int32_t c = 0;
            if (d::sdk_champion_cooldown(handle, static_cast<std::uint32_t>(s), &c) == 0) cd = c;
        }
        sp.cd = static_cast<std::int8_t>(std::clamp(cd, 0, 99));
    }
    // Live shield and stun.
    d::buff_data buffs[6];
    std::uint32_t nb = 0;
    if (d::sdk_champion_buff_data(handle, buffs, 6, &nb) == 0)
        for (std::uint32_t i = 0; i < nb && i < 6; ++i) {
            if (buffs[i].type == 0) {
                u.shield = buffs[i].data.shield.value;
                u.shield_turns = static_cast<std::int8_t>(std::clamp(buffs[i].data.shield.duration, 0, 9));
            } else if (buffs[i].type == 1 && buffs[i].data.stun.duration > 0) {
                u.stunned = 1;
            } else if (buffs[i].type == 2) {
                u.flags |= ts::U_ENHANCED;
            }
        }
}
} // namespace

// Loads one champion kit per call (designs cost fuel to read): run on calm turns so fights never pay it.
void Bot::ts_prewarm() {
    runtime::Probe probe(runtime::Stage::Prewarm);
    static int next = 0;
    if (next >= 12 || bc::fuel_remaining() < 60000) return;
    kit_of(static_cast<bc::champion_type>(next++));
}

// The local fight as a ts::State: us (unit 0), allies within 7, enemies within 8; false when no enemy is in range.
bool Bot::ts_snapshot(ts::State& s, ts::Map& m, const EnemyRec** eref) {
    runtime::Probe probe(runtime::Stage::Snapshot);
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision = ts::DecisionInfo{};
#endif
    m.blocked = w.static_blocked.ptr();
    m.w = w.width;
    m.h = w.height;
    m.stride = static_cast<int>(bc::detail::layout().chunk_stride);
    if (ts_home[0].x < 0) {
        auto centre = [](const bc::bitboard& b) {
            long sx = 0, sy = 0, n = 0;
            for (vec2 p : b.positions()) sx += p.x, sy += p.y, ++n;
            return n ? vec2{static_cast<int>(sx / n), static_cast<int>(sy / n)} : vec2{0, 0};
        };
        ts_home[0] = centre(w.ally_spawn);
        ts_home[1] = centre(w.enemy_spawn);
    }
    m.home_x[0] = static_cast<std::int16_t>(ts_home[0].x);
    m.home_y[0] = static_cast<std::int16_t>(ts_home[0].y);
    m.home_x[1] = static_cast<std::int16_t>(ts_home[1].x);
    m.home_y[1] = static_cast<std::int16_t>(ts_home[1].y);

    // Unit 0 is us; then allies within 7 and enemies within 8 (nearest first by insertion order).
    const int my_cd[2] = {cooldown[0], cooldown[1]};
    fill_unit(s.u[0], 0, type, pos, hp, max_hp, strength, focus, range, gems, my_id, false, my_cd);
#if NOVA_TS_SIM_FIXES
    s.u[0].cast_used = cast_done;
    s.u[0].walked = walked;
    s.u[0].move_left = bc::self().can_move();
    s.u[0].attack_left = bc::self().can_attack();
#endif
    s.n = 1;
    int ci_idx[2][2] = {{-1, -1}, {-1, -1}}; // [team][cassi, issac]
    auto note_ci = [&](int team, bc::champion_type t, int idx) {
        if (t == bc::champion_type::cassi) ci_idx[team][0] = idx;
        if (t == bc::champion_type::issac) ci_idx[team][1] = idx;
    };
    note_ci(0, type, 0);
#if NOVA_TS_RELEVANCE
    // Stable relevance selection; a linked observed partner enters as one dependency group.
    struct Candidate { const AllyRec* ally; const EnemyRec* enemy; std::uint32_t id; bc::champion_type type; int team, distance; float priority; bool chosen; };
    Candidate cs[24];
    int count = 0;
    auto add = [&](const AllyRec* a, const EnemyRec* e) {
        const vec2 p = a ? a->pos : e->pos;
        const int dd = cheb(p, pos), tm = a ? 0 : 1;
        const auto ct = a ? a->type : e->type;
        const bool linked = ct == bc::champion_type::cassi || ct == bc::champion_type::issac;
        if (dd > (tm ? 8 : 7) && !linked) return;
        if (dd <= (tm ? 8 : 7)) {
            if (tm) ++ts::last_decision.eligible_enemies;
            else ++ts::last_decision.eligible_allies;
        }
        if (count >= 24) { ts::last_decision.omitted_enemy |= tm != 0; return; }
        const float ehp = a ? a->hp : e->hp;
        const float power = a ? a->strength : e->strength;
        const float foc = a ? a->focus : e->focus;
        const int reach = a ? a->range : e->range;
        // Close reachable burst dominates; distance then vulnerability break broad threat bands.
        const float burst = power + foc;
        const float pr = (tm ? 1000.0f : 0.0f) + (dd <= reach + 2 ? 2000.0f + burst : 0.0f)
            + (dd <= range + 1 ? 400.0f : 0.0f) - 100.0f * dd + 100.0f / std::max(1.0f, ehp);
        cs[count++] = Candidate{a, e, a ? a->id : e->id, ct, tm, dd, pr, false};
    };
    ts::last_decision.eligible_allies = 1;
    for (const auto& a : w.allies) if (a.hp > 0) add(&a, nullptr);
    for (const auto& e : w.enemies) if (e.hp > 0) add(nullptr, &e);
    std::sort(cs, cs + count, [](const Candidate& a, const Candidate& b) {
        return a.priority != b.priority ? a.priority > b.priority : a.id < b.id;
    });
    const int cap = std::clamp(NOVA_TS_MAX_UNITS, 2, ts::kMaxUnits);
    ts::last_decision.selected[0] = my_id;
    auto partner = [&](int i) {
        const auto ct = cs[i].type;
        if (ct != bc::champion_type::cassi && ct != bc::champion_type::issac) return -2;
        const auto wanted = ct == bc::champion_type::cassi ? bc::champion_type::issac : bc::champion_type::cassi;
        if (cs[i].team == 0 && type == wanted) return -1; // self is already retained
        for (int j = 0; j < count; ++j) if (cs[j].team == cs[i].team && cs[j].type == wanted) return j;
        return -3; // hidden/outside observation: never fabricate a partner
    };
    auto take = [&](int i) {
        Candidate& c = cs[i];
        if (c.chosen || s.n >= cap) return;
        if (c.ally) {
            const auto& a = *c.ally;
            d::champion_info info{};
            if (d::sdk_champion(a.id, &info) != 0) return;
            std::uint32_t g = 0;
            d::sdk_champion_gems(a.id, &g);
            fill_unit(s.u[s.n], 0, a.type, a.pos, a.hp, a.max_hp, a.strength, a.focus, a.range, static_cast<int>(g), a.id,
                (static_cast<std::uint32_t>(info.state) & (1u << 5)) != 0, nullptr);
        } else {
            const auto& e = *c.enemy;
            fill_unit(s.u[s.n], 1, e.type, e.pos, e.hp, e.max_hp, e.strength, e.focus, e.range, e.gems, e.id,
                (e.state & (1u << 5)) != 0, nullptr);
            if (e.tributing() || e.recalling()) s.u[s.n].flags |= ts::U_ACTED;
            eref[s.n] = &e;
        }
        note_ci(c.team, c.type, s.n);
        ts::last_decision.selected[s.n] = c.id;
        ++s.n;
        c.chosen = true;
    };
    // Self's observed linked body has first claim on a slot, even beyond the usual radius.
    if (type == bc::champion_type::cassi || type == bc::champion_type::issac) {
        bool found = false;
        for (int i = 0; i < count; ++i) if (partner(i) == -1) { take(i); found = cs[i].chosen; }
        ts::last_decision.incomplete_link |= !found;
    }
    for (int i = 0; i < count; ++i) {
        if (cs[i].chosen || cs[i].distance > (cs[i].team ? 8 : 7)) continue;
        const int p = partner(i);
        const int need = 1 + (p >= 0 && !cs[p].chosen ? 1 : 0);
        if (s.n + need > cap) continue;
        take(i);
        if (p >= 0) take(p);
        if (cs[i].chosen && (p == -3 || (p >= 0 && !cs[p].chosen))) ts::last_decision.incomplete_link = true;
    }
    int enemies = 0, near_d = 99;
    for (int i = 0; i < count; ++i) {
        const Candidate& c = cs[i];
        if (!c.chosen && c.distance <= (c.team ? 8 : 7)) {
            ++ts::last_decision.dropped;
            ts::last_decision.omitted_enemy |= c.team != 0;
#if NOVA_RUNTIME_TELEMETRY
            std::printf("TSDROP r%d id%u team%d d%d reasoncapacity\n", round, c.id, c.team, c.distance);
#endif
        }
        if (c.chosen && c.team) { ++enemies; near_d = std::min(near_d, c.distance); }
    }
    m.observed_occupancy = w.unit_tiles.ptr();
    m.modeled = s.n;
    for (int i = 0; i < s.n; ++i) { m.origin_x[i] = s.u[i].x; m.origin_y[i] = s.u[i].y; }
#else
    for (const auto& a : w.allies) {
        if (s.n >= 4 || cheb(a.pos, pos) > 7) continue;
        d::champion_info info{};
        if (d::sdk_champion(a.id, &info) != 0) continue;
        std::uint32_t g = 0;
        d::sdk_champion_gems(a.id, &g);
        const bool acted = (static_cast<std::uint32_t>(info.state) & (1u << 5)) != 0;
        fill_unit(s.u[s.n], 0, a.type, a.pos, a.hp, a.max_hp, a.strength, a.focus, a.range, static_cast<int>(g), a.id, acted, nullptr);
        note_ci(0, a.type, s.n);
        ++s.n;
    }
    int enemies = 0, near_d = 99;
    for (const auto& e : w.enemies) {
        if (s.n >= ts::kMaxUnits) break;
        const int dd = cheb(e.pos, pos);
        if (dd > 8 || e.hp <= 0) continue;
        near_d = std::min(near_d, dd);
        const bool acted = (e.state & (1u << 5)) != 0;
        fill_unit(s.u[s.n], 1, e.type, e.pos, e.hp, e.max_hp, e.strength, e.focus, e.range, e.gems, e.id, acted, nullptr);
        if (e.tributing() || e.recalling()) s.u[s.n].flags |= ts::U_ACTED; // channelling: it will not fight back now
        note_ci(1, e.type, s.n);
        eref[s.n] = &e;
        ++s.n;
        ++enemies;
    }
#endif
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision.retained = s.n;
    if (!NOVA_TS_RELEVANCE) {
        ts::last_decision.eligible_allies = 1;
        for (const auto& a : w.allies) ts::last_decision.eligible_allies += a.hp > 0 && cheb(a.pos, pos) <= 7;
        for (const auto& e : w.enemies) ts::last_decision.eligible_enemies += e.hp > 0 && cheb(e.pos, pos) <= 8;
        ts::last_decision.omitted_enemy = ts::last_decision.eligible_enemies > enemies;
        ts::last_decision.dropped = ts::last_decision.eligible_allies + ts::last_decision.eligible_enemies - s.n;
        ts::last_decision.selected[0] = my_id;
        for (int i = 1; i < s.n; ++i) if (eref[i]) ts::last_decision.selected[i] = eref[i]->id;
        ts::last_decision.incomplete_link = (ci_idx[0][0] >= 0) != (ci_idx[0][1] >= 0)
            || (ci_idx[1][0] >= 0) != (ci_idx[1][1] >= 0);
    }
    // Conservative risk guard: a nearby dangerous group may wake during the comparison.
    for (const auto& c : w.camps)
        if (!c.dead && c.hp > 0 && c.strength > 0 && cheb(c.pos, pos) <= c.range + 2) ts::last_decision.camp_risk = true;
#endif
    if (!enemies || near_d > tune::kTsRange) return false;
    s.nc = 0;
    if (tune::kTsCamps)
        for (const auto& g : w.groups) {
            if (g.state != GroupState::alive) continue;
            for (int ci : g.camps) {
                const CampRec& c = w.camps[ci];
                if (c.dead || c.hp <= 0 || c.strength <= 0 || cheb(c.pos, pos) > 7 || s.nc >= ts::kMaxCamps) continue;
                s.camps[s.nc++] = ts::Camp{c.strength, static_cast<std::int16_t>(c.pos.x), static_cast<std::int16_t>(c.pos.y),
                    static_cast<std::int8_t>(c.range)};
            }
        }
    for (int t = 0; t < 2; ++t)
        if (ci_idx[t][0] >= 0 && ci_idx[t][1] >= 0) {
            s.u[ci_idx[t][0]].link = static_cast<std::int8_t>(ci_idx[t][1]);
            s.u[ci_idx[t][1]].link = static_cast<std::int8_t>(ci_idx[t][0]);
        }
    return true;
}

bool Bot::tactical_search() {
    PROF(tsearch);
    runtime::Probe probe(runtime::Stage::ActionSearch);
    const auto fuel0 = bc::fuel_remaining();
    if (fuel0 < static_cast<std::uint64_t>(tune::kTsReserve) + 12000) return false;
    ts::State s;
    ts::Map m;
    const EnemyRec* eref[ts::kMaxUnits] = {};
    if (!ts_snapshot(s, m, eref)) return false;
#if NOVA_TS_RELEVANCE
    if (!ts::last_decision.allow_fight_override()) return false;
#endif

    ts::Action cands[24];
    const int nc = ts::gen_root(s, m, cands, 24);
    if (nc < 2) return false;
    // Most promising first (fuel may only cover a few): fight from here, then the steps that hit, the
    // flight steps (furthest from the nearest enemy), and the rest.
    {
        const ts::Unit& me0 = s.u[0];
        int ne = -1, nd = 99;
        for (int j = 1; j < s.n; ++j)
            if (s.u[j].team == 1 && ts::cheb(me0.x, me0.y, s.u[j].x, s.u[j].y) < nd) nd = ts::cheb(me0.x, me0.y, s.u[j].x, s.u[j].y), ne = j;
        for (int i = 0; i < nc; ++i) {
            ts::Action& a = cands[i];
            const int x = a.dash_slot >= 0 ? a.dash_x : a.move_x >= 0 ? a.move_x : me0.x;
            const int y = a.dash_slot >= 0 ? a.dash_y : a.move_x >= 0 ? a.move_y : me0.y;
            const bool stay = a.dash_slot < 0 && a.move_x < 0;
            float pr = 0;
            if (a.attack >= 0) pr += 4;
            if (a.pre_slot >= 0) pr += 3;
            if (stay) pr += 2;
            if (ne >= 0) pr += 0.5f * static_cast<float>(ts::cheb(x, y, s.u[ne].x, s.u[ne].y) - nd); // flight steps rank next
            a.prior = pr;
        }
        std::sort(cands, cands + nc, [](const ts::Action& p, const ts::Action& q) { return p.prior > q.prior; });
        // Keep the best flight step near the front too.
        int flee = -1, flee_d = -1;
        for (int i = 0; i < nc; ++i) {
            const ts::Action& a = cands[i];
            if (a.attack >= 0 || ne < 0) continue;
            const int x = a.dash_slot >= 0 ? a.dash_x : a.move_x >= 0 ? a.move_x : me0.x;
            const int y = a.dash_slot >= 0 ? a.dash_y : a.move_x >= 0 ? a.move_y : me0.y;
            const int dd = ts::cheb(x, y, s.u[ne].x, s.u[ne].y);
            if (dd > flee_d) flee_d = dd, flee = i;
        }
        if (flee > 2) std::rotate(cands + 2, cands + flee, cands + flee + 1);
    }
    ts::Weights wts{tune::kTsAlive, tune::kTsHp, tune::kTsThreat};
    if (tune::kTsLearned) {
        // Learned exchange rates (tools/exchange.py on 311k top-team positions): a life is worth ~8 gems at
        // the start and ~25 by round 450, carried gems ~0.75 of banked, health in itself little.
        const float t = std::min(1.0f, static_cast<float>(round) / 450.0f);
        wts.alive = 8.0f + 17.0f * t;
        wts.hp = 3.0f;
        wts.gem = 0.75f;
    }
    ts::Policy models[2];
    models[0].flee[0] = models[1].flee[0] = tune::kTsAllyFlee;
    models[0].flee[1] = tune::kTsFleeA;
    models[1].flee[1] = tune::kTsFleeC;
    if (tune::kOppModel) {
        // This opponent's learned back-off health, bracketed by the two models.
        const float est = opp_flee_estimate();
        if (est >= 0.0f) {
            models[0].flee[1] = std::max(0.02f, est - 0.1f);
            models[1].flee[1] = std::min(0.8f, est + 0.1f);
        }
    }
    float pass_v[2][24] = {};
    int passes[24] = {};
    bool legal[24];
    for (int i = 0; i < nc; ++i) legal[i] = true;
    // Stage 1: every candidate's position right after our action (cheap), to order the rollouts.
    {
        float stat[24];
        for (int i = 0; i < nc; ++i) {
            ts::State c = s;
            if (!ts::apply_root(c, m, cands[i])) {
                legal[i] = false;
                stat[i] = -1e30f;
                continue;
            }
            stat[i] = ts::evaluate(c, wts);
        }
        int order[24];
        for (int i = 0; i < nc; ++i) order[i] = i;
        std::sort(order, order + nc, [&](int p, int q) { return stat[p] > stat[q]; });
        ts::Action sorted[24];
        bool lsorted[24];
        for (int i = 0; i < nc; ++i) sorted[i] = cands[order[i]], lsorted[i] = legal[order[i]];
        for (int i = 0; i < nc; ++i) cands[i] = sorted[i], legal[i] = lsorted[i];
    }
    int rollouts = 0;
    std::uint64_t worst = 4000; // most expensive rollout so far (a floor before the first one)
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < nc; ++i) {
            if (!legal[i]) continue;
            // Never let the search run the turn out of fuel: a suspended turn loses the action.
            const std::uint64_t left = bc::fuel_remaining();
            if (left < static_cast<std::uint64_t>(tune::kTsReserve) + 2 * worst) goto done;
            ts::State c = s;
            if (!ts::apply_root(c, m, cands[i])) {
                legal[i] = false;
                continue;
            }
            ts::rollout(c, m, models[pass], tune::kTsRounds);
            pass_v[pass][i] = ts::evaluate(c, wts);
            ++passes[i];
            ++rollouts;
            const std::uint64_t now = bc::fuel_remaining();
            if (now < left) worst = std::max(worst, left - now);
        }
done:
    // Compare like with like: both models' mean when every candidate has both, else the first model only
    // (over the candidates it reached).
    bool all_two = true;
    for (int i = 0; i < nc; ++i)
        if (legal[i] && passes[i] < 2) all_two = false;
    int best = -1, evaluated = 0;
    float best_v = -1e30f;
    for (int i = 0; i < nc; ++i) {
        if (!legal[i] || passes[i] == 0) continue;
        ++evaluated;
        const float v = all_two ? 0.5f * (pass_v[0][i] + pass_v[1][i]) : pass_v[0][i];
        if (v > best_v) best_v = v, best = i;
    }
    if (best < 0 || evaluated < 2) return false;
#ifdef APEX_TSLOG
    if (round % 7 == 0) {
        // Snapshot dump: what the model believes.
        for (int i = 0; i < s.n; ++i) {
            const ts::Unit& u = s.u[i];
            std::printf("TSU R%d i%d t%d p(%d,%d) hp%.0f/%.0f str%.0f rng%d g%d f%d stn%d sh%.0f sp0[k%d cd%d r%d v%.0f f%d] sp1[k%d cd%d r%d v%.0f f%d]\n",
                round, i, u.team, u.x, u.y, u.hp, u.max_hp, u.str, u.range, u.gems, u.flags, u.stunned, u.shield, u.sp[0].kind,
                u.sp[0].cd, u.sp[0].range, u.sp[0].value, u.sp[0].flags, u.sp[1].kind, u.sp[1].cd, u.sp[1].range, u.sp[1].value,
                u.sp[1].flags);
        }
        for (int i = 0; i < nc; ++i)
            std::printf("TSC R%d c%d pre%d move(%d,%d) dash(%d,%d) atk%d passes%d v0 %.1f v1 %.1f%s\n", round, i, cands[i].pre_slot,
                cands[i].move_x, cands[i].move_y, cands[i].dash_x, cands[i].dash_y, cands[i].attack, passes[i], pass_v[0][i],
                pass_v[1][i], i == best ? " <== best" : "");
    }
    {
        const auto now = bc::fuel_remaining();
        std::printf("TS R%d %s units %d cands %d rollouts %d fuel %lld worst %llu both %d best %.1f act pre%d dash%d move%d atk%d\n",
            round, bc::to_string(type).c_str(), s.n, nc, rollouts, static_cast<long long>(fuel0) - static_cast<long long>(now),
            static_cast<unsigned long long>(worst), all_two ? 1 : 0, best_v, cands[best].pre_slot, cands[best].dash_slot,
            cands[best].move_x >= 0 ? 1 : 0, cands[best].attack);
    }
#endif
    (void)rollouts;
#ifdef APEX_TSTRACE
    {
        // Compact record of this search, printed if we die soon after (play_turn).
        char buf[900];
        int len = std::snprintf(buf, sizeof(buf), "TRACE R%d me(%d,%d) hp%.0f/%.0f g%d |", round, pos.x, pos.y, hp, max_hp, gems);
        for (int i = 1; i < s.n && len < 600; ++i) {
            const ts::Unit& u = s.u[i];
            len += std::snprintf(buf + len, sizeof(buf) - len, " %c(%d,%d)hp%.0f s%.0f r%d%s%s", u.team ? 'E' : 'A', u.x, u.y, u.hp, u.str, u.range,
                (u.flags & ts::U_ACTED) ? "*" : "", u.stunned ? "z" : "");
        }
        len += std::snprintf(buf + len, sizeof(buf) - len, " || picked c%d of %d:", best, nc);
        for (int i = 0; i < nc && len < 860; ++i)
            len += std::snprintf(buf + len, sizeof(buf) - len, " [%s%s%s%s %.1f]", cands[i].move_x >= 0 ? "m" : "", cands[i].dash_slot >= 0 ? "d" : "",
                cands[i].attack >= 0 ? "a" : "", cands[i].pre_slot >= 0 ? "s" : "", passes[i] ? pass_v[0][i] : -999.0f);
        ts_trace[ts_trace_i & 1] = buf;
        ++ts_trace_i;
    }
#endif

    // Carry it out. Engine refusals (a hidden unit on the tile) leave the rest of the plan to run.
    const ts::Action& a = cands[best];
    auto& me = bc::self();
    if (a.pre_slot >= 0 && spell_ready(a.pre_slot)) {
        try {
            const auto ab = me.abilities()[a.pre_slot];
            const auto kind = spells[a.pre_slot].kind;
            bool ok = false;
            if (kind == bc::ability_kind::aoe_damage) ok = ab.use(vec2{a.pre_x, a.pre_y});
            else if (kind == bc::ability_kind::shield || kind == bc::ability_kind::attack_reset) ok = ab.use();
            else if (a.pre_target >= 0 && eref[a.pre_target]) ok = ab.use(eref[a.pre_target]->unit);
            if (ok) cast_done = true;
        } catch (const std::exception& ex) {
            LOG("ts cast error %s\n", ex.what());
        }
    }
    if (a.dash_slot >= 0 && spell_ready(a.dash_slot)) {
        try {
            if (me.abilities()[a.dash_slot].use(vec2{a.dash_x, a.dash_y})) {
                cast_done = true;
                pos = vec2{a.dash_x, a.dash_y};
            }
        } catch (const std::exception& ex) {
            LOG("ts dash error %s\n", ex.what());
        }
    }
    if (a.move_x >= 0 && me.can_move()) {
        try {
            if (me.move_absolute(vec2{a.move_x, a.move_y})) {
                pos = vec2{a.move_x, a.move_y};
                walked = true;
            }
        } catch (const std::exception& ex) {
            LOG("ts move error %s\n", ex.what());
        }
    }
    if (a.attack >= 0 && eref[a.attack]) {
        const EnemyRec& e = *eref[a.attack];
        if (attack_unit(e.unit, e.pos) && a.reset_slot >= 0) use_reset_and_attack(e.unit, e.pos);
    }
    return true;
}

// Opponent model: for every enemy seen this round and last round within 3 tiles of us, record whether it moved
// away (backed off) against its health fifth last round.
void Bot::update_opp_model() {
    std::vector<OppSeen> now;
    now.reserve(w.enemies.size());
    for (const auto& e : w.enemies) {
        const int dd = cheb(e.pos, pos);
        const float frac = e.hp / std::max(e.max_hp, 1.0f);
        for (const auto& o : opp_last)
            if (o.id == e.id && o.round == round - 1 && o.dist <= 3) {
                const int b = std::clamp(static_cast<int>(o.frac * 5.0f), 0, 4);
                ++opp_obs[b];
                opp_ret[b] += dd > o.dist;
            }
        now.push_back({e.id, e.pos, frac, dd, round});
    }
    opp_last.swap(now);
}

// The health fraction below which this opponent backs off: the threshold (in fifths) that best separates backing
// off from staying, with a prior pulling to 0.25. -1 until 12 observations below 80% health.
float Bot::opp_flee_estimate() const {
    // Above 80% health "moving away" is mostly repositioning or chasing someone else, so only wounded enemies count.
    int total = 0;
    for (int b = 0; b < 4; ++b) total += opp_obs[b];
    if (total < 12) return -1.0f;
    float best_t = 0.25f, best_s = -1e9f;
    for (int k = 0; k <= 4; ++k) { // threshold k / 5: below it they back off
        float s = 0;
        for (int b = 0; b < 4; ++b) s += b < k ? static_cast<float>(opp_ret[b]) : static_cast<float>(opp_obs[b] - opp_ret[b]);
        const float t = static_cast<float>(k) / 5.0f;
        s -= 3.0f * std::fabs(t - 0.25f); // prior
        if (s > best_s) best_s = s, best_t = t;
    }
    return std::clamp(best_t, 0.05f, 0.7f);
}

// Fight or run, by search (APEX_TS_DECIDE): play the next rounds forward twice, once with us fighting and once
// with us running (everyone else on the usual policy; both enemy models while fuel lasts), judge both end
// states with the learned exchange rates, and return +1 (fight), -1 (run) or 0 (no clear answer, or no fuel).
// The local-fight evaluator's 22 inputs for unit 0 (same definitions as tools/local_fights.py).
static void local_features(const ts::State& s, const ts::Map& m, int round, float* f) {
    const ts::Unit& me = s.u[0];
    auto d = [&](const ts::Unit& u) { return ts::cheb(me.x, me.y, u.x, u.y); };
    int k = 0;
    f[k++] = static_cast<float>(round) / 1000.0f;
    f[k++] = me.range <= 1 ? 1.0f : 0.0f;
    f[k++] = me.hp / std::max(me.max_hp, 1.0f);
    f[k++] = me.hp / 1000.0f;
    f[k++] = me.str / 100.0f;
    f[k++] = static_cast<float>(me.gems) / 50.0f;
    f[k++] = static_cast<float>((me.sp[0].cd == 0) + (me.sp[1].cd == 0)) / 2.0f;
    float an = 0, ahp = 0, astr = 0, amin = 1, aclose = 0, en = 0, ehp = 0, estr = 0, emin = 1, enear = 6, egems = 0, erng = 0, eclose = 0, eready = 0;
    for (int i = 1; i < s.n; ++i) {
        const ts::Unit& u = s.u[i];
        if (!u.alive() || d(u) > 6) continue;
        const float frac = u.hp / std::max(u.max_hp, 1.0f);
        if (u.team == 0) {
            an += 1, ahp += u.hp, astr += u.str, amin = std::min(amin, frac), aclose += d(u) <= 2;
        } else {
            en += 1, ehp += u.hp, estr += u.str, emin = std::min(emin, frac), enear = std::min(enear, static_cast<float>(d(u)));
            egems += u.gems, erng += u.range > 1, eclose += d(u) <= 2, eready += (u.sp[0].cd == 0) + (u.sp[1].cd == 0);
        }
    }
    const float fs[] = {an / 3.0f, ahp / 1000.0f, astr / 100.0f, amin, aclose / 3.0f, en / 3.0f, ehp / 1000.0f, estr / 100.0f, emin,
        enear / 6.0f, egems / 50.0f, erng / 3.0f, eclose / 3.0f, eready / 4.0f,
        static_cast<float>(ts::cheb(me.x, me.y, m.home_x[0], m.home_y[0])) / 30.0f};
    for (float v : fs) f[k++] = v;
}

// A plan's worth: what the simulated rounds won or lost (lives at the phase's value plus bags), plus the
// learned evaluator's prediction for the next 10 rounds from where we stand at the end (APEX_TS_LOCALNET).
static float plan_value(const ts::State& start, const ts::State& end, const ts::Map& m, int round, float life) {
    float v = 0;
    for (int i = 0; i < start.n; ++i) {
        if (!start.u[i].alive() || end.u[i].alive()) continue;
        const float lost = life + static_cast<float>(start.u[i].gems);
        v += start.u[i].team == 0 ? -lost : lost;
    }
    if (end.u[0].alive()) {
        float f[local_model::kIn];
        local_features(end, m, round, f);
        v += local_model::eval(f);
    }
    return v;
}
// The fight and the flight themselves stay c1's (positioning, kiting, the escape planner).
int Bot::tactical_decide() {
    PROF(tdecide);
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision = ts::DecisionInfo{};
    const auto decision_fuel = bc::fuel_remaining();
    auto finish = [&](ts::VerdictReason reason, int value) {
        ts::last_decision.reason = reason;
#if NOVA_RUNTIME_TELEMETRY
        const auto& d = ts::last_decision;
        std::printf("TDV r%d reason%u original%d kept%d dropped%d omit%d link%d camp%d models%d horizon%d pairs%d prep%llu pair0%llu pair1%llu\n",
            round, static_cast<unsigned>(reason), d.eligible_allies + d.eligible_enemies, d.retained, d.dropped,
            d.omitted_enemy, d.incomplete_link, d.camp_risk, d.models, d.horizon, d.completed_pairs,
            static_cast<unsigned long long>(d.prepare_cost), static_cast<unsigned long long>(d.pair_cost[0]),
            static_cast<unsigned long long>(d.pair_cost[1]));
#endif
        return value;
    };
#else
    auto finish = [](ts::VerdictReason, int value) { return value; };
#endif
#if APEX_PROFILE
    prof::td_round = round;
#endif
    if (bc::fuel_remaining() < (tune::kDet ? static_cast<std::uint64_t>(tune::kDetSafety) + 10000
                                           : static_cast<std::uint64_t>(tune::kTsReserve) + 30000))
        return finish(ts::VerdictReason::SafetyReserve, 0);
    ts::State s;
    ts::Map m;
    const EnemyRec* eref[ts::kMaxUnits] = {};
    if (!ts_snapshot(s, m, eref)) return finish(ts::VerdictReason::NoEnemy, 0);
    if (tune::kTsPhantom && tune::kBelief &&
        (tune::kDet || bc::fuel_remaining() > static_cast<std::uint64_t>(tune::kTsReserve) + 45000)) {
        // Reinforcements (APEX_TS_PHANTOM): enemies we cannot see that could join before the horizon, from the
        // beliefs (expected enemies within reach minus the visible ones there). Each whole expected one enters the
        // playouts as a fresh copy of the nearest visible enemy, a horizon's walk away on the enemies' side.
        const int reach = 2 + tune::kTsDecideRounds;
        int nearest = -1, visible_in = 0;
        for (int i = 1; i < s.n; ++i) {
            if (s.u[i].team != 1) continue;
            const int dd = ts::cheb(s.u[i].x, s.u[i].y, pos.x, pos.y);
            visible_in += dd <= reach;
            if (nearest < 0 || dd < ts::cheb(s.u[nearest].x, s.u[nearest].y, pos.x, pos.y)) nearest = i;
        }
        const float hidden = w.expected_enemies(pos, reach) - static_cast<float>(visible_in);
        int add = static_cast<int>(hidden + 0.4f); // 0.6 expected -> one phantom
        if (nearest >= 0)
            for (; add > 0 && s.n < ts::kMaxUnits; --add) {
                const ts::Unit& src = s.u[nearest];
                const int dx = ts::sgn(src.x - pos.x), dy = ts::sgn(src.y - pos.y);
                int px = -1, py = -1;
                for (int k = reach; k >= 2 && px < 0; --k)
                    for (int jx = -1; jx <= 1 && px < 0; ++jx)
                        for (int jy = -1; jy <= 1 && px < 0; ++jy) {
                            const int x = pos.x + dx * k + jx, y = pos.y + dy * k + jy;
                            if (ts::free_tile(s, m, x, y)) px = x, py = y;
                        }
                if (px < 0) break;
                ts::Unit ph = src;
                ph.x = static_cast<std::int16_t>(px);
                ph.y = static_cast<std::int16_t>(py);
                ph.hp = ph.max_hp;
                ph.gems = 0;
                ph.link = -1;
                ph.stunned = 0;
                ph.flags = static_cast<std::uint8_t>(ts::U_ALIVE | (src.flags & (ts::U_SPLASH | ts::U_STUN_IMMUNE | ts::U_RESET_ON_KILL)));
                for (auto& sp : ph.sp)
                    if (sp.cd < 99) sp.cd = 0;
                s.u[s.n++] = ph;
            }
    }
    // Weights of the fight-or-run search, in banked gems. Defaults: learned in self-play (a life 8 -> 25 gems by
    // round 450, a health bar 3, a carried gem 0.75). NOVA_TD_*: set from real games (tools/exchange_fit.py).
    const float t = std::min(1.0f, static_cast<float>(round) / static_cast<float>(NOVA_TD_ROUND));
    const float life = NOVA_TD_LIFE0 + (NOVA_TD_LIFE1 - NOVA_TD_LIFE0) * t;
    ts::Weights wts{life, NOVA_TD_HP_SHARE > 0.0f ? NOVA_TD_HP_SHARE * life : NOVA_TD_HP, tune::kTsThreat, NOVA_TD_GEM};
    ts::Policy models[2];
    models[0].flee[0] = models[1].flee[0] = tune::kTsAllyFlee;
    models[0].flee[1] = tune::kTsFleeA;
    models[1].flee[1] = tune::kTsFleeC;
    if (tune::kOppModel) {
        // This opponent's learned back-off health, bracketed by the two models.
        const float est = opp_flee_estimate();
        if (est >= 0.0f) {
            models[0].flee[1] = std::max(0.02f, est - 0.1f);
            models[1].flee[1] = std::min(0.8f, est + 0.1f);
        }
    }
    if (tune::kTsCalib) {
        // Calibrated against the engine (tools/simcheck.py, 2,590 real fight rounds): units engage only when they win
        // the race (aggression 1.4), no automatic double hits, units run below 25-45% health, and the team that
        // has acted more this round started it.
        int acted[2] = {0, 0};
        for (int i = 1; i < s.n; ++i)
            if (s.u[i].flags & ts::U_ACTED) ++acted[s.u[i].team];
        for (auto& mdl : models) {
            mdl.aggr[0] = mdl.aggr[1] = 1.4f;
            mdl.use_reset = false;
            mdl.first = acted[1] >= acted[0] ? 1 : 0;
        }
        models[0].flee[0] = models[1].flee[0] = 0.35f;
        models[0].flee[1] = 0.25f;
        models[1].flee[1] = 0.45f;
    }
    float v[2][2] = {};
    int done = 0;
    std::uint64_t worst = 8000;
    int rounds = tune::kTsDecideRounds;
    std::uint64_t reserve = static_cast<std::uint64_t>(tune::kTsReserve);
    if (tune::kTsBudget) {
        // Budgeted search (APEX_TS_BUDGET): a playout costs ~0.7-1k fuel x units^2 (tools/simbench.py: 15k with 4
        // units, 42-49k with 8), so a big fight used to burn 40-70k on one playout, decide nothing and run the turn
        // out of fuel. Predict the cost first, shorten the horizon if that makes a full comparison fit, and never
        // start a model whose two playouts cannot both finish above the reserve.
        reserve = static_cast<std::uint64_t>(tune::kTsBudgetReserve);
        const auto n2 = static_cast<std::uint64_t>(s.n * s.n);
        worst = static_cast<std::uint64_t>(tune::kTsUnitCost) * n2 * static_cast<std::uint64_t>(rounds + 1) / 4;
        const std::uint64_t left = bc::fuel_remaining();
        while (rounds > 1 && left < reserve + 2 * worst) {
            --rounds;
            worst = static_cast<std::uint64_t>(tune::kTsUnitCost) * n2 * static_cast<std::uint64_t>(rounds + 1) / 4;
        }
        if (left < reserve + 2 * worst) return finish(ts::VerdictReason::PredictedBudget, 0);
    }
    int n_models = 2;
    if (tune::kDet) {
        // Deterministic (APEX_DET): the plan depends on the fight size only. Predicted playout cost
        // kDetUnitCost per unit per simulated round; drop to one enemy model, then shorten the horizon, until the
        // playouts fit kDetTsBudget. The fuel left only matters below the safety floor.
        auto cost = [&](int r) {
            return static_cast<std::uint64_t>(tune::kDetUnitCost) * static_cast<std::uint64_t>(s.n) * static_cast<std::uint64_t>(r + 1);
        };
        while (static_cast<std::uint64_t>(n_models * 2) * cost(rounds) > static_cast<std::uint64_t>(tune::kDetTsBudget)) {
            if (n_models == 2) n_models = 1;
            else if (rounds > 2) --rounds; // a one-round horizon cannot tell a fight from a flight
            else return finish(ts::VerdictReason::PredictedBudget, 0);
        }
        worst = cost(rounds);
        reserve = static_cast<std::uint64_t>(tune::kDetSafety);
    }
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision.models = n_models;
    ts::last_decision.horizon = rounds;
    const auto ready_fuel = bc::fuel_remaining();
    ts::last_decision.prepare_cost = decision_fuel >= ready_fuel ? decision_fuel - ready_fuel : 0;
#endif
    ts_ran_round = round;
    bool started_comparison = false;
    for (int model = 0; model < n_models; ++model) {
        if (!runtime::admit(runtime::Stage::TacticalPair, 2 * worst, std::max(reserve, runtime::kActionReserve))) break;
        runtime::Probe probe(runtime::Stage::TacticalPair);
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
        const auto pair_fuel = bc::fuel_remaining();
#endif
        bool complete = true;
        for (int plan = 0; plan < 2; ++plan) { // 0 fight, 1 run
            const std::uint64_t left = bc::fuel_remaining();
            // Budgeted: only the playouts this model still needs must fit; unbudgeted: the old rule.
            const std::uint64_t need = tune::kTsBudget || tune::kDet ? static_cast<std::uint64_t>(2 - plan) * worst : 2 * worst;
            if (left < reserve + need) {
                complete = false;
                break;
            }
            ts::State c = s;
            started_comparison = true;
            ts::Policy pol = models[model];
            pol.root_mode = plan == 0 ? 1 : 2;
            ts::act(c, m, 0, pol); // our turn, now
            ts::rollout(c, m, pol, rounds);
            v[model][plan] = tune::kTsLocalNet ? plan_value(s, c, m, round, wts.alive) : ts::evaluate(c, wts);
            const std::uint64_t now = bc::fuel_remaining();
            if (!tune::kDet && now < left) worst = std::max(worst, left - now);
        }
        if (!complete) break;
        ++done;
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
        const auto paired_fuel = bc::fuel_remaining();
        ts::last_decision.pair_cost[model] = pair_fuel >= paired_fuel ? pair_fuel - paired_fuel : 0;
        ts::last_decision.completed_pairs = done;
#endif
    }
#if APEX_PROFILE
    {
        const std::uint64_t out_fuel = bc::fuel_remaining();
        if (out_fuel < 35000)
            std::printf("TDLOW R%d units %d done %d worst %llu left %llu\n", round, s.n, done, static_cast<unsigned long long>(worst),
                static_cast<unsigned long long>(out_fuel));
    }
#endif
    if (done == 0) return finish(started_comparison ? ts::VerdictReason::IncompletePair : ts::VerdictReason::SafetyReserve, 0);
    float fight = 0, run = 0;
    for (int k = 0; k < done; ++k) fight += v[k][0], run += v[k][1];
    fight /= static_cast<float>(done);
    run /= static_cast<float>(done);
#ifdef APEX_TSLOG
    std::printf("TD R%d %s units %d models %d fight %.1f run %.1f -> %s\n", round, bc::to_string(type).c_str(), s.n, done, fight, run,
        fight > run + tune::kTsDecideMargin ? "FIGHT" : run > fight + tune::kTsDecideMargin ? "RUN" : "-");
#endif
    if (fight > run + tune::kTsDecideMargin) return finish(ts::VerdictReason::Fight, 1);
    if (run > fight + tune::kTsDecideMargin) return finish(ts::VerdictReason::Retreat, -1);
    return finish(ts::VerdictReason::InsideMargin, 0);
}

// Team commander (APEX_TFC): plays three team plans forward APEX_TFC_ROUNDS rounds against both enemy models:
// everyone here fights as one (on one focus target), everyone runs, or each follows its own policy. Returns
// 1 (engage, focus = the enemy's handle), 2 (retreat) or 0 (no plan beats the status quo by the margin).
int Bot::team_decide(std::uint32_t& focus_handle) {
    PROF(tfc);
    focus_handle = 0;
    if (bc::fuel_remaining() < static_cast<std::uint64_t>(tune::kTsReserve) + 30000) return 0;
    ts::State s;
    ts::Map m;
    const EnemyRec* eref[ts::kMaxUnits] = {};
    if (!ts_snapshot(s, m, eref)) return 0;
    bool any_ally = false;
    for (int i = 1; i < s.n; ++i) any_ally |= s.u[i].team == 0;
    if (!any_ally && !tune::kTfcSolo) return 0;
    // Focus target: the enemy within 6 of us that dies fastest for what it is worth.
    int focus = -1;
    float best_key = 1e30f;
    for (int i = 1; i < s.n; ++i) {
        const ts::Unit& u = s.u[i];
        if (u.team != 1 || !u.alive() || ts::cheb(u.x, u.y, s.u[0].x, s.u[0].y) > 6) continue;
        const float key = (u.hp + u.shield) / (1.0f + static_cast<float>(u.gems) / 30.0f);
        if (key < best_key) best_key = key, focus = i;
    }
    if (focus < 0) return 0;
    const float t = std::min(1.0f, static_cast<float>(round) / 450.0f);
    const ts::Weights wts{8.0f + 17.0f * t, 3.0f, tune::kTsThreat, 0.75f};
    int acted[2] = {0, 0};
    for (int i = 1; i < s.n; ++i)
        if (s.u[i].flags & ts::U_ACTED) ++acted[s.u[i].team];
    ts::Policy models[2];
    for (auto& mdl : models) {
        mdl.aggr[0] = mdl.aggr[1] = 1.4f; // engine-calibrated engagement for whoever follows the policy
        mdl.use_reset = true;
        mdl.first = acted[1] >= acted[0] ? 1 : 0;
        mdl.flee[0] = 0.35f;
        mdl.focus[0] = focus;
    }
    models[0].flee[1] = 0.25f;
    models[1].flee[1] = 0.45f;
    float v[2][3] = {};
    int done = 0;
    std::uint64_t worst = 8000;
    for (int model = 0; model < 2; ++model) {
        bool complete = true;
        for (int plan = 0; plan < 3; ++plan) { // 0 status quo, 1 all fight, 2 all run
            const std::uint64_t left = bc::fuel_remaining();
            if (left < static_cast<std::uint64_t>(tune::kTsReserve) + 2 * worst) {
                complete = false;
                break;
            }
            ts::State c = s;
            ts::Policy pol = models[model];
            pol.team_mode[0] = plan;
            ts::act(c, m, 0, pol);
            ts::rollout(c, m, pol, tune::kTfcRounds);
            v[model][plan] = ts::evaluate(c, wts);
            const std::uint64_t now = bc::fuel_remaining();
            if (now < left) worst = std::max(worst, left - now);
        }
        if (!complete) break;
        ++done;
    }
    if (done == 0) return 0;
    float mean[3] = {};
    for (int k = 0; k < done; ++k)
        for (int p2 = 0; p2 < 3; ++p2) mean[p2] += v[k][p2] / static_cast<float>(done);
#ifdef APEX_TSLOG
    std::printf("TFC R%d %s units %d models %d quo %.1f fight %.1f run %.1f\n", round, bc::to_string(type).c_str(), s.n, done, mean[0],
        mean[1], mean[2]);
#endif
    if (mean[1] >= mean[2] && mean[1] > mean[0] + (tune::packOn ? NOVA_PACK_MARGIN : tune::kTfcMargin)) {
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY
        if (!ts::last_decision.allow_fight_override()) return 0;
#endif
        focus_handle = eref[focus] ? eref[focus]->id : 0;
        return focus_handle ? 1 : 0;
    }
    if (mean[2] > mean[1] && mean[2] > mean[0] + (tune::packOn ? NOVA_PACK_MARGIN : tune::kTfcMargin)) return 2;
    return 0;
}
// ---------------------------------------------------------------- caster micro (APEX_CMICRO)
// A ranged champion moves OR basic-attacks each turn (GameRules), and most caster nukes exclude walking, so every
// fight turn is a choice between holding a tile to shoot and stepping. Score each option: stay (best basic attack
// plus any ready spell) or step to a free neighbour (plus walk-compatible spells, cast from either tile), as damage
// dealt (with kill and stun bonuses) minus risk x the damage the visible enemies can put on that tile before our next
// turn: a step plus their basic range (two more with a ready dash), their ready offensive spells, less for enemies an
// ally of ours stands nearer to, nothing from enemies stunned or killed this turn. Awake camps add their hits.
bool Bot::caster_micro(const EnemyRec* target, bool engage) {
    (void)target;
    auto& me = bc::self();
    struct Threat {
        const EnemyRec* e;
        int reach;             // basic attack reach next turn: step + range (+2 with a dash ready)
        int sreach;            // offensive spell reach (cast range, +1 when the spell allows walking)
        float basic, spell;    // damage of each
        float cover;           // 1, or 0.6 when one of our allies stands nearer to it than the tile in question
        bool stunned;
        float value;           // what killing it is worth (life plus bag), in health points
    };
    Threat th[8];
    int nt = 0;
    for (const auto& e : w.enemies) {
        if (nt >= 8 || cheb(e.pos, pos) > 9 || e.hp <= 0) continue;
        Threat& t = th[nt++];
        t.e = &e;
        const Kit& k = kit_of(e.type);
        bool dash = false;
        float sval = 0.0f;
        int sr = -1;
        for (int s = 0; s < 2; ++s) {
            std::int32_t cd = 0;
            if (d::sdk_champion_cooldown(e.id, static_cast<std::uint32_t>(s), &cd) != 0) cd = 0; // unknown: ready
            if (cd != 0) continue;
            const KitSpell& ks = k.sp[s];
            if (ks.kind == ts::K_DASH) dash = true;
            if ((ks.kind == ts::K_DAMAGE || ks.kind == ts::K_CC || ks.kind == ts::K_AOE) && (ks.flags & ts::SF_ENEMY)) {
                const float v = ks.mod * e.focus;
                const int r = ks.range + ((ks.flags & ts::SF_EXCL_WALK) ? 0 : 1);
                if (v > sval) sval = v, sr = r;
            }
        }
        t.reach = e.range + 1 + (dash ? 2 : 0);
        t.sreach = sr;
        t.basic = e.strength;
        t.spell = sval;
        t.stunned = false;
        d::buff_data buffs[6];
        std::uint32_t nb = 0;
        if (d::sdk_champion_buff_data(e.id, buffs, 6, &nb) == 0)
            for (std::uint32_t i = 0; i < nb && i < 6; ++i)
                if (buffs[i].type == 1 && buffs[i].data.stun.duration > 0) t.stunned = true;
        if (e.tributing() || e.recalling()) t.stunned = true; // channelling: it will not hit back this turn
        t.value = 300.0f + 4.0f * static_cast<float>(e.gems);
        t.cover = 1.0f;
    }
    if (nt == 0) return false;
    const float frac = hp / std::max(max_hp, 1.0f);
    float risk = tune::kCMicroRisk * (1.0f + 1.5f * (1.0f - frac));
    if (type == bc::champion_type::cassi) risk *= tune::kCiRisk; // her death takes Issac too
    if (NOVA_CMICRO_MELEE && type == bc::champion_type::issac) risk *= tune::kCiRisk; // so does his
    if (!engage) risk *= 1.5f;

    auto danger_at = [&](vec2 q, int stunned_i, int killed_i) {
        float dmg = 0.0f;
        for (int i = 0; i < nt; ++i) {
            const Threat& t = th[i];
            if (i == stunned_i || i == killed_i || t.stunned) continue;
            const int dq = cheb(t.e->pos, q);
            float hit = (dq <= t.reach ? t.basic : 0.0f) + (dq <= t.sreach ? t.spell : 0.0f);
            if (hit <= 0.0f) continue;
            for (const auto& a : w.allies)
                if (cheb(a.pos, t.e->pos) < dq) {
                    hit *= 0.6f;
                    break;
                }
            dmg += hit;
        }
        for (int ci : w.awake_camps_near(q)) dmg += w.camps[ci].strength;
        return dmg;
    };
    // Our spells usable against champions: slot, damage, stun, area radius (-1 single), walk-compatible.
    struct MySpell {
        int slot;
        float dmg;
        bool stun, reset;
        int radius, range;
        bool walk_ok;
    };
    MySpell ms[2];
    int nms = 0;
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s)) continue;
        const auto& sp = spells[s];
        if (sp.kind == bc::ability_kind::attack_reset) {
            ms[nms++] = {s, 0.0f, false, true, -1, 0, true};
            continue;
        }
        if (!sp.enemies) continue;
        if (sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::cc || sp.kind == bc::ability_kind::aoe_damage)
            ms[nms++] = {s, sp.modifier * focus, sp.kind == bc::ability_kind::cc,
                false, sp.kind == bc::ability_kind::aoe_damage ? std::max(1, static_cast<int>(sp.radius)) : -1,
                static_cast<int>(sp.range), !sp.excludes_walk};
    }
    struct Option {
        vec2 to;
        int attack = -1;     // threat index for the basic attack (stay only)
        int spell = -1;      // index into ms
        int spell_on = -1;   // threat index (centre for area spells)
        bool cast_first = true;
        float score = -1e30f;
    };
    Option best;
    const bool can_attack = me.can_attack();
    const bool can_move = me.can_move();
    auto evaluate = [&](vec2 at, bool stay, int atk, int sp_i, int sp_on, vec2 cast_from) {
        float dmg_to[8] = {};
        int stunned_i = -1;
        float offense = 0.0f;
        if (sp_i >= 0) {
            const MySpell& s = ms[sp_i];
            if (s.reset) {
                if (atk < 0) return -1e30f;
                dmg_to[atk] += strength; // second basic attack
            } else {
                if (sp_on < 0 || cheb(cast_from, th[sp_on].e->pos) > s.range) return -1e30f;
                if (s.radius >= 0) {
                    for (int i = 0; i < nt; ++i)
                        if (cheb(th[i].e->pos, th[sp_on].e->pos) <= s.radius) dmg_to[i] += s.dmg;
                } else {
                    dmg_to[sp_on] += s.dmg;
                }
                if (s.stun && !th[sp_on].stunned) stunned_i = sp_on;
            }
        }
        if (atk >= 0) {
            // Ranged basic attacks end movement; melee champions may step first (NOVA_CMICRO_MELEE).
            if ((!stay && !melee) || !can_attack || cheb(at, th[atk].e->pos) > range) return -1e30f;
            dmg_to[atk] += strength;
        }
        int killed_i = -1;
        for (int i = 0; i < nt; ++i) {
            if (dmg_to[i] <= 0.0f) continue;
            const float eff = th[i].e->hp;
            offense += std::min(dmg_to[i], eff) * (1.0f + static_cast<float>(th[i].e->gems) / 30.0f) *
                (NOVA_STUN_FOCUS > 0.0f && th[i].stunned ? 1.0f + NOVA_STUN_FOCUS : 1.0f);
            if (dmg_to[i] >= eff) {
                offense += th[i].value;
                killed_i = i;
            }
        }
        if (stunned_i >= 0) offense += 60.0f; // a stunned champion cannot chase, flee or hit an ally either
        float danger = danger_at(at, stunned_i, killed_i);
        if (danger >= hp) danger *= 3.0f; // lethal tiles: almost never
        return offense - risk * danger;
    };
    auto consider = [&](vec2 at, bool stay, int atk, int sp_i, int sp_on, bool cast_first) {
        const vec2 from = cast_first ? pos : at;
        const float s = evaluate(at, stay, atk, sp_i, sp_on, from);
        if (s > best.score) {
            best.score = s;
            best.to = at;
            best.attack = atk;
            best.spell = sp_i;
            best.spell_on = sp_on;
            best.cast_first = cast_first;
        }
    };
    // Stay: every basic target (or none) with every spell choice.
    for (int a = -1; a < nt; ++a) {
        if (a >= 0 && (!can_attack || cheb(pos, th[a].e->pos) > range)) continue;
        consider(pos, true, a, -1, -1, true);
        for (int s = 0; s < nms; ++s) {
            if (ms[s].reset) {
                consider(pos, true, a, s, -1, true);
                continue;
            }
            for (int o = 0; o < nt; ++o) consider(pos, true, a, s, o, true);
        }
    }
    const float stay_score = best.score;
    // Steps: no basic attack; walk-compatible spells from either tile.
    if (can_move) {
        static const vec2 kSteps[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (vec2 dlt : kSteps) {
            const vec2 q = pos + dlt;
            if (q.x < 0 || q.y < 0 || q.x >= w.width || q.y >= w.height || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
            consider(q, false, -1, -1, -1, true);
            if (melee && can_attack)
                for (int a = 0; a < nt; ++a) {
                    if (cheb(q, th[a].e->pos) > range) continue;
                    consider(q, false, a, -1, -1, true); // step, then hit
                    for (int s = 0; s < nms; ++s)
                        if (ms[s].reset) consider(q, false, a, s, -1, true);
                        else if (ms[s].walk_ok)
                            for (int o = 0; o < nt; ++o) consider(q, false, a, s, o, false);
                }
            for (int s = 0; s < nms; ++s) {
                if (!ms[s].walk_ok || ms[s].reset) continue;
                for (int o = 0; o < nt; ++o) {
                    consider(q, false, -1, s, o, true);
                    consider(q, false, -1, s, o, false);
                }
            }
        }
    }
    (void)stay_score;
    // Nothing to shoot and nothing threatens us here: leave the turn to farming.
    if (best.to == pos && best.attack < 0 && best.spell < 0 && danger_at(pos, -1, -1) <= 0.0f) return false;
    // Melee without a hit this turn: closing in, chasing and fleeing belong to the normal fight logic.
    if (melee && best.attack < 0 && best.spell < 0) return false;
    auto do_spell = [&]() {
        if (best.spell < 0 || cast_done) return;
        const MySpell& s = ms[best.spell];
        try {
            const auto ab = me.abilities()[s.slot];
            bool ok = false;
            if (s.reset) ok = ab.use();
            else if (s.radius >= 0) ok = ab.use(th[best.spell_on].e->pos);
            else ok = ab.use(th[best.spell_on].e->unit);
            if (ok) cast_done = true;
#ifdef APEX_SPELLSTAT
            if (ok && !s.reset) ++spellstat_enemy;
#endif
        } catch (const std::exception& ex) {
            LOG("cmicro cast error %s\n", ex.what());
        }
    };
    const bool reset_spell = best.spell >= 0 && ms[best.spell].reset;
    if (best.cast_first && !reset_spell) do_spell();
    if (best.to != pos) {
        try {
            if (me.move_absolute(best.to)) {
                pos = best.to;
                walked = true;
            }
        } catch (const std::exception& ex) {
            LOG("cmicro move error %s\n", ex.what());
        }
    }
    if (!best.cast_first) do_spell();
    if (best.attack >= 0) {
        const EnemyRec& e = *th[best.attack].e;
        if (attack_unit(e.unit, e.pos) && reset_spell) use_reset_and_attack(e.unit, e.pos);
    }
    return true;
}
