// Draft: farming speed first, then durability. Both teams may own the same champion.
#include "draft.h"
#include "common.h"
#include "strategy.h"
#include <cstdio>

#ifndef APEX_DRAFT
#define APEX_DRAFT 6
#endif

using cd = bc::champion_draft;
namespace {
// Candidate priority lists; tests pick the preset (see docs/MASTER_PLAN.md, draft results).
constexpr cd kPresets[][11] = {
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::tavi, cd::stellar, cd::ryth, cd::marcella, cd::ren,
        cd::vesper_vale, cd::willow, cd::prism},
    {cd::vendetta, cd::varg, cd::ryth, cd::marcella, cd::stellar, cd::tavi, cd::ren, cd::vesper_vale, cd::willow,
        cd::prism, cd::cassi_and_issac},
    {cd::vendetta, cd::varg, cd::stellar, cd::tavi, cd::ryth, cd::marcella, cd::ren, cd::vesper_vale, cd::willow,
        cd::prism, cd::cassi_and_issac},
    {cd::vendetta, cd::varg, cd::marcella, cd::ryth, cd::ren, cd::stellar, cd::tavi, cd::vesper_vale, cd::willow,
        cd::prism, cd::cassi_and_issac},
    {cd::varg, cd::vendetta, cd::willow, cd::ryth, cd::marcella, cd::stellar, cd::tavi, cd::ren, cd::vesper_vale,
        cd::prism, cd::cassi_and_issac},
    {cd::vendetta, cd::varg, cd::vesper_vale, cd::stellar, cd::ryth, cd::tavi, cd::marcella, cd::ren, cd::willow,
        cd::prism, cd::cassi_and_issac},
    // 6: Ren fourth. With dash escapes, Ren beat Marcella as the fourth pick (+59 on 4-champion
    // maps) and Stellar as the fifth (+91 on 5-champion maps).
    {cd::vendetta, cd::varg, cd::ryth, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::vesper_vale, cd::willow,
        cd::prism, cd::cassi_and_issac},
};
} // namespace

#ifdef APEX_DRAFT_LIST
// Test override: an explicit priority list (comma-separated cd:: names), then the preset as fallback.
constexpr cd kOverride[] = {APEX_DRAFT_LIST};
#endif
// Test overrides per team size (the draft can read the team size, 3-5).
#ifdef APEX_DRAFT_T3
constexpr cd kT3[] = {APEX_DRAFT_T3};
#endif
#ifdef APEX_DRAFT_T4
constexpr cd kT4[] = {APEX_DRAFT_T4};
#endif
#ifdef APEX_DRAFT_T5
constexpr cd kT5[] = {APEX_DRAFT_T5};
#endif

