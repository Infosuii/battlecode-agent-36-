// Tribute denial (APEX_TRIB_DENY & 2), ported from apex_v18b / apex_v20 (deny.cpp there).
#include "bot.h"

namespace {
float ally_turn_dmg(const AllyRec& a) { return std::max(a.strength * 1.15f, 30.0f) + 12.0f; } // as ally_dps in bot.cpp
} // namespace

// APEX_TRIB_DENY & 2. A tributing enemy cannot act until its channel ends, and killing it takes its
// whole bag. So hit it while it channels, and cancel the tribute (altar lock, else a stun) only on our
// last turn before it lands, unless that turn kills it. A tributer we cannot hit in time is still
// worth a walk to lock range of its altar. Returns true when this turn went to a tributer.
bool Bot::deny_tribute() {
    auto& me = bc::self();
    int locks = 0;
    try {
        locks = static_cast<int>(me.consumables()[bc::consumable_type::altar_lock].count());
    } catch (...) {
    }
    bool reset_ready = false, nuke_ready = false, stun_ready = false;
    float nuke = 0.0f;
    bool dash_ready = false;
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s)) continue;
        const auto k = spells[s].kind;
        reset_ready |= k == bc::ability_kind::attack_reset;
        dash_ready |= k == bc::ability_kind::dash;
        if (!spells[s].enemies) continue;
        if (k == bc::ability_kind::damage || k == bc::ability_kind::aoe_damage) nuke_ready = true, nuke = std::max(nuke, spells[s].modifier * focus);
        stun_ready |= k == bc::ability_kind::cc ||
            (NOVA_DENY_RYTH && type == bc::champion_type::ryth && k == bc::ability_kind::aoe_damage); // area stun
    }

    const EnemyRec* best = nullptr;
    vec2 best_altar{-1, -1};
    int best_k = 0;
    bool best_kill = false, best_hit = false;
    float best_score = 0.0f, best_now = 0.0f;
    for (const auto& e : w.enemies) {
        if (!e.tributing() || e.gems < 10) continue;
        std::int32_t left = 0;
        if (cambc::detail::sdk_champion_channel_time(e.id, &left) != 0) continue;
        // Our turns (this one included) before the tribute lands: its timer ticks at its turn end and
        // the tribute lands at its next turn start once the timer is zero.
        const bool acted = (e.state & (1u << 5)) != 0;
        const int k = acted ? left + 1 : std::max(left, 1);
        vec2 altar{-1, -1};
        for (vec2 a : w.altars)
            if (cheb(a, e.pos) <= static_cast<int>(bc::constants::submit_gems_range) &&
                (altar.x < 0 || cheb(a, pos) < cheb(altar, pos)))
                altar = a;
        const int d = cheb(e.pos, pos);
        int walk = std::max(0, d - range);
        if (dash_ready) walk = std::max(0, walk - 2);
        const int lost = melee ? std::max(0, walk - 1) : walk; // turns that cannot hit it
        const int hits = k - lost;
        float allies_dmg = 0.0f;
        for (const auto& a : w.allies)
            if (cheb(a.pos, e.pos) <= a.range + 1) allies_dmg += ally_turn_dmg(a);
        const float dmg = hits <= 0 ? 0.0f
            : static_cast<float>(hits) * (strength + allies_dmg) + (reset_ready ? strength : 0.0f) + (nuke_ready ? nuke : 0.0f);
        const bool kill = hits > 0 && dmg >= e.hp * 1.1f;
        const bool lock_ok = locks > 0 && altar.x >= 0 && std::max(0, cheb(pos, altar) - 2) <= k - 1;
        const bool stun_ok = stun_ready && hits > 0;
        if (!kill && !lock_ok && !stun_ok) continue;
        const float score = static_cast<float>(e.gems) * (kill ? 2.0f : 1.0f) - static_cast<float>(d);
        if (!best || score > best_score) {
            best = &e;
            best_altar = altar;
            best_k = k;
            best_kill = kill;
            best_hit = hits > 0;
            best_score = score;
            best_now = (d <= range || (melee && d <= range + 1)) ? strength + (reset_ready ? strength : 0.0f) + (nuke_ready ? nuke : 0.0f) + allies_dmg : 0.0f;
        }
    }
    if (!best) return false;
    const EnemyRec& e = *best;
    mode = best_kill ? "trib-kill" : "trib-deny";
#ifdef APEX_DENYSTATS
    std::printf("TDENY R%d s%d id%u g%d hp%.0f k%d kill%d hit%d d%d\n", round, slot, e.id, e.gems, e.hp, best_k, best_kill ? 1 : 0,
        best_hit ? 1 : 0, cheb(e.pos, pos));
#endif
    // Get there: into attack range when we hit it in time, otherwise within lock range of its altar.
    if (cheb(e.pos, pos) > range) {
        bc::bitboard goal;
        goal.clear();
        if (best_hit) {
            goal.draw_chebyshev(e.pos, static_cast<float>(range));
        } else {
            goal.draw_chebyshev(best_altar, 2.0f);
        }
        walk_toward(goal);
    }
    const bool last = best_k <= 1;
    const bool kills_now = cheb(e.pos, pos) <= range && best_now >= e.hp;
    keep_channel = !last || kills_now;
    if (last && !kills_now) {
        // Our last chance before it lands: cancel it.
        bool cancelled = false;
        if (locks > 0 && best_altar.x >= 0 && cheb(best_altar, pos) <= 2 && !bc::vision::has_altar_lock(best_altar)) {
            try {
                cancelled = me.consumables()[bc::consumable_type::altar_lock].use(best_altar);
#ifdef APEX_DENYSTATS
                if (cancelled) std::printf("LOCKUSE R%d s%d id%u g%d\n", round, slot, e.id, e.gems);
#endif
            } catch (const std::exception& ex) {
                LOG("trib lock error %s\n", ex.what());
            }
        }
        if (!cancelled) cast_on_enemy(e); // a stun cancels it too
    }
    if (cheb(e.pos, pos) <= range) {
        cast_on_enemy(e);
        if (attack_unit(e.unit, e.pos)) use_reset_and_attack(e.unit, e.pos);
    }
    return true;
}
