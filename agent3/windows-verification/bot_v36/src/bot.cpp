// Apex champion logic: farm camp groups efficiently, bank gems at altars, shop on spawn,
// and take good fights while avoiding bad ones.
#include "bot.h"
#include "strategy.h"
#include "fight.h"
#include "learner.h"

#include <span>
#include <string_view>
#include <tuple>
#include <variant>

namespace d = cambc::detail;

int nova_map_id(int w, int h, unsigned size, int groups); // draft.cpp: official map index, -1 = unseen

namespace {

bc::bitboard single(vec2 p) {
    bc::bitboard b;
    b.clear();
    b.set(p, true);
    return b;
}

bool in_bounds(vec2 p, int w, int h) { return p.x >= 0 && p.y >= 0 && p.x < w && p.y < h; }

// Shards a champion keeps buying, in order, with the total count wanted of each.
struct BuildStep {
    bc::item_type item;
    int total;
};

std::vector<BuildStep> build_for(bc::champion_type t) {
    using it = bc::item_type;
    const bool caster = t == bc::champion_type::stellar || t == bc::champion_type::cassi || t == bc::champion_type::prism
        || (NOVA_CASTER_SET >= 1 && (t == bc::champion_type::tavi))
        || (NOVA_CASTER_SET >= 2 && (t == bc::champion_type::willow || t == bc::champion_type::marcella));
    switch (tune::kBuild) {
    case 1: // raw power: strength first, health later
        return {{it::power_shard_of_strength, 20}, {it::power_shard_of_health, 10}, {it::power_shard_of_strength, 40},
            {it::power_shard_of_health, 60}};
    case 2: // survive camps first: jungle health emblem, then monster strength
        return {{it::jungle_shard_of_health, 5}, {it::monster_shard_of_strength, 5}, {it::power_shard_of_strength, 30},
            {it::power_shard_of_health, 60}};
    case 3: // balanced: alternate strength and health emblems
        return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_health, 5}, {it::power_shard_of_strength, 5},
            {it::durability_shard_of_health, 5}, {it::power_shard_of_strength, 30}, {it::power_shard_of_health, 60}};
    case 4: // the top teams' build (Mite, WHIM): passives taken as strength shards, strength only
        if (caster) break;
        return {{it::jungle_shard_of_strength, 5}, {it::power_shard_of_strength, 25}, {it::durability_shard_of_strength, 5},
            {it::power_shard_of_strength, 90}};
    case 5: // our order, with every shard as strength
        if (caster) break;
        return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_strength, 5}, {it::power_shard_of_strength, 30},
            {it::durability_shard_of_strength, 5}, {it::power_shard_of_strength, 90}};
    case 6: // our order with the home emblem (recall 3 turns) in place of durability
        if (caster) break;
        return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_health, 5}, {it::power_shard_of_strength, 30},
            {it::home_shard_of_health, 5}, {it::power_shard_of_health, 60}};
    case 7: // home emblem right after the two camp emblems
        if (caster) break;
        return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_health, 5}, {it::home_shard_of_health, 5},
            {it::power_shard_of_strength, 30}, {it::durability_shard_of_health, 5}, {it::power_shard_of_health, 60}};
    case 8: // v26 ladder study: the teams that beat us carried 2-3x our item health from round 100 on
        if (caster)
            return {{it::power_shard_of_health, 5}, {it::monster_shard_of_focus, 5}, {it::power_shard_of_health, 10},
                {it::power_shard_of_focus, 30}, {it::power_shard_of_health, 60}};
        return {{it::power_shard_of_health, 5}, {it::monster_shard_of_strength, 5}, {it::power_shard_of_health, 10},
            {it::power_shard_of_strength, 30}, {it::power_shard_of_health, 60}};
    case 10: // top bot B (4 decoded top-vs-top games): power main stat (or bruiser health) first, durability
             // health, then power of the main stat; casters and Ren take focus. No monster emblem.
    case 11: { // top bot A: jungle, durability and bruiser emblems of the main stat first, then power
        const bool foc = t == bc::champion_type::stellar || t == bc::champion_type::cassi || t == bc::champion_type::prism ||
            t == bc::champion_type::tavi || t == bc::champion_type::ren || t == bc::champion_type::willow;
        if (tune::kBuild == 10) {
            if (foc)
                return {{it::bruiser_shard_of_health, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_focus, 40},
                    {it::power_shard_of_health, 60}};
            return {{it::power_shard_of_strength, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_strength, 40},
                {it::power_shard_of_health, 60}};
        }
        if (foc)
            return {{it::jungle_shard_of_focus, 5}, {it::durability_shard_of_focus, 5}, {it::bruiser_shard_of_health, 5},
                {it::power_shard_of_focus, 40}, {it::power_shard_of_health, 60}};
        return {{it::jungle_shard_of_strength, 5}, {it::durability_shard_of_strength, 5}, {it::bruiser_shard_of_health, 5},
            {it::power_shard_of_strength, 40}, {it::power_shard_of_health, 60}};
    }
    case 12: { // consensus of the top-12 teams' purchases in 1,076 platform games (tools: summaries, buyItem):
               // durability health emblem (-10% damage taken) on almost every champion, bruiser health on the
               // frontline, Ren and Tavi on focus, jungle first for the junglers, monster emblems mostly skipped.
        using ct = bc::champion_type;
        switch (t) {
        case ct::stellar:
            return {{it::jungle_shard_of_focus, 5}, {it::power_shard_of_focus, 10}, {it::durability_shard_of_health, 5},
                {it::power_shard_of_focus, 25}, {it::bruiser_shard_of_health, 5}, {it::power_shard_of_focus, 60}};
        case ct::cassi:
            return {{it::jungle_shard_of_focus, 5}, {it::power_shard_of_focus, 5}, {it::bruiser_shard_of_health, 5},
                {it::durability_shard_of_focus, 5}, {it::power_shard_of_focus, 60}};
        case ct::issac:
            return {{it::jungle_shard_of_strength, 5}, {it::power_shard_of_strength, 5}, {it::durability_shard_of_health, 5},
                {it::power_shard_of_strength, 30}, {it::bruiser_shard_of_health, 5}, {it::power_shard_of_strength, 60}};
        case ct::ren:
            return {{it::bruiser_shard_of_health, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_focus, 40},
                {it::jungle_shard_of_focus, 5}, {it::power_shard_of_focus, 60}};
        case ct::ryth:
        case ct::marcella:
            return {{it::bruiser_shard_of_health, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_strength, 30},
                {it::jungle_shard_of_strength, 5}, {it::power_shard_of_strength, 60}};
        case ct::varg:
            return {{it::jungle_shard_of_strength, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_strength, 30},
                {it::durability_shard_of_strength, 5}, {it::power_shard_of_strength, 60}};
        case ct::tavi:
            return {{it::durability_shard_of_focus, 5}, {it::power_shard_of_focus, 10}, {it::durability_shard_of_health, 5},
                {it::power_shard_of_focus, 60}};
        case ct::prism:
            return {{it::bruiser_shard_of_focus, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_focus, 60}};
        case ct::vendetta:
            return {{it::jungle_shard_of_strength, 5}, {it::power_shard_of_strength, 5}, {it::monster_shard_of_strength, 5},
                {it::power_shard_of_strength, 60}};
        case ct::willow:
            return {{it::bruiser_shard_of_strength, 5}, {it::durability_shard_of_health, 5}, {it::power_shard_of_strength, 60}};
        case ct::vesper_vale:
            return {{it::durability_shard_of_health, 5}, {it::jungle_shard_of_strength, 5}, {it::monster_shard_of_strength, 5},
                {it::power_shard_of_strength, 60}};
        default: break;
        }
        break;
    }
    case 9: // health and the build's own stat alternating, durability emblem early
        if (caster)
            return {{it::power_shard_of_health, 5}, {it::power_shard_of_focus, 5}, {it::durability_shard_of_health, 5},
                {it::power_shard_of_focus, 30}, {it::power_shard_of_health, 60}};
        return {{it::power_shard_of_health, 5}, {it::power_shard_of_strength, 5}, {it::durability_shard_of_health, 5},
            {it::power_shard_of_strength, 30}, {it::power_shard_of_health, 60}};
    default: break;
    }
    // NOVA_CASSI_BUILD: every Cassi & Issac double death started with Cassi (700 health) dying, and
    // it costs two bodies; buy her health first (71 per shard), focus after.
    if (NOVA_CASSI_BUILD && t == bc::champion_type::cassi)
        return {{it::jungle_shard_of_health, 5}, {it::power_shard_of_health, 5}, {it::monster_shard_of_focus, 5},
            {it::power_shard_of_focus, 30}, {it::power_shard_of_health, 60}};
    // Casters' spells scale with focus (Stellar: 2.0x and 1.4x focus every 8 turns), and their
    // strength growth is only 3.5 per shard, so a focus shard is worth more than a strength one.
    if (caster && tune::kCasterFocus)
        return {{it::monster_shard_of_focus, 5}, {it::jungle_shard_of_health, 5}, {it::power_shard_of_focus, 30},
            {it::power_shard_of_health, 60}};
    if (caster)
        return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_health, 5}, {it::power_shard_of_focus, 5},
            {it::power_shard_of_strength, 30}, {it::power_shard_of_health, 60}};
    return {{it::monster_shard_of_strength, 5}, {it::jungle_shard_of_health, 5}, {it::power_shard_of_strength, 30},
        {it::durability_shard_of_health, 5}, {it::power_shard_of_health, 60}};
}

// Base stats and growth per shard (health, strength, focus), indexed by champion_type.
struct BaseStats {
    float hp, str, focus, ghp, gstr, gfocus;
};
constexpr BaseStats kBase[12] = {
    {1200, 50, 100, 100, 5, 10},  // ryth (+1% of bonus health as strength)
    {900, 45, 100, 100, 5, 10},   // vesper vale
    {800, 70, 100, 100, 5, 10},   // vendetta
    {700, 30, 70, 71.25f, 3.5f, 15}, // cassi
    {900, 40, 100, 95, 4.75f, 10}, // issac
    {750, 35, 100, 75, 3.5f, 10}, // stellar
    {700, 35, 80, 75, 3.5f, 10},  // prism
    {1200, 45, 50, 100, 5, 10},   // marcella
    {700, 45, 105, 50, 3, 5},     // willow
    {900, 40, 100, 100, 5, 10},   // ren
    {1000, 50, 100, 100, 5, 10},  // varg
    {750, 35, 100, 75, 5, 10},    // tavi
};
float items_of(bc::champion_type t, float max_hp, float str, float focus) {
    const int i = static_cast<int>(t);
    if (i < 0 || i >= 12) return 0.0f;
    const auto& b = kBase[i];
    const float bonus_hp = std::max(0.0f, max_hp - b.hp);
    if (t == bc::champion_type::ryth) str -= 0.01f * bonus_hp;
    return bonus_hp / b.ghp + std::max(0.0f, str - b.str) / b.gstr + std::max(0.0f, focus - b.focus) / b.gfocus;
}

} // namespace

// ---------------------------------------------------------------- setup

void Bot::init() {
    w.init();
    auto& me = bc::self();
    const auto allies = bc::champions(team::ally);
    for (std::size_t i = 0; i < allies.size(); ++i) {
        if (allies[i] == me) slot = static_cast<int>(i);
        if (i < ally_handle.size()) ally_handle[i] = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(allies[i]));
        if (i < slot_type.size()) {
            try {
                slot_type[i] = allies[i].champion_name(); // throws for allies out of vision
            } catch (...) {
                slot_type[i] = bc::champion_type::vesper_vale; // unknown until seen (never in a pack)
            }
        }
    }
    team_size = static_cast<int>(allies.size());
    my_id = static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(me));
    type = me.champion_name();
    const auto abilities = me.abilities();
    for (int s = 0; s < 2; ++s) {
        const auto& info = abilities[s].info();
        Spell sp;
        if (info) {
            sp.present = true;
            std::visit(
                [&](const auto& a) {
                    using A = std::decay_t<decltype(a)>;
                    sp.kind = A::kind;
                    sp.excludes_walk = a.exclusive_with_movement;
                    sp.camps = a.valid_targets.contains(bc::ability_target_mask::camps);
                    sp.enemies = a.valid_targets.contains(bc::ability_target_mask::enemies);
                    sp.allies = a.valid_targets.contains(bc::ability_target_mask::allies);
                    sp.ground = a.valid_targets.contains(bc::ability_target_mask::ground);
                    if constexpr (requires { a.range; }) sp.range = a.range;
                    if constexpr (requires { a.modifier; }) sp.modifier = a.modifier;
                    if constexpr (requires { a.radius; }) sp.radius = a.radius;
                },
                *info);
        }
        spells[s] = sp;
    }
    seen_claim.fill(-1);
    write_claim(-1);
    inited = true;
}

void Bot::refresh_self() {
    auto& me = bc::self();
    d::champion_info info{};
    d::check(d::sdk_champion(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(me)), &info));
    pos = {info.x, info.y};
    prev_hp = hp;
    hp = info.health;
    max_hp = std::max(1.0f, info.max_health);
    strength = info.strength;
    focus = info.focus;
    range = info.attack_range;
    melee = range <= 1;
    gems = static_cast<int>(me.gem_count());
    gold = static_cast<int>(me.gold_count());
    for (int s = 0; s < 2; ++s) {
        std::int32_t cd = 99;
        if (spells[s].present) d::sdk_champion_cooldown(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(me)), s, &cd);
        cooldown[s] = cd;
    }
    ally_score = static_cast<int>(bc::global_submitted_gems(team::ally));
    enemy_score = static_cast<int>(bc::global_submitted_gems(team::enemy));
}

bool Bot::spell_ready(int s) const { return spells[s].present && cooldown[s] == 0 && !cast_done; }

// ---------------------------------------------------------------- shared memory

std::uint64_t Bot::read_shared() const {
    std::uint64_t v = 0;
    bc::shared_memory.read(0, std::span<std::uint64_t, 1>{&v, 1});
    return v;
}

void Bot::write_claim(int group) {
    if (slot >= kClaimSlots) return;
    std::uint64_t v = read_shared();
    if (!inited && v == 0) v = (std::uint64_t{1} << (kClaimBits * kClaimSlots)) - 1; // first writer: all slots idle
    // 8 bits per slot (teams have 3-5 champions; 100x100 maps can hold over 125 groups):
    // 255 = alive without a target, 254 = dead, otherwise the claimed group index.
    const std::uint64_t code = group == kClaimDead ? kClaimCodeDead
        : group < 0 || group >= static_cast<int>(kClaimCodeDead) ? kClaimCodeIdle : static_cast<std::uint64_t>(group);
    v &= ~(kClaimMask << (slot * kClaimBits));
    v |= code << (slot * kClaimBits);
    bc::shared_memory.write(0, std::span<std::uint64_t, 1>{&v, 1});
    shared = v;
}

// A raw code in our claim slot (fight orders 112-118: engage enemy k, retreat).
void Bot::write_order(int code) {
    if (slot >= kClaimSlots) return;
    std::uint64_t v = read_shared();
    v &= ~(kClaimMask << (slot * kClaimBits));
    v |= (static_cast<std::uint64_t>(code) & kClaimMask) << (slot * kClaimBits);
    bc::shared_memory.write(0, std::span<std::uint64_t, 1>{&v, 1});
    shared = v;
}

void Bot::share_sighting(bool call_for_help) {
    // Publish the enemy nearest to us; teammates fold it into their threat map. With
    // call_for_help (we are fleeing it) bit 62 marks a distress call that allies may answer.
    const EnemyRec* best = nullptr;
    for (const auto& e : w.enemies)
        if (!best || cheb(e.pos, pos) < cheb(best->pos, pos)) best = &e;
    if (!best) return;
    std::uint64_t v = read_shared();
    if (!call_for_help && ((v >> kDistressBit) & 1)) {
        // Leave a teammate's fresh distress call in place.
        int age = (round & kSightRoundMask) - static_cast<int>((v >> kSightRound) & kSightRoundMask);
        if (age < 0) age += kSightRoundMask + 1;
        if (age <= 2) return;
    }
    v &= (std::uint64_t{1} << kSightX) - 1;
    v |= static_cast<std::uint64_t>(best->pos.x & 127) << kSightX;
    v |= static_cast<std::uint64_t>(best->pos.y & 127) << kSightY;
    v |= static_cast<std::uint64_t>(round & kSightRoundMask) << kSightRound;
    if (call_for_help) v |= std::uint64_t{1} << kDistressBit;
    bc::shared_memory.write(0, std::span<std::uint64_t, 1>{&v, 1});
    shared = v;
}

void Bot::read_sighting() {
#if APEX_EVENTS
    if ((shared >> 63) & 1) {
        // A teammate's group event: group (7 bits) and its respawn-ready round mod 256 (8 bits).
        const int gi = static_cast<int>((shared >> kSightX) & 127);
        const int mod = static_cast<int>((shared >> kSightY) & 255);
        const int base = round - 64;
        const int ready = base + ((mod - base) % 256 + 256) % 256;
        w.apply_event(gi, ready, round);
        return;
    }
#endif
    const int r = static_cast<int>((shared >> kSightRound) & kSightRoundMask);
    if ((shared >> kSightX) == 0) return; // nothing published yet
    int age = (round & kSightRoundMask) - r;
    if (age < 0) age += kSightRoundMask + 1;
    if (age > 10) return;
    const vec2 p{static_cast<int>((shared >> kSightX) & 127), static_cast<int>((shared >> kSightY) & 127)};
    if (p.x < w.width && p.y < w.height) {
        w.add_sighting(p, round - age);
        if ((shared >> kDistressBit) & 1) distress = {p, round - age};
    }
}

// Claims on a group by allies acting before us (lower slots) and after us.
std::pair<int, int> Bot::claims_split(int group) const {
    int lower = 0, higher = 0;
    for (int s = 0; s < std::min(team_size, kClaimSlots); ++s) {
        if (s == slot || s == duo_ignore_slot || claim_of(s) != group) continue;
        (s < slot ? lower : higher) += 1;
    }
    return {lower, higher};
}

bool Bot::open_call_by_lower() {
    for (int s = 0; s < std::min(team_size, slot); ++s) {
        const int c = claim_of(s);
        if (c >= static_cast<int>(w.groups.size())) continue;
        const auto& g = w.groups[c];
        if (g.state != GroupState::alive || g.harmless) continue;
        if (claim_count(c) + 1 < champions_needed(g, g.hp_left(w.camps))) return true;
    }
    return false;
}

void Bot::track_claims() {
    for (int s = 0; s < kClaimSlots; ++s) {
        const int c = claim_of(s);
        if (c != seen_claim[s]) {
            seen_claim[s] = c;
            claim_since[s] = round;
        }
    }
}

// Other claimants of `group` that were there before us (all of them when it is not our target).
int Bot::senior_claims(int group) const {
    const bool ours = group == target_group;
    int n = 0;
    for (int s = 0; s < std::min(team_size, kClaimSlots); ++s) {
        if (s == slot || s == duo_ignore_slot || claim_of(s) != group) continue;
        if (!ours || claim_since[s] < my_claim_since || (claim_since[s] == my_claim_since && s < slot)) ++n;
    }
    return n;
}

int Bot::claim_count(int group) const {
    int n = 0;
    for (int s = 0; s < kClaimSlots; ++s)
        if (s != slot && claim_of(s) == group) ++n;
    return n;
}

// ---------------------------------------------------------------- shopping

void Bot::shop() {
    PROF(shop);
    auto& me = bc::self();
    try {
        if (tune::kAltarLocks && gold >= 10 && me.can_buy(bc::consumable_type::altar_lock)) {
            me.buy(bc::consumable_type::altar_lock);
            gold -= 10;
        }
    } catch (const std::exception& e) {
        LOG("lock buy error %s\n", e.what());
    }
    if (gold < 30) return;
    try {
        const auto counts = me.item_count();
        for (const auto& step : build_for(type)) {
            int owned = static_cast<int>(counts[step.item]);
            if (const auto emblem = step.item.result()) owned += static_cast<int>(counts[*emblem]) * 5;
            int want = std::min(step.total - owned, gold / 30);
            if (want <= 0) continue;
            if (!me.can_buy(step.item, static_cast<std::uint32_t>(want))) return;
            me.buy(step.item, static_cast<std::uint32_t>(want));
            gold -= want * 30;
            if (gold < 30) return;
        }
    } catch (const std::exception& e) {
        LOG("shop error %s\n", e.what());
    }
}

// Emblem passives change how much damage we take from and deal to camps; the soak model uses them.
void Bot::refresh_emblems() {
    if (!tune::kEmblemModel) return;
    try {
        using it = bc::item_type;
        const auto counts = bc::self().item_count();
        auto has = [&](it a, it b, it c) { return counts[a] + counts[b] + counts[c] > 0; };
        camp_taken_mult = 1.0f;
        if (has(it::jungle_emblem_of_strength, it::jungle_emblem_of_focus, it::jungle_emblem_of_health)) camp_taken_mult *= 0.7f;
        if (has(it::durability_emblem_of_strength, it::durability_emblem_of_focus, it::durability_emblem_of_health))
            camp_taken_mult *= 0.9f;
        camp_dealt_mult =
            has(it::monster_emblem_of_strength, it::monster_emblem_of_focus, it::monster_emblem_of_health) ? 1.1f : 1.0f;
    } catch (const std::exception& e) {
        LOG("emblem error %s\n", e.what());
    }
}

// ---------------------------------------------------------------- movement

namespace {
// Sets a flag for the rest of a scope (dashes inside fights and hunts are combat moves).
struct FlagScope {
    bool& flag;
    explicit FlagScope(bool& f) : flag(f) { flag = true; }
    ~FlagScope() { flag = false; }
};
} // namespace

bool Bot::travel_dash_ok() const {
    if (tune::kSaveDash >= 2) return false;
    for (const auto& sg : w.sightings)
        if (round - sg.round <= 15 && cheb(sg.pos, pos) <= 15) return false;
    return true;
}

bool Bot::try_dash(const bc::distance_field& field, const bc::bitboard& blocked) {
    if (tune::kSaveDash && !combat_move && !travel_dash_ok()) return false;
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s) || spells[s].kind != bc::ability_kind::dash) continue;
        const std::uint16_t here = fdist(field, pos);
        if (here == kUnreach || here < 3) return false;
        vec2 best = pos;
        std::uint16_t best_d = here;
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy) {
                const vec2 q{pos.x + dx, pos.y + dy};
                if (!in_bounds(q, w.width, w.height) || bit(blocked, q)) continue;
                const std::uint16_t dq = fdist(field, q);
                if (dq + 1 < best_d) {
                    best_d = dq;
                    best = q;
                }
            }
        if (best == pos) return false;
        try {
            if (bc::self().abilities()[s].use(best)) {
                pos = best;
                cast_done = true;
                return true;
            }
        } catch (const std::exception& e) {
            LOG("dash error %s\n", e.what());
        }
        return false;
    }
    return false;
}

bool Bot::follow_field(const bc::distance_field& field, const bc::bitboard& blocked, bool allow_dash) {
    auto& me = bc::self();
    if (!me.can_move()) return false;
    if (allow_dash) try_dash(field, blocked);
    const std::uint16_t here = fdist(field, pos);
    if (here == 0) return walked;
    struct Cand {
        vec2 p;
        int score;
    };
    std::array<Cand, 8> cands{};
    int n = 0;
    for (vec2 dir : kDirs) {
        const vec2 q = pos + dir;
        if (!in_bounds(q, w.width, w.height) || bit(blocked, q)) continue;
        const std::uint16_t dq = fdist(field, q);
        if (dq == kUnreach || dq >= here) continue;
        // Prefer the steepest descent, then diagonals (they reveal more tiles).
        cands[n++] = {q, dq * 4 + ((dir.x != 0 && dir.y != 0) ? 0 : 1)};
    }
    std::sort(cands.begin(), cands.begin() + n, [](const Cand& a, const Cand& b) { return a.score < b.score; });
    for (int i = 0; i < n; ++i) {
        try {
            if (me.move_absolute(cands[i].p)) {
                pos = cands[i].p;
                walked = true;
                return true;
            }
        } catch (const std::exception& e) {
            LOG("move error %s\n", e.what());
        }
    }
    return walked;
}

bool Bot::walk_toward(const bc::bitboard& goals_in) {
    PROF(move);
    // A path search could suspend the turn (APEX_DET: a floor nearer its real cost, so it binds less often).
    if (bc::fuel_remaining() < (tune::kDet ? 9000 : tune::kMinMoveFuel)) return false;
    bc::bitboard blocked = w.static_blocked | w.unit_tiles;
    blocked.set(pos, false);
    const bc::bitboard goals = goals_in & ~blocked;
    if (goals.is_empty() || bit(goals, pos)) return false;
    bc::distance_field field;
    bc::bitboard visited;
    walk_boxed = false;
    field.compute_to(blocked, goals, single(pos), visited);
    if (fdist(field, pos) == kUnreach) {
        // Units block every route: plan over terrain only and let the step check occupancy.
        bc::bitboard terrain = w.static_blocked;
        terrain.set(pos, false);
        field.compute_to(terrain, goals_in & ~terrain, single(pos), visited);
        if (fdist(field, pos) == kUnreach) return false;
        walk_boxed = true;
    }
    if (tune::kThreatPath > 0 && !combat_move && hp < max_hp * tune::kThreatHp && travel_around_threats(field, blocked, goals))
        return walked;
    return follow_field(field, blocked, true);
}

// APEX_THREAT_PATH: take a route that keeps clear of recent enemy sightings when it is only a little
// longer than the shortest one. Returns false (use the shortest route) when no sighting is recent, no goal
// lies outside the zone, the detour is too long, or there is no fuel for a second search.
bool Bot::travel_around_threats(const bc::distance_field& field, const bc::bitboard& blocked, const bc::bitboard& goals) {
    if (bc::fuel_remaining() < tune::kMinMoveFuel * 2) return false;
    bc::bitboard zone;
    zone.clear();
    bool any = false;
    for (const auto& s : w.sightings) {
        const int age = round - s.round;
        if (age > tune::kThreatAge || cheb(s.pos, pos) > 20) continue;
        zone.draw_chebyshev(s.pos, static_cast<float>(tune::kThreatPath + age / 2));
        any = true;
    }
    if (!any) return false;
    clear_bit(zone, pos);
    const bc::bitboard safe_goals = goals & ~zone;
    if (safe_goals.is_empty()) return false;
    bc::distance_field safe;
    bc::bitboard visited;
    safe.compute_to(blocked | zone, safe_goals, single(pos), visited);
    const int around = fdist(safe, pos), direct = fdist(field, pos);
    if (around == kUnreach || around > direct + tune::kThreatDetour) return false;
    follow_field(safe, blocked | zone, true);
    return true;
}

// A bush tile no nearby enemy shares a cluster with: they cannot see it, so cannot target us there.
bool Bot::hidden_from_threats(vec2 q) {
    if (!tune::kHideLos) {
        if (w.bush_cluster(q) < 0 || w.warded(w.bush_cluster(q))) return false;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, q) <= 9 && !w.bush_hides(q, e.pos)) return false;
        return true;
    }
    // Full line of sight: out of every nearby enemy's vision (walls and bushes block it).
    if (threat_vision_round != round) {
        threat_vision_round = round;
        threat_vision.clear();
        for (const auto& e : w.enemies) {
            if (cheb(e.pos, pos) > 10 || threat_vision.size() >= 5) continue;
            w.vision_from(e.pos, threat_vision.emplace_back());
        }
    }
    for (const auto& vis : threat_vision)
        if (bit(vis, q)) return false;
    return true;
}

