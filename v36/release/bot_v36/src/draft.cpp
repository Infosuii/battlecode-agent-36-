// Draft: farming speed first, then durability. Both teams may own the same champion.
#include "draft.h"
#include "common.h"
#include "strategy.h"
#include <cstdio>

#if NOVA_STRICT_MAP_CACHE && !NOVA_STRICT_MAP_ID
#error "NOVA_STRICT_MAP_CACHE requires NOVA_STRICT_MAP_ID"
#endif
#if NOVA_STRICT_MAP_CACHE
#include "draft_map_cache.h"
#endif

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

// Official map pool identity: width, height, team size and camp-group count.
int nova_map_id(int w, int h, unsigned size, int groups) {
    struct Key { int w, h; unsigned size; int groups; };
    constexpr Key kMaps[] = {
        {100, 100, 5, 12}, {41, 41, 3, 20}, {50, 50, 5, 14}, {71, 71, 5, 30}, {41, 30, 5, 8}, {30, 30, 5, 10},
        {80, 80, 3, 24}, {30, 30, 3, 6}, {40, 40, 4, 14}, {90, 30, 5, 42}, {50, 50, 5, 22}, {31, 31, 3, 8},
        {50, 65, 5, 23}, {50, 50, 4, 9}, {30, 80, 4, 18}, {60, 50, 5, 13}, {45, 30, 4, 22},
    };
    for (int i = 0; i < static_cast<int>(sizeof(kMaps) / sizeof(kMaps[0])); ++i) {
        const auto& k = kMaps[i];
        if (((k.w == w && k.h == h) || (k.w == h && k.h == w)) && k.size == size && k.groups == groups) return i;
    }
    return -1;
}

#if NOVA_STRICT_MAP_ID
#include "official_map_layouts.h"
#endif

