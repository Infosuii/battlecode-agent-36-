// Static map knowledge cached once per bot, plus remembered camp-group state.
#pragma once
#include "common.h"

#include <memory>
#include <optional>

enum class GroupState : std::uint8_t { alive, reward, respawning };

struct CampRec {
    bc::camp camp;
    vec2 pos;
    bc::camp_type type;
    float max_hp;
    float strength;
    float hp;          // last observed health; max_hp when never seen damaged
    bool dead = false; // last observed dead inside an alive group
    int range = 3;     // attack range, read from the engine's camp design
#if V37_CAMP_STATE
    int awake_state = -1;
    int awake_round = -1;
#endif
};

struct GroupRec {
    std::uint32_t handle = 0;
    std::uint32_t gems = 0, gold = 0;
    int respawn = 0;
    std::vector<int> camps; // indices into World::camps
    bc::bitboard tiles;     // original spawn tiles
    float max_hp = 0, strength = 0;
    bool harmless = true;   // no member deals damage

    GroupState state = GroupState::alive;
    int respawn_round = 0;  // round the group is expected back when respawning
    int seen_round = -1;    // last round any spawn tile was visible
    int available_since = 0; // round the group last became available (alive) by our knowledge
    int reward_seen = -1;   // round we last saw the reward waiting
    int enemy_round = -1000; // last round an enemy champion was seen near the group

    float hp_left(const std::vector<CampRec>& all) const;
};

struct EnemyRec {
    bc::champion unit;
    vec2 pos;
    float hp, max_hp, strength, focus;
    int range;
    bc::champion_type type;
    int gems;
    std::uint32_t id;
    std::uint32_t state; // champion_state bits (recalling = 1<<1, submitting gems = 1<<2)
    bool tributing() const { return (state & (1u << 2)) != 0; }
    bool recalling() const { return (state & (1u << 1)) != 0; }
};

struct Sighting {
    vec2 pos;
    int round;
};

struct AllyRec {
    bc::champion unit;
    vec2 pos;
    float hp, max_hp, strength;
    int range;
    std::uint32_t id;
    float focus;
    bc::champion_type type;
};

class World {
public:
#if V37_CAMP_STATE
    int observation_round=-1;
#endif
    void init();
    void update(int round);

    int width = 0, height = 0;
    bc::bitboard static_blocked; // walls, holes, altars and the enemy spawn
    bc::bitboard ally_spawn, enemy_spawn;
    std::vector<vec2> spawn_tiles; // our spawn tiles (cached)
    bc::bitboard enemy_spawn_zone; // the enemy spawn plus 2 tiles: enemies there heal and cannot be chased
    std::vector<vec2> altars;
    bc::bitboard altar_ring;     // walkable tiles within tribute range of an altar
    // Bushes: from outside a bush cluster nobody can see in, so attacks cannot target us there.
    bc::bitboard bushes;
    bc::bitboard bush_labeled;                  // bush tiles whose cluster has been labelled
    std::unique_ptr<std::uint16_t[]> bush_label; // cluster id per tile (valid only where labelled)
    int bush_clusters = 0;
    int bush_cluster(vec2 p);                    // -1 when p is not a bush
    void cluster_board(vec2 p, bc::bitboard& out); // the whole bush cluster containing p
    // What a unit standing at `viewer` sees (engine shadowcasting, radius 8): walls and bushes
    // block sight, except the viewer's own bush cluster; tiles inside other clusters stay hidden.
    void vision_from(vec2 viewer, bc::bitboard& out);
    // True when a unit at `viewer` cannot see tile q because q is in a bush cluster viewer is not in.
    bool bush_hides(vec2 q, vec2 viewer) {
        const int cq = bush_cluster(q);
        return cq >= 0 && bush_cluster(viewer) != cq;
    }
    bc::distance_field altar_field; // built lazily: ensure_altar_field()
    bool altar_ready = false;
    bool ensure_altar_field();
    bc::distance_field spawn_field;
    // Walking distance from the enemy spawn (terrain only), built lazily from spare fuel. A group
    // nearer their spawn than ours is enemy territory: contested, and where their roamers are.
    bc::distance_field enemy_field;
    bool enemy_ready = false;
    bool ensure_enemy_field();
    std::vector<std::int16_t> group_margin; // per group: enemy-spawn distance minus ours (needs enemy_field)