// NOVA_BUDDY: a fixed partner slot (-1 = none). Cassi and Issac pair (one fate); the others pair a melee
// champion with a ranged one where possible, in slot order; an odd champion out farms alone.
int Bot::buddy() {
    if (buddy_slot != -2) return buddy_slot;
    const int n = std::min(team_size, kClaimSlots);
    std::array<int, 8> partner;
    partner.fill(-1);
    auto melee_type = [](bc::champion_type t) {
        return t == bc::champion_type::ryth || t == bc::champion_type::vendetta || t == bc::champion_type::issac ||
            t == bc::champion_type::marcella || t == bc::champion_type::ren || t == bc::champion_type::varg;
    };
    int ci = -1, is = -1;
    for (int s = 0; s < n; ++s) {
        if (slot_type[static_cast<std::size_t>(s)] == bc::champion_type::cassi) ci = s;
        if (slot_type[static_cast<std::size_t>(s)] == bc::champion_type::issac) is = s;
    }
    if (ci >= 0 && is >= 0) {
        partner[static_cast<std::size_t>(ci)] = is;
        partner[static_cast<std::size_t>(is)] = ci;
    }
    std::vector<int> mel, rng;
    for (int s = 0; s < n; ++s)
        if (partner[static_cast<std::size_t>(s)] < 0) (melee_type(slot_type[static_cast<std::size_t>(s)]) ? mel : rng).push_back(s);
    auto link = [&](int a, int b) {
        partner[static_cast<std::size_t>(a)] = b;
        partner[static_cast<std::size_t>(b)] = a;
    };
    std::size_t i = 0, j = 0;
    for (; i < mel.size() && j < rng.size(); ++i, ++j) link(mel[i], rng[j]);
    std::vector<int> rest(mel.begin() + static_cast<std::ptrdiff_t>(i), mel.end());
    rest.insert(rest.end(), rng.begin() + static_cast<std::ptrdiff_t>(j), rng.end());
    for (std::size_t k = 0; k + 1 < rest.size(); k += 2) link(rest[k], rest[k + 1]);
    buddy_slot = slot < 8 ? partner[static_cast<std::size_t>(slot)] : -1;
    debug_block({ std::printf("BUDDY slot %d %s -> %d\n", slot, bc::to_string(type).c_str(), buddy_slot); });
    return buddy_slot;
}

// Where our buddy works: its claimed group, else where we see it; (-1, -1) when unknown, dead or alone.
vec2 Bot::buddy_anchor() {
    const int b = buddy();
    if (b < 0) return {-1, -1};
    const int c = claim_of(b);
    if (c >= 0 && c < static_cast<int>(w.groups.size()) && !w.groups[static_cast<std::size_t>(c)].camps.empty())
        return w.camps[static_cast<std::size_t>(w.groups[static_cast<std::size_t>(c)].camps[0])].pos;
    if (c == static_cast<int>(kClaimCodeDead)) return {-1, -1};
    for (const auto& a : w.allies)
        if (a.id == ally_handle[static_cast<std::size_t>(b)]) return a.pos;
    return {-1, -1};
}

// NOVA_FIGHT2 bit 4, regroup: flee toward the nearest healthy teammate that stands farther from the threat
// than we do (top bots keep a teammate within 3 tiles in 57% of contact turns, we in 28%).
bool Bot::regroup_from(vec2 threat) {
    const AllyRec* best = nullptr;
    int best_d = 1 << 20;
    const int mine = cheb(pos, threat);
    for (const auto& a : w.allies) {
        if (a.id == my_id || a.hp < a.max_hp * 0.3f) continue;
        const int d = cheb(a.pos, pos);
        if (d < 2 || d > 12 || cheb(a.pos, threat) <= mine) continue;
        if (d < best_d) {
            best_d = d;
            best = &a;
        }
    }
    if (!best) return false;
    bc::bitboard goal;
    goal.clear();
    goal.draw_chebyshev(best->pos, 1.0f);
    return walk_toward(goal);
}

bool Bot::retreat_from(vec2 threat) {
    auto& me = bc::self();
    if (!me.can_move()) return false;
    // Step to the free neighbour that is farthest from the threat, breaking ties toward spawn. A
    // hidden unit on the tile makes the move return false without spending it, so fall through to
    // the next-best tile.
    struct Cand {
        vec2 p;
        int score;
    };
    Cand cands[8];
    int n = 0;
    // Optionally run toward the nearest healthy ally: the chaser follows us into its reach.
    const AllyRec* friend_near = nullptr;
    if (tune::kFleeAlly > 0)
        for (const auto& a : w.allies)
            if (a.hp >= a.max_hp * 0.5f && cheb(a.pos, pos) <= 10 &&
                (!friend_near || cheb(a.pos, pos) < cheb(friend_near->pos, pos)))
                friend_near = &a;
    auto ally_pull = [&](vec2 q) { return friend_near ? tune::kFleeAlly * cheb(q, friend_near->pos) : 0; };
    // Spawn heals fully and enemies cannot enter it; APEX_FLEE_HOME weights the way home.
    const int home_w = 1 + tune::kFleeHome;
    // Distance from the threats: the nearest one, or (APEX_FLEE_MULTI, two or more enemies close)
    // every visible enemy weighted by its damage share.
    float total_dps = 0.0f;
    int close = 0;
    if (tune::kFleeMulti)
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= 8) ++close, total_dps += e.strength + e.focus * 0.25f;
    auto away = [&](vec2 q) {
        if (close < 2 || total_dps <= 0.0f) return cheb(q, threat) * 100;
        float sum = 0.0f;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= 8) sum += static_cast<float>(std::min(cheb(q, e.pos), 6)) * (e.strength + e.focus * 0.25f);
        return static_cast<int>(sum / total_dps * 100.0f);
    };
    int here = away(pos) - home_w * fdist(w.spawn_field, pos) - ally_pull(pos);
    if (tune::kBushHide && hidden_from_threats(pos)) here += tune::kBushHide;
    for (vec2 dir : kDirs) {
        const vec2 q = pos + dir;
        if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
        int sc = away(q) - home_w * static_cast<int>(fdist(w.spawn_field, q)) - ally_pull(q);
        if (tune::kBushHide && hidden_from_threats(q)) sc += tune::kBushHide;
        if (sc <= here) continue;
        int i = n++;
        for (; i > 0 && cands[i - 1].score < sc; --i) cands[i] = cands[i - 1];
        cands[i] = {q, sc};
    }
    for (int i = 0; i < n; ++i) {
        try {
            if (me.move_absolute(cands[i].p)) {
                pos = cands[i].p;
                walked = true;
                return true;
            }
        } catch (...) {
        }
    }
    return false;
}

// ---------------------------------------------------------------- combat helpers

float Bot::my_dps(const GroupRec* g) const {
    float dps = strength;
    for (int s = 0; s < 2; ++s) {
        const auto& sp = spells[s];
        if (!sp.present) continue;
        if (sp.kind == bc::ability_kind::attack_reset) dps += strength / 8.0f;
        if ((sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::cc || sp.kind == bc::ability_kind::aoe_damage)
            && sp.camps)
            dps += sp.modifier * focus / 8.0f;
    }
    if (type == bc::champion_type::varg && g && g->camps.size() > 1) dps *= 1.6f;
    if (g) dps *= camp_dealt_mult;
    return std::max(dps, 1.0f);
}

bool Bot::attack_unit(const bc::unit& target, vec2 where) {
    auto& me = bc::self();
    if (!me.can_attack() || cheb(pos, where) > range) return false;
    try {
        me.attack(target);
        return true;
    } catch (const std::exception& e) {
        LOG("attack error %s\n", e.what());
        return false;
    }
}

bool Bot::use_reset_and_attack(const bc::unit& target, vec2 where) {
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s) || spells[s].kind != bc::ability_kind::attack_reset) continue;
        try {
            if (bc::self().abilities()[s].use()) {
                cast_done = true;
                return attack_unit(target, where);
            }
        } catch (const std::exception& e) {
            LOG("reset error %s\n", e.what());
        }
    }
    return false;
}

bool Bot::cast_on_camp(int ci) {
    const auto& c = w.camps[ci];
    if (tune::kSpellHold & 1) {
        // An enemy is (or was just) near: keep the spells for it.
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= tune::kSpellHoldRange) return false;
        for (const auto& sg : w.sightings)
            if (round - sg.round <= tune::kSpellHoldAge && cheb(sg.pos, pos) <= tune::kSpellHoldRange) return false;
    }
    int best = -1;
    float best_value = 0;
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s)) continue;
        const auto& sp = spells[s];
        if (sp.excludes_walk && walked) continue;
        if (cheb(pos, c.pos) > static_cast<int>(sp.range)) continue;
        // Stuns win champion fights; on a camp they only add a little damage.
        if ((tune::kSpellHold & 2) && (sp.kind == bc::ability_kind::cc || (type == bc::champion_type::ryth && s == 0)))
            continue;
        float value = 0;
        if ((sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::cc) && sp.camps)
            value = sp.modifier * focus;
        else if (sp.kind == bc::ability_kind::aoe_damage && sp.camps) {
            int hits = 0;
            for (int other : w.groups[w.group_of_camp_tile(c.pos)].camps)
                if (!w.camps[other].dead && cheb(w.camps[other].pos, c.pos) <= static_cast<int>(sp.radius)) ++hits;
            value = sp.modifier * focus * static_cast<float>(hits);
        }
        if (value > best_value) {
            best_value = value;
            best = s;
        }
    }
    if (best < 0) return false;
    try {
        const auto ab = bc::self().abilities()[best];
        const bool ok = spells[best].kind == bc::ability_kind::aoe_damage ? ab.use(c.pos) : ab.use(c.camp);
        if (ok) cast_done = true;
#ifdef APEX_SPELLSTAT
        if (ok) ++spellstat_camp;
#endif
        return ok;
    } catch (const std::exception& e) {
        LOG("camp cast error %s\n", e.what());
        return false;
    }
}

bool Bot::cast_on_enemy(const EnemyRec& e) {
    int best = -1;
    float best_value = 0;
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s)) continue;
        const auto& sp = spells[s];
        if (sp.excludes_walk && walked) continue;
        // NOVA_AOE_AIM: a ground area spell reaches range + radius (aimed at a tile short of the target).
        const int reach_s = (NOVA_AOE_AIM && sp.kind == bc::ability_kind::aoe_damage && sp.ground)
            ? static_cast<int>(sp.range + sp.radius) : static_cast<int>(sp.range);
        if (!sp.enemies || cheb(pos, e.pos) > reach_s) continue;
        float value = 0;
        if (sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::aoe_damage) value = sp.modifier * focus;
        if (sp.kind == bc::ability_kind::cc) {
            value = sp.modifier * focus + 120.0f; // stuns stop escapes
            if (keep_channel && e.tributing()) continue; // the stun would end its channel and free it
            if (e.tributing()) value += 400.0f;  // and cancel a tribute channel
        }
        if (value > best_value) {
            best_value = value;
            best = s;
        }
    }
    if (best < 0) return false;
    try {
        const auto ab = bc::self().abilities()[best];
        vec2 aim = e.pos;
        if (NOVA_AOE_AIM && spells[best].kind == bc::ability_kind::aoe_damage && spells[best].ground) {
            // NOVA_AOE_AIM (from the review of OpenAI Five's offset targets): among the tiles within range whose area
            // still covers the target, take the one that also covers the most other enemies, without waking camps.
            const int rad = static_cast<int>(spells[best].radius);
            const int rng = static_cast<int>(spells[best].range);
            int best_hits = -1000;
            for (int dy = -rad; dy <= rad; ++dy)
                for (int dx = -rad; dx <= rad; ++dx) {
                    const vec2 t{e.pos.x + dx, e.pos.y + dy};
                    if (!in_bounds(t, w.width, w.height) || cheb(pos, t) > rng) continue;
                    int hits = 0;
                    for (const auto& o : w.enemies)
                        if (o.id != e.id && cheb(o.pos, t) <= rad) hits += 2;
                    for (const auto& c : w.camps)
                        if (!c.dead && cheb(c.pos, t) <= rad) hits -= 3;
                    if (dx == 0 && dy == 0) hits += 1; // ties: centre on the target
                    if (hits > best_hits) {
                        best_hits = hits;
                        aim = t;
                    }
                }
            if (best_hits == -1000) return false;
        }
        const bool ok = spells[best].kind == bc::ability_kind::aoe_damage ? ab.use(aim) : ab.use(e.unit);
        if (ok) cast_done = true;
#ifdef APEX_SPELLSTAT
        if (ok) ++spellstat_enemy;
#endif
        return ok;
    } catch (const std::exception& ex) {
        LOG("enemy cast error %s\n", ex.what());
        return false;
    }
}

// Shields and heals that do not depend on a target choice.
void Bot::support_casts() {
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s)) continue;
        const auto& sp = spells[s];
        try {
            // With APEX_SHIELD_PVP the shield is kept for champion fights (an enemy within 5).
            bool enemy_close = false;
            for (const auto& e : w.enemies) enemy_close |= cheb(e.pos, pos) <= 5;
            const bool want_shield = tune::kShieldPvp ? enemy_close && hp < max_hp * 0.95f
                                                      : hp < max_hp * 0.85f && (!w.enemies.empty() || target_group >= 0);
            if (sp.kind == bc::ability_kind::shield && want_shield) {
                if (bc::self().abilities()[s].use()) cast_done = true;
            } else if (sp.kind == bc::ability_kind::heal) {
                const AllyRec* best = nullptr;
                float best_missing = sp.modifier * focus * 0.6f;
                for (const auto& a : w.allies) {
                    if (cheb(a.pos, pos) > static_cast<int>(sp.range)) continue;
                    const float missing = a.max_hp - a.hp;
                    if (missing > best_missing) {
                        best_missing = missing;
                        best = &a;
                    }
                }
                if (best && bc::self().abilities()[s].use(best->unit)) cast_done = true;
            }
        } catch (const std::exception& e) {
            LOG("support cast error %s\n", e.what());
        }
    }
}

// ---------------------------------------------------------------- behaviours

namespace {
// Rough damage per turn: basic attacks plus an averaged spell (about 2x focus every 8 turns).
float enemy_dps(const EnemyRec& e) { return e.strength + e.focus * 0.25f; }
float ally_dps(const AllyRec& a) { return std::max(a.strength * 1.15f, 30.0f) + 12.0f; }
} // namespace

// Lock any altar within reach that has no lock: our team still tributes there, while enemies must
// spend three basic attacks on it first (their tribute attempts turn into those attacks).
void Bot::prelock_altars() {
    if (!tune::kPrelock || enemy_score < tune::kPrelockScore) return;
    auto& me = bc::self();
    try {
        auto& lock = me.consumables()[bc::consumable_type::altar_lock];
        if (lock.count() == 0) return;
        for (vec2 a : w.altars) {
            if (cheb(a, pos) > 2 || bc::vision::has_altar_lock(a)) continue;
            if (lock.use(a)) return;
        }
    } catch (const std::exception& ex) {
        LOG("prelock error %s\n", ex.what());
    }
}

// An enemy channelling a tribute at an altar we can reach: our lock cancels the channel.
bool Bot::try_lock_altar() {
    auto& me = bc::self();
    try {
        auto& lock = me.consumables()[bc::consumable_type::altar_lock];
        if (lock.count() == 0) return false;
        for (const auto& e : w.enemies) {
            if (!e.tributing() || e.gems < 10) continue;
            for (vec2 a : w.altars) {
                if (cheb(a, e.pos) > bc::constants::submit_gems_range || cheb(a, pos) > 2) continue;
                if (bc::vision::has_altar_lock(a)) continue;
                if (lock.use(a)) {
                    LOG("R%d LOCK altar (%d,%d) vs carrier gems=%d\n", round, a.x, a.y, e.gems);
                    return true;
                }
            }
        }
    } catch (const std::exception& ex) {
        LOG("lock error %s\n", ex.what());
    }
    return false;
}

// Bush ambush: a bush tile in a cluster that no visible enemy stands in is invisible to all of them,
// and attacking from it does not reveal us (bots/probe_bush). When such a tile here, or one step
// away, reaches an enemy, fight from it: hit from here, or step in (melee then still hits).
bool Bot::bush_ambush() {
    if (!tune::kBushAmbush || w.enemies.empty()) return false;
    // NOVA_AMBUSH_SAFE: cover does not stop wards, area spells or enemies stepping into the bush; a hurt
    // champion takes the usual fight-or-flee decision instead (Cassi died ambushing from 130 health).
    if (NOVA_AMBUSH_SAFE > 0.0f && hp < max_hp * NOVA_AMBUSH_SAFE) return false;
    auto& me = bc::self();
    auto hidden = [&](vec2 q) {
        const int cq = w.bush_cluster(q);
        if (cq < 0 || w.warded(cq)) return false;
        for (const auto& e : w.enemies)
            if (w.bush_cluster(e.pos) == cq) return false;
        return true;
    };
    // The enemy to hit from tile q: carried gems first, then lowest health; none on their spawn.
    auto victim_from = [&](vec2 q, float& key) -> const EnemyRec* {
        const EnemyRec* best = nullptr;
        key = 1e30f;
        for (const auto& e : w.enemies) {
            if (cheb(e.pos, q) > range || bit(w.enemy_spawn, e.pos)) continue;
            const float k = e.hp - 20.0f * static_cast<float>(e.gems);
            if (k < key) {
                key = k;
                best = &e;
            }
        }
        return best;
    };
    float key = 0.0f;
    const EnemyRec* victim = hidden(pos) ? victim_from(pos, key) : nullptr;
    vec2 step = pos;
    if (!victim && me.can_move()) {
        float best_key = 1e30f;
        for (vec2 dir : kDirs) {
            const vec2 q = pos + dir;
            if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q) || bit(w.hot_zone, q) ||
                !hidden(q))
                continue;
            const EnemyRec* v = victim_from(q, key);
            if (v && key < best_key) {
                best_key = key;
                step = q;
                victim = v;
            }
        }
    }
    if (!victim && tune::kBushAmbush >= 2 && hidden(pos)) {
        // From cover, spells reach further than a basic attack (4 tiles): cast at the weakest enemy in
        // spell reach. Casting does not reveal us either.
        const EnemyRec* best = nullptr;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= 4 && !bit(w.enemy_spawn, e.pos) && (!best || e.hp < best->hp)) best = &e;
        if (best && cast_on_enemy(*best)) {
            mode = "ambush-cast";
            return true;
        }
    }
    if (!victim) return false;
    if (step != pos) {
        try {
            if (!me.move_absolute(step)) return false; // an unseen unit holds the tile
        } catch (...) {
            return false;
        }
        pos = step;
        walked = true;
        if (!melee) {
            mode = "ambush-step"; // ranged: the step used the attack; we hit from cover next turn
            return true;
        }
    }
    mode = "ambush";
    cast_on_enemy(*victim);
    if (attack_unit(victim->unit, victim->pos)) use_reset_and_attack(victim->unit, victim->pos);
    return true;
}

// Turn order (APEX_PRIORITY): a champion in contact with enemies asks to act early next round, so
// it strikes, finishes a kill or steps out of reach before enemies placed later in the round act.
void Bot::update_priority() {
    if (!tune::kPriority) return;
    int nearest = 99;
    for (const auto& e : w.enemies) nearest = std::min(nearest, cheb(e.pos, pos));
    int want = 0;
    const int mode = tune::kPriority;
    if (nearest <= 8) {
        want = 100 + (8 - nearest) * 10;
        // 2, 5: the weaker we are, the earlier we act (escape before the chaser moves).
        want += (mode == 2 || mode == 5) ? static_cast<int>(60.0f * (1.0f - hp / max_hp)) : (hp < max_hp * 0.5f ? 5 : 0);
        // 3, 5: a kill is available in reach: act first and take it (and the bag).
        if (mode == 3 || mode == 5)
            for (const auto& e : w.enemies)
                if (cheb(e.pos, pos) <= range + 1 && e.hp <= strength * 2.0f + focus * 2.0f) want += 200;
    } else if (mode == 4 || mode == 5) {
        // 4, 5: an enemy was seen close by in the last two rounds: contact is likely next round.
        for (const auto& s : w.sightings)
            if (round - s.round <= 2 && cheb(s.pos, pos) <= 10) want = std::max(want, 50);
    }
    if (want == priority_set) return;
    try {
        bc::self().set_priority(want);
        priority_set = want;
    } catch (const std::exception& e) {
        LOG("priority error %s\n", e.what());
    }
}

// Ward reveal (APEX_WARD_REVEAL): a ward inside a bush cluster shows us everyone in it. Ward the bush
// that must hold an invisible attacker (health lost that no visible enemy or camp explains), or the
// bush an enemy seen close by last round vanished into. Returns true when a ward was placed.
// NOVA_WARD_AHEAD: keep our ward in the bush cluster beside us that no ally ward covers (top teams ward bushes ahead
// of time; we warded only after a surprise hit).
bool Bot::ward_ahead() {
    if (!w.enemies.empty()) return false;
    if (ward_pos.x >= 0 && cheb(ward_pos, pos) <= NOVA_WARD_AHEAD) return false; // our ward still covers this area
    auto& me = bc::self();
    try {
        if (me.ward_cooldown() > 0) return false;
    } catch (...) {
        return false;
    }
    const int mine = w.bush_cluster(pos);
    // Clusters already under an allied ward.
    std::array<int, 16> covered{};
    int n_cov = 0;
    try {
        const bc::bitboard wt = bc::vision::ward_tiles(team::ally);
        if (!wt.is_empty())
            for (vec2 p : wt.positions()) {
                const int c = w.bush_cluster(p);
                if (c >= 0 && n_cov < 16) covered[n_cov++] = c;
            }
    } catch (...) {
    }
    vec2 aim = pos;
    if (target_group >= 0 && target_group < static_cast<int>(w.groups.size()) && !w.groups[target_group].camps.empty())
        aim = w.camps[w.groups[target_group].camps[0]].pos;
    vec2 best{-1, -1};
    int best_d = 1 << 20;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            const vec2 q{pos.x + dx, pos.y + dy};
            if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q)) continue;
            const int c = w.bush_cluster(q);
            if (c < 0 || c == mine) continue;
            bool cov = false;
            for (int k = 0; k < n_cov; ++k) cov |= covered[k] == c;
            if (cov) continue;
            const int d = cheb(q, aim) * 4 + cheb(q, pos);
            if (d < best_d) {
                best_d = d;
                best = q;
            }
        }
    if (best.x < 0) return false;
    try {
        if (me.place_ward(best)) {
            ward_pos = best;
            return true;
        }
    } catch (const std::exception& e) {
        LOG("ward ahead error %s\n", e.what());
    }
    return false;
}

bool Bot::ward_reveal() {
    if (!tune::kWardReveal) return false;
    bool hit = prev_hp > 0.0f && hp < prev_hp - 1.0f;
    if (hit) {
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= std::max(e.range, 4) + 1) hit = false;
        if (bit(w.hot_zone, pos)) hit = false;
        for (const auto& c : w.camps)
            if (!c.dead && c.strength > 0 && cheb(c.pos, pos) <= c.range) hit = false;
    }
    vec2 lost{-1, -1};
    for (const auto& s : w.sightings) {
        if (round - s.round != 1 || cheb(s.pos, pos) > 4) continue;
        bool still = false;
        for (const auto& e : w.enemies) still |= cheb(e.pos, s.pos) <= 1;
        if (!still) lost = s.pos;
    }
    if (!hit && lost.x < 0 && tune::kWardReveal >= 2) {
        // Pre-emptive: an enemy seen within 8 tiles in the last 3 rounds and none visible now: a bush
        // within ward range could hold it. Aim the ward at the bush nearest that sighting.
        if (w.enemies.empty())
            for (const auto& s : w.sightings)
                if (round - s.round <= 3 && cheb(s.pos, pos) <= 8) lost = s.pos;
    }
    if (!hit && lost.x < 0) return NOVA_WARD_AHEAD > 0 && ward_ahead();
    const int mine = w.bush_cluster(pos);
    struct Cand {
        vec2 p;
        int d;
    };
    Cand cands[25];
    int n = 0;
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            const vec2 q{pos.x + dx, pos.y + dy};
            if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q)) continue;
            const int c = w.bush_cluster(q);
            if (c < 0 || c == mine) continue;
            const int d = lost.x >= 0 ? cheb(q, lost) : cheb(q, pos);
            int i = n++;
            for (; i > 0 && cands[i - 1].d > d; --i) cands[i] = cands[i - 1];
            cands[i] = {q, d};
        }
    if (n == 0) return false;
    auto& me = bc::self();
    try {
        if (me.ward_cooldown() > 0) return false;
        for (int i = 0; i < n && i < 3; ++i)
            if (me.place_ward(cands[i].p)) {
                ward_pos = cands[i].p;
                return true;
            }
    } catch (const std::exception& e) {
        LOG("ward reveal error %s\n", e.what());
    }
    return false;
}

bool Bot::hidden_here() {
    const int c = w.bush_cluster(pos);
    if (c < 0 || w.warded(c)) return false;
    for (const auto& e : w.enemies)
        if (w.bush_cluster(e.pos) == c) return false;
    return true;
}

// Counter-ambush: we lost health since our last turn, yet no visible enemy reaches us and no awake
// camp does either, so someone hides in a bush within spell reach. Healthy: step into an adjacent
// bush of another cluster (sharing its cluster shows us whoever is inside). Hurt: step to the free
// neighbour farthest from foreign bush tiles. Returns true when we moved.
bool Bot::counter_ambush() {
    if (!tune::kBushDef || prev_hp <= 0.0f || hp >= prev_hp - 1.0f) return false;
    for (const auto& e : w.enemies)
        if (cheb(e.pos, pos) <= std::max(e.range, 4) + 1) return false; // a visible enemy explains it
    if (bit(w.hot_zone, pos)) return false;                               // an awake camp explains it
    for (std::size_t ci = 0; ci < w.camps.size(); ++ci) {
        const auto& c = w.camps[ci];
        if (!c.dead && c.strength > 0 && cheb(c.pos, pos) <= c.range) return false;
    }
    auto& me = bc::self();
    if (!me.can_move()) return false;
    const int mine = w.bush_cluster(pos);
    auto foreign = [&](vec2 q) {
        const int c = w.bush_cluster(q);
        return c >= 0 && c != mine;
    };
    auto near_foreign = [&](vec2 q) {
        int best = 99;
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                const vec2 r{q.x + dx, q.y + dy};
                if (in_bounds(r, w.width, w.height) && foreign(r)) best = std::min(best, std::max(std::abs(dx), std::abs(dy)));
            }
        return best;
    };
    const int here = near_foreign(pos);
    if (here > 4) return false; // no bush could hold the attacker
    if (hp >= max_hp * 0.5f) {
        for (vec2 dir : kDirs) {
            const vec2 q = pos + dir;
            if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q) || !foreign(q)) continue;
            try {
                if (me.move_absolute(q)) {
                    pos = q;
                    walked = true;
                    mode = "counter-ambush";
                    return true;
                }
            } catch (...) {
            }
        }
    }
    vec2 best = pos;
    int best_d = here;
    for (vec2 dir : kDirs) {
        const vec2 q = pos + dir;
        if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q) || foreign(q)) continue;
        const int d = near_foreign(q);
        if (d > best_d) {
            best_d = d;
            best = q;
        }
    }
    if (best == pos) return false;
    try {
        if (me.move_absolute(best)) {
            pos = best;
            walked = true;
            mode = "evade-ambush";
            return true;
        }
    } catch (...) {
    }
    return false;
}