int nova_official_map_id(int w, int h, unsigned size, int groups) {
#if NOVA_STRICT_MAP_ID
    // Every bot module belongs to one match. Cache failures as unknown, never
    // weaken recognition to the descriptor when any static SDK query fails.
    static int cached = -2;
    if (cached != -2) return cached;
    cached = -1;
    try {
#if NOVA_STRICT_MAP_CACHE
        if (!bc::is_draft()) {
            // Own team draft memory is an immutable startup snapshot. Never
            // re-run the expensive layout query when cache data is unavailable.
            const auto* data = reinterpret_cast<const std::uint8_t*>(bc::draft_memory.data());
            const int id = nova_cache::decode({data, bc::draft_memory.size()}, w, h, size, groups);
            if (id >= 0 && nova_map_id(w, h, size, groups) == id) cached = id;
            return cached;
        }
#endif
        const int descriptor = nova_map_id(w, h, size, groups);
        if (descriptor < 0) return cached;
        const nova_identity::Layout* expected = nullptr;
        for (const auto& layout : nova_identity::kLayouts)
            if (layout.id == descriptor && layout.w == w && layout.h == h && layout.picks == static_cast<int>(size))
                expected = &layout;
        if (!expected) return cached;
        bc::detail::metadata metadata{};
        int symmetry = -1;
        bc::detail::aabb spawn{};
        if (bc::detail::sdk_bc_metadata(&metadata) != 0 ||
            bc::detail::sdk_bc_map_symmetry(&symmetry) != 0 ||
            bc::detail::sdk_spawn_region(0, &spawn) != 0) return cached;
        if (metadata.width != w || metadata.height != h ||
            metadata.chunk_stride != static_cast<unsigned>(expected->stride) ||
            metadata.chunked_height != static_cast<unsigned>(expected->ch) || symmetry != expected->symmetry ||
            spawn.min_x != expected->spawn[0] || spawn.min_y != expected->spawn[1] ||
            spawn.max_x != expected->spawn[2] || spawn.max_y != expected->spawn[3]) return cached;
        const unsigned words = metadata.chunk_stride * metadata.chunked_height;
        if (expected->mask_words != 4 * words || metadata.board_bytes != words * sizeof(std::uint64_t)) return cached;
        const bc::bitboard* masks[] = {&bc::map::walls(), &bc::map::bushes(), &bc::map::holes(), &bc::map::altars()};
        for (unsigned mask = 0; mask < 4; ++mask)
            for (unsigned k = 0; k < words; ++k)
                if (masks[mask]->ptr()[k] != expected->masks[mask * words + k]) return cached;
        std::uint32_t count = 0;
        if (bc::detail::sdk_map_num_camp_groups(&count) != 0 || count != static_cast<unsigned>(groups) || count > 256)
            return cached;
        std::vector<std::uint32_t> handles(count);
        if (bc::detail::sdk_map_camp_groups(handles.data(), count, &count) != 0 || count != handles.size()) return cached;
        std::vector<std::vector<std::uint32_t>> canonical;
        for (auto handle : handles) {
            bc::detail::camp_group_info reward{};
            std::int32_t respawn = -1;
            std::uint32_t members = 0;
            if (bc::detail::sdk_map_camp_group(handle, &reward) != 0 ||
                bc::detail::sdk_bc_camp_group_max_respawn(handle, &respawn) != 0 || respawn < 0 ||
                bc::detail::sdk_map_camp_group_num_camps(handle, &members) != 0 || members == 0 || members > 512)
                return cached;
            std::vector<std::uint32_t> camps(members);
            if (bc::detail::sdk_map_camp_group_camps(handle, camps.data(), members, &members) != 0 || members != camps.size())
                return cached;
            std::vector<std::array<std::uint32_t, 3>> specs;
            for (auto camp : camps) {
                bc::detail::camp_info spec{};
                if (bc::detail::sdk_map_camp_info(camp, &spec) != 0 || spec.x < 0 || spec.x >= w || spec.y < 0 || spec.y >= h ||
                    spec.type < 0 || spec.type > 4) return cached;
                specs.push_back({static_cast<unsigned>(spec.type), static_cast<unsigned>(spec.x), static_cast<unsigned>(spec.y)});
            }
            std::sort(specs.begin(), specs.end());
            std::vector<std::uint32_t> row{reward.gems, reward.gold, static_cast<unsigned>(respawn), members};
            for (auto spec : specs) row.insert(row.end(), spec.begin(), spec.end());
            canonical.push_back(std::move(row));
        }
        std::sort(canonical.begin(), canonical.end());
        unsigned pos = 0;
        for (const auto& row : canonical) {
            if (pos >= expected->group_words || expected->groups[pos++] != row.size()) return cached;
            for (auto word : row)
                if (pos >= expected->group_words || expected->groups[pos++] != word) return cached;
        }
        if (pos != expected->group_words) return cached;
        return cached = descriptor;
    } catch (...) { return cached; }
#else
    (void)w; (void)h; (void)size; (void)groups;
    return -1;
#endif
}

bool nova_record_map_cache(std::array<std::uint8_t, 16>& bytes, unsigned actual_picks) {
#if NOVA_STRICT_MAP_ID && NOVA_STRICT_MAP_CACHE
    try {
        if (!bc::is_draft() || bc::constants::draft_memory_bytes < bytes.size()) return false;
        const int w = bc::map::width(), h = bc::map::height();
        std::uint32_t groups = 0;
        if (bc::detail::sdk_map_num_camp_groups(&groups) != 0 || groups > 65535) return false;
        const int id = nova_official_map_id(w, h, actual_picks, static_cast<int>(groups));
        return nova_cache::encode(bytes, w, h, actual_picks, static_cast<int>(groups), id);
    } catch (...) { return false; }
#else
    (void)bytes; (void)actual_picks;
    return false;
#endif
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
    {
        int w = 0, h = 0, ng = 0;
        try {
            w = bc::map::width();
            h = bc::map::height();
            std::uint32_t n = 0;
            if (bc::detail::sdk_map_num_camp_groups(&n) == 0) ng = static_cast<int>(n);
        } catch (...) {
        }
        const int mid = NOVA_STRICT_MAP_ID ? nova_official_map_id(w, h, size, ng) : nova_map_id(w, h, size, ng);
        int li = NOVA_ALL_LINEUP >= 0 ? NOVA_ALL_LINEUP
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
#ifdef APEX_MAPINFO
        std::printf("NOVA_MAPINFO %d %d %u %d %d %d\n", w, h, size, ng, mid, li);
#endif
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
    pick_from(kPresets[APEX_DRAFT], chosen);
}
