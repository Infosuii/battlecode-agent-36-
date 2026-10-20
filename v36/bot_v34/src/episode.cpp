// Fight episodes (APEX_EPISODES): learning data and the learned aggression policy. An episode starts when an
// enemy comes within 8 tiles and ends after 6 quiet turns, at death, or after 40 rounds. At the start a
// multiplier for c1's fight formula is chosen (aggression x m, danger threshold / sqrt(m)): at random in
// the data build (APEX_EP_EXPLORE), by the learned model otherwise (ep_model.h). The end logs the features
// at the start and the outcome in gem units (tools/learn_aggr.py trains on these lines).
#include "bot.h"
#include "strategy.h"

#include <cmath>

#ifndef APEX_EPISODES
#define APEX_EPISODES 0
#endif
#ifndef APEX_EP_EXPLORE
#define APEX_EP_EXPLORE 0
#endif
#ifndef APEX_EP_LOG
#define APEX_EP_LOG 0
#endif
#include "ep_model.h"

namespace d = cambc::detail;

namespace {
constexpr float kGrid[Bot::kEpGrid] = {0.5f, 0.75f, 1.0f, 1.35f, 1.8f, 2.5f};
} // namespace

// Enemy deaths so far (the engine answers is_alive for every enemy, seen or not).
void Bot::count_enemy_deaths() {
    if (enemy_handles.empty()) {
        for (const auto& e : bc::champions(team::enemy)) enemy_handles.push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(e)));
        enemy_alive.assign(enemy_handles.size(), 1);
    }
    for (std::size_t i = 0; i < enemy_handles.size(); ++i) {
        std::int32_t alive = 1;
        if (d::sdk_champion_is_alive(enemy_handles[i], &alive) != 0) continue;
        if (enemy_alive[i] && !alive) ++enemy_deaths;
        enemy_alive[i] = static_cast<char>(alive != 0);
    }
}

void Bot::ep_features(float* f) const {
    int k = 0;
    auto put = [&](float v) { f[k++] = v; };
    bool dash = false, stun = false, nuke = false;
    for (int s = 0; s < 2; ++s) {
        if (!spells[s].present || cooldown[s] != 0) continue;
        dash |= spells[s].kind == bc::ability_kind::dash;
        stun |= spells[s].kind == bc::ability_kind::cc || (type == bc::champion_type::ryth && spells[s].kind == bc::ability_kind::aoe_damage);
        nuke |= spells[s].kind == bc::ability_kind::damage || spells[s].kind == bc::ability_kind::aoe_damage;
    }
    put(melee ? 1.0f : 0.0f);
    put(hp / std::max(max_hp, 1.0f));
    put(max_hp / 1000.0f);
    put(strength / 100.0f);
    put(focus / 100.0f);
    put(static_cast<float>(gems) / 50.0f);
    put(dash ? 1.0f : 0.0f);
    put(stun ? 1.0f : 0.0f);
    put(nuke ? 1.0f : 0.0f);
    put(type == bc::champion_type::cassi || type == bc::champion_type::issac ? 1.0f : 0.0f);
    int ne = 0, ner = 0, nci = 0, near = 99;
    float ehp = 0, estr = 0, emin = 1, egems = 0;
    for (const auto& e : w.enemies) {
        const int dd = cheb(e.pos, pos);
        if (dd > 8) continue;
        ++ne;
        ehp += e.hp;
        estr += e.strength;
        ner += e.range > 1;
        emin = std::min(emin, e.hp / std::max(e.max_hp, 1.0f));
        nci += e.type == bc::champion_type::cassi || e.type == bc::champion_type::issac;
        egems += static_cast<float>(e.gems);
        near = std::min(near, dd);
    }
    put(static_cast<float>(ne) / 3.0f);
    put(ehp / 1000.0f);
    put(estr / 100.0f);
    put(static_cast<float>(ner) / 3.0f);
    put(emin);
    put(nci ? 1.0f : 0.0f);
    put(egems / 50.0f);
    put(static_cast<float>(std::min(near, 8)) / 8.0f);
    int na = 0;
    float ahp = 0, astr = 0;
    for (const auto& a : w.allies) {
        if (cheb(a.pos, pos) > 6) continue;
        ++na;
        ahp += a.hp;
        astr += a.strength;
    }
    put(static_cast<float>(na) / 3.0f);
    put(ahp / 1000.0f);
    put(astr / 100.0f);
    put(static_cast<float>(std::min(static_cast<int>(fdist(w.spawn_field, pos)), 60)) / 30.0f);
    put(static_cast<float>(std::min(cheb(pos, ts_home[1].x >= 0 ? ts_home[1] : pos), 60)) / 30.0f);
    put(static_cast<float>(ally_score) / 1000.0f);
    put(static_cast<float>(enemy_score) / 1000.0f);
    put(static_cast<float>(round) / 1000.0f);
    const int cls = brain::state.ready ? brain::state.cls : 0;
    for (int c = 0; c < 6; ++c) put(c == cls ? 1.0f : 0.0f);
    put(static_cast<float>(team_size) / 5.0f);
}