// Burst-kill search: for each visible enemy, try our options this turn (stay, or one step for
// melee; then basic attack plus either an attack reset or one damage spell, since only one
// ability cast fits in a turn). If the best combination kills, do it, even when about to flee.
bool Bot::try_burst_kill() {
    auto& me = bc::self();
    if (!me.can_attack()) return false;
    const EnemyRec* victim = nullptr;
    vec2 step{-1, -1};
    int spell_slot = -1;
    bool use_reset = false;
    float best_margin = 0.0f;
    for (const auto& e : w.enemies) {
        const int d = cheb(e.pos, pos);
        vec2 to{-1, -1};
        if (d > range) {
            // Only melee champions can step and still attack in the same turn.
            if (!melee || d != 2 || !me.can_move()) continue;
            for (vec2 dir : kDirs) {
                const vec2 q = pos + dir;
                if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
                if (cheb(q, e.pos) <= 1) {
                    to = q;
                    break;
                }
            }
            if (to.x < 0) continue;
        }
        const bool walks = to.x >= 0;
        const int dist_after = walks ? cheb(to, e.pos) : d;
        float best_extra = 0.0f;
        int slot = -1;
        bool reset = false;
        for (int s2 = 0; s2 < 2; ++s2) {
            if (!spell_ready(s2)) continue;
            const auto& sp = spells[s2];
            if (sp.kind == bc::ability_kind::attack_reset) {
                if (strength > best_extra) {
                    best_extra = strength;
                    slot = s2;
                    reset = true;
                }
            } else if ((sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::cc ||
                           sp.kind == bc::ability_kind::aoe_damage) &&
                sp.enemies && dist_after <= static_cast<int>(sp.range) && !(walks && sp.excludes_walk)) {
                const float dmg = sp.modifier * focus;
                if (dmg > best_extra) {
                    best_extra = dmg;
                    slot = s2;
                    reset = false;
                }
            }
        }
        const float burst = strength + best_extra;
        const float margin = burst - e.hp * 1.05f;
        if (margin < 0.0f) continue;
        // Prefer the kill worth most: carried gems first.
        const float score = margin + static_cast<float>(e.gems) * 20.0f;
        if (!victim || score > best_margin) {
            victim = &e;
            step = to;
            spell_slot = slot;
            use_reset = reset;
            best_margin = score;
        }
    }
    if (!victim) return false;
    if (step.x >= 0) {
        try {
            if (!me.move_absolute(step)) return false;
            pos = step;
            walked = true;
        } catch (...) {
            return false;
        }
    }
    mode = "burst-kill";
    if (spell_slot >= 0 && !use_reset) {
        try {
            const auto ab = me.abilities()[spell_slot];
            const bool ok = spells[spell_slot].kind == bc::ability_kind::aoe_damage ? ab.use(victim->pos) : ab.use(victim->unit);
            if (ok) cast_done = true;
        } catch (const std::exception& ex) {
            LOG("burst cast error %s\n", ex.what());
        }
        attack_unit(victim->unit, victim->pos);
    } else {
        if (attack_unit(victim->unit, victim->pos) && use_reset) use_reset_and_attack(victim->unit, victim->pos);
    }
    return true;
}

// Positioning search for melee fights: among our tile and the free neighbours that still reach
// the target, move to the one the other enemies threaten least next turn (an enemy threatens a
// tile it can reach with one step plus its range), then attack.
// Distance to the nearest enemy that fights in melee (casters kite these).
// Reach of our ready spells that can hit a champion (0 when none is ready or casting is no longer allowed).
int Bot::enemy_spell_reach() const {
    int r = 0;
    for (int s = 0; s < 2; ++s) {
        const auto& sp = spells[s];
        if (!spell_ready(s) || !sp.enemies || (sp.excludes_walk && walked)) continue;
        if (sp.kind != bc::ability_kind::damage && sp.kind != bc::ability_kind::cc && sp.kind != bc::ability_kind::aoe_damage)
            continue;
        r = std::max(r, static_cast<int>(sp.range));
    }
    return r;
}

int Bot::nearest_enemy_melee() const {
    int md = 99;
    for (const auto& e : w.enemies)
        if (e.range <= 1 && !e.tributing() && !e.recalling()) md = std::min(md, cheb(e.pos, pos));
    return md;
}

// APEX_KITE: a ranged champion steps to the neighbour that keeps enemy melee farthest away (capped at
// the kite distance), preferring tiles still in range of the target and closer to spawn. Shooting
// excludes moving for ranged champions, so a caster either holds a safe tile and fires or repositions.
bool Bot::kite_step(const EnemyRec& target) {
    auto& me = bc::self();
    if (!me.can_move()) return false;
    auto score = [&](vec2 q) {
        int md = 99;
        for (const auto& e : w.enemies)
            if (e.range <= 1 && !e.tributing() && !e.recalling()) md = std::min(md, cheb(e.pos, q));
        return 100 * std::min(md, tune::kKite) + (cheb(q, target.pos) <= range ? 20 : 0) -
            static_cast<int>(std::min<int>(fdist(w.spawn_field, q), 60));
    };
    const int here = score(pos);
    vec2 best = pos;
    int best_s = here;
    for (vec2 dir : kDirs) {
        const vec2 q = pos + dir;
        if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
        const int s = score(q);
        if (s > best_s) best_s = s, best = q;
    }
    if (best == pos) return false;
    try {
        if (me.move_absolute(best)) {
            pos = best;
            walked = true;
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool Bot::position_and_attack(const EnemyRec& target) {
    auto& me = bc::self();
    if (!melee || !me.can_move()) return false;
    auto exposure = [&](vec2 q) {
        float threat = 0.0f;
        for (const auto& e : w.enemies)
            if (e.id != target.id && cheb(q, e.pos) <= e.range + 1) threat += e.strength + e.focus * 0.25f;
        for (int ci : w.awake_camps_near(q)) threat += w.camps[ci].strength;
        return threat;
    };
    vec2 best = pos;
    float best_threat = cheb(pos, target.pos) <= range ? exposure(pos) : 1e9f;
    for (vec2 dir : kDirs) {
        const vec2 q = pos + dir;
        if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
        if (cheb(q, target.pos) > range) continue;
        const float t = exposure(q);
        if (t + 1.0f < best_threat) { // move only for a real improvement
            best_threat = t;
            best = q;
        }
    }
    if (best != pos) {
        try {
            if (me.move_absolute(best)) {
                pos = best;
                walked = true;
            }
        } catch (...) {
        }
    }
    return cheb(pos, target.pos) <= range;
}

// Dash to the tile within reach that is farthest from every visible enemy (ties toward spawn).
bool Bot::dash_away() {
    for (int s = 0; s < 2; ++s) {
        if (!spell_ready(s) || spells[s].kind != bc::ability_kind::dash) continue;
        struct Cand {
            vec2 p;
            int score;
        };
        Cand cands[24];
        int n = 0;
        auto safety = [&](vec2 q) {
            int d = 99;
            for (const auto& e : w.enemies) d = std::min(d, cheb(e.pos, q) - e.range);
            int sc = d * 100 - static_cast<int>(fdist(w.spawn_field, q));
            if (tune::kBushHide && hidden_from_threats(q)) sc += tune::kBushHide;
            return sc;
        };
        const int here = safety(pos);
        const int reach = std::max(1, static_cast<int>(spells[s].range));
        for (int dx = -reach; dx <= reach; ++dx)
            for (int dy = -reach; dy <= reach; ++dy) {
                const vec2 q{pos.x + dx, pos.y + dy};
                if ((dx == 0 && dy == 0) || !in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) ||
                    bit(w.unit_tiles, q) || n >= 24)
                    continue;
                const int sc = safety(q);
                if (sc <= here) continue;
                int i = n++;
                for (; i > 0 && cands[i - 1].score < sc; --i) cands[i] = cands[i - 1];
                cands[i] = {q, sc};
            }
        for (int i = 0; i < n && i < 4; ++i) {
            try {
                if (bc::self().abilities()[s].use(cands[i].p)) {
                    pos = cands[i].p;
                    cast_done = true;
                    return true;
                }
            } catch (const std::exception& e) {
                LOG("dash-away error %s\n", e.what());
            }
        }
        return false;
    }
    return false;
}

namespace {
// Champions whose slot 1 is a dash.
bool has_dash(bc::champion_type t) {
    return t == bc::champion_type::vendetta || t == bc::champion_type::ren || t == bc::champion_type::vesper_vale ||
        t == bc::champion_type::issac;
}
} // namespace

// Escape search (APEX_ESCAPE_SEARCH): plan the next tune::kEscDepth turns of a flight. Each nearby
// enemy chases one tile per turn along its own shortest path (two more, once its dash is ready), so
// after its k-th turn it may stand anywhere within k of where it is now: a melee enemy then hits a
// tile at walking distance at most k + 1 from it, a ranged one at most k - 1 + its range (a tributing
// or recalling enemy stays put). Each turn we may step to any free neighbour or stay; on the first,
// also dash and then step. A plan costs the damage it takes, a death penalty when that damage kills
// us, the exposure left at its end (enemies that still reach us next turn) and the walk home.
// Fuel (measured, probe12 and APEX_ESC_LOG): a capped field per chaser (~2.5k on 50x50, ~5k on
// 100x100) plus ~450 per tile in reach for the facts and ~130 per tile and turn for the search. With a
// dash ready the plan looks one turn less, so it never covers more than 81 tiles (~45k in all).
bool Bot::plan_escape() {
    PROF(escape);
    esc_plan = {};
    // Keep enough back that the turn never suspends (a suspended turn hands the chaser free hits).
    if (bc::fuel_remaining() < tune::kEscFuel || !bc::self().can_move()) {
#ifdef APEX_ESCDBG
        if (bc::fuel_remaining() < tune::kEscFuel) std::printf("ESCSKIP R%d fuel %llu\n", round, static_cast<unsigned long long>(bc::fuel_remaining()));
#endif
        return false;
    }
#ifdef APEX_ESCDBG
    std::printf("ESCRUN R%d fuel %llu\n", round, static_cast<unsigned long long>(bc::fuel_remaining()));
#endif
#ifdef APEX_ESC_LOG
    std::uint64_t fm[8] = {bc::fuel_remaining()};
#define ESC_AT(i) fm[i] = bc::fuel_remaining()
#else
#define ESC_AT(i) ((void)0)
#endif
    int dash_slot = -1;
    for (int s = 0; s < 2; ++s)
        if (spell_ready(s) && spells[s].kind == bc::ability_kind::dash) dash_slot = s;
    const int reach = dash_slot >= 0 ? 2 : 0; // extra radius a dash adds
    const int depth = std::clamp(tune::kEscDepth - (dash_slot >= 0 ? 1 : 0), 1, 4);
    const int R = depth + reach;
    // Window around us: radius up to 6 (4 turns + a dash), plus a border of one for neighbour reads.
    constexpr int kMaxR = 7, kW = 2 * kMaxR + 1, kCells = kW * kW;
    constexpr std::int32_t kNone = 0x3fffffff;

    struct Chaser {
        const EnemyRec* e;
        int dist, base, grow, dash_from, dps;
    };
    Chaser ch[3];
    int nch = 0;
    for (const auto& e : w.enemies) {
        const int d0 = cheb(e.pos, pos);
        if (d0 > depth + 2 + std::max(e.range, 1) + 4) continue; // cannot reach anything the plan can
        const Chaser c{&e, d0, 0, 1, 99, static_cast<int>(enemy_dps(e))};
        int i = nch < 3 ? nch++ : 3;
        if (i == 3 && d0 >= ch[2].dist) continue;
        if (i == 3) i = 2;
        for (; i > 0 && ch[i - 1].dist > d0; --i) ch[i] = ch[i - 1];
        ch[i] = c;
    }
    if (nch == 0) return false;
    ESC_AT(1);

    if (!esc_boards) {
        esc_boards = std::make_unique<EscapeBoards>();
        const bc::bitboard walls = bc::map::walls();
        const bc::bitboard altars = bc::map::altars();
        esc_boards->chase_blocked = walls | bc::map::holes() | altars | w.ally_spawn;
        esc_boards->dash_stop = walls | altars | w.enemy_spawn;
        esc_boards->root.clear();
    }
    auto& B = *esc_boards;
    ESC_AT(2);

    // Hit thresholds per chaser and turn (1..depth + 1; the last is "next turn" after the plan).
    int thr[3][6];
    for (int i = 0; i < nch; ++i) {
        auto& c = ch[i];
        const EnemyRec& e = *c.e;
        const bool still = e.tributing() || e.recalling();
        c.grow = still ? 0 : 1;
        c.base = e.range <= 1 ? 1 : (still ? e.range : e.range - 1);
        if (!still && has_dash(e.type)) {
            std::int32_t cd = 0;
            if (d::sdk_champion_cooldown(e.id, 1, &cd) != 0) cd = 0; // unknown: assume ready
            c.dash_from = std::max(1, static_cast<int>(cd) + 1);
        }
        for (int k = 1; k <= depth + 1; ++k) thr[i][k] = c.base + c.grow * k + (k >= c.dash_from ? 2 : 0);
        set_bit(B.root, e.pos);
        B.field[static_cast<std::size_t>(i)].compute(B.chase_blocked, B.root, B.visited,
            static_cast<std::uint16_t>(thr[i][depth + 1] + 6));
        clear_bit(B.root, e.pos);
    }
    ESC_AT(3);

    // One pass over the tiles in reach: walkability, the damage taken there at the end of each turn,
    // and the cost of ending the plan there (exposure next turn plus the walk home; 0 on our spawn,
    // which heals and which enemies cannot enter). Engine buffers are read directly.
    static constexpr int kExposure[6] = {20, 9, 5, 3, 1, 0}; // tenths of a chaser's hit, by margin 0..5
    const auto& L = bc::detail::layout();
    const std::uint16_t* hit_ptr[3] = {B.field[0].ptr(), B.field[1].ptr(), B.field[2].ptr()};
    const std::uint16_t* spawn_ptr = w.spawn_field.ptr();
    const std::uint64_t* blocked_ptr = w.static_blocked.ptr();
    const std::uint64_t* units_ptr = w.unit_tiles.ptr();
    const std::uint64_t* hot_ptr = w.hot_zone.ptr();
    std::uint8_t ok[kCells]; // 0 = blocked, 1 = walkable, 2 = walkable and free this turn
    std::int16_t dmg[4][kCells];
    std::int16_t leaf[kCells];
    std::int32_t a[kCells], b[kCells]; // plan keys of the previous and the current turn (see below)
    auto at = [](int dx, int dy) { return (dy + kMaxR) * kW + dx + kMaxR; };
    for (int dy = -R - 1; dy <= R + 1; ++dy)
        for (int dx = -R - 1; dx <= R + 1; ++dx) {
            const int c = at(dx, dy);
            a[c] = b[c] = kNone;
            ok[c] = 0;
        }
    for (int dy = -R; dy <= R; ++dy) {
        const int y = pos.y + dy;
        if (y < 0 || y >= w.height) continue;
        const std::uint32_t chunk_row = static_cast<std::uint32_t>(y) >> 3, in_row = static_cast<std::uint32_t>(y & 7) << 3;
        for (int dx = -R; dx <= R; ++dx) {
            const int x = pos.x + dx;
            if (x < 0 || x >= w.width) continue;
            const std::uint32_t word = chunk_row * L.chunk_stride + (static_cast<std::uint32_t>(x) >> 3);
            const std::uint32_t shift = in_row | static_cast<std::uint32_t>(x & 7);
            if ((blocked_ptr[word] >> shift) & 1u) continue;
            const int c = at(dx, dy);
            const bool free_now = (dx == 0 && dy == 0) || (!((units_ptr[word] >> shift) & 1u) && vec2{x, y} != esc_failed);
            ok[c] = free_now ? 2 : 1;
            const std::size_t fi = (chunk_row * L.chunked_width + (static_cast<std::uint32_t>(x) >> 3)) * 64 + shift;
            const int home = std::min<int>(spawn_ptr[fi], 60);
            if (home == 0) {
                for (int k = 0; k < depth; ++k) dmg[k][c] = 0;
                leaf[c] = 0;
                continue;
            }
            int per_turn[5] = {(hot_ptr[word] >> shift) & 1u ? 40 : 0, 0, 0, 0, 0}; // added from turn k on
            int expo = 0;
            for (int i = 0; i < nch; ++i) {
                const int h = std::min<int>(hit_ptr[i][fi], 250);
                int k = 1;
                while (k <= depth && h > thr[i][k]) ++k;
                if (k <= depth) per_turn[k - 1] += ch[i].dps;
                expo += ch[i].dps * kExposure[std::clamp(h - thr[i][depth + 1], 0, 5)];
            }
            int run = 0;
            for (int k = 0; k < depth; ++k) dmg[k][c] = static_cast<std::int16_t>(run += per_turn[k]);
            leaf[c] = static_cast<std::int16_t>(expo / 10 + tune::kEscHome * home);
        }
    }

    ESC_AT(4);
    // Plans as packed keys: (cost << 8) | first tile, so a min over keys keeps the cheapest plan and
    // remembers where it starts. Turn 1: a step or staying; or a dash, then a step (costs dash_cost).
    std::uint8_t land_of[kCells]; // per first tile: the dash landing, or 255 for a plain step
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int c = at(dx, dy);
            if (ok[c] != 2) continue;
            a[c] = (static_cast<std::int32_t>(dmg[0][c]) << 8) | c;
            land_of[c] = 255;
        }
    const int dash_cost = static_cast<int>(tune::kEscDashCost * max_hp);
    if (dash_slot >= 0) {
        const int dr = std::clamp(static_cast<int>(spells[dash_slot].range), 1, 2);
        for (int ly = -dr; ly <= dr; ++ly)
            for (int lx = -dr; lx <= dr; ++lx) {
                const int l = at(lx, ly);
                if ((lx == 0 && ly == 0) || ok[l] != 2) continue;
                if (bit(B.dash_stop, vec2{pos.x + lx, pos.y + ly})) continue;
                if (std::max(std::abs(lx), std::abs(ly)) == 2) {
                    // A two-tile dash needs a middle tile next to both ends that it can cross.
                    bool path = false;
                    for (int my = -1; my <= 1 && !path; ++my)
                        for (int mx = -1; mx <= 1 && !path; ++mx) {
                            if ((mx == 0 && my == 0) || std::abs(lx - mx) > 1 || std::abs(ly - my) > 1) continue;
                            const vec2 m{pos.x + mx, pos.y + my};
                            path = in_bounds(m, w.width, w.height) && !bit(B.dash_stop, m) && !bit(w.unit_tiles, m);
                        }
                    if (!path) continue;
                }
                for (int sy = -1; sy <= 1; ++sy)
                    for (int sx = -1; sx <= 1; ++sx) {
                        const int c = l + sy * kW + sx;
                        if (ok[c] != 2) continue;
                        const std::int32_t key = (static_cast<std::int32_t>(dmg[0][c] + dash_cost) << 8) | c;
                        if (key < a[c]) {
                            a[c] = key;
                            land_of[c] = static_cast<std::uint8_t>(l);
                        }
                    }
            }
    }
    ESC_AT(5);
    // Turns 2..depth: each tile takes the cheapest plan among its neighbours and itself (other units
    // are ignored after the first turn: they move too). Tiles a plan cannot reach stay at kNone.
    std::int32_t* prev = a;
    std::int32_t* cur = b;
    for (int k = 2; k <= depth; ++k) {
        const int rk = std::min(R, k + reach);
        for (int dy = -rk; dy <= rk; ++dy)
            for (int dx = -rk; dx <= rk; ++dx) {
                const int c = at(dx, dy);
                if (!ok[c]) continue;
                const std::int32_t* p = prev + c;
                const std::int32_t m = std::min(std::min(std::min(p[-kW - 1], p[-kW]), std::min(p[-kW + 1], p[-1])),
                    std::min(std::min(p[0], p[1]), std::min(std::min(p[kW - 1], p[kW]), p[kW + 1])));
                cur[c] = m >= kNone ? kNone : m + (static_cast<std::int32_t>(dmg[k - 1][c]) << 8);
            }
        std::swap(prev, cur);
    }

    ESC_AT(6);
    // The cheapest end: damage taken, a death penalty, and the end-tile cost.
    const int death = static_cast<int>(3.0f * max_hp) + 15 * gems;
    int best = 0x7fffffff, best_first = -1;
    const int rf = std::min(R, depth + reach);
    for (int dy = -rf; dy <= rf; ++dy)
        for (int dx = -rf; dx <= rf; ++dx) {
            const int c = at(dx, dy);
            const std::int32_t key = prev[c];
            if (key >= kNone || !ok[c]) continue;
            const int cost = key >> 8, f = key & 255;
            const int taken = cost - (land_of[f] != 255 ? dash_cost : 0);
            const int v = cost + leaf[c] + (static_cast<float>(taken) >= hp ? death : 0);
            if (v < best) {
                best = v;
                best_first = f;
            }
        }
    ESC_AT(7);
    if (best_first < 0) return false;
    auto cell_pos = [&](int c) { return vec2{pos.x + c % kW - kMaxR, pos.y + c / kW - kMaxR}; };
    esc_plan.valid = true;
    esc_plan.step = cell_pos(best_first);
    if (land_of[best_first] != 255) {
        esc_plan.dash_slot = dash_slot;
        esc_plan.land = cell_pos(land_of[best_first]);
    }
#ifdef APEX_ESC_LOG
    std::printf("ESCF R%d sel%llu boards%llu fields%llu facts%llu turn1%llu dp%llu score%llu R%d\n", round,
        static_cast<unsigned long long>(fm[0] - fm[1]), static_cast<unsigned long long>(fm[1] - fm[2]),
        static_cast<unsigned long long>(fm[2] - fm[3]), static_cast<unsigned long long>(fm[3] - fm[4]),
        static_cast<unsigned long long>(fm[4] - fm[5]), static_cast<unsigned long long>(fm[5] - fm[6]),
        static_cast<unsigned long long>(fm[6] - fm[7]), R);
    std::printf("ESC R%d %s p(%d,%d) hp%.0f chasers%d best%d dash%d land(%d,%d) step(%d,%d)\n", round, bc::to_string(type).c_str(),
        pos.x, pos.y, hp, nch, best, esc_plan.dash_slot, esc_plan.land.x, esc_plan.land.y, esc_plan.step.x, esc_plan.step.y);
#endif
    return true;
}

int Bot::run_escape(bool dash_only) {
    if (esc_round != round) {
        esc_round = round;
        esc_failed = {-1, -1};
        plan_escape();
    }
    auto& me = bc::self();
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!esc_plan.valid || (dash_only && esc_plan.dash_slot < 0)) return 0;
        if (esc_plan.dash_slot >= 0) {
            bool dashed = false;
            try {
                dashed = me.abilities()[esc_plan.dash_slot].use(esc_plan.land);
            } catch (const std::exception& e) {
                LOG("escape dash error %s\n", e.what());
            }
            if (dashed) {
                pos = esc_plan.land;
                cast_done = true;
                esc_plan.dash_slot = -1; // done; a replan from here plans steps only
                if (esc_plan.step != pos && me.can_move()) {
                    try {
                        if (me.move_absolute(esc_plan.step)) {
                            pos = esc_plan.step;
                            walked = true;
                        }
                    } catch (...) {
                    }
                }
                return 1;
            }
            esc_failed = esc_plan.land;
        } else {
            if (esc_plan.step == pos) return 2;
            if (!me.can_move()) return 0;
            try {
                if (me.move_absolute(esc_plan.step)) {
                    pos = esc_plan.step;
                    walked = true;
                    return 1;
                }
            } catch (...) {
            }
            esc_failed = esc_plan.step;
        }
        // The engine refused a tile (an unseen unit): plan once more around it.
        if (attempt == 0 && !plan_escape()) return 0;
    }
    return 0;
}

// We are losing the fight. Escape if something can open a gap (a dash, a stun on a lone chaser
// that still lets us walk, or a spawn we reach before dying); otherwise fight back, since moving
// away from an enemy that keeps pace only gives it free hits.
// Engine buff data: is this enemy stunned right now (NOVA_STUN_FOCUS)?
static bool enemy_stunned(const EnemyRec& e) {
    d::buff_data buffs[6];
    std::uint32_t nb = 0;
    if (d::sdk_champion_buff_data(e.id, buffs, 6, &nb) != 0) return false;
    for (std::uint32_t i = 0; i < nb && i < 6; ++i)
        if (buffs[i].type == 1 && buffs[i].data.stun.duration > 0) return true;
    return false;
}

bool Bot::escape_or_stand(const EnemyRec* target, const EnemyRec* nearest, float ttd) {
    auto& me = bc::self();
    int catching = 0; // enemies that can hit us next turn even if we step away
    const EnemyRec* chaser = nullptr;
    for (const auto& e : w.enemies)
        if (cheb(e.pos, pos) <= e.range) {
            ++catching;
            if (!chaser || cheb(e.pos, pos) < cheb(chaser->pos, pos)) chaser = &e;
        }
    if (catching == 0) return false; // stepping away keeps us out of reach: the usual flee
    const int spawn_steps = fdist(w.spawn_field, pos);
    // 1. Dash out of reach, then keep stepping away (dashes are walking-compatible). The escape
    //    search dashes only when its plan does (the dash and the step are both in the plan).
    if (tune::kEscapeSearch) {
        if (run_escape(true) == 1) {
            mode = "flee-dash";
            heading_home = hp < max_hp * 0.5f;
            return true;
        }
    } else if ((tune::kStand & 1) && dash_away()) {
        mode = "flee-dash";
        heading_home = hp < max_hp * 0.5f;
        retreat_from(nearest->pos);
        return true;
    }
    // 2. Stun the only enemy on us and step away.
    if ((tune::kStand & 2) && catching == 1 && chaser) {
        for (int s = 0; s < 2; ++s) {
            const auto& sp = spells[s];
            if (!spell_ready(s) || sp.kind != bc::ability_kind::cc || !sp.enemies || sp.excludes_walk) continue;
            if (cheb(pos, chaser->pos) > static_cast<int>(sp.range)) continue;
            try {
                if (me.abilities()[s].use(chaser->unit)) {
                    cast_done = true;
                    mode = "flee-stun";
                    heading_home = hp < max_hp * 0.5f;
                    retreat_from(chaser->pos);
                    return true;
                }
            } catch (const std::exception& e) {
                LOG("stun error %s\n", e.what());
            }
        }
    }
    // 3. Home is close enough to reach alive (enemies cannot enter our spawn), or standing is off:
    //    the usual flee.
    if (!(tune::kStand & 4) || spawn_steps <= 2 || static_cast<float>(spawn_steps) < ttd * 0.8f) return false;
    // 4. No way out: make the chaser pay. Hit whatever is in reach, lowest health first.
    mode = "stand";
    const EnemyRec* victim = nullptr;
    for (const auto& e : w.enemies)
        if (cheb(e.pos, pos) <= range && (!victim || e.hp < victim->hp)) victim = &e;
    if (!victim) victim = target ? target : nearest;
    cast_on_enemy(*victim);
    if (cheb(victim->pos, pos) <= range) {
        if (attack_unit(victim->unit, victim->pos)) use_reset_and_attack(victim->unit, victim->pos);
    } else {
        bc::bitboard goal;
        goal.clear();
        goal.draw_chebyshev(victim->pos, static_cast<float>(range));
        walk_toward(goal);
        if (cheb(victim->pos, pos) <= range) attack_unit(victim->unit, victim->pos);
    }
    return true;
}