    std::vector<CampRec> camps;
    std::vector<GroupRec> groups;

    // Per-turn observations.
    bc::bitboard visible;
    bc::bitboard unit_tiles;     // visible champions, camps and teleport reservations
    std::vector<EnemyRec> enemies;
    std::vector<AllyRec> allies; // visible allies, excluding self
    bc::bitboard hot_zone;       // within reach of damaged (likely awake) camps that still hit back
    // Bush clusters holding a visible enemy ward: the ward shows them everything inside, so they are
    // no cover (APEX_WARD_AWARE).
    std::vector<int> warded_clusters;
    bool warded(int cluster) const {
        return cluster >= 0 && std::find(warded_clusters.begin(), warded_clusters.end(), cluster) != warded_clusters.end();
    }

    int group_of_camp_tile(vec2 p) const;

    // Static lookups built once: which camp sits on each tile, and all camp spawn tiles.
    std::vector<std::int16_t> camp_group; // group index per camp
    bc::bitboard camp_tiles_all;
    std::vector<std::uint8_t> group_visible, camp_visible; // refreshed every turn

    // Static distance (terrain only) from every tile to each group's camps, row-major.
    // Built a few per turn from spare fuel; lookups then replace a per-turn full-map field
    // (which cost 20-40k fuel on 100x100 arenas).
    // Static walking distance to each camp group, built a few per turn from spare fuel (nearest
    // to our spawn first) and then read for free instead of recomputing a field from us each turn.
    std::vector<bc::distance_field> group_dist;
    std::vector<int> field_slot;  // group -> index into group_dist, -1 until built
    std::vector<const std::uint16_t*> field_ptr; // group -> its distance buffer (nullptr until built)
    std::vector<int> field_order; // groups by distance from our spawn
    std::size_t fields_built = 0;
    bool fields_ready() const { return fields_built >= groups.size(); }
    bool has_field(int gi) const { return field_slot.size() > static_cast<std::size_t>(gi) && field_slot[gi] >= 0; }
    void prepare_field_order();
    bool build_next_group_field(std::uint64_t budget); // false when the budget cannot fit a useful field
    int group_distance(int gi, vec2 p) const { return fdist(group_dist[static_cast<std::size_t>(field_slot[gi])], p); }
    // Walking distance from group `from` to the nearest camp of group `to` (static; kUnreach until
    // `from` has its field). One row per source group, filled on first use.
    std::vector<std::vector<std::uint16_t>> pair_rows;
    int group_to_group(int from, int to);
    // Camps that are awake (damaged, alive) and would reach tile q (small list; for exposure).
    std::vector<int> awake_camps_near(vec2 q) const;
    // The whole row for `from` (nullptr when its field is not built yet, or when building it is
    // not allowed this turn).
    const std::uint16_t* pair_row(int from, bool may_build);

    // Enemy beliefs (APEX_BELIEF): for every enemy champion, the tiles it could stand on now. Seen: its tile.
    // Unseen: grows one step a round (8 directions, terrain only) minus every tile we see this turn. Dead (the
    // engine answers is_alive for every enemy): empty. Respawned: its spawn. Lost track (6+ rounds unadvanced):
    // anywhere we cannot see.
    struct Belief {
        std::uint32_t handle = 0;
        bc::bitboard reach;
        std::uint32_t area = 0;
        int round = -1; // round the belief was last advanced
        bool alive = true;
    };
    std::vector<Belief> beliefs;
    bc::bitboard enemy_walk; // tiles an enemy champion can stand on (not walls, holes, altars or our spawn)
    void update_beliefs(int round, std::uint64_t min_fuel);
    // Expected number of enemy champions within Chebyshev `radius` of p: each enemy's share of its belief there.
    float expected_enemies(vec2 p, int radius);

    // Recent enemy sightings (ours and teammates' via shared memory).
    std::vector<Sighting> sightings;
    void add_sighting(vec2 p, int round);
    bc::bitboard threat_zone(int round, int max_age, float radius) const;

private:
    void observe_groups(int round);
public:
    // APEX_EVENTS: groups whose respawn round we just learned (to publish), and teammates' events.
    std::vector<int> pending_events;
    void apply_event(int gi, int ready_round, int round);
private:
    void observe_champions();
};