namespace {
// Pick the first champion in the list we have not taken yet.
template <std::size_t N>
bool pick_from(const cd (&list)[N], const std::vector<cd>& chosen, bool skip_casters = false) {
    for (auto pick : list) {
        if (std::find(chosen.begin(), chosen.end(), pick) != chosen.end()) continue;
        // NOVA_COUNTER: fight enemy casters with bodies, not more casters of ours.
        if (skip_casters && (pick == cd::stellar || pick == cd::prism || pick == cd::tavi || pick == cd::willow))
            continue;
        bc::draft::pick(pick);
        return true;
    }
    return false;
}

// Read the map through the raw engine calls (draft phase: the map is loaded, w/h/groups already
// answer): total gem value, groups, monolith presence. False on anything unexpected.
struct MapFacts {
    int groups = 0, gems = 0, gold = 0, max_gems = 0;
    bool monolith = false, ok = false;
};
MapFacts read_facts() {
    static MapFacts cached{};
    static bool cached_ok = false;
    if (cached_ok) return cached;
    namespace d = cambc::detail;
    MapFacts f;
    std::uint32_t n = 0;
    if (bc::detail::sdk_map_num_camp_groups(&n) != 0) return f;
    std::vector<std::uint32_t> h(n == 0 ? 1 : n);
    if (bc::detail::sdk_map_camp_groups(h.data(), n, &n) != 0 || n == 0) return f;
    f.groups = static_cast<int>(n);
    f.ok = true;
    for (std::uint32_t k = 0; k < n; ++k) {
        d::camp_group_info info{};
        if (bc::detail::sdk_map_camp_group(h[k], &info) != 0) {
            f.ok = false;
            break;
        }
        f.gems += static_cast<int>(info.gems);
        f.gold += static_cast<int>(info.gold);
        f.max_gems = std::max(f.max_gems, static_cast<int>(info.gems));
        if (f.monolith) continue; // monolith check costs one extra loop per group: only until found
        std::uint32_t members = 0;
        if (bc::detail::sdk_map_camp_group_num_camps(h[k], &members) != 0 || members == 0) continue;
        std::vector<std::uint32_t> spawns(members);
        if (bc::detail::sdk_map_camp_group_camps(h[k], spawns.data(), members, &members) != 0) continue;
        for (std::uint32_t m = 0; m < members && !f.monolith; ++m) {
            d::camp_info spec{};
            if (bc::detail::sdk_map_camp_info(spawns[m], &spec) != 0) continue;
            f.monolith |= static_cast<bc::camp_type>(spec.type) == bc::camp_type::monolith;
        }
    }
    cached = f;
    cached_ok = f.ok;
    return f;
}

// Share of dangerous camp groups (wolves, elks, monoliths) with a camp of another dangerous group
// within 3 tiles (0 on fifteen of the seventeen official maps, ~0.9 on noise-generated maps).
// Computed once per draft; -1 when the map cannot be read.
float cluster_frac() {
    static float cached = -2.0f;
    if (cached > -1.5f) return cached;
    cached = -1.0f;
    std::uint32_t n = 0;
    if (bc::detail::sdk_map_num_camp_groups(&n) != 0 || n == 0) return cached;
    std::vector<std::uint32_t> h(n);
    if (bc::detail::sdk_map_camp_groups(h.data(), n, &n) != 0) return cached;
    std::vector<std::vector<vec2>> dang;
    for (std::uint32_t k = 0; k < n; ++k) {
        std::uint32_t members = 0;
        if (bc::detail::sdk_map_camp_group_num_camps(h[k], &members) != 0 || members == 0) continue;
        std::vector<std::uint32_t> spawns(members);
        if (bc::detail::sdk_map_camp_group_camps(h[k], spawns.data(), members, &members) != 0) continue;
        std::vector<vec2> cells;
        for (std::uint32_t m = 0; m < members; ++m) {
            bc::detail::camp_info spec{};
            if (bc::detail::sdk_map_camp_info(spawns[m], &spec) != 0) continue;
            const auto t = static_cast<bc::camp_type>(spec.type);
            if (t == bc::camp_type::wolf || t == bc::camp_type::elk || t == bc::camp_type::monolith) cells.push_back({spec.x, spec.y});
        }
        if (!cells.empty()) dang.push_back(std::move(cells));
    }
    if (dang.empty()) return cached = 0.0f;
    int packed = 0;
    for (std::size_t i = 0; i < dang.size(); ++i) {
        bool near = false;
        for (std::size_t j = 0; j < dang.size() && !near; ++j) {
            if (j == i) continue;
            for (vec2 a : dang[i]) {
                for (vec2 b : dang[j])
                    if (std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) <= 3) {
                        near = true;
                        break;
                    }
                if (near) break;
            }
        }
        packed += near;
    }
    return cached = static_cast<float>(packed) / static_cast<float>(dang.size());
}
} // namespace