// Losing, with a melee enemy adjacent (APEX_STAND2): walking away cannot shake it, so either reach
// our spawn while hitting it, or stand and spend everything on the best target in reach.
bool Bot::melee_caught() {
    const EnemyRec* chaser = nullptr;
    float chase_dps = 0.0f; // damage we keep taking per turn while walking away
    for (const auto& e : w.enemies) {
        const int d = cheb(e.pos, pos);
        if (e.range <= 1 && d <= 1) {
            chase_dps += enemy_dps(e);
            if (!chaser || e.hp < chaser->hp) chaser = &e;
        } else if (e.range > 1 && d <= e.range) {
            chase_dps += enemy_dps(e) * 0.5f; // ranged: shoots while we are in reach, then must walk
        }
    }
    if (!chaser) return false;
    for (int s = 0; s < 2; ++s)
        if (spell_ready(s) && spells[s].kind == bc::ability_kind::dash) return false; // a dash breaks contact
    auto& me = bc::self();
    const int spawn_steps = fdist(w.spawn_field, pos);
    const bool reach_home = spawn_steps != kUnreach && static_cast<float>(spawn_steps) * chase_dps < hp * 0.9f;
    if (reach_home && (tune::kStand2 & 1)) {
        mode = "flee-home";
        if (melee) attack_unit(chaser->unit, chaser->pos); // melee can hit and still walk
        bc::bitboard blocked = w.static_blocked | w.unit_tiles;
        blocked.set(pos, false);
        if (!me.can_move() || !follow_field(w.spawn_field, blocked, false)) retreat_from(chaser->pos);
        heading_home = true;
        return true;
    }
    if (reach_home || !(tune::kStand2 & 2)) return false;
    mode = "stand2";
    // Finish whatever we can kill this turn; otherwise the weakest enemy in reach.
    const float burst = strength * 2.0f + focus * 2.0f;
    const EnemyRec* victim = nullptr;
    for (const auto& e : w.enemies) {
        if (cheb(e.pos, pos) > range) continue;
        const bool kill = e.hp <= burst;
        const bool vkill = victim && victim->hp <= burst;
        if (!victim || (kill && !vkill) || (kill == vkill && e.hp < victim->hp)) victim = &e;
    }
    if (!victim) victim = chaser;
    cast_on_enemy(*victim);
    if (attack_unit(victim->unit, victim->pos)) use_reset_and_attack(victim->unit, victim->pos);
    return true;
}

bool Bot::handle_enemies() {
    if (w.enemies.empty()) return false;
    FlagScope combat{combat_move};
    PROF(enemies);
    if (!(tune::kTribDeny & 2)) try_lock_altar(); // (& 2: deny_tribute picks the moment instead)
    if (tune::kBurst && try_burst_kill()) return true;
    PROF_CHECK("e_lock");
    if (bush_ambush()) return true;
    PROF_CHECK("e_bush");

    // Damage the enemy can put on us next turns, and how long we last under it.
    float threat_on_me = 0;
    const EnemyRec* nearest = nullptr;
    int nearest_d = 1000;
    for (const auto& e : w.enemies) {
        const int dist = cheb(e.pos, pos);
        // An enemy parked on its own spawn (shopping, healing, idling) only matters when it can reach us
        // next turn; otherwise fleeing from it freezes us (rushbait-v2: adjacent spawns, 0 gems vs an idle team).
        if (NOVA_SPAWN_IGNORE && bit(w.enemy_spawn, e.pos) && dist > e.range + NOVA_SPAWN_IGNORE) continue;
        if (dist < nearest_d) {
            nearest_d = dist;
            nearest = &e;
        }
        if ((tune::kTribDeny & 1) && e.tributing()) continue; // it cannot act until its channel ends
        if (dist <= std::max(e.range + 2, 5)) threat_on_me += enemy_dps(e);
    }
    if (!nearest || nearest_d > 8) return false;
    const float ttd = threat_on_me > 0 ? hp / threat_on_me : 1e9f;
    // Gem carriers demand a bigger margin: dying hands the bag to the killer.
    const float risk = gems >= 60 ? 2.0f : gems >= 25 ? 1.4f : 1.0f;
    const float ci_risk = (type == bc::champion_type::cassi || type == bc::champion_type::issac) ? tune::kCiRisk : 1.0f;

    // Pick the enemy we kill fastest relative to what it is worth.
    const EnemyRec* target = nullptr;
    float target_ttk = 1e9f, best_key = 1e30f;
    const float mine = my_dps(nullptr);
    for (const auto& e : w.enemies) {
        const int dist = cheb(e.pos, pos);
        if (dist > range + 4) continue;
        if (tune::kHuntFix && bit(w.enemy_spawn, e.pos)) continue; // heals fully there; chasing is futile
        float dmg = dist <= range + 2 ? mine : mine * 0.5f;
        for (const auto& a : w.allies)
            if (cheb(a.pos, e.pos) <= a.range + 2) dmg += ally_dps(a);
        float ttk = e.hp / std::max(dmg, 1.0f);
        float worth = 1.0f + static_cast<float>(e.gems) / 30.0f;
        if (tgtNew && (!blendRace || (brain::state.ready && brain::state.race))) {
            // Distance discount: an enemy past standard reach pays one extra walk step per gap
            // (beyond the half-dps assumption the base formula already makes for range+3..4).
            if (dist > range + 2) ttk += static_cast<float>(dist - range - 2) * 1.0f;
            // Fire-focus: a target with big sustained dps is worth killing first — we deny
            // the damage it keeps dealing while the tank soaks.
            worth += tgtThreat * enemy_dps(e);
            // Finishing: wounded champions die cheap in practice (allies converge on low hp).
            if (e.hp < e.max_hp * tgtWoundedHp) worth += tgtWounded;
        }
        // NOVA_FIGHT2 bit 2, focus fire: every champion values the same wounded target more, so allies
        // converge on it (top bots score 2.06 champions per kill, we 1.64).
        if (NOVA_FIGHT2 & 2) worth *= 1.0f + NOVA_F2_FOCUS * (1.0f - e.hp / std::max(1.0f, e.max_hp));
        if (NOVA_STUN_FOCUS > 0.0f && enemy_stunned(e)) worth *= 1.0f + NOVA_STUN_FOCUS; // follow up a stun
        if (e.tributing()) worth += 2.0f; // its whole bag is on the table and it cannot run
        if (e.recalling()) worth += 0.5f;
        if (tune::kCiFocus && (e.type == bc::champion_type::cassi || e.type == bc::champion_type::issac)) {
            // Cassi and Issac die together: one kill removes two bodies and pays both bags.
            int partner_gems = 15; // unseen partner: assume a typical bag
            for (const auto& o : w.enemies)
                if (o.id != e.id && (o.type == bc::champion_type::cassi || o.type == bc::champion_type::issac)) partner_gems = o.gems;
            worth += 1.0f + static_cast<float>(partner_gems) / 30.0f;
        }
        const float key = ttk / worth;
        if (key < best_key) {
            best_key = key;
            target = &e;
            target_ttk = ttk;
        }
    }

    float aggression = tune::kAggression * ep_aggr;
    if (NOVA_RUSH_UNTIL > 0 && round < NOVA_RUSH_UNTIL && rush_active()) aggression *= NOVA_RUSH_AGGR;
    // Phase (APEX_PHASE_AGGR): the position evaluator values a kill at ~8 gems early and ~25 late, and
    // champion contact at -13 early and -2 late; so fights are worth less early and more late. Aggression
    // runs from APEX_PHASE_EARLY at round 0 to APEX_PHASE_LATE at round APEX_PHASE_ROUND.
    if (tune::kPhaseAggr) {
        const float t = std::min(1.0f, static_cast<float>(round) / static_cast<float>(tune::kPhaseRound));
        aggression *= tune::kPhaseEarly + (tune::kPhaseLate - tune::kPhaseEarly) * t;
    }
    if (tune::kNoDashAggr > 0.0f && spells[0].kind != bc::ability_kind::dash && spells[1].kind != bc::ability_kind::dash)
        aggression = tune::kNoDashAggr;
    if (tune::kLowHpAggr > 0.0f) aggression = 1.0f + (aggression - 1.0f) * std::min(1.0f, hp / (max_hp * tune::kLowHpAggr));
    const float self_ttd = melee ? 1.0f : tune::kCasterSelfTtd;
    bool winning = target && target_ttk * risk * ci_risk < std::min(ttd, 8.0f) * aggression;
    bool in_danger = threat_on_me > 0 && (ttd < tune::kDangerTtd * risk * ci_risk * self_ttd * ep_danger || hp < max_hp * tune::kDangerHp);
    const int outnumbered_k = round < NOVA_EARLY_SAFE_UNTIL ? NOVA_EARLY_SAFE_K : tune::kOutnumbered;
    if (outnumbered_k > 0) {
        // Most ladder deaths were a lone champion caught by two or more enemies: leave before they
        // close in, at any health, unless the fight is already a sure kill.
        int foes = 0, friends = 1;
        for (const auto& e : w.enemies) foes += cheb(e.pos, pos) <= 8;
        for (const auto& a : w.allies) friends += cheb(a.pos, pos) <= 5 && a.hp >= a.max_hp * 0.4f;
        const bool sure_kill = target && target_ttk <= 1.0f;
        if (foes >= friends + outnumbered_k && !sure_kill) {
            in_danger = true;
            winning = false;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
        }
    }
    if (NOVA_BURST_K > 0.0f) {
        // Burst-aware danger: what the visible enemies can land on us next turn (ready nukes within spell
        // reach, basic attacks within walking reach). Caster teams kill from 15-20% health in one turn.
        float burst = 0.0f;
        for (const auto& e : w.enemies) {
            const int dist = cheb(e.pos, pos);
            if (dist > 5) continue;
            if (dist <= e.range + 1) burst += e.strength;
            if (dist <= 5) {
                const auto kit = spell_kit(e.type, e.focus);
                bool ready = false;
                for (std::uint32_t s2 = 0; s2 < 2; ++s2) {
                    std::int32_t cd = 1;
                    if (d::sdk_champion_cooldown(e.id, s2, &cd) == 0 && cd <= 1) ready = true;
                }
                if (ready) burst += kit.nuke;
            }
        }
        const bool sure_kill = target && target_ttk <= 1.0f;
        float my_hp = hp;
        if (type == bc::champion_type::cassi || type == bc::champion_type::issac)
            for (const auto& a : w.allies)
                if (a.type == (type == bc::champion_type::cassi ? bc::champion_type::issac : bc::champion_type::cassi))
                    my_hp = std::min(my_hp, a.hp); // one fate: the weaker body is our health
        if (burst > 0.0f && my_hp < NOVA_BURST_K * burst && !sure_kill) {
            in_danger = true;
            winning = false;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
        }
    }
    if (tune::kCiOutnumbered >= 0 && (type == bc::champion_type::cassi || type == bc::champion_type::issac)) {
        // C&I outnumbered (APEX_CI_OUTNUMBERED = k): one pick, two lives on one fate. Cassi died from full health to
        // three enemies' burst in two turns, so the pair leaves when enemies within 8 reach the healthy allies
        // within 5 (us included) plus k, unless this turn is a sure kill.
        int foes = 0, friends = 1;
        for (const auto& e : w.enemies) foes += cheb(e.pos, pos) <= 8;
        for (const auto& a : w.allies) friends += cheb(a.pos, pos) <= 5 && a.hp >= a.max_hp * 0.4f;
        const bool sure_kill = target && target_ttk <= 1.0f;
        if (foes >= friends + tune::kCiOutnumbered && foes >= 2 && !sure_kill) {
            in_danger = true;
            winning = false;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
        }
    }
    if (tune::kSideHp > 0.0f && winning && target_ttk > 0.5f * ttd) {
        float ours = hp, theirs = 0.0f;
        for (const auto& a : w.allies)
            if (cheb(a.pos, pos) <= 6) ours += a.hp;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= 6) theirs += e.hp;
        if (ours < tune::kSideHp * theirs) winning = false;
    }
    // NOVA_COMMIT_RETREAT = f (0 = off): a hurt champion already on its way home (below f of max health) keeps
    // retreating while enemies are in view instead of re-entering the fight every turn (59% of our deaths to
    // champions came while fleeing; ladder: our champions spend 3x the top teams' time hurt near enemies).
    if (NOVA_COMMIT_RETREAT > 0.0f && heading_home && hp < max_hp * NOVA_COMMIT_RETREAT && nearest_d <= 8) {
        const bool sure_kill = target && target_ttk <= 1.0f;
        if (!sure_kill) {
            winning = false;
            in_danger = true;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
        }
    }
    // Team commander (APEX_TFC): a lower-slot ally within 8 that has issued a fight order leads; otherwise we
    // search the team plans ourselves and, with a clear verdict, issue the order in our claim slot.
    bool tfc_applied = false;
    if ((tune::kTfc || tune::packOn) && nearest_d <= tune::kTsRange && w.groups.size() < 112) {
        if (enemy_handles.empty())
            for (const auto& e : bc::champions(team::enemy)) enemy_handles.push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(e)));
        int order = 0;
        std::uint32_t order_target = 0;
        for (int s2 = 0; s2 < std::min(team_size, kClaimSlots) && s2 < slot; ++s2) {
            const int c = claim_of(s2);
            if (c < 112 || c > 118) continue;
            const AllyRec* lead = nullptr;
            for (const auto& al : w.allies)
                if (al.id == ally_handle[static_cast<std::size_t>(s2)]) lead = &al;
            if (!lead || cheb(lead->pos, pos) > 8) continue;
            order = c == 118 ? 2 : 1;
            if (order == 1 && static_cast<std::size_t>(c - 112) < enemy_handles.size()) order_target = enemy_handles[static_cast<std::size_t>(c - 112)];
            break;
        }
        bool commander = false;
        if (!order) {
            order = team_decide(order_target);
            commander = order != 0;
        }
        if (order == 1) {
            const EnemyRec* ft = nullptr;
            for (const auto& e : w.enemies)
                if (e.id == order_target) ft = &e;
            if (ft && (commander || hp >= max_hp * tune::kTfcFollowHp)) {
                target = ft;
                winning = true;
                in_danger = false;
                tfc_applied = true;
            }
            if (commander) {
                int k = 0;
                while (k < static_cast<int>(enemy_handles.size()) && enemy_handles[static_cast<std::size_t>(k)] != order_target) ++k;
                if (k < 6) write_order(112 + k);
            }
        } else if (order == 2) {
            winning = false;
            in_danger = true;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
            tfc_applied = true;
            if (commander) write_order(118);
        }
    }    if (!tfc_applied && tune::kTsDecide && target && nearest_d <= tune::kTsRange) {
        // Fight or run by search (tactical_decide): both plans played forward with the full kits and judged with
        // the learned exchange rates; a clear verdict overrides the formula, c1 still carries it out.
        const int verdict = tactical_decide();
        PROF_CHECK("e_td");
        if (verdict > 0) {
            winning = true;
            in_danger = false;
        } else if (verdict < 0) {
            winning = false;
            in_danger = true;
            if (threat_on_me <= 0) threat_on_me = 1.0f;
        }
    }
    if (tune::kFightSim && target && nearest_d <= 7 && bc::fuel_remaining() > 30000) {
        // Play the skirmish forward both ways and keep the better plan.
        std::vector<SimUnit> units;
        SimUnit m;
        m.team = 0;
        m.pos = pos;
        m.hp = hp;
        m.max_hp = max_hp;
        m.str = strength;
        m.range = range;
        m.death_cost = deathBase + deathBag * static_cast<float>(gems);
        for (int s = 0; s < 2; ++s) {
            const auto& sp = spells[s];
            if (!sp.present || !sp.enemies) continue;
            if (sp.kind != bc::ability_kind::damage && sp.kind != bc::ability_kind::cc &&
                sp.kind != bc::ability_kind::aoe_damage)
                continue;
            const float dmg = sp.modifier * focus;
            if (dmg > m.nuke) {
                m.nuke = dmg;
                m.stun = sp.kind == bc::ability_kind::cc;
                m.cd = cooldown[s];
            }
        }
        units.push_back(m);
        for (const auto& a : w.allies) {
            if (cheb(a.pos, pos) > 8 || units.size() >= 15) continue;
            SimUnit u;
            u.team = 0;
            u.pos = a.pos;
            u.hp = a.hp;
            u.max_hp = std::max(a.max_hp, 1.0f);
            u.str = a.strength;
            u.range = a.range;
            const auto kit = spell_kit(a.type, a.focus);
            u.nuke = kit.nuke;
            u.stun = kit.stun;
            u.spell_cd = kit.two_spells ? 4 : 8;
            u.death_cost = deathBase;
            units.push_back(u);
        }
        for (const auto& e : w.enemies) {
            if (cheb(e.pos, pos) > 9 || units.size() >= 16) continue;
            SimUnit u;
            u.team = 1;
            u.pos = e.pos;
            u.hp = e.hp;
            u.max_hp = std::max(e.max_hp, 1.0f);
            u.str = e.strength;
            u.range = e.range;
            const auto kit = spell_kit(e.type, e.focus);
            u.nuke = kit.nuke;
            u.stun = kit.stun;
            u.spell_cd = kit.two_spells ? 4 : 8;
            u.death_cost = deathBase + deathBag * static_cast<float>(e.gems);
            units.push_back(u);
        }
        const auto engage = simulate_fight(units, false, tune::kSimTurns);
        const auto retreat = simulate_fight(units, true, tune::kSimTurns);
        const float margin = gems >= 25 ? 40.0f : 0.0f; // carriers need a clearer edge
        winning = engage.score > retreat.score + margin;
        in_danger = !winning && threat_on_me > 0 && nearest_d <= 5;
    }
    // NOVA_COMMIT_LATE = f (0 = off): the same retreat commitment as NOVA_COMMIT_RETREAT, applied after the
    // tactical search and the fight simulation so their verdicts cannot send a hurt, homebound champion back in.
    if (NOVA_COMMIT_LATE > 0.0f && heading_home && hp < max_hp * NOVA_COMMIT_LATE && nearest_d <= 8 &&
        !(target && target_ttk <= 1.0f)) {
        winning = false;
        in_danger = true;
        if (threat_on_me <= 0) threat_on_me = 1.0f;
    }
    // NOVA_FIGHT2 bit 1, strength gate (v29 fight rewrite; 4 decoded top-vs-top games: an enemy within 4
    // tiles in only 3% of rounds, casters 5-8 tiles from enemy melee, 78% of contact while locally ahead).
    // Local strength = summed health x summed damage (spell burst spread over a short fight) within
    // NOVA_F2_RADIUS. Clearly stronger: fight the target. Clearly weaker: leave before contact, at any health.
    if ((NOVA_FIGHT2 & 1) && nearest) {
        auto eff = [](float base, bc::champion_type t, float foc) { return base + spell_kit(t, foc).nuke / 4.0f; };
        float us_hp = hp, us_dps = eff(my_dps(nullptr), type, focus);
        for (const auto& a : w.allies) {
            if (a.id == my_id || cheb(a.pos, pos) > NOVA_F2_RADIUS) continue;
            us_hp += a.hp;
            us_dps += eff(ally_dps(a), a.type, a.focus);
        }
        float th_hp = 0, th_dps = 0;
        int foes = 0;
        for (const auto& e : w.enemies) {
            const int de = cheb(e.pos, pos);
            if (de > NOVA_F2_RADIUS + 1) continue;
            if (bit(w.enemy_spawn, e.pos) && de > e.range + 2) continue;
            th_hp += e.hp;
            th_dps += eff(enemy_dps(e), e.type, e.focus);
            ++foes;
        }
        if (foes > 0) {
            const float ratio = (us_hp * us_dps) / std::max(1.0f, th_hp * th_dps);
            if (ratio >= NOVA_F2_ENGAGE && target && hp >= max_hp * 0.3f) {
                winning = true;
                in_danger = false;
            } else if (ratio <= NOVA_F2_AVOID && nearest_d <= NOVA_F2_REACH) {
                winning = false;
                in_danger = true;
                if (threat_on_me <= 0) threat_on_me = 1.0f;
            }
        }
    }
#ifdef APEX_FIGHTLOG
    // Fight dataset (analysis only): one line per contact turn; deaths are logged in play_turn.
    if (nearest_d <= 4) {
        int ne = 0, na = 0;
        float ehp = 0, edps = 0, ahp = 0, adps = 0;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= 6) ++ne, ehp += e.hp, edps += enemy_dps(e);
        for (const auto& a : w.allies)
            if (cheb(a.pos, pos) <= 6) ++na, ahp += a.hp, adps += ally_dps(a);
        bool dash = false;
        for (int s2 = 0; s2 < 2; ++s2) dash |= spell_ready(s2) && spells[s2].kind == bc::ability_kind::dash;
        std::printf("FT R%d %s hp%.0f/%.0f s%.0f f%.0f g%d T %s hp%.0f/%.0f s%.0f d%d E%d ehp%.0f edps%.0f A%d ahp%.0f adps%.0f ttk%.2f ttd%.2f win%d dang%d dash%d sp%d\n",
            round, bc::to_string(type).c_str(), hp, max_hp, strength, focus, gems,
            target ? bc::to_string(target->type).c_str() : "-", target ? target->hp : 0.0f, target ? target->max_hp : 0.0f,
            target ? target->strength : 0.0f, target ? cheb(target->pos, pos) : 99, ne, ehp, edps, na, ahp, adps,
            target_ttk > 1e8f ? 99.0f : target_ttk, ttd > 1e8f ? 99.0f : ttd, winning ? 1 : 0, in_danger ? 1 : 0, dash ? 1 : 0,
            static_cast<int>(fdist(w.spawn_field, pos)));
    }
#endif
    if (tune::kTribDeny & 2) {
        if (in_danger && !winning) try_lock_altar(); // leaving: cancel whatever tribute we can reach
        else if (deny_tribute()) return true;
    }
    PROF_CHECK("e_mid");
    if (in_danger && !winning) last_flee_round = round;
    if (in_danger && !winning && tune::kBushRecall && hp < max_hp * 0.5f && hidden_here() && !bit(w.hot_zone, pos) &&
        fdist(w.spawn_field, pos) > tune::kShopWalkDist) {
        // Losing, hurt and hidden: recall from cover (nobody can hit us here to cancel it).
        try {
            bc::self().recall();
            mode = "bush-recall";
            heading_home = true;
            return true;
        } catch (const std::exception& e) {
            LOG("bush recall error %s\n", e.what());
        }
    }
    // Tactical search (APEX_TSEARCH): play the candidate actions forward and take the best line, in the
    // turns where we would fight or flee anyway (otherwise farming goes on).
    const bool c1_acts = (in_danger && !winning) ||
        (target && (winning || (cheb(target->pos, pos) <= range && ttd > 6.0f * risk)));
    if (tune::kTSearch && c1_acts && nearest_d <= tune::kTsRange && tactical_search()) {
        mode = "tsearch";
        heading_home = in_danger && !winning && hp < max_hp * 0.5f;
        return true;
    }
    if (in_danger && !winning && tune::kStand2 && melee_caught()) return true;
    // Caster micro (APEX_CMICRO): ranged champions pick stay-and-shoot or a step by damage dealt against damage risked.
    if (tune::kCMicro && (!melee || NOVA_CMICRO_MELEE) && nearest && nearest_d <= tune::kCMicroRange &&
        (type != bc::champion_type::cassi || tune::kCMicroCassi == 1 || (tune::kCMicroCassi == 2 && [&] {
            // Auto (2): Cassi uses the micro in caster lineups only; beside a melee lineup she fights with Issac.
            if (team_casters < 0) {
                team_casters = 0;
                for (const auto& c : bc::champions(team::ally)) {
                    d::champion_info info{};
                    if (d::sdk_champion(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)), &info) != 0) continue;
                    const auto ct = static_cast<bc::champion_type>(info.type);
                    team_casters += ct == bc::champion_type::stellar || ct == bc::champion_type::tavi ||
                        ct == bc::champion_type::prism || ct == bc::champion_type::willow;
                }
            }
            return team_casters > 0;
        }())) &&
        !(in_danger && !winning && hp < max_hp * tune::kCMicroFleeHp) && caster_micro(target, winning || !in_danger)) {
        mode = "cmicro";
        heading_home = in_danger && !winning && hp < max_hp * 0.5f;
        return true;
    }
    PROF_CHECK("e_prestand");
    if (in_danger && !winning && tune::kStand && escape_or_stand(target, nearest, ttd)) return true;
    PROF_CHECK("e_stand");
    if (in_danger && !winning) {
        if (tune::kStaleBag > 0 && gems >= tune::kStaleBag) {
            // The enemy holds this altar (the collision/corners bank stalls: carriers approach, flee and return
            // for 250+ rounds). Send the next banking trip to another altar.
            for (std::size_t i = 0; i < w.altars.size() && i < altar_watch.size(); ++i)
                if (cheb(w.altars[i], nearest->pos) <= 6 || cheb(w.altars[i], pos) <= 6) altar_watch[i] = round + tune::kStaleWatch;
        }
        mode = "flee";
        if (tune::kRangedFlee && !melee && nearest && cheb(nearest->pos, pos) <= 4) {
            // Ranged flight (APEX_RANGED_FLEE, from apex_v21): a ranged basic attack ends our movement. Stun the
            // nearest chaser when the stun still lets us walk, step away, and shoot only when no step is possible.
            for (int s2 = 0; s2 < 2; ++s2) {
                const auto& sp = spells[s2];
                if (!spell_ready(s2) || sp.kind != bc::ability_kind::cc || !sp.enemies || sp.excludes_walk) continue;
                if (cheb(nearest->pos, pos) > static_cast<int>(sp.range)) continue;
                try {
                    if (bc::self().abilities()[s2].use(nearest->unit)) cast_done = true;
                } catch (const std::exception& ex) {
                    LOG("flee stun error %s\n", ex.what());
                }
                break;
            }
            if ((tune::kKiteCast & 4) && !cast_done && !melee) {
                int foes = 0;
                for (const auto& e : w.enemies) foes += cheb(e.pos, pos) <= 5;
                if (foes == 1 && nearest->range <= 1) {
                    for (int s2 = 0; s2 < 2; ++s2) {
                        const auto& sp = spells[s2];
                        if (!spell_ready(s2) || sp.kind != bc::ability_kind::cc || !sp.enemies || !sp.excludes_walk) continue;
                        if (cheb(nearest->pos, pos) > static_cast<int>(sp.range)) continue;
                        try {
                            if (bc::self().abilities()[s2].use(nearest->unit)) {
                                cast_done = true;
                                mode = "flee-stun";
                                return true;
                            }
                        } catch (const std::exception& ex) {
                            LOG("flee stun error %s\n", ex.what());
                        }
                    }
                }
            }
            const int planned_r = tune::kEscapeSearch ? run_escape(false) : 0;
            const bool moved_r = planned_r == 1 || (planned_r == 0 && (((NOVA_FIGHT2 & 4) && regroup_from(nearest->pos)) || retreat_from(nearest->pos)));
            if (!moved_r && target && cheb(target->pos, pos) <= range) {
                cast_on_enemy(*target);
                attack_unit(target->unit, target->pos);
            }
            heading_home = hp < max_hp * 0.5f;
            return true;
        }
        const bool kite_run = tune::kKite > 0 && !melee && nearest_enemy_melee() < tune::kKite;
        if (!kite_run && target && cheb(target->pos, pos) <= range && (!melee || cheb(target->pos, pos) <= 1)) {
            if (!melee) {
                cast_on_enemy(*target);
                attack_unit(target->unit, target->pos);
                return true; // ranged: shooting used the move
            }
            attack_unit(target->unit, target->pos);
        }
        const int planned = tune::kEscapeSearch ? run_escape(false) : 0;
        const bool moved = planned == 1 || (planned == 0 && (((NOVA_FIGHT2 & 4) && regroup_from(nearest->pos)) || retreat_from(nearest->pos)));
        if (!moved && target) attack_unit(target->unit, target->pos);
        heading_home = hp < max_hp * 0.5f;
        return true;
    }
    PROF_CHECK("e_noflee");
    if (!target) return false;

    const int dist = cheb(target->pos, pos);
    // Fight if we win the race, or trade when the enemy is already in our range and we are healthy.
    const bool engage = winning || (dist <= range && ttd > 6.0f * risk);
    if (!engage) return false;
    if ((tune::kKiteCast & 1) && !melee && dist <= enemy_spell_reach() && cast_on_enemy(*target)) {
        // Spell first; a kite step can wait for the cooldown turns.
        mode = "kite-cast";
        if (dist <= range) attack_unit(target->unit, target->pos);
        else if (!walked && tune::kKite > 0 && nearest_enemy_melee() < tune::kKite) kite_step(*target);
        return true;
    }
    if (tune::kKite > 0 && !melee && nearest_enemy_melee() < tune::kKite && kite_step(*target)) {
        mode = "kite";
        return true;
    }
    PROF_CHECK("e_prefight");
    mode = "fight-champ";
    if (tune::kPosition && dist <= range + 1 && position_and_attack(*target)) {
        cast_on_enemy(*target);
        if (attack_unit(target->unit, target->pos)) use_reset_and_attack(target->unit, target->pos);
        return true;
    }
    if (dist <= range) {
        cast_on_enemy(*target);
        if (attack_unit(target->unit, target->pos)) use_reset_and_attack(target->unit, target->pos);
        return true;
    }
    if ((tune::kKiteCast & 2) && dist <= enemy_spell_reach() && cast_on_enemy(*target)) {
        mode = "fight-cast";
        return true; // the spell reached from here; walking in first would have forbidden it
    }
    if (dist <= range + tune::kChaseRange) {
        bc::bitboard goal;
        goal.clear();
        goal.draw_chebyshev(target->pos, static_cast<float>(range));
        walk_toward(goal);
        if (cheb(target->pos, pos) <= range) {
            cast_on_enemy(*target);
            if (attack_unit(target->unit, target->pos)) use_reset_and_attack(target->unit, target->pos);
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- opening rush

int Bot::rush_leader_slot() const {
    for (int s = 0; s < std::min(team_size, static_cast<int>(ally_handle.size())); ++s) {
        std::int32_t alive = 1;
        if (d::sdk_champion_is_alive(ally_handle[static_cast<std::size_t>(s)], &alive) == 0 && alive) return s;
    }
    return slot;
}

bool Bot::rush_active() const {
    if (round >= NOVA_RUSH_UNTIL) return false;
    if (gems >= NOVA_RUSH_GEMS) return false;
    // Only while we have at least as many living champions as they do.
    int us = 0, them = 0;
    for (const auto& c : bc::champions(team::ally)) {
        std::int32_t a = 1;
        d::sdk_champion_is_alive(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)), &a);
        us += a != 0;
    }
    for (const auto& c : bc::champions(team::enemy)) {
        std::int32_t a = 1;
        d::sdk_champion_is_alive(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)), &a);
        them += a != 0;
    }
    return us >= them + NOVA_RUSH_MARGIN;
}

