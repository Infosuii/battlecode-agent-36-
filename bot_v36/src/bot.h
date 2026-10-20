// Champion decision logic. Each champion runs its own Bot; state persists between turns.
#pragma once
#include "common.h"
#include "world.h"
#include <string>

namespace ts {
struct State;
struct Map;
} // namespace ts

struct Spell {
    bool present = false;
    bc::ability_kind kind{};
    float range = 0, modifier = 0, radius = 0;
    bool excludes_walk = false;
    bool camps = false, enemies = false, allies = false, ground = false;
};

inline constexpr int kClaimDead = -2;
// Shared memory (8 bytes): 6 claim slots x 7 bits (bits 0-41; a team with Cassi & Issac has 6
// bodies), the latest enemy sighting x (42-48), y (49-55), round mod 64 (56-61), distress (62).
inline constexpr int kClaimBits = 7;
inline constexpr std::uint64_t kClaimMask = 127;
inline constexpr std::uint64_t kClaimCodeDead = 126, kClaimCodeIdle = 127;
inline constexpr int kClaimSlots = 6;
inline constexpr int kSightX = 42, kSightY = 49, kSightRound = 56, kDistressBit = 62;
inline constexpr int kSightRoundMask = 63;

class Bot {
public:
    void play_turn();

private:
    void init();
    void refresh_self();
    void shop();

    // Shared memory: per-slot camp-group claims plus the last enemy sighting.
    std::uint64_t read_shared() const;
    void write_claim(int group);
    int claim_count(int group) const;
    std::pair<int, int> claims_split(int group) const;
    bool open_call_by_lower();
    void share_sighting(bool distress = false);
    void read_sighting();
    int claim_of(int slot) const;
    // Claim ages (APEX_INCUMBENT): the round each slot's current claim was first seen, ours included.
    std::array<int, kClaimSlots> seen_claim{}, claim_since{};
    int my_claim_since = 0;
    void track_claims();
    int senior_claims(int group) const;

    // Movement.
    bool walk_toward(const bc::bitboard& goals);
    bool travel_around_threats(const bc::distance_field& field, const bc::bitboard& blocked, const bc::bitboard& goals);
    bool follow_field(const bc::distance_field& field, const bc::bitboard& blocked, bool allow_dash);
    bool try_dash(const bc::distance_field& field, const bc::bitboard& blocked);
    bool combat_move = false; // inside handle_enemies / hunt: dashes are for the fight
    bool travel_dash_ok() const;
    bool retreat_from(vec2 threat);
    bool regroup_from(vec2 threat); // NOVA_FIGHT2 bit 4
    int buddy();                    // NOVA_BUDDY: partner slot, -1 none
    vec2 buddy_anchor();
    int buddy_slot = -2;
    int prev_enemy_score = -1; // NOVA_ALTAR_INFER
    int duo_map_ok = -1;       // NOVA_DUO_MAPS: cached per game (-1 = not decided yet)
    int official_map = -1;     // 1 = official pool map (nova_map_id >= 0), 0 = unseen, -1 = not decided yet
    bool is_official_map();
    bool ward_ahead();
    std::vector<char> big_group; // NOVA_BIGJOB: groups holding a dominant share of the map's gems
    int duo_ignore_slot = -1; // NOVA_DUO: the leader's follower, whose claims the leader ignores
    bool dash_away();
    bool try_burst_kill();
    bool bush_ambush();
    bool counter_ambush();
    bool ward_reveal(); // true when a ward went into a bush (look again)
    bool hidden_here(); // in a bush cluster that no visible enemy stands in
    void update_priority();
    int priority_set = 0;
    bool position_and_attack(const EnemyRec& target);
    void prelock_altars();
    bool hidden_from_threats(vec2 q);
    std::vector<bc::bitboard> threat_vision; // what each nearby enemy sees (built on demand)
    int threat_vision_round = -1;
    bool escape_or_stand(const EnemyRec* target, const EnemyRec* nearest, float ttd);
    bool melee_caught();
    bool kite_step(const EnemyRec& target);
    int nearest_enemy_melee() const;
    int enemy_spell_reach() const;
    bool caster_micro(const EnemyRec* target, bool engage); // APEX_CMICRO (tsearch.cpp)
    int team_casters = -1; // Stellar, Tavi, Prism and Willow on our team (-1 until read)