void Bot::episode_tick(bool dead_now) {
    if (!APEX_EPISODES) return;
    count_enemy_deaths();
    // Score timeline for horizon labels (every champion prints it; the parser keeps one per round).
    if (APEX_EP_LOG && !dead_now && round % 10 == 0) std::printf("SC R%d a%d e%d\n", round, ally_score, enemy_score);
    auto finish = [&](bool died) {
        const int dur = round - ep.start;
        const float dhp = died ? -ep.hp0 : hp / std::max(max_hp, 1.0f) - ep.hp0;
        const int kills = enemy_deaths - ep.enemy_deaths0;
        const float v = (died ? -(tune::kTsAlive + static_cast<float>(ep.last_gems)) : 0.0f) + tune::kTsAlive * static_cast<float>(kills) +
            tune::kTsHp * dhp - 0.25f * static_cast<float>(dur);
        if (APEX_EP_LOG) {
            char buf[512];
            int len = std::snprintf(buf, sizeof(buf), "EP R%d dur%d m%d V%.2f d%d k%d h%.2f s%d,%d F", ep.start, dur, ep.choice, v, died ? 1 : 0, kills, dhp, ep.score_us, ep.score_them);
            for (int i = 0; i < kEpFeat && len < 500; ++i) len += std::snprintf(buf + len, sizeof(buf) - len, "%s%.3g", i ? "," : ":", ep.f[i]);
            std::printf("%s\n", buf);
        }
        ep.active = false;
        ep_aggr = 1.0f;
        ep_danger = 1.0f;
    };
    if (dead_now) {
        if (ep.active) finish(true);
        return;
    }
    if (ep.active) {
        ep.last_gems = gems;
        if (last_enemy_dist <= 10) ep.last_contact = round;
        if (round - ep.last_contact >= 6 || round - ep.start >= 40) finish(false);
        return;
    }
    if (last_enemy_dist > 8) return;
    ep.active = true;
    ep.start = ep.last_contact = round;
    ep.hp0 = hp / std::max(max_hp, 1.0f);
    ep.last_gems = gems;
    ep.enemy_deaths0 = enemy_deaths;
    ep.score_us = ally_score;
    ep.score_them = enemy_score;
    if (ts_home[1].x < 0) {
        long sx = 0, sy = 0, n = 0;
        for (vec2 p : w.enemy_spawn.positions()) sx += p.x, sy += p.y, ++n;
        if (n) ts_home[1] = vec2{static_cast<int>(sx / n), static_cast<int>(sy / n)};
    }
    ep_features(ep.f);
#if APEX_EP_EXPLORE
    // Deterministic per champion and round, spread evenly over the grid.
    std::uint32_t h = my_id * 2654435761u ^ static_cast<std::uint32_t>(round) * 40503u ^ static_cast<std::uint32_t>(w.width * 131 + w.height);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    ep.choice = static_cast<int>(h % kEpGrid);
#elif defined(APEX_EP_CONST)
    ep.choice = APEX_EP_CONST; // a fixed multiplier (tests)
#else
    ep.choice = ep_model_choose(ep.f);
#endif
    ep_aggr = kGrid[ep.choice];
    ep_danger = 1.0f / std::sqrt(ep_aggr);
}