bool Bot::rush() {
    if (!rush_active()) return false;
    if (hp < max_hp * 0.45f) return false;
    FlagScope combat{combat_move};
    if (rush_visited.size() != w.groups.size()) rush_visited.assign(w.groups.size(), 0);
    const int leader = rush_leader_slot();
    if (leader == slot) {
        // Next enemy-side camp: nearest to the enemy spawn among those not yet checked.
        const vec2 home = ts_home[1].x >= 0 ? ts_home[1] : w.enemy_spawn.positions().front();
        auto visited_here = [&](int gi) {
            const auto& g = w.groups[static_cast<std::size_t>(gi)];
            for (int ci : g.camps)
                if (cheb(w.camps[static_cast<std::size_t>(ci)].pos, pos) <= 2) return true;
            return false;
        };
        if (rush_target >= 0 && visited_here(rush_target)) rush_visited[static_cast<std::size_t>(rush_target)] = 1;
        if (rush_target < 0 || rush_visited[static_cast<std::size_t>(rush_target)]) {
            rush_target = -1;
            int best_d = 1 << 30;
            for (std::size_t gi = 0; gi < w.groups.size(); ++gi) {
                const auto& g = w.groups[gi];
                if (rush_visited[gi] || g.camps.empty() || (g.gems == 0 && g.gold < 60)) continue;
                const vec2 cp = w.camps[static_cast<std::size_t>(g.camps[0])].pos;
                const int de = cheb(cp, home), dm = cheb(cp, pos);
                if (de > NOVA_RUSH_RADIUS) continue;
                const int key = de * 2 + dm;
                if (key < best_d) best_d = key, rush_target = static_cast<int>(gi);
            }
        }
        if (rush_target < 0) return false;
        write_claim(rush_target);
        // Wait for the pack: do not run more than 3 tiles ahead of the nearest follower we can see.
        int behind = 0;
        for (const auto& a : w.allies) behind = std::max(behind, cheb(a.pos, pos));
        mode = "rush-lead";
        if (behind > 3 && behind <= 8) return true;
        bc::bitboard goal;
        goal.clear();
        for (int ci : w.groups[static_cast<std::size_t>(rush_target)].camps)
            goal.draw_chebyshev(w.camps[static_cast<std::size_t>(ci)].pos, 2.0f);
        walk_toward(goal);
        return true;
    }
    // Follower: stay within 2 tiles of the leader, or head for the leader's claim when it is out of sight.
    for (const auto& a : w.allies) {
        if (a.id != ally_handle[static_cast<std::size_t>(leader)]) continue;
        mode = "rush-follow";
        if (cheb(a.pos, pos) <= 2) return true;
        bc::bitboard goal;
        goal.clear();
        goal.draw_chebyshev(a.pos, 1.0f);
        walk_toward(goal);
        return true;
    }
    const int lc = claim_of(leader);
    if (lc >= 0 && lc < static_cast<int>(w.groups.size())) {
        mode = "rush-join";
        bc::bitboard goal;
        goal.clear();
        for (int ci : w.groups[static_cast<std::size_t>(lc)].camps) goal.draw_chebyshev(w.camps[static_cast<std::size_t>(ci)].pos, 2.0f);
        walk_toward(goal);
        return true;
    }
    return false;
}

// Move toward the freshest nearby enemy sighting when healthy and not carrying much.
bool Bot::hunt() {
    FlagScope combat{combat_move};
    // Tribute snipe (APEX_SNIPE): an enemy channelling a tribute stands still until the channel ends (ceil(bag / 10)
    // turns at most from now); reach it in time and its bag is ours.
    if (tune::kSnipe > 0 && hp >= max_hp * 0.6f && gems < 60) {
        const EnemyRec* best = nullptr;
        int best_d = 1000;
        for (const auto& e : w.enemies) {
            if (!e.tributing() || e.gems < 20) continue;
            const int walk = std::max(0, cheb(e.pos, pos) - range);
            if (walk > tune::kSnipe || walk > (e.gems + 9) / 10) continue;
            int guards = 0;
            for (const auto& o : w.enemies) guards += o.id != e.id && cheb(o.pos, e.pos) <= 4;
            if (guards > 1) continue;
            if (walk < best_d) best_d = walk, best = &e;
        }
        if (best) {
            mode = "snipe";
            if (cheb(best->pos, pos) <= range) {
                cast_on_enemy(*best);
                if (attack_unit(best->unit, best->pos)) use_reset_and_attack(best->unit, best->pos);
                return true;
            }
            bc::bitboard goal;
            goal.clear();
            goal.draw_chebyshev(best->pos, static_cast<float>(range));
            walk_toward(goal);
            if (cheb(best->pos, pos) <= range) {
                cast_on_enemy(*best);
                attack_unit(best->unit, best->pos);
            }
            return true;
        }
    }
    // Answer a teammate's distress call: the enemy chasing them is out of position.
    if (tune::kDistress && round - distress.round <= 3 && hp >= max_hp * 0.5f && gems < 40) {
        const int d = cheb(distress.pos, pos);
        if (d > 0 && d <= tune::kDistressRange) {
            mode = "rescue";
            bc::bitboard goal;
            goal.clear();
            goal.draw_chebyshev(distress.pos, static_cast<float>(range));
            walk_toward(goal);
            return true;
        }
    }
    if (!tune::kHunt || gems >= tune::kHuntGems || hp < max_hp * tune::kHuntHp) return false;
    // Hunting toward enemies we then flee from loops forever (and enemies on their spawn heal).
    if (tune::kHuntFix && round - last_flee_round < 10) return false;
    const Sighting* best = nullptr;
    int best_d = tune::kHuntRange + 1;
    for (const auto& s : w.sightings) {
        if (round - s.round > 3) continue;
        if (tune::kHuntFix && bit(w.enemy_spawn_zone, s.pos)) continue;
        const int d0 = cheb(s.pos, pos);
        if (d0 < best_d) {
            best_d = d0;
            best = &s;
        }
    }
    if (!best) return false;
    mode = "hunt";
    bc::bitboard goal;
    goal.clear();
    goal.draw_chebyshev(best->pos, static_cast<float>(range));
    walk_toward(goal);
    return true;
}

bool Bot::go_home_if_needed() {
    auto& me = bc::self();
    const bool on_spawn = bit(w.ally_spawn, pos);
    if (on_spawn) {
        heading_home = false;
        // NOVA_SPAWN_HOT: an awake camp that reaches our spawn hits us every round before the heal, so
        // "heal to full" never ends (two champions held spawn 1,100 turns, one with a 120-gem bag).
        if (NOVA_SPAWN_HOT && bit(w.hot_zone, pos) && hp >= max_hp * 0.85f) return false;
        return hp < max_hp; // stay put this turn so the spawn heals us
    }
    if (type == bc::champion_type::tavi && hp >= max_hp * tune::kRetreatHp) return false; // Tavi shops anywhere
    // NOVA_CI_RETREAT: a Cassi or Issac death kills both bodies (57% of the body-rounds we lost to
    // champion kills), so they head home at a higher health fraction.
    const bool ci_body = type == bc::champion_type::cassi || type == bc::champion_type::issac;
    bool low = hp < max_hp * tune::kRetreatHp * (ci_body ? NOVA_CI_RETREAT : 1.0f);
    if (NOVA_CI_LINK) {
        // One fate: Cassi and Issac share deaths and healing, so retreat on the weaker body's health.
        const auto other =
            type == bc::champion_type::cassi ? bc::champion_type::issac : bc::champion_type::cassi;
        if ((type == bc::champion_type::cassi || type == bc::champion_type::issac))
            for (const auto& a : w.allies)
                if (a.type == other) low = low || std::min(hp, a.hp) < max_hp * tune::kRetreatHp;
    }
    if (!low && tune::kThreatRetreat > 0.0f && hp < max_hp * tune::kThreatRetreat) {
        bool threat = !w.enemies.empty();
        for (const auto& sg : w.sightings) threat |= round - sg.round <= 6 && cheb(sg.pos, pos) <= 10;
        low = threat;
    }
    const bool rich = gold >= tune::kShopGold;
    if (!heading_home && !low && !rich) return false;
    // Do not abandon a capture we are standing on.
    if (!low && target_group >= 0 && w.groups[target_group].state == GroupState::reward &&
        bit(w.groups[target_group].tiles, pos))
        return false;
    heading_home = true;
    // Camp damage would cancel a recall: leave every awake camp's reach first.
    if (bit(w.hot_zone, pos)) {
        mode = "home-escape";
        walk_toward(~w.hot_zone & ~w.static_blocked);
        return true;
    }
    if (tune::kAwakeZone && hp < prev_hp - 0.5f && w.enemies.empty()) {
        // Hit since our last turn with no enemy in sight: some camp still reaches us, so a recall
        // here would be cancelled again. Step out of every live camp's reach first.
        bc::bitboard reach;
        reach.clear();
        for (std::size_t ci = 0; ci < w.camps.size(); ++ci) {
            const auto& c = w.camps[ci];
            if (c.dead || c.strength <= 0 || cheb(c.pos, pos) > c.range + 1) continue;
            const int gi = w.camp_group[ci];
            if (gi < 0 || w.groups[static_cast<std::size_t>(gi)].state != GroupState::alive) continue;
            reach.draw_chebyshev(c.pos, static_cast<float>(c.range));
        }
        if (bit(reach, pos)) {
            mode = "home-escape";
            walk_toward(~reach & ~w.static_blocked);
            return true;
        }
    }
    const int dist = fdist(w.spawn_field, pos);
    bool enemy_close = false;
    for (const auto& e : w.enemies) enemy_close |= cheb(e.pos, pos) <= 6;
    for (const auto& s : w.sightings) enemy_close |= round - s.round <= 2 && cheb(s.pos, pos) <= 5;
    // From cover no enemy can hit us, so their presence cannot cancel the recall.
    if (enemy_close && tune::kBushRecall && hidden_here() && !bit(w.hot_zone, pos)) enemy_close = false;
    if (dist > tune::kShopWalkDist && !enemy_close) {
        try {
            me.recall();
            return true;
        } catch (const std::exception& e) {
            LOG("recall error %s\n", e.what());
        }
    }
    bc::bitboard blocked = w.static_blocked | w.unit_tiles | w.hot_zone;
    blocked.set(pos, false);
    if (!follow_field(w.spawn_field, blocked, true)) {
        blocked = w.static_blocked | w.unit_tiles;
        blocked.set(pos, false);
        if (!follow_field(w.spawn_field, blocked, true) && tune::kUnstick && !walk_toward(w.ally_spawn)) break_out();
    }
    return true;
}

bool Bot::tribute_if_needed() {
    if (gems <= 0) return false;
    auto& me = bc::self();
    // Route around recently seen enemies when carrying a real bag.
    bc::distance_field safe_field;
    bool use_safe = false;
    w.ensure_altar_field();
    int dist = fdist(w.altar_field, pos);
    bc::bitboard watched;
    watched.clear();
    bool any_watch = false, any_free = false;
    if (tune::kStaleBag > 0 && gems >= tune::kStaleBag) {
        for (std::size_t i = 0; i < w.altars.size() && i < altar_watch.size(); ++i) {
            if (altar_watch[i] > round) {
                watched.draw_chebyshev(w.altars[i], 2.0f);
                any_watch = true;
            } else {
                any_free = true;
            }
        }
        any_watch = any_watch && any_free;
    }
    if (gems >= 15) {
        bc::bitboard threat = w.threat_zone(round, 10, 4.0f);
        threat.set(pos, false);
        if (!threat.is_empty() || any_watch) {
            bc::bitboard blocked = w.static_blocked | threat;
            blocked.set(pos, false);
            bc::bitboard ring = w.altar_ring & ~blocked;
            if (any_watch) ring = ring & ~watched;
            if (!ring.is_empty()) {
                bc::bitboard visited;
                safe_field.compute_to(blocked, ring, single(pos), visited);
                if (fdist(safe_field, pos) != kUnreach) {
                    use_safe = true;
                    dist = fdist(safe_field, pos);
                }
            }
        }
    }
    if (dist == kUnreach) return false;
    const bool wins = ally_score + gems >= static_cast<int>(bc::constants::winning_gems) ||
        // Endgame: our bag plus what teammates probably carry finishes the race, so bank it now
        // rather than lose the race with gems in our bags.
        (tune::kEndgameMargin > 0 && gems >= 10 && dist <= 25 &&
            ally_score + gems + tune::kEndgameMargin >= static_cast<int>(bc::constants::winning_gems));
    const bool late = round + dist + gems / 10 + 15 >= static_cast<int>(bc::constants::max_rounds);
    bool threatened = false;
    for (const auto& e : w.enemies) threatened |= cheb(e.pos, pos) <= 6;
    // Endgame banking (APEX_END_BANK, from apex_v19): once either score passes the threshold, unbanked gems may be
    // worth nothing (the race ends at 1,000), so any bag of 10+ goes to an altar within 20 steps.
    const bool end_bank = tune::kEndBank > 0 && gems >= 10 && dist <= 20 && std::max(ally_score, enemy_score) >= tune::kEndBank;
    // Bank rhythm (APEX_BANK_RATIO): a short trip for the bag we carry is worth taking now.
    const bool rhythm = tune::kBankRatio > 0.0f && gems >= 20 && static_cast<float>(dist) * tune::kBankRatio <= static_cast<float>(gems);
    // NOVA_BANK_FIX: sparse maps bank small bags through the opening (see common.h); big-strip
    // maps keep the carry discipline. Only fires once the draft recorded a known biggest group,
    // while no capture is in progress under our feet, the trip is short, and the road is clear
    // (v2 reshape: the first version lost farm tempo on ambush maps because carriers left
    // captures and strip cycles mid-way).
    bool capturing = false;
    for (const auto& gr : w.groups)
        if (gr.state == GroupState::reward && bit(gr.tiles, pos)) {
            capturing = true;
            break;
        }
    const bool clearish = w.enemies.empty() || use_safe;
    // NOVA_RACE_BANK / NOVA_MONO_BANK (default off): on dense race maps and shared-monolith maps the
    // opening bank also fires when a big strip exists there — bags convert to score, not to carry.
    const bool raceMono = brain::state.ready && ((raceBank && brain::state.race) || (monoBank && brain::state.mono));
    const bool edge = bankFix && gems >= tune::kEdgeBankBag && round <= tune::kEdgeBankUntil && !capturing &&
        clearish && dist <= tune::kEdgeBankDist && brain::state.ready && brain::state.max_group > 0 &&
        (brain::state.max_group < tune::kBigMapGems || raceMono);
    const bool go = force_tribute || wins || late || end_bank || rhythm || edge || gems >= tune::kTributeCarry ||
        dist <= tune::kTributeNearDist || (threatened && gems >= 20 && dist <= 8);
    if (!go) return false;
    if (dist == 0) {
        for (vec2 a : w.altars) {
            if (cheb(a, pos) > bc::constants::submit_gems_range) continue;
            if (bc::vision::has_altar_lock(a)) {
                const auto lock = bc::vision::altar_lock_at(a);
                if (lock.is_enemy()) {
                    if (me.can_attack()) {
                        try {
                            me.attack(lock);
                        } catch (...) {
                        }
                    }
                    return true;
                }
            }
            try {
                me.submit_gems(a);
                return true;
            } catch (const std::exception& e) {
                LOG("submit error %s\n", e.what());
            }
        }
        return false;
    }
    bc::bitboard blocked = w.static_blocked | w.unit_tiles;
    blocked.set(pos, false);
    // The static field ignores units: when camps or champions fill every downhill tile, plan around them.
    if (!follow_field(use_safe ? safe_field : w.altar_field, blocked, true) && tune::kUnstick && !walk_toward(w.altar_ring))
        break_out();
    return true;
}

// Camps attack the nearest champion within range 3; equal distance goes to the lower unit id.
bool Bot::camp_targets_me(int ci, vec2 at) const {
    const vec2 cp = w.camps[ci].pos;
    const int mine = cheb(cp, at);
    if (mine > w.camps[ci].range) return false;
    for (const auto& a : w.allies) {
        const int da = cheb(cp, a.pos);
        if (da < mine || (da == mine && a.id < my_id)) return false;
    }
    for (const auto& e : w.enemies) {
        const int de = cheb(cp, e.pos);
        if (de < mine || (de == mine && e.id < my_id)) return false;
    }
    return true;
}

float Bot::incoming_from_group(const GroupRec& g, vec2 at) const {
    float dmg = 0;
    for (int ci : g.camps)
        if (!w.camps[ci].dead && w.camps[ci].strength > 0 && camp_targets_me(ci, at)) dmg += w.camps[ci].strength;
    return dmg;
}

// Damage taken per unit of team damage-per-turn while killing the dangerous camps weakest-first
// (soak = coefficient / team_dps). Cached per group per turn: this sat in the hottest loop.
float Bot::soak_coefficient(int gi, float hp_left) {
    // Depends only on the group's camps (health and state), so it is cached across turns and
    // recomputed when the group's remaining health or state changes.
    if (soak_cache.size() != w.groups.size()) {
        soak_cache.assign(w.groups.size(), -1.0f);
        soak_key_hp.assign(w.groups.size(), -1.0f);
        soak_key_state.assign(w.groups.size(), -1);
    }
    const auto& g = w.groups[static_cast<std::size_t>(gi)];
    const int state = static_cast<int>(g.state);
    float& slot_value = soak_cache[static_cast<std::size_t>(gi)];
    if (slot_value < 0 || soak_key_hp[static_cast<std::size_t>(gi)] != hp_left || soak_key_state[static_cast<std::size_t>(gi)] != state) {
        slot_value = group_soak(g, 1.0f);
        soak_key_hp[static_cast<std::size_t>(gi)] = hp_left;
        soak_key_state[static_cast<std::size_t>(gi)] = state;
    }
    return slot_value;
}

// Damage the tanks absorb while the team kills the dangerous camps, weakest first.
float Bot::group_soak(const GroupRec& g, float team_dps) const {
    std::array<std::pair<float, float>, 12> live{};
    int n = 0;
    float str_alive = 0;
    for (int ci : g.camps) {
        const auto& c = w.camps[ci];
        if (c.strength <= 0 || n >= 12) continue;
        if (g.state == GroupState::alive && c.dead) continue;
        const float hp_c = g.state == GroupState::alive ? c.hp : c.max_hp;
        live[n++] = {hp_c, c.strength};
        str_alive += c.strength;
    }
    std::sort(live.begin(), live.begin() + n);
    float soak = 0;
    for (int i = 0; i < n; ++i) {
        soak += str_alive * live[i].first / team_dps;
        str_alive -= live[i].second;
    }
    return soak;
}

int Bot::champions_needed(const GroupRec& g, float hp_left) {
    if (g.harmless || hp_left <= 0) return 1;
    // Camps focus one champion, but tanks can rotate out (camps never chase or heal),
    // so the team's combined health pays for the fight.
    const float dps = my_dps(&g);
    const float coef = soak_coefficient(static_cast<int>(&g - w.groups.data()), hp_left) * camp_taken_mult;
    for (int n = 1; n <= 6; ++n) {
        const float pool = hp * tune::kNeedPool + static_cast<float>(n - 1) * max_hp * 0.7f;
        if (coef / (dps * static_cast<float>(n)) <= pool) return n;
    }
    return 7;
}

int Bot::allies_near_group(const GroupRec& g) const {
    int n = 0;
    for (const auto& a : w.allies) {
        if (a.hp < a.max_hp * 0.35f) continue;
        for (int ci : g.camps)
            if (cheb(a.pos, w.camps[ci].pos) <= 5) {
                ++n;
                break;
            }
    }
    return n;
}