    // Escape search (APEX_ESCAPE_SEARCH): this turn's flight plan, made once and then carried out.
    struct EscapePlan {
        bool valid = false;
        int dash_slot = -1; // dash first (to `land`), or -1
        vec2 land{}, step{}; // `step` == where we stand afterwards (pos when staying)
    };
    // Boards and distance-field buffers, allocated on the first flight (each costs fuel to create).
    struct EscapeBoards {
        bc::bitboard chase_blocked; // what stops enemy walking: walls, holes, altars, our spawn
        bc::bitboard dash_stop;     // what a dash path cannot cross: walls, altars, the enemy spawn
        bc::bitboard root, visited; // root stays empty between searches (bits set and cleared directly)
        std::array<bc::distance_field, 3> field;
    };
    std::unique_ptr<EscapeBoards> esc_boards;
    EscapePlan esc_plan;
    int esc_round = -1;
    vec2 esc_failed{-1, -1}; // a planned tile the engine refused this turn (hidden unit)
    bool plan_escape();
    int run_escape(bool dash_only); // 0 = no plan (use the old flee), 1 = moved or dashed, 2 = plan stays

    // Combat helpers.
    bool attack_unit(const bc::unit& target, vec2 where);
    bool cast_on_camp(int camp_index);
    bool cast_on_enemy(const EnemyRec& e);
    bool use_reset_and_attack(const bc::unit& target, vec2 where);
    void support_casts();
    float my_dps(const GroupRec* g) const;

    // Tactical search (tsearch.cpp, APEX_TSEARCH): plays the local fight forward and acts on the best line.
    bool tactical_search();
    void ts_prewarm();
    int tactical_decide();
    int team_decide(std::uint32_t& focus_handle);            // APEX_TFC: 1 engage, 2 retreat, 0 none
    void write_order(int code);                              // a fight order in our claim slot
    int tfc_order_round = -1;                                  // +1 fight, -1 run, 0 no verdict (APEX_TS_DECIDE)
    bool ts_snapshot(ts::State& s, ts::Map& m, const EnemyRec** eref);

public:
    // Fight episodes (episode.cpp, APEX_EPISODES): learning data and the learned aggression multiplier.
    static constexpr int kEpFeat = 33, kEpGrid = 6;

private:
    struct Episode {
        bool active = false;
        int start = 0, last_contact = 0, choice = 2, enemy_deaths0 = 0, last_gems = 0, score_us = 0, score_them = 0;
        float hp0 = 0;
        float f[kEpFeat]{};
    };
    Episode ep;
    float ep_aggr = 1.0f, ep_danger = 1.0f; // this episode's multipliers for the fight formula
    std::vector<std::uint32_t> enemy_handles;
    // Opponent model (APEX_OPP_MODEL): does an enemy within 3 tiles back off, by its health fifth? Learned from
    // consecutive sightings; sets the fight search's enemy flee thresholds once enough is seen.
    struct OppSeen {
        std::uint32_t id;
        vec2 pos;
        float frac;
        int dist, round;
    };
    std::vector<OppSeen> opp_last;
    int opp_obs[5] = {0, 0, 0, 0, 0}, opp_ret[5] = {0, 0, 0, 0, 0};
    void update_opp_model();
    float opp_flee_estimate() const; // < 0 until enough observations
    // Numbers (APEX_NUMBERS): living champions per side (the engine answers is_alive for every champion).
    std::vector<std::uint32_t> num_handles[2];
    int alive_us = 0, alive_them = 0;
    void update_numbers();
#ifdef APEX_SPELLSTAT
    int spellstat_camp = 0, spellstat_enemy = 0, spellstat_contacts = 0, spellstat_has = 0, spellstat_ready = 0, spellstat_last_contact = -100;
#endif
    int ts_ran_round = -1;   // round the fight search last played out (APEX_DET: farming keeps its target then)
    int field_calls = 0;     // build_fields calls so far (APEX_DET: the first one has the doubled budget)
    std::vector<char> enemy_alive;
    int enemy_deaths = 0;
    void count_enemy_deaths();
    void ep_features(float* f) const;
    void episode_tick(bool dead_now);
    vec2 ts_home[2] = {{-1, -1}, {-1, -1}}; // spawn centres (ours, theirs)
    std::string ts_trace[2];                  // APEX_TSTRACE: the last two searches, printed on death
    int ts_trace_i = 0;
    int ts_trace_round = -1000;
    bool ts_dead_logged = false;