// Map-aware draft (APEX_MAPDRAFT): the draft reads the map size and team size, which identify each
// official map, and picks a lineup for it. APEX_MD_<MAP> = lineup index (-1 = the default preset).
// Lineups: 0 melee (preset 6), 1 C&I+Stellar+Tavi, 2 C&I+Stellar+Varg, 3 Prism+Stellar+Tavi,
// 4 Vendetta+Varg+Stellar, 5 C&I+Tavi+Varg.
#ifndef APEX_MAPDRAFT
#define APEX_MAPDRAFT 0
#endif
#ifndef APEX_MD_COLLISION
#define APEX_MD_COLLISION -1
#endif
#ifndef APEX_MD_CORNERS
#define APEX_MD_CORNERS -1
#endif
#ifndef APEX_MD_ELKRING
#define APEX_MD_ELKRING -1
#endif
#ifndef APEX_MD_EXPANSE
#define APEX_MD_EXPANSE -1
#endif
#ifndef APEX_MD_INTERLOCK
#define APEX_MD_INTERLOCK -1
#endif
#ifndef APEX_MD_LANES
#define APEX_MD_LANES -1
#endif
#ifndef APEX_MD_MONOLITH
#define APEX_MD_MONOLITH -1
#endif
#ifndef APEX_MD_PACKS
#define APEX_MD_PACKS -1
#endif
#ifndef APEX_MD_ROWS
#define APEX_MD_ROWS -1
#endif
#ifndef APEX_MD_OTHER
#define APEX_MD_OTHER -1
#endif
// Class draft (APEX_CLASSDRAFT): lineup per map class (team size 3/4/5 x small/large, see strategy.h);
// APEX_L<k> = lineup index, -1 = fall through to the other draft rules.
#ifndef APEX_CLASSDRAFT
#define APEX_CLASSDRAFT 0
#endif
#ifndef APEX_L0
#define APEX_L0 -1
#endif
#ifndef APEX_L1
#define APEX_L1 -1
#endif
#ifndef APEX_L2
#define APEX_L2 -1
#endif
#ifndef APEX_L3
#define APEX_L3 -1
#endif
#ifndef APEX_L4
#define APEX_L4 -1
#endif
#ifndef APEX_L5
#define APEX_L5 -1
#endif
namespace {
constexpr cd kLineups[][11] = {
    {cd::vendetta, cd::varg, cd::ryth, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::vesper_vale, cd::willow,
        cd::prism, cd::cassi_and_issac},
    {cd::cassi_and_issac, cd::stellar, cd::tavi, cd::varg, cd::ren, cd::prism, cd::ryth, cd::vendetta, cd::willow,
        cd::marcella, cd::vesper_vale},
    {cd::cassi_and_issac, cd::stellar, cd::varg, cd::tavi, cd::ren, cd::prism, cd::ryth, cd::vendetta, cd::willow,
        cd::marcella, cd::vesper_vale},
    {cd::prism, cd::stellar, cd::tavi, cd::cassi_and_issac, cd::ren, cd::varg, cd::ryth, cd::vendetta, cd::willow,
        cd::marcella, cd::vesper_vale},
    {cd::vendetta, cd::varg, cd::stellar, cd::tavi, cd::ren, cd::ryth, cd::cassi_and_issac, cd::prism, cd::willow,
        cd::marcella, cd::vesper_vale},
    {cd::cassi_and_issac, cd::tavi, cd::varg, cd::stellar, cd::ren, cd::prism, cd::ryth, cd::vendetta, cd::willow,
        cd::marcella, cd::vesper_vale},
    // 6: Mite-style (v13b), 7: + Tavi fourth, 8: + Stellar fourth, 9: C&I second
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::ryth, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::tavi, cd::ren, cd::ryth, cd::stellar, cd::marcella, cd::prism,
        cd::willow, cd::vesper_vale},
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::stellar, cd::ren, cd::ryth, cd::tavi, cd::marcella, cd::prism,
        cd::willow, cd::vesper_vale},
    {cd::vendetta, cd::cassi_and_issac, cd::varg, cd::ryth, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 10: the top team's stampede caster lineup (Ryth tanks, casters nuke from range 4).
    {cd::stellar, cd::tavi, cd::ryth, cd::prism, cd::willow, cd::ren, cd::varg, cd::cassi_and_issac, cd::vendetta,
        cd::marcella, cd::vesper_vale},
    // 11: the top team's interlock/contest lineup (burst and stuns).
    {cd::tavi, cd::stellar, cd::prism, cd::ren, cd::willow, cd::ryth, cd::varg, cd::cassi_and_issac, cd::vendetta,
        cd::marcella, cd::vesper_vale},
    // 12: casters first, Varg as the front line.
    {cd::stellar, cd::tavi, cd::varg, cd::prism, cd::ryth, cd::ren, cd::cassi_and_issac, cd::willow, cd::vendetta,
        cd::marcella, cd::vesper_vale},
    // 13: Stellar + Tavi beside the melee core.
    {cd::stellar, cd::vendetta, cd::varg, cd::tavi, cd::ryth, cd::cassi_and_issac, cd::ren, cd::prism, cd::willow,
        cd::marcella, cd::vesper_vale},
    // v25 clean-map screen (inert unless a chart or NOVA_FALLBACK_LINEUP names them); all keep C&I in
    // the first three picks (one pick, two bodies).
    // 14: lineup 6 with Marcella fifth instead of Ren.
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::ryth, cd::marcella, cd::ren, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 15: lineup 6 with Marcella fourth instead of Ryth.
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::marcella, cd::ren, cd::ryth, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 16: Ryth in place of Vendetta.
    {cd::ryth, cd::varg, cd::cassi_and_issac, cd::vendetta, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 17: Ryth in place of Varg.
    {cd::vendetta, cd::ryth, cd::cassi_and_issac, cd::varg, cd::ren, cd::marcella, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 18: lineup 6 with Ren fourth and Ryth fifth.
    {cd::vendetta, cd::varg, cd::cassi_and_issac, cd::ren, cd::ryth, cd::marcella, cd::stellar, cd::tavi, cd::prism,
        cd::willow, cd::vesper_vale},
    // 19: ladder meta (platform top-12 teams, 5.6k games): C&I + Stellar + Ren (3), + Ryth (4), + Varg (5).
    {cd::cassi_and_issac, cd::stellar, cd::ren, cd::ryth, cd::varg, cd::tavi, cd::prism, cd::willow, cd::vendetta,
        cd::marcella, cd::vesper_vale},
    // 20: top teams' stampede comp: Ryth + C&I + Stellar + Prism + Tavi.
    {cd::cassi_and_issac, cd::stellar, cd::ryth, cd::prism, cd::tavi, cd::ren, cd::varg, cd::willow, cd::vendetta,
        cd::marcella, cd::vesper_vale},
};
constexpr int kNumLineups = sizeof(kLineups) / sizeof(kLineups[0]);

// Which official map is this? Width, height and team size are unique per map. -1 = unknown map.
int map_lineup(int w, int h, unsigned size) {
    struct Key { int w, h; unsigned size; int lineup; };
    constexpr Key kMaps[] = {
        {41, 41, 3, APEX_MD_COLLISION}, {71, 71, 5, APEX_MD_CORNERS}, {30, 30, 5, APEX_MD_ELKRING},
        {80, 80, 3, APEX_MD_EXPANSE}, {40, 40, 4, APEX_MD_INTERLOCK}, {90, 30, 5, APEX_MD_LANES},
        {50, 50, 5, APEX_MD_MONOLITH}, {31, 31, 3, APEX_MD_PACKS}, {50, 50, 4, APEX_MD_ROWS},
    };
    for (const auto& k : kMaps)
        if (((k.w == w && k.h == h) || (k.w == h && k.h == w)) && k.size == size) return k.lineup;
    return APEX_MD_OTHER;
}
} // namespace

// NOVA_MAPID_STRICT (v36): an official map must also match its total camp gems and gold. Width/height/size/groups
// alone matched an official map on 4 of 247 generated maps (1.6%), which then drafted the official lineup and
// turned off the unseen-map play settings. Totals are invariant under rotation and transposition; a map whose facts
// cannot be read counts as unknown. 0 = v34 behaviour.
#ifndef NOVA_MAPID_STRICT
#define NOVA_MAPID_STRICT 0
#endif

namespace {
// Total camp gems and gold, one engine call per camp group (no per-camp scan), cached once read. False if unreadable.
bool map_totals(int& gems, int& gold) {
    static int cg = 0, cd = 0;
    static bool done = false;
    if (!done) {
        std::uint32_t n = 0;
        if (bc::detail::sdk_map_num_camp_groups(&n) != 0 || n == 0) return false;
        std::vector<std::uint32_t> h(n);
        if (bc::detail::sdk_map_camp_groups(h.data(), n, &n) != 0) return false;
        int tg = 0, td = 0;
        for (std::uint32_t k = 0; k < n; ++k) {
            cambc::detail::camp_group_info info{};
            if (bc::detail::sdk_map_camp_group(h[k], &info) != 0) return false;
            tg += static_cast<int>(info.gems);
            td += static_cast<int>(info.gold);
        }
        cg = tg;
        cd = td;
        done = true;
    }
    gems = cg;
    gold = cd;
    return true;
}
} // namespace

// Official map pool identity: width, height, team size and camp-group count (+ gem/gold totals when strict).
// Facts measured with the NOVA_MAPFACTS_PRINT probe on the 19 Oct 2026 pool (v36/agent4/data/mapfacts.json).
int nova_map_id(int w, int h, unsigned size, int groups) {
    struct Key { int w, h; unsigned size; int groups, gems, gold; };
    constexpr Key kMaps[] = {
        {100, 100, 5, 12, 692, 2270}, {41, 41, 3, 20, 536, 2530}, {50, 50, 5, 14, 400, 1300},
        {71, 71, 5, 30, 916, 4200}, {41, 30, 5, 8, 540, 2210}, {30, 30, 5, 10, 360, 1950},
        {80, 80, 3, 24, 1012, 3700}, {30, 30, 3, 6, 204, 1000}, {40, 40, 4, 14, 420, 2100},
        {90, 30, 5, 42, 876, 3870}, {50, 50, 5, 22, 1252, 2750}, {31, 31, 3, 8, 416, 1500},
        {50, 65, 5, 23, 464, 2340}, {50, 50, 4, 9, 844, 2430}, {30, 80, 4, 18, 508, 2180},
        {60, 50, 5, 13, 360, 1940}, {45, 30, 4, 22, 1728, 6630},
    };
    for (int i = 0; i < static_cast<int>(sizeof(kMaps) / sizeof(kMaps[0])); ++i) {
        const auto& k = kMaps[i];
        if (!(((k.w == w && k.h == h) || (k.w == h && k.h == w)) && k.size == size && k.groups == groups)) continue;
        if (NOVA_MAPID_STRICT) {
            int gems = 0, gold = 0;
            if (!map_totals(gems, gold) || gems != k.gems || gold != k.gold) return -1;
        }
        return i;
    }
    return -1;
}

// Lineup per official map (order: ambush collision contest corners eight elkring expanse fragment interlock
// lanes monolith packs pilgrimage rows rushbait-v2 stampede swirl); -1 = the older draft rules.
#ifndef NOVA_ALL_LINEUP
#define NOVA_ALL_LINEUP -1
#endif
// Lineup for maps outside the official pool (unseen maps get the best overall lineup).
// v28: comp 2 (C&I, Stellar, Varg, Tavi, Ren): 61.8% vs v26 on realistic fresh maps (no bush camps,
// separated groups); comp 6's edge had come from bush-camp artifact maps.
// v24: comp 6 = the strongest measured headhunter vs frozen opponents on fresh maps
// (58.8% n=250 + 52.0% + 56.0% on two more suites = 56.7% over 450 games vs the v22 artifact).
#ifndef NOVA_FALLBACK_LINEUP
#define NOVA_FALLBACK_LINEUP 2
#endif
// NOVA_CLUSTER_DRAFT (v28): packed-camp unknown maps draft NOVA_CLUSTER_LINEUP instead of the fallback.
#ifndef NOVA_CLUSTER_DRAFT
#define NOVA_CLUSTER_DRAFT 1
#endif
#ifndef NOVA_CLUSTER_FRAC
#define NOVA_CLUSTER_FRAC 0.5f
#endif
#ifndef NOVA_CLUSTER_LINEUP
#define NOVA_CLUSTER_LINEUP 6
#endif
#ifndef NOVA_MAP_LINEUPS
// v30: ladder-meta lineups (modal comps of the platform's top-12 teams over 5.6k games): 19 everywhere, the
// top teams' 3-pick C&I+Stellar+Varg on expanse (2), their stampede casters (20), and our own rows set (8),
// which is itself a top-team choice there. Official-map order as in nova_map_id.
#define NOVA_MAP_LINEUPS 19, 19, 19, 19, 19, 19, 2, 19, 19, 19, 19, 19, 19, 8, 19, 20, 19
#endif
constexpr int kNovaMapLineup[17] = {NOVA_MAP_LINEUPS};

// NOVA_V36_DRAFT (v36, Agent 4): per-size picks for unknown, spread-out maps (where lineup 2 went), through the
// NOVA_UNSEEN3/4/5 hook. Clustered maps keep lineup 6 and official maps are unchanged. Confirmed vs v34 on 1,000
// fresh generated maps per size (v36/agent4/EXPERIMENTS.md E4, E6b, E6c):
//   3 picks: C&I, Stellar, Ren (lineup 19 order)    53.4% (51.2-55.5), +48 gems
//   4 picks: C&I, Ren, Ryth, Varg                   62.4% (60.4-64.3), +116 gems
//   5 picks: C&I, Stellar, Ren, Ryth, Varg (19)     53.0% (51.1-55.1), +19 gems
// 0 = v34 draft. An explicit NOVA_UNSEEN3/4/5 definition wins over this switch.
#ifndef NOVA_V36_DRAFT
#define NOVA_V36_DRAFT 1
#endif
#if NOVA_V36_DRAFT && !defined(NOVA_UNSEEN3) && !defined(NOVA_UNSEEN4) && !defined(NOVA_UNSEEN5)
#define NOVA_UNSEEN3 cd::cassi_and_issac, cd::stellar, cd::ren, cd::ryth, cd::varg, cd::tavi, cd::prism, cd::willow, \
    cd::vendetta, cd::marcella, cd::vesper_vale
#define NOVA_UNSEEN4 cd::cassi_and_issac, cd::ren, cd::ryth, cd::varg, cd::stellar, cd::tavi, cd::prism, cd::willow, \
    cd::vendetta, cd::marcella, cd::vesper_vale
#define NOVA_UNSEEN5 cd::cassi_and_issac, cd::stellar, cd::ren, cd::ryth, cd::varg, cd::tavi, cd::prism, cd::willow, \
    cd::vendetta, cd::marcella, cd::vesper_vale
#endif

void play_draft() {
    const auto chosen = bc::draft::current_draft(team::ally);
    [[maybe_unused]] std::uint32_t size = 0;
    try {
        size = bc::draft::team_size();
    } catch (...) {
    }
    // Draft memory (APEX_BRAIN): enemies' picks are visible and the pick order alternates, so record
    // what the champions need to know on every turn (the last write wins and includes all picks).
    try {
        const MapFacts mf = read_facts(); // cached; gives the biggest camp-group gem value too
        unsigned fl = 0;
        try {
            const int aw = bc::map::width(), ah = bc::map::height();
            // bit0 = dense-score-race map, bit1 = monolith map; both swallowed by an exception
            // guard. NOVA_CLASSIFY writes the bits even when the banking switches are off.
            if ((raceBank || classify) && mf.ok && aw > 0 && ah > 0 &&
                static_cast<float>(mf.gems) / static_cast<float>(aw * ah) >= raceDens)
                fl |= 1u;
        } catch (...) {
        }
        if ((monoBank || classify) && mf.ok && mf.monolith) fl |= 2u;
        brain::record_draft(size, mf.ok ? mf.max_gems : 0, fl);
    } catch (...) {
        try {
            brain::record_draft(size);
        } catch (...) {
        }
    }
    // Test overrides (v36): explicit pick lists win over every policy below. In v34 they sat after the NOVA lineup
    // block, which always returns, so a declared override never changed the draft. Release builds define none.
#ifdef APEX_DRAFT_T3
    if (size == 3 && pick_from(kT3, chosen)) return;
#endif
#ifdef APEX_DRAFT_T4
    if (size == 4 && pick_from(kT4, chosen)) return;
#endif
#ifdef APEX_DRAFT_T5
    if (size == 5 && pick_from(kT5, chosen)) return;
#endif
#ifdef APEX_DRAFT_LIST
    if (pick_from(kOverride, chosen)) return;
#endif
    {
        int w = 0, h = 0, ng = 0;
        try {
            w = bc::map::width();
            h = bc::map::height();
            std::uint32_t n = 0;
            if (bc::detail::sdk_map_num_camp_groups(&n) == 0) ng = static_cast<int>(n);
        } catch (...) {
        }
        const int mid = nova_map_id(w, h, size, ng);
#ifdef NOVA_MAPFACTS_PRINT
        // Probe builds only: the facts the draft can read, for building and checking the map-identity table.
        {
            static bool printed = false;
            if (!printed) {
                printed = true;
                const MapFacts f = read_facts();
                int altars = -1;
                try {
                    altars = static_cast<int>(bc::map::altars().positions().size());
                } catch (...) {
                }
                std::printf("MAPFACTS w=%d h=%d size=%u groups=%d gems=%d gold=%d maxgems=%d mono=%d altars=%d "
                            "cluster=%.3f mid=%d\n", w, h, size, ng, f.gems, f.gold, f.max_gems, f.monolith ? 1 : 0,
                    altars, cluster_frac(), mid);
            }
        }
#endif
        int li =NOVA_ALL_LINEUP >= 0 ? NOVA_ALL_LINEUP
                                      : (mid >= 0 ? kNovaMapLineup[mid] : NOVA_FALLBACK_LINEUP);
        // NOVA_CLUSTER_DRAFT (v28): unknown maps whose dangerous camp groups sit packed together
        // (another dangerous group within 3 tiles for at least NOVA_CLUSTER_FRAC of them) get the melee
        // splash comp; spread-out maps (like every official map but lanes/rushbait) the caster comp.
        if (NOVA_CLUSTER_DRAFT && mid < 0 && NOVA_ALL_LINEUP < 0) {
            const float frac = cluster_frac();
            if (frac >= NOVA_CLUSTER_FRAC) li = NOVA_CLUSTER_LINEUP;
        }
        // NOVA_DRAFT_AI: unknown maps choose by reading the map itself (monolith -> melee wrecking
        // crew; big and sparse -> the siege-farm caster comp; otherwise the proven fallback).
        if (NOVA_DRAFT_AI && mid < 0 && (w > 0 && h > 0)) {
            const MapFacts f = read_facts();
            const int area = w * h;
            if (f.ok) {
                if (f.monolith) li = NOVA_AI_MONO_LINEUP;
                else if (area >= NOVA_AI_BIG_AREA && area / std::max(f.groups, 1) >= NOVA_AI_SPARSE) li = NOVA_AI_BIG_LINEUP;
            }
        }
        // NOVA_COUNTER: on board enemy caster picks (picks alternate and are visible).
        bool skip = false;
        if (NOVA_COUNTER > 0) {
            try {
                int ec = 0;
                for (auto c : bc::draft::current_draft(team::enemy)) ec += brain::is_caster(c) ? 1 : 0;
                skip = ec >= NOVA_COUNTER;
            } catch (...) {
                skip = false;
            }
        }
#ifdef NOVA_UNSEEN_LIST
        // Draft search hook: an explicit pick list for unknown, spread-out maps (where comp 2 would go).
        {
            constexpr cd kUnseen[] = {NOVA_UNSEEN_LIST};
            if (mid < 0 && li == NOVA_FALLBACK_LINEUP && pick_from(kUnseen, chosen, skip)) return;
        }
#endif
#if defined(NOVA_UNSEEN3) && defined(NOVA_UNSEEN4) && defined(NOVA_UNSEEN5)
        // Per-pick-count lists for unknown, spread-out maps (the best set differs by draft size).
        {
            constexpr cd k3[] = {NOVA_UNSEEN3}, k4[] = {NOVA_UNSEEN4}, k5[] = {NOVA_UNSEEN5};
            if (mid < 0 && li == NOVA_FALLBACK_LINEUP) {
                if (size == 3 && pick_from(k3, chosen, skip)) return;
                if (size == 4 && pick_from(k4, chosen, skip)) return;
                if (size >= 5 && pick_from(k5, chosen, skip)) return;
            }
        }
#endif
        if (li >= 0 && li < kNumLineups && pick_from(kLineups[li], chosen, skip)) return;
    }
#ifdef APEX_MAPINFO
    {
        static bool printed = false;
        if (!printed) {
            printed = true;
            std::printf("MAPINFO %d %d %u\n", bc::map::width(), bc::map::height(), size);
        }
    }
#endif
    // Official maps first (APEX_MAPDRAFT), then the map-class draft for any other map.
    if (APEX_MAPDRAFT) {
        int w = 0, h = 0;
        try {
            w = bc::map::width();
            h = bc::map::height();
        } catch (...) {
        }
        const int li = map_lineup(w, h, size);
        if (li >= 0 && li < kNumLineups && pick_from(kLineups[li], chosen)) return;
    }
    if (APEX_CLASSDRAFT) {
        constexpr int kClassLineup[6] = {APEX_L0, APEX_L1, APEX_L2, APEX_L3, APEX_L4, APEX_L5};
        int w = 0, h = 0;
        try {
            w = bc::map::width();
            h = bc::map::height();
        } catch (...) {
        }
        const int li = kClassLineup[brain::map_class(w, h, static_cast<int>(size))];
        if (li >= 0 && li < kNumLineups && pick_from(kLineups[li], chosen)) return;
    }
    pick_from(kPresets[APEX_DRAFT], chosen);
}