bool Bot::fight_camps(GroupRec& g, int need) {
    auto pick_target = [&]() {
        int best = -1;
        float best_hp = 1e30f;
        for (int ci : g.camps) {
            const auto& c = w.camps[ci];
            if (c.dead || !bit(w.visible, c.pos) || cheb(c.pos, pos) > range) continue;
            // Kill the camps that hurt first, then the weakest.
            float key = c.hp - c.strength * 1000.0f;
            if (type == bc::champion_type::varg) {
                int neighbours = 0;
                for (int o : g.camps)
                    if (o != ci && !w.camps[o].dead && cheb(w.camps[o].pos, c.pos) <= 1) ++neighbours;
                key -= neighbours * 2000.0f;
            }
            if (key < best_hp) {
                best_hp = key;
                best = ci;
            }
        }
        return best;
    };
    // Tiles from which we can hit a live camp, and tiles inside any dangerous camp's reach.
    bc::bitboard attack_zone, danger_zone;
    attack_zone.clear();
    danger_zone.clear();
    for (int ci : g.camps) {
        if (w.camps[ci].dead) continue;
        attack_zone.draw_chebyshev(w.camps[ci].pos, static_cast<float>(range));
        if (w.camps[ci].strength > 0) danger_zone.draw_chebyshev(w.camps[ci].pos, static_cast<float>(w.camps[ci].range));
    }

    const int present = 1 + allies_near_group(g);
    const float incoming = incoming_from_group(g, pos);
    // Wait outside camp range until enough allies have gathered (unless the fight already started).
    if (need > present && incoming <= 0) {
        if (staging_since < 0) staging_since = round;
        mode = "stage";
        const bool sieging = tune::kSiege && !in_role_pack && siege_dps(g) > 0.0f;
        if (sieging) {
            mode = "siege";
            bc::bitboard ring;
            ring.clear();
            for (int ci : g.camps)
                if (!w.camps[ci].dead) ring.draw_chebyshev(w.camps[ci].pos, static_cast<float>(w.camps[ci].range + 1));
            ring &= ~danger_zone & ~w.static_blocked;
            if (NOVA_SIEGE_LOS) {
                // Spells need the camp in view: on contest the ring tile behind a wall row saw nothing and
                // champions "sieged" a monolith for 1,500 turns without one cast. Keep tiles with a clear
                // straight line (no wall, no foreign bush) to a live camp.
                bc::bitboard seeing;
                seeing.clear();
                const bc::bitboard opaque = bc::map::walls() | w.bushes;
                for (vec2 q : ring.positions()) {
                    for (int ci : g.camps) {
                        const auto& c = w.camps[ci];
                        if (c.dead || cheb(c.pos, q) > 4) continue;
                        bool clear = true;
                        const int n = cheb(c.pos, q);
                        for (int k = 1; k < n && clear; ++k) {
                            const vec2 m{q.x + ((c.pos.x - q.x) * k * 2 + (c.pos.x >= q.x ? n : -n)) / (2 * n),
                                q.y + ((c.pos.y - q.y) * k * 2 + (c.pos.y >= q.y ? n : -n)) / (2 * n)};
                            clear = !bit(opaque, m);
                        }
                        if (clear) {
                            seeing.set(q, true);
                            break;
                        }
                    }
                }
                if (!seeing.is_empty()) ring = seeing;
            }
            if (!bit(ring, pos)) walk_toward(ring);
            // Hit the camp that hurts most first (AoE picks its own best tile in cast_on_camp).
            int best = -1;
            for (int ci : g.camps) {
                const auto& c = w.camps[ci];
                if (c.dead || !bit(w.visible, c.pos) || cheb(c.pos, pos) > 4) continue;
                if (best < 0 || c.strength > w.camps[best].strength ||
                    (c.strength == w.camps[best].strength && c.hp < w.camps[best].hp))
                    best = ci;
            }
            if (best >= 0) cast_on_camp(best);
            return true;
        }
        if (round - staging_since > tune::kStageTimeout) {
            avoid_until[target_group] = round + 60;
            staging_since = -1;
            if (tune::kCallCooldown > 0) call_ban_until = round + tune::kCallCooldown;
            return false;
        }
        bc::bitboard stage;
        stage.clear();
        for (int ci : g.camps)
            if (!w.camps[ci].dead) stage.draw_chebyshev(w.camps[ci].pos, 5.0f);
        stage &= ~danger_zone;
        if (!bit(stage, pos)) walk_toward(stage);
        // Spells reach 4 tiles, one more than camps: chip damage for free while we wait.
        for (int ci : g.camps)
            if (!w.camps[ci].dead && bit(w.visible, w.camps[ci].pos) && cast_on_camp(ci)) break;
        return true;
    }
    staging_since = -1;

    const float hp_left = g.hp_left(w.camps);
    const float team_dps = my_dps(&g) * static_cast<float>(present);
    // NOVA_CAMP_SAFE = K: every camp in reach may switch to us next round (the tank in front of us can
    // step away later this round: Cassi kept hitting elks at 160 health because Issac stood closer,
    // then Issac disengaged and both died). Leave their reach while K x that damage would kill us.
    // Counted: this group's camps and those of any other group already awake (damaged or part-killed):
    // splash and area spells wake diagonal neighbours, which are separate groups (four elks hit Ryth
    // while it fought a one-elk group).
    struct NearCamp {
        vec2 p;
        float s;
        int r;
    };
    std::array<NearCamp, 32> near_camps{};
    int n_near = 0;
    if (NOVA_CAMP_SAFE > 0.0f)
        for (const auto& g2 : w.groups) {
            if (g2.state != GroupState::alive || g2.harmless) continue;
            bool awake = &g2 == &g;
            for (int ci : g2.camps)
                if (!awake && (w.camps[ci].dead || w.camps[ci].hp < w.camps[ci].max_hp)) awake = true;
            if (!awake) continue;
            for (int ci : g2.camps) {
                const auto& c = w.camps[ci];
                if (c.dead || c.strength <= 0 || cheb(c.pos, pos) > c.range + 2 || n_near >= 32) continue;
                near_camps[n_near++] = {c.pos, c.strength, c.range};
            }
        }
    auto worst_at = [&](vec2 q) {
        float s = 0.0f;
        for (int i = 0; i < n_near; ++i)
            if (cheb(near_camps[i].p, q) <= near_camps[i].r) s += near_camps[i].s;
        return s * camp_taken_mult;
    };
    const bool doomed = NOVA_CAMP_SAFE > 0.0f && worst_at(pos) * NOVA_CAMP_SAFE >= hp;
    // Tank rotation: when the camps are focused on us and we are low, step to a tile they do not
    // target (an ally closer to them takes over); if none is adjacent, leave their reach.
    if ((incoming > 0 && hp < incoming * 3.0f + max_hp * 0.1f && hp_left > team_dps * 1.5f) || doomed) {
        mode = "disengage";
        const int t = pick_target();
        if (t >= 0 && melee) attack_unit(w.camps[t].camp, w.camps[t].pos);
        auto& me = bc::self();
        vec2 best = pos;
        int best_score = -1000000;
        for (vec2 dir : kDirs) {
            const vec2 q = pos + dir;
            if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q) || bit(w.unit_tiles, q)) continue;
            if (incoming_from_group(g, q) > 0) continue;
            if (doomed && worst_at(q) * NOVA_CAMP_SAFE >= hp) continue;
            int sc = bit(attack_zone, q) ? 100 : 0;
            sc -= static_cast<int>(fdist(w.spawn_field, q));
            if (sc > best_score) {
                best_score = sc;
                best = q;
            }
        }
        bool moved = false;
        if (best != pos && me.can_move()) {
            try {
                moved = me.move_absolute(best);
                if (moved) {
                    pos = best;
                    walked = true;
                }
            } catch (...) {
            }
        }
        if (!moved) walk_toward(~danger_zone & ~w.static_blocked);
        if (hp < max_hp * 0.45f) heading_home = true;
        if (!melee && !walked) {
            const int t2 = pick_target();
            if (t2 >= 0) attack_unit(w.camps[t2].camp, w.camps[t2].pos);
        }
        return true;
    }

    int t = pick_target();
    mode = "fight";
    if (t < 0 && NOVA_BUSH_WARD && reveal_bush_camp(g)) t = pick_target();
    if (t < 0 && std::string_view(mode) == "bush-ward") return true;
    if (t < 0) {
        mode = "approach";
        // Ranged champions stand at full range so a melee ally (closer) draws the camps' fire.
        bc::bitboard goal = attack_zone;
        if (!melee) {
            bc::bitboard inner;
            inner.clear();
            for (int ci : g.camps)
                if (!w.camps[ci].dead) inner.draw_chebyshev(w.camps[ci].pos, static_cast<float>(range - 1));
            const bc::bitboard ring = attack_zone & ~inner & ~w.static_blocked & ~w.unit_tiles;
            if (!ring.is_empty()) goal = ring;
        }
        if (!walk_toward(goal) && !bit(goal, pos) && break_out()) return true;
        if (!melee) return true; // ranged units spent their allowance walking
        t = pick_target();
        if (t < 0) return true;
    }
    cast_on_camp(t);
    if (attack_unit(w.camps[t].camp, w.camps[t].pos)) {
        // Refresh the target in case the first hit killed it.
        d::camp_state cs{};
        const int code = d::sdk_camp(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(w.camps[t].camp)), &cs);
        if (code != 0 || cs.health <= 0) {
            w.camps[t].dead = true;
            t = pick_target();
        }
        if (t >= 0) use_reset_and_attack(w.camps[t].camp, w.camps[t].pos);
    }
    return true;
}

// NOVA_UNBOX: live units, not terrain, block every route to where we are going (typically camps that
// respawned around us after a capture: champions sat boxed in with full bags for the rest of the
// game). With no enemy in view, hit the camp in reach that is cheapest to clear, harmless first.
bool Bot::break_out() {
    if (!NOVA_UNBOX || !walk_boxed || walked || !w.enemies.empty()) return false;
    int best = -1;
    float best_key = 1e30f;
    for (std::size_t ci = 0; ci < w.camps.size(); ++ci) {
        const auto& c = w.camps[ci];
        if (c.dead || !bit(w.visible, c.pos) || cheb(c.pos, pos) > range) continue;
        const int gi = w.camp_group[ci];
        if (gi < 0 || w.groups[static_cast<std::size_t>(gi)].state != GroupState::alive) continue;
        const float key = c.strength * 1000.0f + c.hp;
        if (key < best_key) {
            best_key = key;
            best = static_cast<int>(ci);
        }
    }
    if (best < 0) return false;
    mode = "break-out";
    cast_on_camp(best);
    attack_unit(w.camps[static_cast<std::size_t>(best)].camp, w.camps[static_cast<std::size_t>(best)].pos);
    return true;
}

// NOVA_BUSH_WARD: no live camp of the group is in view, yet one stands within our attack range (on a
// bush tile, or behind a corner): we would wait beside it forever. Step within ward range and put the
// ward on its tile. True when the ward went down (look again); mode "bush-ward" while walking or
// waiting for the ward to recharge.
bool Bot::reveal_bush_camp(const GroupRec& g) {
    // (Live camps in view but out of reach do not help: the approach goal also counts this camp's
    // attack zone, so we would stand here anyway.)
    int hidden = -1, hd = 1 << 20;
    for (int ci : g.camps) {
        const auto& c = w.camps[ci];
        if (c.dead || bit(w.visible, c.pos)) continue;
        const int d = cheb(c.pos, pos);
        if (d < hd) {
            hd = d;
            hidden = ci;
        }
    }
    if (hidden < 0 || hd > range) return false;
    const vec2 at = w.camps[hidden].pos;
    mode = "bush-ward";
    const int reach = std::min(range, 2); // ward range 2; melee must stand next to it to attack
    if (hd > reach) {
        bc::bitboard goal;
        goal.clear();
        goal.draw_chebyshev(at, static_cast<float>(reach));
        goal &= ~w.static_blocked;
        walk_toward(goal);
        hd = cheb(at, pos);
    }
    if (hd > 2) return false;
    auto& me = bc::self();
    try {
        if (me.ward_cooldown() > 0) return false;
        // False means a hidden obstruction (typically an ally's ward already on the tile): the engine
        // then reveals the tile to us for the rest of the round, which is all an attack needs.
        if (me.place_ward(at)) ward_pos = at;
        w.update(round);
        return bit(w.visible, at);
    } catch (const std::exception& e) {
        LOG("bush ward error %s\n", e.what());
    }
    return false;
}

// Spell damage per turn we can deal to this group from 4 tiles, outside every camp's reach (range 3).
float Bot::siege_dps(const GroupRec& g) const {
    int live = 0, reach = 0;
    for (int ci : g.camps)
        if (!w.camps[ci].dead) {
            ++live;
            reach = std::max(reach, w.camps[ci].range);
        }
    float dps = 0;
    for (const auto& sp : spells) {
        // Only spells that outrange every live camp siege for free.
        if (!sp.present || !sp.camps || sp.range < static_cast<float>(reach + 1)) continue;
        if (sp.kind == bc::ability_kind::damage || sp.kind == bc::ability_kind::cc)
            dps += sp.modifier * focus / 8.0f;
        else if (sp.kind == bc::ability_kind::aoe_damage)
            dps += sp.modifier * focus * static_cast<float>(std::min(live, 3)) / 8.0f;
    }
    return dps;
}

int Bot::claim_of(int s) const { return static_cast<int>((shared >> (s * kClaimBits)) & kClaimMask); }

bool Bot::group_capture_covered(const GroupRec& g) const {
    if (g.state != GroupState::reward) return false;
    const float mine = tune::kShareGold ? my_items() : 0.0f;
    for (const auto& a : w.allies) {
        if (!bit(g.tiles, a.pos)) continue;
        // A clearly richer ally on the region does not cover it: we take over and it hands over.
        if (tune::kShareGold && items_of(a.type, a.max_hp, a.strength, a.focus) > mine + static_cast<float>(tune::kShareGold))
            continue;
        return true;
    }
    return false;
}

float Bot::my_items() const { return items_of(type, max_hp, strength, focus); }

// A clearly poorer ally will take this finished group's reward: it stands on the region, or (unless
// on_tile_only) it claims the group and stands within 2 tiles of it.
bool Bot::poorer_ally_takes(const GroupRec& g, int gi, bool on_tile_only) const {
    const float mine = my_items();
    for (const auto& a : w.allies) {
        if (items_of(a.type, a.max_hp, a.strength, a.focus) + static_cast<float>(tune::kShareGold) >= mine) continue;
        if (bit(g.tiles, a.pos)) return true;
        if (on_tile_only) continue;
        int s = -1;
        for (int k = 0; k < std::min(team_size, kClaimSlots); ++k)
            if (ally_handle[static_cast<std::size_t>(k)] == a.id) s = k;
        if (s < 0 || claim_of(s) != gi) continue;
        for (int ci : g.camps)
            if (cheb(a.pos, w.camps[ci].pos) <= 2) return true;
    }
    return false;
}

int Bot::score_groups(int participants_hint, int& best_need, float& best_score) {
    if constexpr (!(farm_score::kCorrectBound || farm_score::kDiagnostics ||
        farm_score::kSingleGroupDiagnostic || farm_score::kExhaustive)) {
#include "farm_legacy.inc"
    } else {
    PROF(score);
    const float dps = my_dps(nullptr);
    int best = -1;
    best_need = 1;
    best_score = 0;
    // Visit groups nearest-first and stop scoring a group as soon as even a free kill could not beat
    // the best score so far (branch and bound): most groups are pruned after one distance lookup.
    // Buffers are left uninitialised on purpose: filling memory costs 1 fuel per byte.
    constexpr int kMaxOrder = 256;
    struct Entry {
        int first, second;
    };
    Entry order_storage[kMaxOrder]; // trivial type: no per-element construction cost
    std::vector<Entry> diagnostic_order;
    if constexpr (farm_score::kSingleGroupDiagnostic) diagnostic_order.resize(w.groups.size());
    Entry* order = farm_score::kSingleGroupDiagnostic ? diagnostic_order.data() : order_storage;
    int n_order = 0;
    {
    PROF(s_order);
    // Distances: one tile index for our position, then one load per group from its static field.
    const std::size_t here_idx = field_index(pos);
    const bool have_ptrs = w.field_ptr.size() == w.groups.size();
    // Visit groups in last turn's distance order: we move a step or two per turn, so the list is
    // nearly sorted and an insertion sort is close to free (the old counting sort cost ~4k fuel).
    if (group_order.size() != w.groups.size()) {
        group_order.resize(w.groups.size());
        for (std::size_t gi = 0; gi < w.groups.size(); ++gi) group_order[gi] = static_cast<int>(gi);
    }
    for (int gi : group_order) {
        if (!farm_score::kSingleGroupDiagnostic && n_order >= kMaxOrder) {
            farm_stats.add(farm_score::order_capped, w.groups.size() - n_order);
            break;
        }
        if (avoid_until[static_cast<std::size_t>(gi)] > round) continue;
        int dist = kUnreach;
        const std::uint16_t* f = have_ptrs ? w.field_ptr[static_cast<std::size_t>(gi)] : nullptr;
        if (f)
            dist = f[here_idx];
        else
            // Field not built yet (first turns only): straight-line distance is a lower bound.
            for (int ci : w.groups[static_cast<std::size_t>(gi)].camps) dist = std::min<int>(dist, cheb(w.camps[ci].pos, pos));
        if (dist == kUnreach) continue;
        int i = n_order++;
        for (; i > 0 && order[i - 1].first > dist; --i) order[i] = order[i - 1];
        order[i] = {dist, gi};
    }
    // Remember the order for next turn (groups skipped this turn keep their place at the end).
    {
        int k = 0;
        for (int i = 0; i < n_order; ++i) group_order[static_cast<std::size_t>(k++)] = order[i].second;
        if (k < static_cast<int>(group_order.size())) {
            // Append the groups not visited this turn, in their old relative order.
            std::vector<char>& seen = order_seen;
            seen.assign(w.groups.size(), 0);
            for (int i = 0; i < n_order; ++i) seen[static_cast<std::size_t>(order[i].second)] = 1;
            for (std::size_t gi = 0; gi < w.groups.size(); ++gi)
                if (!seen[gi]) group_order[static_cast<std::size_t>(k++)] = static_cast<int>(gi);
        }
    }
    }
    // Score the current target first: its (sticky) score is a strong bound.
    for (int oi = 1; oi < n_order; ++oi)
        if (order[oi].second == target_group) {
            std::swap(order[0], order[oi]);
            break;
        }
    // Route search keeps the K best first targets (pruning against the K-th best score).
    const int keep = !farm_score::kSingleGroupDiagnostic && tune::kRouteDepth >= 2 ? std::clamp(tune::kRouteK, 1, 8) : 1;
    RouteCand top[8];
    int n_top = 0;
    // With the route search, groups within kRouteSlack of the best single score stay candidates.
    auto prune_bound = [&] {
        if (keep == 1) return best_score;
        const float kth = n_top < keep ? 0.0f : top[n_top - 1].score;
        return std::max(kth, best_score * tune::kRouteSlack);
    };
    if (NOVA_BIGJOB > 0.0f && big_group.size() != w.groups.size()) {
        // Static per map: total gems over all groups, and which groups hold a dominant share of them.
        float total = 0.0f;
        for (const auto& g2 : w.groups) total += static_cast<float>(g2.gems);
        big_group.assign(w.groups.size(), 0);
        for (std::size_t k = 0; k < w.groups.size(); ++k)
            big_group[k] = total > 0.0f && w.groups[k].gems > 0 && static_cast<float>(w.groups[k].gems) >= NOVA_BIGJOB * total;
    }
    bool boot_items = false;
    if constexpr (farm_score::kCorrectBound || farm_score::kDiagnostics)
        boot_items = APEX_BOOT_ITEMS > 0 && my_items() < static_cast<float>(APEX_BOOT_ITEMS);
    // An enabled zero-weight model is an identity even with the work cap:
    // reserving learner::hi for it would admit extra candidates and consume
    // slots before reaching later groups. Inspect only the 16 used weights.
    constexpr float residual_bound = [] {
        if constexpr (learner::on) {
            for (int i = 0; i < learner::n && i < 16; ++i)
                if (learner::w[i] != 0.0f) return learner::hi;
        }
        return 1.0f;
    }();
    int scored_count = 0;
    int evaluated = 0; // groups past the bound (APEX_DET caps them)
    for (int oi = 0; oi < n_order; ++oi) {
        // Out of fuel (huge maps, first turn): settle for the best group scored so far.
        if (tune::kDet && !farm_score::kSingleGroupDiagnostic) {
            if (best >= 0 && evaluated >= tune::kDetScoreN) {
                farm_stats.add(farm_score::work_capped, n_order - oi);
                break;
            }
            if (best >= 0 && bc::fuel_remaining() < static_cast<std::uint64_t>(tune::kDetSafety)) {
                farm_stats.add(farm_score::fuel_capped, n_order - oi);
                break;
            }
        } else if (!tune::kDet && !farm_score::kSingleGroupDiagnostic && (oi & 3) == 3 && best >= 0 && bc::fuel_remaining() < tune::kScoreFloor) {
            farm_stats.add(farm_score::fuel_capped, n_order - oi);
            break;
        }
        farm_stats.add(farm_score::considered);
        const int dist = order[oi].first;
        const std::size_t gi = static_cast<std::size_t>(order[oi].second);
        auto& g = w.groups[gi];
        const int approach = std::max(0, dist - (g.state == GroupState::reward ? 0 : range));
        int wait = 0;
        if (g.state == GroupState::respawning)
            wait = static_cast<int>(static_cast<float>(std::max(0, g.respawn_round - round - approach)) * tune::kWaitWeight);
        if (NOVA_BIGCAMP > 0 && g.gems >= 120)
            wait = static_cast<int>(wait * (NOVA_BIGCAMP * 0.01f)); // big strips respawn on a cycle: cheap waiting
        farm_score::Bound admissible_bound;
        if constexpr (farm_score::kCorrectBound || farm_score::kDiagnostics) {
            const bool dominant = NOVA_BIGJOB > 0.0f && round >= NOVA_BIGJOB_FROM && gi < big_group.size() && big_group[gi];
            const bool boot = g.harmless && boot_items;
            const bool altar = tune::kAltarBonus > 0.0f && w.altar_ready && !g.camps.empty() &&
                fdist(w.altar_field, w.camps[g.camps[0]].pos) <= 6;
            const bool numbers = alive_us - alive_them >= tune::kNumbersInvade && w.enemy_ready && w.group_margin[gi] < 0;
            // Territory is the only omitted discount allowed to be negative by
            // its enable condition. Negative factors break monotonicity.
            admissible_bound = farm_score::upper_bound(
                static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold), approach, wait,
                tune::kCaptureTime, boot ? 1.0f + tune::kBootBonus : 1.0f,
                dominant ? NOVA_BIGJOB_MULT : 1.0f, altar ? 1.0f + tune::kAltarBonus : 1.0f,
                numbers ? 1.0f + tune::kNumbersBonus : 1.0f,
                NOVA_SEED > 0 ? std::abs(NOVA_SEED_EPS) : 0.0f,
                static_cast<int>(gi) == target_group ? tune::kStickiness : 1.0f,
                residual_bound, tune::kTerritory >= 0.0f);
            if (!admissible_bound.admissible) farm_stats.add(farm_score::bound_bypassed);
        }
        {
            const float legacy_bound = (static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold)) *
                tune::kStickiness / (static_cast<float>(approach + wait) + tune::kCaptureTime + 4.0f);
            const float best_case = farm_score::kCorrectBound ? admissible_bound.score : legacy_bound;
            const bool may_prune = !farm_score::kExhaustive && (!farm_score::kCorrectBound || admissible_bound.admissible);
            if (may_prune && best_case <= prune_bound()) {
                farm_stats.add(farm_score::math_pruned);
#ifdef APEX_SCOREDBG
                if (g.harmless && g.gems > 0 && round < 60)
                    std::printf("SDBG R%d slot%d G%zu pruned d%d st%d best_case %.3f bound %.3f\n", round, slot, gi, dist,
                        static_cast<int>(g.state), best_case, prune_bound());
#endif
                continue;
            }
        }
        ++evaluated;
        farm_stats.add(farm_score::evaluated);
        if (group_capture_covered(g)) {
            avoid_until[gi] = round + 6; // an ally is already capturing it
            continue;
        }
        if ((NOVA_DUO & 8) && duo_ignore_slot >= 0 && g.state == GroupState::reward && !g.camps.empty() && w.enemies.empty()) {
            // NOVA_DUO bit 8: our ranged follower takes the capture; we walk on and tank the next camp.
            bool follower_near = false;
            for (const auto& a : w.allies)
                if (a.id == ally_handle[static_cast<std::size_t>(duo_ignore_slot)] && cheb(a.pos, w.camps[g.camps[0]].pos) <= 4)
                    follower_near = true;
            if (follower_near) {
                avoid_until[gi] = round + 3;
                continue;
            }
        }
        if (tune::kShareGold && g.state == GroupState::reward && w.enemies.empty() &&
            poorer_ally_takes(g, static_cast<int>(gi), false)) {
            avoid_until[gi] = round + 4; // a poorer ally takes this reward
            continue;
        }
        int lower = 0, higher = 0;
        {
            PROF(s_claims);
            std::tie(lower, higher) = claims_split(static_cast<int>(gi));
        }
        const int claims = lower + higher;
        const float hp_left = g.hp_left(w.camps);
        int need = 1;
        {
            PROF(s_need);
            need = champions_needed(g, hp_left);
        }
        // NOVA_BIGJOB: the soak model caps at 7 and kept stampede's 17k-health gem camp "impossible" until round 250;
        // opponents with our lineup take it near round 150. A dominant camp is a whole-team job.
        if (NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi] && need > team_size) need = team_size;
        if (NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi] && round % 25 == 0)
            LOG("BIG R%d slot%d G%zu need%d team%d claims%d hp_left%.0f state%d dist%d\n", round, slot, gi, need, team_size,
                claims, hp_left, static_cast<int>(g.state), dist);
        if (need > team_size) continue;
        // A job that needs more champions than are on it: open it as a call for help (allies see the
        // claim and join), but only if no earlier slot already has an open call we could answer.
        bool initiating = need > std::max(claims + 1, participants_hint);
        // Siege: spells reach 4 tiles and camps only 3, so casters can wear any camp down safely.
        const float sdps = initiating && tune::kSiege && !in_role_pack ? siege_dps(g) : 0.0f;
        const bool siege = sdps > 0.0f;
        if (siege) initiating = false;
        else if (initiating) {
            const bool big = NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi];
            if (!tune::kInitiateJobs || no_initiate || round < call_ban_until || need > (big ? team_size : 2) || open_call_by_lower())
                continue;
        }
        const bool solo_job = need == 1 && (g.state == GroupState::reward || hp_left < 1500.0f);
        // Staffing cap (APEX_CAP_STAFF = n): a group that earlier-acting allies already staff to its
        // need (plus n - 1 spare) is theirs. Without it whole teams piled onto one wolf group, or
        // waited two at a time beside one respawning group, while the next group sat untouched.
        if (tune::kCapStaff > 0 && !solo_job && g.state != GroupState::reward && lower >= need + tune::kCapStaff - 1) continue;
        // Small jobs belong to the earliest-acting claimant; later slots leave them alone.
        // (Everyone reacting to everyone else's claims made whole teams flip targets each turn.)
        const int helpers = solo_job ? 0 : claims;
        const int participants = siege ? 1 : std::max({need, helpers + 1, participants_hint});
        const float kill = siege ? hp_left / sdps : hp_left / (my_dps(&g) + static_cast<float>(participants - 1) * dps);
        float value = static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold);
        if (APEX_BOOT_ITEMS > 0 && g.harmless &&
            ((farm_score::kCorrectBound || farm_score::kDiagnostics) ? boot_items : my_items() < static_cast<float>(APEX_BOOT_ITEMS)))
            value *= 1.0f + tune::kBootBonus;
        // (With gold sharing, finished groups ignore claims: the poorest champion there takes them.)
        if (solo_job && !(tune::kShareGold && g.state == GroupState::reward) &&
            (tune::kIncumbent ? senior_claims(static_cast<int>(gi)) : lower) > 0)
            value *= 0.15f;
        if (round - g.enemy_round < 15 && !(NOVA_BIGJOB > 0.0f && gi < big_group.size() && big_group[gi])) value *= 0.6f;
        if (NOVA_BIGJOB > 0.0f && round >= NOVA_BIGJOB_FROM && gi < big_group.size() && big_group[gi]) value *= NOVA_BIGJOB_MULT;
        // Altar-side groups (APEX_ALTAR_BONUS): their gems bank at once, without a trip.
        if (tune::kAltarBonus > 0.0f && w.altar_ready && !g.camps.empty() && fdist(w.altar_field, w.camps[g.camps[0]].pos) <= 6)
            value *= 1.0f + tune::kAltarBonus;
        // Numbers (APEX_NUMBERS_INVADE): with enough extra living champions, take the enemy side's groups.
        if (alive_us - alive_them >= tune::kNumbersInvade && w.enemy_ready && w.group_margin[gi] < 0) value *= 1.0f + tune::kNumbersBonus;
        // Territory (APEX_TERRITORY, until round APEX_TERRITORY_UNTIL): our own side first. The evaluator puts
        // early contact at -13 gems per champion; c1 spent 63% of its time on the enemy half against 46% for
        // the top teams and earned 30% less in the first 150 rounds.
        if (tune::kTerritory < 1.0f && round < tune::kTerritoryUntil && w.enemy_ready && w.group_margin[gi] < -tune::kTerritoryMargin)
            value *= tune::kTerritory;
        // Cohesion (APEX_COHESION): a camp far from every teammate gives up the safety of numbers. The top
        // teams operate 7.5 tiles apart against our 11.3 and die a third as often (540 decoded ladder games):
        // value falls by APEX_COHESION per tile beyond APEX_COHESION_DIST from the nearest living teammate.
        if (tune::kCohesion > 0.0f && !w.allies.empty() && !g.camps.empty()) {
            const vec2 gp = w.camps[g.camps[0]].pos;
            int dmin = 999;
            for (const auto& a : w.allies) dmin = std::min(dmin, cheb(a.pos, gp));
            const int over = dmin - tune::kCohesionDist;
            if (over > 0) value /= 1.0f + tune::kCohesion * static_cast<float>(over);
        }
        // NOVA_BUDDY: farm our own group, but near our buddy's target (read from its claim slot, so it works
        // out of vision): top bots farm separate camps a few tiles apart and fight with help close by.
        if (NOVA_BUDDY > 0.0f && !g.camps.empty()) {
            const vec2 anchor = buddy_anchor();
            if (anchor.x >= 0) {
                const int over = cheb(anchor, w.camps[g.camps[0]].pos) - NOVA_BUDDY_DIST;
                if (over > 0) value /= 1.0f + NOVA_BUDDY * static_cast<float>(over);
            }
        }
        // NOVA_SEED (0 = off): a small deterministic jitter on group values so one official map can be replayed
        // as many distinct games (evaluation only; neutral on average).
        if (NOVA_SEED > 0) {
            std::uint32_t hs = static_cast<std::uint32_t>(NOVA_SEED) * 2654435761u ^ static_cast<std::uint32_t>(gi) * 40503u ^
                static_cast<std::uint32_t>(round / 20) * 2246822519u ^ static_cast<std::uint32_t>(slot + 1) * 3266489917u;
            hs ^= hs >> 15;
            hs *= 2246822519u;
            hs ^= hs >> 13;
            value *= 1.0f + NOVA_SEED_EPS * (static_cast<float>(hs & 0xffffu) / 32767.5f - 1.0f);
        }
        // Champion-turns the team spends on this group, plus the health it drains: lost health costs a
        // trip home later (APEX_HP_COST turns per full health bar), which made wolves look better
        // than harmless gem clusters.
        float drain = 0.0f;
        if (tune::kHpCost > 0.0f && !g.harmless && g.state != GroupState::reward) {
            const float team_dps = my_dps(&g) + static_cast<float>(participants - 1) * dps;
            drain = soak_coefficient(static_cast<int>(gi), hp_left) * camp_taken_mult / std::max(team_dps, 1.0f) /
                std::max(max_hp, 1.0f) * tune::kHpCost;
        }
        const float cost = static_cast<float>(participants) * (static_cast<float>(approach + wait) + kill) +
            tune::kCaptureTime + 4.0f + drain;
        float score = value / cost;