    // Behaviours, highest priority first.
    bool handle_enemies();
    // Opening rush (NOVA_RUSH_UNTIL): the team moves as one pack to the enemy's nearest camps and fights there.
    bool rush();
    bool rush_active() const;
    std::vector<char> rush_visited;
    int rush_target = -1;
    int rush_leader_slot() const;
    bool try_lock_altar();
    bool deny_tribute();
    bool hunt();
    float siege_dps(const GroupRec& g) const;
    void drop_ward();
    bool go_home_if_needed();
    bool tribute_if_needed();
    bool farm();
    bool fight_camps(GroupRec& g, int need);
    bool reveal_bush_camp(const GroupRec& g); // NOVA_BUSH_WARD
    bool break_out();                         // NOVA_UNBOX
    bool walk_boxed = false;                  // last walk_toward: units (not terrain) blocked every route
    int champions_needed(const GroupRec& g, float hp_left);
    float soak_coefficient(int group, float hp_left);
    float group_soak(const GroupRec& g, float team_dps) const;
    bool camp_targets_me(int camp_index, vec2 at) const;
    float incoming_from_group(const GroupRec& g, vec2 at) const;
    int allies_near_group(const GroupRec& g) const;
    bool group_capture_covered(const GroupRec& g) const;
    // Gold sharing (APEX_SHARE_GOLD): shards bought, estimated from stats; a poorer ally that is on
    // (or claims and stands beside) a finished group takes its reward.
    float my_items() const;
    bool poorer_ally_takes(const GroupRec& g, int gi, bool on_tile_only) const;
    std::array<std::uint32_t, 8> ally_handle{}; // slot -> champion handle
    std::array<bc::champion_type, 8> slot_type{}; // slot -> champion type
    bool in_role_pack = false;                    // APEX_ROLE_SQUAD: this champion farms with the pack
    int score_groups(int participants_hint, int& best_need, float& best_score);
    struct RouteCand {
        int gi = -1, need = 1;
        float value = 0, cost = 1, score = 0;
        int finish = 0;
    };
    int route_search(const RouteCand* top, int n_top);
    int plan_search(const RouteCand* top, int n_top, bool& bank_first); // APEX_PLAN
    int plan_bank_round = -1;
    void refresh_follow_ups();
    // Per-turn follow-up table: gem value for one champion alone (0 = not worth or not safe solo),
    // fixed turns (kill + capture), and the respawn round when the group is down (else -1).
    std::vector<int> group_order; // groups by last turn's distance (nearly sorted each turn)
    std::vector<char> order_seen;
    std::vector<float> follow_value, follow_fixed;
    std::vector<int> follow_respawn;
    int follow_round = -1;
    void opportunistic_attack();

    bool spell_ready(int slot) const;

    World w;
    bool inited = false;
    int round = 0;
    int slot = 0;
    std::uint32_t my_id = 0;
    int team_size = 1;
    std::uint64_t shared = 0;

    // Self snapshot, refreshed each turn.
    vec2 pos;
    float hp = 0, max_hp = 1, strength = 0, focus = 0;
    float prev_hp = 0; // health at the start of our previous turn (APEX_AWAKE_ZONE)
    int gems = 0, gold = 0, range = 1;
    bool melee = true;
    bc::champion_type type{};
    std::array<Spell, 2> spells;
    std::array<int, 2> cooldown{};
    bool cast_done = false;
    std::array<int, 8> altar_watch{}; // APEX_STALE_BAG: round until which each altar (w.altars order) is camped
    bool keep_channel = false; // this turn: no stuns on a tributer we are hitting (a stun would free it)
    bool walked = false;

    void build_fields();
    std::vector<float> soak_cache, soak_key_hp;
    std::vector<int> soak_key_state;
    int target_group = -1;
    int staging_since = -1;          // round we started waiting for allies at target_group
    std::vector<int> avoid_until;    // per group: skip it until this round
    bool heading_home = false;
    bool force_tribute = false;
    const char* mode = "";
    bool was_dead = false;
    Sighting distress{{-1, -1}, -1000}; // latest teammate call for help (an enemy chasing them)
    // Last few turns, dumped when we die (APEX_DEATHLOG=1).
    struct TurnNote {
        int round = 0, hp = 0, max_hp = 0, gems = 0, enemies = 0, near_enemy = 99, enemy_hp = 0, allies_near = 0, target = -1, spawn_dist = 0;
        const char* mode = "";
        vec2 pos{};
    };
    TurnNote notes[4];
    int note_i = 0;
    int last_enemy_dist = 99;
    int last_flee_round = -1000; // hunting resumes only a while after we last fled
    bool no_initiate = false;    // solo members in pair mode: no help calls this turn
    int call_ban_until = -1;     // after a help call times out, no new calls until this round
    float camp_taken_mult = 1.0f; // camp damage we take: jungle emblem 0.7, durability 0.9
    float camp_dealt_mult = 1.0f; // camp damage we deal: monster emblem 1.1
    void refresh_emblems();
    // Our bag alone finishes the race (switch APEX_WIN_FIRST): tribute outranks shopping and recalls.
    // Gold buys items, which matter early; gems win. With APEX_GOLD_TAPER the weight falls linearly
    // from APEX_GOLD_WEIGHT at score 0 to a fifth of it at that score.
    float gold_weight() const {
        if (tune::kGoldTaper <= 0) return tune::kGoldWeight;
        const float t = std::min(1.0f, static_cast<float>(ally_score) / static_cast<float>(tune::kGoldTaper));
        return tune::kGoldWeight * (1.0f - 0.8f * t);
    }
    bool win_now() const { return tune::kWinFirst && gems > 0 && ally_score + gems >= static_cast<int>(bc::constants::winning_gems); }
    vec2 ward_pos{-100, -100};
    int ally_score = 0, enemy_score = 0;
    int team_kills_seen = 0;
};