#ifdef APEX_SCOREDBG
        if (g.harmless && g.gems > 0 && round < 60)
            std::printf("SDBG R%d slot%d G%zu d%d st%d claims %d/%d value %.1f cost %.1f score %.3f best %.3f\n", round, slot, gi,
                dist, static_cast<int>(g.state), lower, higher, value, cost, score, best_score);
#endif
        if (initiating) score *= 0.7f; // uncertain help: prefer jobs we can finish now
        if (siege) score *= 0.8f;
        if (static_cast<int>(gi) == target_group) score *= tune::kStickiness; // finish what we started
        if constexpr (learner::on) {
            // NOVA_LEARNER: learned residual on the hand score. Zero weights = identity, so the
            // switch is a no-op until trained weights land in NOVA_LEARNER_W.
            const int state_ord = static_cast<int>(g.state);
            float phi[16];
            learner::gather(&g, hp_left, kill, drain, approach, wait, dist, participants, need, state_ord,
                g.harmless, lower, higher, value, score, siege, initiating, solo_job, phi);
            phi[0] = static_cast<float>(g.gems) / 64.0f > 3.0f ? 3.0f : static_cast<float>(g.gems) / 64.0f;
            phi[1] = static_cast<float>(g.gold) / 64.0f > 3.0f ? 3.0f : static_cast<float>(g.gold) / 64.0f;
            score = learner::fix(score, phi);
        }
        if (tune::kBelief && tune::kBeliefW > 0.0f && score > prune_bound() && !g.camps.empty() && bc::fuel_remaining() > (tune::kDet ? static_cast<std::uint64_t>(tune::kDetSafety) : 30000)) {
            // Enemies that could be at this group before we are done with it (beliefs), beyond what our side
            // brings: contested jobs get champions caught alone.
            const int finish = approach + wait + static_cast<int>(kill);
            const vec2 gp = w.camps[g.camps[0]].pos;
            const float e = w.expected_enemies(gp, std::min(finish, 12) + 2);
            int friends = 1;
            for (const auto& a : w.allies) friends += cheb(a.pos, gp) <= 6;
            const float excess = e - tune::kBeliefAlly * static_cast<float>(friends);
            if (excess > 0.0f) score /= 1.0f + tune::kBeliefW * excess;
        }
        farm_stats.add(farm_score::scored);
        if constexpr (farm_score::kDiagnostics) ++scored_count;
        if constexpr (farm_score::kDiagnostics) {
            if (!std::isfinite(score) || (admissible_bound.admissible && score > admissible_bound.score)) {
                farm_stats.add(farm_score::violations);
                std::printf("FARM_BOUND_VIOLATION R%d slot%d G%zu score=%.9g bound=%.9g valid=%d\n",
                    round, slot, gi, score, admissible_bound.score, admissible_bound.admissible);
            }
        }
        if (score > best_score) {
            best_score = score;
            best = static_cast<int>(gi);
            best_need = need;
        }
        if (keep > 1 && (n_top < keep || score > top[n_top - 1].score)) {
            // Turns until this champion is free again: walk, wait, kill, capture.
            const int finish = approach + wait + static_cast<int>(kill) + tune::kCaptureTime;
            int i = n_top < keep ? n_top++ : keep - 1;
            for (; i > 0 && top[i - 1].score < score; --i) top[i] = top[i - 1];
            top[i] = {static_cast<int>(gi), need, value, cost, score, finish};
        }
    }
    if (keep > 1) {
        farm_stats.add(farm_score::route_candidates, n_top);
        farm_stats.add(farm_score::route_filtered, scored_count - n_top);
    }
    if (keep > 1 && n_top >= 2 && bc::fuel_remaining() > tune::kRouteFuel) {
        farm_stats.add(farm_score::route_calls);
        bool bank = false;
        const int pick = tune::kPlan ? plan_search(top, n_top, bank) : route_search(top, n_top);
        if (tune::kPlan && bank) plan_bank_round = round;
        if (pick >= 0) {
            best = top[pick].gi;
            best_need = top[pick].need;
            best_score = top[pick].score;
        }
    } else if (keep > 1) farm_stats.add(farm_score::route_skipped);
    return best;
    }
}

// Follow-up table for one champion alone, rebuilt once per turn in a single pass over the groups.
void Bot::refresh_follow_ups() {
    if (follow_round == round) return;
    follow_round = round;
    const std::size_t n = w.groups.size();
    follow_value.resize(n);
    follow_fixed.resize(n);
    follow_respawn.resize(n);
    const float base_dps = my_dps(nullptr);
    for (std::size_t gi = 0; gi < n; ++gi) {
        const auto& g = w.groups[gi];
        const float dps = (type == bc::champion_type::varg && g.camps.size() > 1) ? base_dps * 1.6f : base_dps;
        const float kill = g.hp_left(w.camps) / std::max(dps, 1.0f);
        float value = static_cast<float>(g.gems) + gold_weight() * static_cast<float>(g.gold);
        if (round - g.enemy_round < 15) value *= 0.6f;
        // Safe alone: the camps' damage over the kill stays well under our health.
        const bool solo = g.harmless || g.strength * kill < hp * 0.8f;
        if (!solo || avoid_until[gi] > round) value = 0.0f;
        follow_value[gi] = value;
        follow_fixed[gi] = kill + static_cast<float>(tune::kCaptureTime) + 4.0f;
        follow_respawn[gi] = g.state == GroupState::respawning ? g.respawn_round : -1;
    }
    // Groups a teammate has claimed are theirs.
    for (int s = 0; s < std::min(team_size, kClaimSlots); ++s) {
        const int c = claim_of(s);
        if (s != slot && c >= 0 && c < static_cast<int>(n)) follow_value[static_cast<std::size_t>(c)] = 0.0f;
    }
}

// Extend each first target with its best follow-up group(s) and return the index (into top) of
// the route with the best gems per champion-turn. Group-to-group distances are static rows.
int Bot::route_search(const RouteCand* top, int n_top) {
    PROF(route);
    refresh_follow_ups();
    const int n = static_cast<int>(w.groups.size());
    const int depth = std::min(tune::kRouteDepth, 3);
    const int m_keep = std::clamp(tune::kRouteM, 1, 4);
    const float* fv = follow_value.data();
    const float* ff = follow_fixed.data();
    const int* fr = follow_respawn.data();
    int new_rows = 0; // building a row costs ~2k fuel; at most two per turn
    auto row_of = [&](int from) {
        const bool may = new_rows < 2;
        const bool had = w.pair_row(from, false) != nullptr;
        const std::uint16_t* r = w.pair_row(from, may);
        if (r && !had) ++new_rows;
        return r;
    };
    // Cost of walking to g from a point `t0` turns from now, `d` steps away.
    auto leg_cost = [&](int g, int d, int t0, int& done) {
        const int walk = std::max(0, d - range);
        const int arrive = t0 + walk;
        int wait = fr[g] >= 0 ? std::max(0, fr[g] - round - arrive) : 0;
        if (NOVA_BIGCAMP > 0 && w.groups[static_cast<std::size_t>(g)].gems >= 120)
            wait = static_cast<int>(wait * (NOVA_BIGCAMP * 0.01f)); // big strips: waiting on the respawn cycle is cheap
        done = arrive + wait + static_cast<int>(ff[g]);
        return static_cast<float>(walk + wait) + ff[g];
    };
    int best_i = -1;
    float best_rate = 0.0f;
    for (int i = 0; i < n_top; ++i) {
        if (bc::fuel_remaining() < tune::kRouteFuel / 2) break;
        const RouteCand& c = top[i];
        const float sticky = c.gi == target_group ? tune::kStickiness : 1.0f;
        float route = c.score; // the first target alone (stickiness included)
        const std::uint16_t* row1 = row_of(c.gi);
        if (row1) {
            struct Step {
                int gi;
                float value, cost;
                int done;
            };
            Step keep2[4];
            int n2 = 0;
            for (int g2 = 0; g2 < n; ++g2) {
                if (fv[g2] <= 0.0f || g2 == c.gi || row1[g2] == kUnreach) continue;
                farm_stats.add(farm_score::route_expansions);
                int done2 = 0;
                const float cost2 = leg_cost(g2, row1[g2], c.finish, done2);
                const float rate = (c.value + fv[g2]) / (c.cost + cost2) * sticky;
                if (rate > route) route = rate;
                if (depth >= 3) {
                    // Keep the m_keep follow-ups with the best rate of their own for level 3.
                    const float own = fv[g2] / cost2;
                    if (n2 == m_keep && keep2[n2 - 1].value / keep2[n2 - 1].cost >= own) continue;
                    int k = n2 < m_keep ? n2++ : m_keep - 1;
                    for (; k > 0 && keep2[k - 1].value / keep2[k - 1].cost < own; --k) keep2[k] = keep2[k - 1];
                    keep2[k] = {g2, fv[g2], cost2, done2};
                }
            }
            for (int j = 0; j < n2; ++j) {
                const Step& s2 = keep2[j];
                const std::uint16_t* row2 = row_of(s2.gi);
                if (!row2) continue;
                for (int g3 = 0; g3 < n; ++g3) {
                    if (fv[g3] <= 0.0f || g3 == c.gi || g3 == s2.gi || row2[g3] == kUnreach) continue;
                    farm_stats.add(farm_score::route_expansions);
                    int done3 = 0;
                    const float cost3 = leg_cost(g3, row2[g3], s2.done, done3);
                    const float rate = (c.value + s2.value + fv[g3]) / (c.cost + s2.cost + cost3) * sticky;
                    if (rate > route) route = rate;
                }
            }
        }
        if (route > best_rate) {
            best_rate = route;
            best_i = i;
        }
    }
    return best_i;
}

// Farming planner (APEX_PLAN): searches short sequences of jobs, where a job is a camp group (walk, wait for a
// respawn, kill, capture) or a bank trip (walk to the nearest altar, channel ceil(gems / 10)), and returns the
// first job of the sequence with the best gain per turn. Gains use the position evaluator's learned rates: a
// banked gem is worth 1, a carried one APEX_PLAN_CARRY (0.75 from 311k top-team positions), so banking turns
// every carried gem's discount into value. Returns the index into `top`, or -1 with bank_first set.
int Bot::plan_search(const RouteCand* top, int n_top, bool& bank_first) {
    PROF(plan);
    bank_first = false;
    refresh_follow_ups();
    if (!w.ensure_altar_field()) return -1;
    const int n = static_cast<int>(w.groups.size());
    const float kappa = tune::kPlanCarry;
    const float* fv = follow_value.data();
    const float* ff = follow_fixed.data();
    const int* fr = follow_respawn.data();
    int new_rows = 0;
    auto row_of = [&](int from) {
        const bool may = new_rows < 2;
        const bool had = w.pair_row(from, false) != nullptr;
        const std::uint16_t* r = w.pair_row(from, may);
        if (r && !had) ++new_rows;
        return r;
    };
    auto gems_of = [&](int gi) { return static_cast<float>(w.groups[static_cast<std::size_t>(gi)].gems); };
    auto altar_from_group = [&](int gi) {
        const auto& g = w.groups[static_cast<std::size_t>(gi)];
        return g.camps.empty() ? kUnreach : static_cast<int>(fdist(w.altar_field, w.camps[g.camps[0]].pos));
    };
    auto channel = [](float bag) { return std::max(1, static_cast<int>(std::ceil(bag / 10.0f))); };
    const float carried0 = static_cast<float>(gems);
    // Gain per turn of a finished plan: groups collected (value), gems collected, time, and whether it banked.
    auto rate = [&](float value, float collected, float time, bool banked) {
        (void)collected; // banked: everything is worth 1, so the bag we started with gains its discount back
        const float gain = banked ? value + (1.0f - kappa) * carried0 : kappa * value;
        return gain / std::max(time, 1.0f);
    };
    float best = 0.0f;
    int best_i = -1;
    // Bank right now.
    const int here_altar = static_cast<int>(fdist(w.altar_field, pos));
    if (gems >= 10 && here_altar != kUnreach) {
        const float t = static_cast<float>(here_altar + channel(carried0));
        best = (1.0f - kappa) * carried0 / std::max(t, 1.0f) * tune::kPlanBankBias;
        bank_first = true;
    }
    auto leg = [&](int g, int d, int t0, int& done) {
        const int walk = std::max(0, d - range);
        const int arrive = t0 + walk;
        const int wait = fr[g] >= 0 ? std::max(0, fr[g] - round - arrive) : 0;
        done = arrive + wait + static_cast<int>(ff[g]);
        return static_cast<float>(walk + wait) + ff[g];
    };
    // APEX_PLAN 2: the camp choice stays the greedy best (top[0]); the planner only decides whether to bank first.
    if (tune::kPlan == 2) n_top = std::min(n_top, 1);
    for (int i = 0; i < n_top; ++i) {
        if (bc::fuel_remaining() < tune::kRouteFuel / 2) break;
        const RouteCand& c = top[i];
        const float sticky = c.gi == target_group ? tune::kStickiness : 1.0f;
        const float g1 = gems_of(c.gi);
        const float t1 = static_cast<float>(c.finish);
        float route = rate(c.value, g1, t1, false) * sticky;
        // Camp, then bank.
        const int a1 = altar_from_group(c.gi);
        if (a1 != kUnreach) route = std::max(route, rate(c.value, g1, t1 + static_cast<float>(a1 + channel(carried0 + g1)), true) * sticky);
        const std::uint16_t* row1 = row_of(c.gi);
        if (row1) {
            struct Step {
                int gi;
                float value, cost;
                int done;
            };
            Step keep2[4];
            int n2 = 0;
            const int m_keep = std::clamp(tune::kRouteM, 1, 4);
            for (int g2 = 0; g2 < n; ++g2) {
                if (fv[g2] <= 0.0f || g2 == c.gi || row1[g2] == kUnreach) continue;
                farm_stats.add(farm_score::route_expansions);
                int done2 = 0;
                const float cost2 = leg(g2, row1[g2], c.finish, done2);
                const float own = fv[g2] / cost2;
                if (n2 == m_keep && keep2[n2 - 1].value / keep2[n2 - 1].cost >= own) continue;
                int k = n2 < m_keep ? n2++ : m_keep - 1;
                for (; k > 0 && keep2[k - 1].value / keep2[k - 1].cost < own; --k) keep2[k] = keep2[k - 1];
                keep2[k] = {g2, fv[g2], cost2, done2};
            }
            for (int j = 0; j < n2; ++j) {
                const Step& s2 = keep2[j];
                const float v12 = c.value + s2.value, g12 = g1 + gems_of(s2.gi), t12 = t1 + s2.cost;
                route = std::max(route, rate(v12, g12, t12, false) * sticky);
                const int a2 = altar_from_group(s2.gi);
                if (a2 != kUnreach) route = std::max(route, rate(v12, g12, t12 + static_cast<float>(a2 + channel(carried0 + g12)), true) * sticky);
                if (tune::kPlanDepth >= 3) {
                    const std::uint16_t* row2 = row_of(s2.gi);
                    if (!row2) continue;
                    for (int g3 = 0; g3 < n; ++g3) {
                        if (fv[g3] <= 0.0f || g3 == c.gi || g3 == s2.gi || row2[g3] == kUnreach) continue;
                        farm_stats.add(farm_score::route_expansions);
                        int done3 = 0;
                        const float cost3 = leg(g3, row2[g3], s2.done, done3);
                        const float v = v12 + fv[g3], gg = g12 + gems_of(g3), tt = t12 + cost3;
                        route = std::max(route, rate(v, gg, tt, false) * sticky);
                        const int a3 = altar_from_group(g3);
                        if (a3 != kUnreach) route = std::max(route, rate(v, gg, tt + static_cast<float>(a3 + channel(carried0 + gg)), true) * sticky);
                    }
                }
            }
        }
        if (route > best) {
            best = route;
            best_i = i;
            bank_first = false;
        }
    }
    return best_i;
}
// Same identity as the draft (draft.cpp): map size, draft picks and the engine's camp-group count.
bool Bot::is_official_map() {
    if (official_map < 0) {
        std::uint32_t ngroups = 0;
        const int ng = d::sdk_map_num_camp_groups(&ngroups) == 0 ? static_cast<int>(ngroups) : -1;
        unsigned picks = static_cast<unsigned>(team_size); // champions; Cassi & Issac are two bodies for one pick
        try {
            picks = bc::draft::team_size();
        } catch (...) {
        }
        official_map = nova_map_id(static_cast<int>(bc::map::width()), static_cast<int>(bc::map::height()), picks, ng) >= 0;
    }
    return official_map == 1;
}

bool Bot::farm() {
    PROF(farm);
    if (avoid_until.size() != w.groups.size()) avoid_until.assign(w.groups.size(), 0);
    int best = -1, best_need = 1;
    float best_score = 0;

    // Short on fuel (first turn on a huge map, or a heavy fight turn): keep the current target rather
    // than rescoring every group, so the turn cannot run out of fuel and get suspended.
    // Deterministic (APEX_DET): keep the current target in turns the fight search ran (its playouts used the fuel),
    // otherwise only below the safety floor.
    // The first turn (initialisation on the doubled budget) never farms: it ends near the floor on big maps.
    const bool low_fuel = tune::kDet ? (field_calls == 0 || ts_ran_round == round ||
                                        bc::fuel_remaining() < static_cast<std::uint64_t>(tune::kDetSafety) + 10000)
                                     : bc::fuel_remaining() < tune::kLowFuel;
#if APEX_PROFILE
    ++prof::farm_calls;
    prof::farm_low += low_fuel;
    prof::farm_route_skip += bc::fuel_remaining() < tune::kRouteFuel;
    prof::td_then_farm += prof::td_round == round;
#endif
    farm_stats.add(farm_score::stage_attempts);
    if (low_fuel) {
        farm_stats.add(farm_score::stage_skipped);
        if (target_group < 0 || target_group >= static_cast<int>(w.groups.size()) || avoid_until[target_group] > round) {
            farm_stats.add(farm_score::fallback_empty);
#if APEX_PROFILE
            ++prof::farm_gaveup;
#endif
            return false;
        }
        best = target_group;
        farm_stats.add(farm_score::fallback_kept);
    }
    // Escort (APEX_ESCORT): a caster farms where the nearest healthy melee ally farms, so a protector is
    // beside it when enemies arrive (alone, casters spent up to 30% of their turns fleeing).
    if (tune::kEscort > 0 && !low_fuel && !melee && best < 0) {
        int lead = -1, lead_d = tune::kEscort + 1;
        for (const auto& a : w.allies) {
            if (a.range > 1 || a.hp < a.max_hp * 0.3f) continue;
            const int d = cheb(a.pos, pos);
            if (d >= lead_d) continue;
            int s = -1;
            for (int k = 0; k < std::min(team_size, kClaimSlots); ++k)
                if (ally_handle[static_cast<std::size_t>(k)] == a.id) s = k;
            if (s < 0 || s == slot) continue;
            const int lc = claim_of(s);
            if (lc >= static_cast<int>(w.groups.size()) || avoid_until[lc] > round || group_capture_covered(w.groups[lc])) continue;
            lead_d = d;
            lead = lc;
        }
        if (lead >= 0) {
            best = lead;
            best_need = 1;
            mode = "escort";
        }
    }
    if (tune::kRoleSquad > 0)
        for (const auto& a : w.allies)
            for (int k = 0; k < std::min(team_size, 8); ++k)
                if (ally_handle[static_cast<std::size_t>(k)] == a.id) slot_type[static_cast<std::size_t>(k)] = a.type;
    // Role squads (APEX_ROLE_SQUAD): the casters and Issac (and Ren at 2) farm as one pack behind the
    // lowest living member, as the top teams do; the other melee champions farm alone.
    if (tune::kRoleSquad > 0 && !low_fuel && best < 0) {
        auto in_pack = [&](bc::champion_type t) {
            return t == bc::champion_type::cassi || t == bc::champion_type::issac || t == bc::champion_type::stellar ||
                t == bc::champion_type::tavi || t == bc::champion_type::prism || t == bc::champion_type::willow ||
                (tune::kRoleSquad >= 2 && t == bc::champion_type::ren);
        };
        if (in_pack(type)) {
            int leader = -1, melee_leader = -1, alive = 0;
            for (int s = 0; s < std::min(team_size, kClaimSlots); ++s) {
                const auto t = slot_type[static_cast<std::size_t>(s)];
                if (!in_pack(t)) continue;
                if (s != slot && claim_of(s) == static_cast<int>(kClaimCodeDead)) continue;
                ++alive;
                if (leader < 0) leader = s;
                if (melee_leader < 0 && (t == bc::champion_type::issac || t == bc::champion_type::ren)) melee_leader = s;
            }
            if (melee_leader >= 0) leader = melee_leader; // a melee member picks jobs it can fight (no slow sieges)
            in_role_pack = alive >= 2;
            if (alive >= 2) {
                if (leader == slot) {
                    best = score_groups(alive, best_need, best_score);
                    best_need = 1; // the pack is with us: no staging for helpers
                } else if (leader >= 0) {
                    const int lc = claim_of(leader);
                    if (lc < static_cast<int>(w.groups.size()) && avoid_until[lc] <= round &&
                        (tune::kSquadShare || !group_capture_covered(w.groups[lc]))) {
                        best = lc;
                        best_need = 1;
                    } else {
                        // No usable job from the leader: close in on it rather than wander off alone.
                        for (const auto& a : w.allies)
                            if (a.id == ally_handle[static_cast<std::size_t>(leader)] && cheb(a.pos, pos) > 2) {
                                bc::bitboard goal;
                                goal.clear();
                                goal.draw_chebyshev(a.pos, 2.0f);
                                mode = "regroup";
                                walk_toward(goal);
                                return true;
                            }
                        if (lc < static_cast<int>(w.groups.size())) return false; // beside the leader: hold
                    }
                }
            }
        }
    }
    // Squads: slots split round-robin; the lowest living slot leads and the rest follow its claim.
    const int squads = tune::kSquads;
    if (squads > 0 && !low_fuel) {
        const int my_squad = slot % squads;
        int leader = -1, alive = 0;
        for (int s = my_squad; s < std::min(team_size, kClaimSlots); s += squads) {
            if (s != slot && claim_of(s) == static_cast<int>(kClaimCodeDead)) continue; // dead members do not count
            ++alive;
            if (leader < 0) leader = s;
        }
        if (leader == slot) {
            best = score_groups(alive, best_need, best_score);
        } else if (leader >= 0) {
            const int lc = claim_of(leader);
            if (lc < static_cast<int>(w.groups.size()) && avoid_until[lc] <= round && (tune::kSquadShare || !group_capture_covered(w.groups[lc]))) {
                best = lc;
                best_need = 1; // the squad arrives together; no staging for followers
            }
        }
    }
    // NOVA_DUO: the ranged member of a buddy pair joins its melee buddy's camp fight (fight_camps puts ranged
    // champions at full range, so the closer buddy draws the camps). Reward, respawning and harmless groups
    // are not joined: there the ranged member picks its own job.
    bool duo_leader = false;
    duo_ignore_slot = -1;
    if (NOVA_DUO > 0 && duo_map_ok < 0) duo_map_ok = NOVA_DUO_MAPS == 0 || !is_official_map();
    if (NOVA_DUO > 0 && duo_map_ok == 1 && !low_fuel && best < 0 && round < NOVA_DUO_UNTIL) {
        const int b = buddy();
        auto melee_type = [](bc::champion_type t) {
            return t == bc::champion_type::ryth || t == bc::champion_type::vendetta || t == bc::champion_type::issac ||
                t == bc::champion_type::marcella || t == bc::champion_type::ren || t == bc::champion_type::varg;
        };
        const bool buddy_alive = b >= 0 && claim_of(b) != static_cast<int>(kClaimCodeDead);
        if (buddy_alive && melee && !melee_type(slot_type[static_cast<std::size_t>(b)])) {
            duo_leader = (NOVA_DUO & 2) != 0;
            duo_ignore_slot = b; // our follower's claim on our job is help, not a rival claim
        }
        if (buddy_alive && !melee && melee_type(slot_type[static_cast<std::size_t>(b)])) {
            const int bcl = claim_of(b);
            bool keep_own = false;
            if ((NOVA_DUO & 4) && target_group >= 0 && target_group < static_cast<int>(w.groups.size()) && target_group != bcl) {
                const auto& og = w.groups[static_cast<std::size_t>(target_group)];
                if (og.state == GroupState::alive && !og.harmless)
                    for (int ci : og.camps)
                        keep_own |= w.camps[ci].dead || w.camps[ci].hp < w.camps[ci].max_hp; // already engaged
            }
            if ((NOVA_DUO & 8) && target_group >= 0 && target_group < static_cast<int>(w.groups.size()) &&
                w.groups[static_cast<std::size_t>(target_group)].state == GroupState::reward && w.has_field(target_group) &&
                w.group_distance(target_group, pos) <= 4 && !group_capture_covered(w.groups[static_cast<std::size_t>(target_group)])) {
                keep_own = true; // we take the capture; the buddy has moved on to tank the next camp
                best = target_group;
                best_need = 1;
            }
            if (!keep_own && best < 0 && bcl >= 0 && bcl < static_cast<int>(w.groups.size()) && avoid_until[static_cast<std::size_t>(bcl)] <= round) {
                const auto& bg = w.groups[static_cast<std::size_t>(bcl)];
                const int dist = w.has_field(bcl) ? w.group_distance(bcl, pos) : kUnreach;
                if (bg.state == GroupState::alive && !bg.harmless && dist <= NOVA_DUO_DIST) {
                    best = bcl;
                    best_need = 1;
                }
                LOG("DUO R%d slot%d buddy%d claim%d state%d harmless%d dist%d -> best%d\n", round, slot, b, bcl,
                    static_cast<int>(bg.state), bg.harmless ? 1 : 0, dist, best);
            }
        }
    }
    // Pair mode (WHIM's opening shape): the last kPair claim slots farm as one squad (the lowest
    // living one picks, the others follow its claim), so two-champion camps need no help calls;
    // the other champions farm solo.
    no_initiate = false;
    const int claimable = std::min(team_size, kClaimSlots);
    if (tune::kPair > 0 && squads == 0 && !low_fuel && best < 0 && claimable >= tune::kPair + 1) {
        const int first = claimable - tune::kPair;
        if (slot >= first && slot < claimable) {
            int leader = -1, alive = 0;
            for (int s = first; s < claimable; ++s) {
                if (s != slot && claim_of(s) == static_cast<int>(kClaimCodeDead)) continue;
                ++alive;
                if (leader < 0) leader = s;
            }
            if (leader == slot) {
                best = score_groups(alive, best_need, best_score);
            } else if (leader >= 0) {
                const int lc = claim_of(leader);
                if (lc < static_cast<int>(w.groups.size()) && avoid_until[lc] <= round && (tune::kSquadShare || !group_capture_covered(w.groups[lc]))) {
                    best = lc;
                    best_need = 1;
                }
            }
        } else {
            no_initiate = !tune::kSoloInitiate;
        }
    }
    if (best < 0) best = score_groups(duo_leader ? 2 : 1, best_need, best_score);
    if (best < 0 && (tune::kDet ? ts_ran_round != round && bc::fuel_remaining() > static_cast<std::uint64_t>(tune::kDetSafety) + 10000
                                : bc::fuel_remaining() > tune::kLowFuel / 2)) {
        // Nothing is safe to finish alone: chip at the best big camp anyway. Damage persists,
        // and the disengage rule walks us out before the camp kills us.
        best = score_groups(team_size, best_need, best_score);
        best_need = 1;
    }

    // The planner chose a bank trip first (APEX_PLAN).
    if (tune::kPlan && plan_bank_round == round && gems >= 10) {
        force_tribute = true;
        if (tribute_if_needed()) {
            mode = "plan-bank";
            return true;
        }
        force_tribute = false;
    }
    if (best != target_group) {
        staging_since = -1;
        my_claim_since = round;
    }
    target_group = best;
    write_claim(best);
    if (best < 0) return false;
    if (tune::kShareGold && w.groups[best].state == GroupState::reward && bit(w.groups[best].tiles, pos) &&
        w.enemies.empty() && poorer_ally_takes(w.groups[best], best, true)) {
        // A poorer ally is on the region: progress continues without us and it gets the whole reward.
        avoid_until[best] = round + 8;
        best = score_groups(1, best_need, best_score);
        staging_since = -1;
        my_claim_since = round;
        target_group = best;
        write_claim(best);
        if (best < 0) return false;
    }
    auto& g = w.groups[best];
    if (tune::kRecallTravel > 0 && g.state != GroupState::reward && w.enemies.empty() && !bit(w.hot_zone, pos) &&
        !bit(w.ally_spawn, pos) && w.has_field(best)) {
        // The target is far from us but near home: recall (5 turns, heals and shops) and walk from spawn.
        const int here = w.group_distance(best, pos);
        int from_home = kUnreach;
        for (vec2 sp : w.spawn_tiles) from_home = std::min(from_home, w.group_distance(best, sp));
        if (here != kUnreach && from_home != kUnreach && here > from_home + 5 + tune::kRecallTravel) {
            bool threat = false;
            for (const auto& sg : w.sightings) threat |= round - sg.round <= 5 && cheb(sg.pos, pos) <= 8;
            if (!threat) {
                try {
                    bc::self().recall();
                    mode = "recall-travel";
                    return true;
                } catch (const std::exception& e) {
                    LOG("recall-travel error %s\n", e.what());
                }
            }
        }
    }

    switch (g.state) {
    case GroupState::reward: {
        mode = "capture";
        if (bit(g.tiles, pos)) return true; // capturing: hold position
        mode = "to-capture";
        walk_toward(g.tiles);
        return true;
    }
    case GroupState::respawning: {
        mode = "wait-respawn";
        if (tune::kUseWaits) {
            const int wait = g.respawn_round - round;
            const int altar_trip = gems >= 10 && w.ensure_altar_field()
                ? 2 * static_cast<int>(fdist(w.altar_field, pos)) + (gems + 9) / 10 : 0;
            const int home_trip = std::min(2 * static_cast<int>(fdist(w.spawn_field, pos)), 12);
            if (gems >= 10 && wait > altar_trip + 2) {
                force_tribute = true;
                return tribute_if_needed();
            }
            if (gold >= 90 && wait > home_trip + 4 && type != bc::champion_type::tavi) {
                heading_home = true;
                return go_home_if_needed();
            }
        }
        bc::bitboard goal;
        goal.clear();
        for (int ci : g.camps) goal.draw_chebyshev(w.camps[ci].pos, static_cast<float>(range));
        goal &= ~g.tiles; // standing on a spawn tile delays the respawn
        if (bit(g.tiles, pos) || !bit(goal, pos)) walk_toward(goal);
        return true;
    }
    case GroupState::alive: return fight_camps(g, best_need);
    }
    return false;
}

void Bot::opportunistic_attack() {
    auto& me = bc::self();
    if (!me.can_attack()) return;
    for (const auto& e : w.enemies)
        if (cheb(e.pos, pos) <= range && attack_unit(e.unit, e.pos)) return;
}

// Wards are free and permanent: leave one at each objective we work at (a capture or an altar),
// so our vision keeps reporting camp timers and enemy carriers there after we leave.
void Bot::drop_ward() {
    if (!tune::kWards) return;
    const bool at_objective = std::string_view(mode) == "capture" || std::string_view(mode) == "tribute" ||
        std::string_view(mode) == "siege";
    if (!at_objective) return;
    if (ward_pos.x >= 0 && cheb(ward_pos, pos) <= 10) return;
    auto& me = bc::self();
    try {
        if (me.ward_cooldown() > 0) return;
        for (int r = 0; r <= 1; ++r)
            for (int dx = -r; dx <= r; ++dx)
                for (int dy = -r; dy <= r; ++dy) {
                    const vec2 q{pos.x + dx, pos.y + dy};
                    if (!in_bounds(q, w.width, w.height) || bit(w.static_blocked, q)) continue;
                    if (me.place_ward(q)) {
                        ward_pos = q;
                        return;
                    }
                }
    } catch (const std::exception& e) {
        LOG("ward error %s\n", e.what());
    }
}

// ---------------------------------------------------------------- turn

// Precompute group distance fields with whatever fuel the turn left over. Each build's search depth
// is capped to fit (cost is predictable from the depth), so a build never runs the turn out of fuel.
void Bot::update_numbers() {
    if (num_handles[0].empty()) {
        for (const auto& c : bc::champions(team::ally)) num_handles[0].push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)));
        for (const auto& c : bc::champions(team::enemy)) num_handles[1].push_back(static_cast<std::uint32_t>(static_cast<bc::handle_champion_t>(c)));
    }
    int n[2] = {0, 0};
    for (int s = 0; s < 2; ++s)
        for (std::uint32_t h : num_handles[s]) {
            std::int32_t alive = 1;
            d::sdk_champion_is_alive(h, &alive);
            n[s] += alive != 0;
        }
    alive_us = n[0];
    alive_them = n[1];
}

void Bot::build_fields() {
    if (tune::kDet) {
        // Deterministic (APEX_DET): a fixed number of full-depth fields per turn: 2x on the first turn (it also
        // initialises), 4x on dead turns, kDetFields on calm turns, none with an enemy in view.
        const std::uint64_t full = static_cast<std::uint64_t>(w.width + w.height + 40) * field_chunks() + 3000;
        int quota = field_calls++ == 0 ? (NOVA_FUEL_FIRST ? 1000 : 2 * tune::kDetFields)
                                       : was_dead ? (NOVA_FUEL_FIRST ? 1000 : 4 * tune::kDetFields)
                                                  : w.enemies.empty() ? (NOVA_FUEL_OPEN ? 6 : tune::kDetFields)
                                                                      : 0;
        auto room = [&] {
            // NOVA_FUEL_OPEN keeps a route-search-sized reserve: the per-champion work below still
            // needs to clear the 40k route-search gate after prospecting has taken its share.
            return bc::fuel_remaining() >
                full + static_cast<std::uint64_t>(NOVA_FUEL_OPEN ? 30000 : tune::kDetSafety);
        };
        if (!w.altar_ready && quota > 0 && room()) w.ensure_altar_field(), --quota;
        if ((tune::kTerritory < 1.0f || tune::kNumbersInvade < 99) && !w.enemy_ready && quota > 0 && room()) w.ensure_enemy_field(), --quota;
        if (!w.fields_ready() && quota > 0) w.prepare_field_order();
        while (!w.fields_ready() && quota > 0 && !w.field_order.empty() && room()) {
            PROF(field);
            if (!w.build_next_group_field(full)) return;
            --quota;
        }
        return;
    }
    if (!w.altar_ready &&
        bc::fuel_remaining() > tune::kFieldBuildReserve + static_cast<std::uint64_t>(w.width + w.height + 40) * field_chunks())
        w.ensure_altar_field();
    if ((tune::kTerritory < 1.0f || tune::kNumbersInvade < 99) && !w.enemy_ready &&
        bc::fuel_remaining() > tune::kFieldBuildReserve + static_cast<std::uint64_t>(w.width + w.height + 40) * field_chunks())
        w.ensure_enemy_field();
    if (!w.fields_ready() && bc::fuel_remaining() > tune::kFieldBuildReserve + 15000) w.prepare_field_order();
    while (!w.fields_ready()) {
        const std::uint64_t left = bc::fuel_remaining();
        if (left < tune::kFieldBuildReserve || w.field_order.empty()) return;
        PROF(field);
        if (!w.build_next_group_field(left - tune::kFieldBuildReserve)) return;
    }
}

void Bot::play_turn() {
    PROF(turn);
#ifdef APEX_FUEL_NOISE
    {
        // Test only: burn a fixed extra amount of fuel each turn (checks how sensitive decisions are to fuel use).
        volatile std::uint32_t sink = 0;
        for (int i = 0; i < APEX_FUEL_NOISE; ++i) sink = sink + static_cast<std::uint32_t>(i);
    }
#endif
    auto& me = bc::self();
    round = static_cast<int>(bc::round_number());
    if (!inited) {
        PROF(init);
        init();
    }
    if (APEX_BRAIN) {
        brain::load(team_size);
        brain::adapt(static_cast<int>(bc::global_submitted_gems(team::ally)),
            static_cast<int>(bc::global_submitted_gems(team::enemy)));
    }
    // Opening bank (APEX_EARLY_CARRY): c5 carried ~56 gems from round 50 into the mid-map fights (killers take
    // bags) and banked 3 by round 50 against the top teams' 26; bank smaller bags until APEX_EARLY_UNTIL.
    if (tune::kEarlyCarry > 0 && round < tune::kEarlyUntil) tune::kTributeCarry = std::min(tune::kTributeCarry, tune::kEarlyCarry);
    if (NOVA_GOLD_UNSEEN >= 0.0f && inited && !is_official_map()) tune::kGoldWeight = NOVA_GOLD_UNSEEN;
    if (NOVA_LATE_CARRY < 1.0f && round >= NOVA_LATE_CARRY_R)
        tune::kTributeCarry = std::max(8, static_cast<int>(static_cast<float>(tune::kTributeCarry) * NOVA_LATE_CARRY + 0.5f));
    cast_done = false;
    keep_channel = false;
    walked = false;
    force_tribute = false;
    shared = read_shared();
    if (tune::kIncumbent && inited) track_claims();

    const auto state = me.state();
    if (state.is_dead() && !was_dead) {
        // One line per death: what we were doing and what we could see.
        LOG("DEATH R%d %s at(%d,%d) gems%d mode=%s G%d enemies=%zu nearest=%d hpPrev=%.0f\n", round,
            bc::to_string(type).c_str(), pos.x, pos.y, gems, mode, target_group, w.enemies.size(), last_enemy_dist, hp);
        if (APEX_DEATHLOG) {
            std::printf("DEATH R%d %s gems%d\n", round, bc::to_string(type).c_str(), gems);
            for (int k = 0; k < 4; ++k) {
                const auto& n = notes[(note_i + k) & 3];
                if (n.round == 0) continue;
                std::printf("  R%d %s p(%d,%d) hp%d/%d g%d mode=%s G%d enemies=%d near=%d ehp=%d allies4=%d spawn=%d\n",
                    n.round, bc::to_string(type).c_str(), n.pos.x, n.pos.y, n.hp, n.max_hp, n.gems, n.mode, n.target,
                    n.enemies, n.near_enemy, n.enemy_hp, n.allies_near, n.spawn_dist);
            }
        }
    }
#ifdef APEX_FIGHTLOG
    if (state.is_dead() && !was_dead) std::printf("DIED R%d %s\n", round, bc::to_string(type).c_str());
#endif
    was_dead = state.is_dead();
    debug_block({
        if (state.is_dead()) {
            std::printf("P %d %u -1 -1 0 0 0\n", round, static_cast<unsigned>(static_cast<bc::handle_champion_t>(me)));
        } else {
            const auto p = me.position();
            std::printf("P %d %u %d %d %.0f %.0f %u\n", round, static_cast<unsigned>(static_cast<bc::handle_champion_t>(me)), p.x, p.y,
                me.health(), me.max_health(), me.gem_count());
        }
    });
    if (state.is_dead()) {
        episode_tick(true);
#ifdef APEX_TSTRACE
        if (!ts_dead_logged && (ts_dead_logged = true))
            for (int k = 0; k < 2; ++k) {
                const std::string& t = ts_trace[(ts_trace_i + k) & 1];
                if (!t.empty()) std::printf("DIED R%d after %s\n", round, t.c_str());
            }
#endif
        gold = static_cast<int>(me.gold_count());
        type = me.champion_name();
        shop();
        refresh_emblems();
        heading_home = false;
        if (inited && target_group != kClaimDead) {
            target_group = kClaimDead;
            write_claim(kClaimDead);
        }
        // Dead turns have the whole budget free: use it to precompute distance fields.
        if (inited) build_fields();
        return;
    }
    {
        PROF(refresh);
        refresh_self();
    }
    // A recall started before a capture landed: if the bag now finishes the race, walk to an altar instead.
    if (state.contains(bc::champion_state::recalling) && win_now() && hp >= max_hp * tune::kDangerHp) {
        try {
            me.cancel_recall();
        } catch (const std::exception& e) {
            LOG("cancel recall error %s\n", e.what());
        }
    } else if (state.contains(bc::champion_state::recalling) || state.contains(bc::champion_state::submitting_gems) ||
        state.contains(bc::champion_state::teleporting)) {
        // Channel guard (APEX_CHANNEL_GUARD, from apex_v22): a channel used to end our turn unseen, and a quarter
        // of the deaths came mid-tribute or mid-recall with the whole bag. Cancel when the enemies in reach kill
        // us before the channel ends, and play the turn instead.
        if (tune::kChannelGuard <= 0.0f || state.contains(bc::champion_state::teleporting)) return;
        w.update(round);
        std::int32_t left = 0;
        d::sdk_champion_channel_time(my_id, &left);
        float incoming = 0.0f;
        for (const auto& e : w.enemies)
            if (cheb(e.pos, pos) <= e.range + 2) incoming += e.strength + e.focus * 0.5f;
        if (incoming * static_cast<float>(left + 1) * tune::kChannelGuard < hp) return;
        try {
            if (state.contains(bc::champion_state::submitting_gems)) me.cancel_submit_gems();
            else me.cancel_recall();
        } catch (const std::exception& e) {
            LOG("channel cancel error %s\n", e.what());
            return;
        }
    }

    {
        PROF(world);
        w.update(round);
        if (tune::kBelief) w.update_beliefs(round, tune::kDet ? static_cast<std::uint64_t>(tune::kDetSafety) : 40000);
#ifdef APEX_BELIEFLOG
        if (tune::kBelief && slot == 0 && round % 20 == 0) {
            std::printf("BEL R%d p(%d,%d) seen %zu:", round, pos.x, pos.y, w.enemies.size());
            for (const auto& b : w.beliefs) std::printf(" %u/%d", b.area, b.alive ? 1 : 0);
            std::printf(" | E(here,6)=%.2f E(here,12)=%.2f\n", w.expected_enemies(pos, 6), w.expected_enemies(pos, 12));
        }
#endif
    }
    last_enemy_dist = 99;
    for (const auto& e : w.enemies) last_enemy_dist = std::min(last_enemy_dist, cheb(e.pos, pos));
    if (NOVA_ALTAR_INFER && prev_enemy_score >= 0 && enemy_score > prev_enemy_score) {
        // Someone just banked. If no visible enemy stands by a visible altar, the banker is at an unseen one.
        bool seen_banker = false;
        for (vec2 a : w.altars)
            for (const auto& e : w.enemies) seen_banker |= cheb(e.pos, a) <= bc::constants::submit_gems_range + 1;
        if (!seen_banker)
            for (vec2 a : w.altars)
                if (!bit(w.visible, a)) w.add_sighting(a, round - 1);
    }
    prev_enemy_score = enemy_score;
    if (tune::kOppModel) update_opp_model();
    if (tune::kNumbers != 0.0f || tune::kNumbersInvade < 99) {
        update_numbers();
        if (APEX_BRAIN && tune::kNumbers != 0.0f) // adapt() resets the aggression every turn
            tune::kAggression *= std::clamp(1.0f + tune::kNumbers * static_cast<float>(alive_us - alive_them), 0.6f, 1.6f);
    }
#ifdef APEX_OPPLOG
    if (tune::kOppModel && round % 100 == 0)
        std::printf("OPP R%d slot%d obs %d/%d %d/%d %d/%d %d/%d %d/%d est %.2f\n", round, slot, opp_ret[0], opp_obs[0], opp_ret[1],
            opp_obs[1], opp_ret[2], opp_obs[2], opp_ret[3], opp_obs[3], opp_ret[4], opp_obs[4], opp_flee_estimate());
#endif
    PROF_CHECK("world");
    episode_tick(false);
#ifdef APEX_EARLYLOG
    if (round == 50 || round == 100 || round == 150) std::printf("EARLY R%d %s carry %d score %d gold %d maxhp %.0f\n", round, bc::to_string(type).c_str(), gems, ally_score, gold, max_hp);
#endif
#ifdef APEX_TSTRACE
    ts_dead_logged = false;
#endif
#ifdef APEX_CHECK_VISION
    if (round % 7 == 3) {
        bc::bitboard mine;
        w.vision_from(pos, mine);
        const auto a = (mine & ~w.visible).positions().size(), b = (w.visible & ~mine).positions().size();
        std::printf("VISCHECK R%d p(%d,%d) bush%d extra%zu missing%zu\n", round, pos.x, pos.y, w.bush_cluster(pos) >= 0, a, b);
        if (w.bush_cluster(pos) >= 0) {
            // H2: everything opaque (all bushes), then add own cluster.
            bc::bitboard own, h2, h3;
            w.cluster_board(pos, own);
            bc::bitboard opaque = bc::map::walls() | w.bushes;
            opaque.shadow_cast_into(pos, 8, h2);
            h2 &= ~(w.bushes & ~own);
            h2 |= own;
            // H3: own cluster transparent but its tiles hidden beyond radius; 4-connected cluster.
            bc::bitboard own4;
            own4.clear();
            {
                std::vector<vec2> st{pos};
                set_bit(own4, pos);
                const vec2 d4[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                while (!st.empty()) {
                    vec2 c = st.back();
                    st.pop_back();
                    for (vec2 d : d4) {
                        vec2 q = c + d;
                        if (q.x < 0 || q.y < 0 || q.x >= w.width || q.y >= w.height || !bit(w.bushes, q) || bit(own4, q)) continue;
                        set_bit(own4, q);
                        st.push_back(q);
                    }
                }
            }
            bc::bitboard op3 = (bc::map::walls() | w.bushes) & ~own4;
            op3.shadow_cast_into(pos, 8, h3);
            h3 &= ~(w.bushes & ~own4);
            h3 |= own4;
            std::printf("VISBUSH R%d engine%zu mine%zu own8_%zu own4_%zu | H2 extra%zu missing%zu | H3 extra%zu missing%zu\n", round,
                w.visible.positions().size(), mine.positions().size(), own.positions().size(), own4.positions().size(),
                (h2 & ~w.visible).positions().size(), (w.visible & ~h2).positions().size(),
                (h3 & ~w.visible).positions().size(), (w.visible & ~h3).positions().size());
        }
    }
#endif
    if (!NOVA_NO_SIGHT) {
        read_sighting();
        share_sighting();
    }
#if APEX_EVENTS
    // No enemy in view: publish a newly learned respawn timer, unless a fresh sighting would be lost.
    if (w.enemies.empty() && !w.pending_events.empty()) {
        std::uint64_t v = read_shared();
        bool fresh_sighting = false;
        if (!((v >> 63) & 1) && (v >> kSightX) != 0) {
            int age = (round & kSightRoundMask) - static_cast<int>((v >> kSightRound) & kSightRoundMask);
            if (age < 0) age += kSightRoundMask + 1;
            fresh_sighting = age <= (APEX_EVENTS >= 2 ? 11 : 4); // v2: never overwrite a sighting teammates still use (<= 10)
        }
        if (!fresh_sighting) {
            const int gi = w.pending_events.back();
            w.pending_events.pop_back();
            if (gi < 127) {
                v &= (std::uint64_t{1} << kSightX) - 1;
                v |= static_cast<std::uint64_t>(gi) << kSightX;
                v |= static_cast<std::uint64_t>(w.groups[static_cast<std::size_t>(gi)].respawn_round & 255) << kSightY;
                v |= std::uint64_t{1} << 63;
                bc::shared_memory.write(0, std::span<std::uint64_t, 1>{&v, 1});
                shared = v;
            }
        }
    }
    if (w.pending_events.size() > 8) w.pending_events.erase(w.pending_events.begin());
#endif
    if (bit(w.ally_spawn, pos) || type == bc::champion_type::tavi) {
        shop();
        refresh_emblems();
    }


    support_casts();
    mode = "idle";
    if (ward_reveal()) w.update(round); // the ward shows the bush now: look again
    if (counter_ambush()) {
        // We moved (into the attacker's bush, or away from it): look again before deciding.
        w.update(round);
    }
    const bool fought = handle_enemies();
    PROF_CHECK("enemies");
    if (fought) {
        if (std::string_view(mode) == "idle") mode = "enemy";
        if (tune::kDistress && std::string_view(mode).starts_with("flee")) share_sighting(true);
        opportunistic_attack();
    } else if (win_now() && hp >= max_hp * tune::kDangerHp && tribute_if_needed()) {
        // Our bag finishes the race: bank it before anything else (shopping can wait).
        mode = "tribute-win";
    } else if (go_home_if_needed()) {
        if (std::string_view(mode) == "idle") mode = "home";
    } else if (NOVA_RUSH_UNTIL > 0 && rush()) {
    } else if (hunt()) {
    } else if (tribute_if_needed()) {
        mode = "tribute";
    } else {
        if (!farm()) mode = "nofarm";
        opportunistic_attack();
        support_casts();
    }
    PROF_CHECK("decide");
    drop_ward();
    if (tune::kTSearch && w.enemies.empty()) ts_prewarm(); // champion kits for the search, on calm turns
    prelock_altars();
    update_priority();
    PROF_CHECK("tail");
    build_fields();
    PROF_CHECK("fields");
#ifdef APEX_MODESTATS
    // Debug: how many turns each champion spends in each behaviour (printed every 200 rounds).
    {
        static const char* names[24];
        static int counts[24];
        int k = 0;
        while (k < 24 && names[k] && std::string_view(names[k]) != mode) ++k;
        if (k < 24) {
            names[k] = mode;
            ++counts[k];
        }
        // Failure signals: walking modes that did not move (stuck), targets dropped while still
        // alive (abandons), and the longest streak of unproductive turns.
        static vec2 last_pos{-1, -1};
        static int stuck = 0, abandons = 0, idle_run = 0, idle_max = 0, last_target = -9;
        const std::string_view m(mode);
        if ((m == "approach" || m == "to-capture" || m == "home") && pos == last_pos) ++stuck;
        const bool idle = m == "stage" || m == "wait-respawn" || m == "nofarm" || m == "idle";
        idle_run = idle ? idle_run + 1 : 0;
        idle_max = std::max(idle_max, idle_run);
        if (last_target >= 0 && last_target < static_cast<int>(w.groups.size()) && target_group != last_target &&
            target_group != kClaimDead && w.groups[last_target].state == GroupState::alive)
            ++abandons;
        last_target = target_group;
        last_pos = pos;
        if (round % 50 == 0) {
            std::printf("MODES R%d %s slot%d:", round, bc::to_string(type).c_str(), slot);
            for (int i = 0; i < 24 && names[i]; ++i) std::printf(" %s=%d", names[i], counts[i]);
            std::printf("\n");
            std::printf("DIAG R%d %s slot%d stuck=%d abandons=%d idlemax=%d\n", round, bc::to_string(type).c_str(), slot,
                stuck, abandons, idle_max);
        }
    }
#endif
    if (APEX_PROFILE && slot == 0 && round % 250 == 0) prof::report(round);
    if constexpr (farm_score::kDiagnostics)
        if (round % 50 == 0) farm_stats.report(round, slot);
#ifdef APEX_SPELLSTAT
    // Debug: spells spent on camps vs champions, and whether a champion-capable spell is ready when an enemy
    // first comes within 5 tiles after at least 6 quiet rounds (a new contact).
    {
        int near = 99;
        for (const auto& e : w.enemies) near = std::min(near, cheb(e.pos, pos));
        if (near <= 5) {
            if (round - spellstat_last_contact > 6) {
                ++spellstat_contacts;
                bool ready = false;
                for (int s2 = 0; s2 < 2; ++s2)
                    ready |= spells[s2].present && spells[s2].enemies && cooldown[s2] == 0 &&
                        (spells[s2].kind == bc::ability_kind::damage || spells[s2].kind == bc::ability_kind::cc ||
                            spells[s2].kind == bc::ability_kind::aoe_damage);
                bool has = false;
                for (int s2 = 0; s2 < 2; ++s2)
                    has |= spells[s2].present && spells[s2].enemies && (spells[s2].kind == bc::ability_kind::damage ||
                        spells[s2].kind == bc::ability_kind::cc || spells[s2].kind == bc::ability_kind::aoe_damage);
                spellstat_has += has;
                spellstat_ready += ready;
            }
            spellstat_last_contact = round;
        }
        if (round % 100 == 0)
            std::printf("SPELLSTAT R%d %s camp=%d enemy=%d contacts=%d with_spell=%d ready=%d\n", round,
                bc::to_string(type).c_str(), spellstat_camp, spellstat_enemy, spellstat_contacts, spellstat_has, spellstat_ready);
    }
#endif
    if (APEX_PROFILE && round == 400)
        std::printf("FARMFUEL slot%d calls %u low %u route_skip %u gaveup %u td_then_farm %u\n", slot, prof::farm_calls, prof::farm_low,
            prof::farm_route_skip, prof::farm_gaveup, prof::td_then_farm);
    if (APEX_TRACE && slot == 0 && round % 25 == 0)
        std::printf("PHASE R%d us %d them %d kills %d\n", round, ally_score, enemy_score, team_kills_seen);
    if (APEX_DEATHLOG) {
        auto& n = notes[note_i];
        note_i = (note_i + 1) & 3;
        n.round = round;
        n.hp = static_cast<int>(hp);
        n.max_hp = static_cast<int>(max_hp);
        n.gems = gems;
        n.mode = mode;
        n.target = target_group;
        n.pos = pos;
        n.enemies = static_cast<int>(w.enemies.size());
        n.near_enemy = last_enemy_dist;
        n.enemy_hp = 0;
        for (const auto& e : w.enemies) n.enemy_hp += static_cast<int>(e.hp);
        n.spawn_dist = fdist(w.spawn_field, pos);
        n.allies_near = 0;
        for (const auto& a : w.allies)
            if (a.id != my_id && cheb(a.pos, pos) <= 4) ++n.allies_near;
    }
    debug_block({ std::printf("M %d %u %s %d\n", round, my_id, mode, target_group); });
    LOG("R%d %-11s p(%d,%d) hp%.0f/%.0f g%d $%d %s G%d st%d fuel%llu\n", round, bc::to_string(type).c_str(), pos.x, pos.y, hp,
        max_hp, gems, gold, mode, target_group, target_group >= 0 ? static_cast<int>(w.groups[target_group].state) : -1,
        static_cast<unsigned long long>(bc::fuel_remaining()));
}
