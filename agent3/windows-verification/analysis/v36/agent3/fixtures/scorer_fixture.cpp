// Unit fixture: compiles the actual production score_groups body. SDK-dependent
// observation, path and helper inputs are explicit deterministic stubs. This is
// not an engine simulator or evidence of full-match strength/fuel use.
#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <tuple>
#include <vector>
#include "farm_score.h"
#include "learner_fixture.h"

#define PROF(x) ((void)0)
#define LOG(...) ((void)0)
#define APEX_PROFILE 0
#define APEX_BOOT_ITEMS 15
inline float bigjob_ratio = .6f, bigjob_mult = 6.f, seed_eps = .04f, buddy_penalty = 0;
inline int seed_value = 0, bigcamp = 0, duo_bits = 0;
#define NOVA_BIGJOB bigjob_ratio
#define NOVA_BIGJOB_FROM 60
#define NOVA_BIGJOB_MULT bigjob_mult
#define NOVA_BIGCAMP bigcamp
#define NOVA_DUO duo_bits
#define NOVA_BUDDY buddy_penalty
#define NOVA_BUDDY_DIST 6
#define NOVA_SEED seed_value
#define NOVA_SEED_EPS seed_eps
#define NOVA_CAMP_SAFE 1.0f // explicitly exercise the existing experimental block

namespace bc { inline std::uint64_t fuel = 1000000; std::uint64_t fuel_remaining() { return fuel; } }
struct vec2 { int x=0, y=0; };
int cheb(vec2 a, vec2 b) { return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y)); }
int fdist(int, vec2 p) { return std::abs(p.x); }
constexpr int kUnreach = 65535;
enum class GroupState { alive, reward, respawning };
struct CampRec { vec2 pos; float hp=100, max_hp=100, strength=10; int range=3; bool dead=false; };
struct GroupRec {
    std::uint32_t gems=0, gold=0;
    std::vector<int> camps;
    bool harmless=false;
    GroupState state=GroupState::alive;
    int respawn_round=0, enemy_round=-1000;
    float health=100;
    int need=1, lower=0, higher=0;
    bool covered=false, poorer=false;
    float hp_left(const std::vector<CampRec>&) const { return state==GroupState::reward ? 0 : health; }
};
struct Ally { vec2 pos; int id=1; float hp=100,max_hp=100; };
struct World {
    std::vector<GroupRec> groups;
    std::vector<CampRec> camps;
    std::vector<const std::uint16_t*> field_ptr;
    std::vector<std::int16_t> group_margin;
    std::vector<Ally> allies, enemies;
    bool altar_ready=false, enemy_ready=false;
    int altar_field=0;
    float belief=0;
    std::vector<std::vector<std::uint16_t>> pair_rows;
    const std::uint16_t* pair_row(int from, bool) {
        return from < static_cast<int>(pair_rows.size()) ? pair_rows[from].data() : nullptr;
    }
    float expected_enemies(vec2,int) { return belief; }
};
namespace tune {
inline int kCaptureTime=5, kRouteDepth=2, kRouteK=4, kRouteM=4, kDetScoreN=16, kDetSafety=18000;
inline bool kDet=true, kShareGold=false, kSiege=false, kInitiateJobs=true, kIncumbent=false;
inline int kCapStaff=0, kNumbersInvade=2, kTerritoryUntil=300, kTerritoryMargin=0, kCohesionDist=8;
inline int kBelief=0, kPlan=0;
inline float kRouteSlack=.55f, kWaitWeight=3.2398f, kStickiness=1.523f, kBootBonus=.3329f;
inline float kAltarBonus=0, kNumbersBonus=0, kTerritory=1, kCohesion=0, kHpCost=0;
inline float kBeliefW=0, kBeliefAlly=1;
inline std::uint64_t kScoreFloor=25000, kRouteFuel=40000, kLowFuel=30000;
}
class Bot {
public:
    World w;
    vec2 pos;
    std::vector<int> group_order, avoid_until;
    std::vector<char> order_seen, big_group;
    int target_group=-1, round=60, range=1, slot=0, team_size=6, alive_us=6, alive_them=6;
    int duo_ignore_slot=-1, call_ban_until=0, plan_bank_round=-1, my_id=10;
    int field_calls=0, ts_ran_round=-1;
    bool no_initiate=false, in_role_pack=false;
    std::array<int,8> ally_handle{};
    float max_hp=1000, camp_taken_mult=1, items=0, gold_w=.18f, dps=20, siege_rate=0, soak=0;
    farm_score::Stats<farm_score::kDiagnostics> farm_stats;
    int score_groups(int, int&, float&);
    int score_groups_legacy(int, int&, float&);
    bool first_farm_prefix();
    float group_soak(const GroupRec&,float) const;
    bool camp_targets_me(int,vec2) const;
    float incoming_from_group(const GroupRec&,vec2) const;
    float near_worst(const GroupRec&,vec2);
    float my_dps(const GroupRec*) const { return dps; }
    std::size_t field_index(vec2) const { return 0; }
    float gold_weight() const { return gold_w; }
    bool group_capture_covered(const GroupRec& g) const { return g.covered; }
    bool poorer_ally_takes(const GroupRec& g,int,bool) const { return g.poorer; }
    std::pair<int,int> claims_split(int gi) const { return {w.groups[gi].lower,w.groups[gi].higher}; }
    int champions_needed(const GroupRec& g,float) const { return g.need; }
    float siege_dps(const GroupRec&) const { return siege_rate; }
    bool open_call_by_lower() const { return false; }
    float my_items() const { return items; }
    int senior_claims(int gi) const { return w.groups[gi].lower; }
    vec2 buddy_anchor() const { return {2,2}; }
    float soak_coefficient(int,float) const { return soak; }
    struct RouteCand { int gi=-1,need=1; float value=0,cost=1,score=0; int finish=0; };
    std::vector<float> follow_value, follow_fixed;
    std::vector<int> follow_respawn;
    void refresh_follow_ups() {
        if (follow_value.empty()) {
            follow_value.assign(w.groups.size(),0);
            follow_fixed.assign(w.groups.size(),0);
            follow_respawn.assign(w.groups.size(),-1);
        }
    }
    int route_search(const RouteCand*,int);
    int plan_search(const void*,int,bool& b) { b=false; return 0; }
};
#include "candidate_scorer.inc"
#include "baseline_scorer.inc"
#include "camp_helpers.inc"
#include "first_farm_prefix.inc"
#include "route_scorer.inc"
#include "near_safety.inc"

void add_group(Bot& b, unsigned gems, int dist, GroupState state=GroupState::reward, bool harmless=false) {
    GroupRec g;
    g.gems=gems; g.state=state; g.harmless=harmless;
    g.camps.push_back(static_cast<int>(b.w.camps.size()));
    b.w.camps.push_back({{dist,0}});
    b.w.groups.push_back(g); b.w.group_margin.push_back(0); b.avoid_until.push_back(0);
}
std::tuple<int,int,float> run(Bot& b, int participants=1, bool legacy=false) {
    int need; float score;
    int gi=legacy ? b.score_groups_legacy(participants,need,score) : b.score_groups(participants,need,score);
    assert(std::isfinite(score));
    return {gi,need,score};
}
inline std::uint64_t checksum=1469598103934665603ULL, cases=0, candidates=0, violations=0;
inline std::vector<std::uint32_t> choices;
void digest(std::tuple<int,int,float> x) {
    auto [gi,n,s]=x;
    choices.push_back(static_cast<std::uint32_t>(gi+2));
    choices.push_back(static_cast<std::uint32_t>(n));
    choices.push_back(std::bit_cast<std::uint32_t>(s));
    checksum=(checksum ^ static_cast<std::uint64_t>(gi+2))*1099511628211ULL;
    checksum=(checksum ^ static_cast<std::uint64_t>(n))*1099511628211ULL;
    // Fixed precision checksum avoids padding/aliasing issues.
    checksum=(checksum ^ static_cast<std::uint64_t>(s*100000))*1099511628211ULL;
    ++cases;
}
void check(Bot b,int participants=1) {
    Bot legacy=b;
    auto result=run(b,participants);
    if constexpr (!farm_score::kCorrectBound && !farm_score::kSingleGroupDiagnostic) {
        assert(result==run(legacy,participants,true));
        assert(b.avoid_until==legacy.avoid_until && b.group_order==legacy.group_order);
    }
    if constexpr(farm_score::kDiagnostics) {
        candidates+=b.farm_stats.get(farm_score::scored);
        violations+=b.farm_stats.get(farm_score::violations);
        assert(b.farm_stats.get(farm_score::violations)==0);
    }
    digest(result);
}
void focused() {
    tune::kRouteDepth=1;
    Bot b;
    add_group(b,40,0); add_group(b,100,30);
    Bot old=b, now=b;
    auto a=run(old,1,true), c=run(now);
    assert(std::get<0>(a)==0);
    if constexpr(farm_score::kCorrectBound) assert(std::get<0>(c)==1);
    std::printf("dominant_fixture legacy=%d candidate=%d\n",std::get<0>(a),std::get<0>(c));
    b.round=59; check(b); b.round=60; check(b); b.round=61; check(b);
    // 59/100 is below dominance; 60/100 is at the inclusive threshold.
    for(int gems: {59,60,61}) { Bot q; add_group(q,100-gems,0); add_group(q,gems,30); check(q); }
    bigjob_ratio=0;
    tune::kBootBonus=2.0f;
    Bot boot; add_group(boot,10,0); add_group(boot,12,10,GroupState::reward,true);
    auto p=run(boot);
    if constexpr(farm_score::kCorrectBound) assert(std::get<0>(p)==1);
    for(float items:{14.999f,15.f,15.001f}) { boot.items=items; check(boot); }
    // Actual runtime boot settings are not confined to the compile-time .3329.
    for(float bonus:{0.f,.3329f,1.f,2.5f}) { tune::kBootBonus=bonus; boot.items=0; check(boot); }
    tune::kBootBonus=.3329f; bigjob_ratio=.6f;
    // Separate work cap accounting: every candidate passes the bound but is
    // discounted by a prior small-job claimant after consuming a work slot.
    Bot capped; add_group(capped,10,0); for(int i=0;i<40;++i) { add_group(capped,11,0); capped.w.groups.back().lower=1; }
    run(capped);
    if constexpr(farm_score::kDiagnostics && !farm_score::kSingleGroupDiagnostic && !farm_score::kExhaustive) {
        assert(capped.farm_stats.get(farm_score::work_capped)>0);
        assert(capped.farm_stats.get(farm_score::math_pruned)==0);
    }
    // Dense order case exposes the separate 256-entry order cap in production.
    Bot dense; for(int i=0;i<301;++i) add_group(dense,10,i%40); check(dense);
    // First farm prefix: field_calls==0 + no target returns without scoring.
    Bot first; add_group(first,10,0); assert(!first.first_farm_prefix());
    first.target_group=0; assert(first.first_farm_prefix());
    first.avoid_until[0]=100; assert(!first.first_farm_prefix());
    // Invalid configurations must return a finite invalid bound, not prune by
    // an infinity/NaN sentinel. The production score keeps existing semantics.
    auto invalid=farm_score::upper_bound(1,0,0,5,1,1,1,1,2,1,1,true);
    assert(!invalid.admissible && std::isfinite(invalid.score));
    invalid=farm_score::upper_bound(1,0,0,5,1,1,1,1,0,1,1,false);
    assert(!invalid.admissible);
    invalid=farm_score::upper_bound(std::numeric_limits<float>::max(),0,0,5,2,1,1,1,0,1,1,true);
    assert(!invalid.admissible && std::isfinite(invalid.score));
}
void camp_checks() {
    Bot b; add_group(b,10,0,GroupState::alive); b.w.camps[0].pos={0,0};
    b.w.allies.push_back({{1,0},1});
    assert(b.incoming_from_group(b.w.groups[0],{2,0})==0);
    b.w.allies[0].pos={5,0}; // actual tank departure immediately changes aggro
    assert(b.incoming_from_group(b.w.groups[0],{2,0})==10);
    b.w.allies[0].pos={2,0}; b.w.allies[0].id=11;
    assert(b.incoming_from_group(b.w.groups[0],{2,0})==10);
    b.w.allies[0].id=9;
    assert(b.incoming_from_group(b.w.groups[0],{2,0})==0);
    // Actual design range is used, rather than the old comment's fixed 3.
    b.w.allies.clear(); b.w.camps[0].range=5;
    assert(b.incoming_from_group(b.w.groups[0],{5,0})==10);
    assert(b.incoming_from_group(b.w.groups[0],{6,0})==0);
    // Reproduce the 12-camp truncation without silently changing policy.
    Bot dense; GroupRec g;
    for(int i=0;i<13;++i) { dense.w.camps.push_back({{i,0},100,100,10,3,false}); g.camps.push_back(i); }
    float actual=dense.group_soak(g,10);
    assert(actual==7800); // all 13 would be 9100
    std::printf("camp_dense_soak actual=%.0f full=9100 omitted=1300\n",actual);
    Bot near;
    for(int i=0;i<33;++i) {
        add_group(near,10,0,GroupState::alive);
        auto& camp=near.w.camps.back();
        camp.pos={i%7-3,i/7-3}; camp.hp=50; camp.max_hp=100;
        camp.strength=i==32 ? 100 : 1;
    }
    assert(near.near_worst(near.w.groups[0],{0,0})==32);
    std::printf("camp_dense_safety actual=32 full=132 omitted=100\n");
    // A healthy camp member is included if a different member wakes the group.
    Bot group; add_group(group,10,0,GroupState::alive);
    group.w.camps[0].strength=5;
    auto healthy=group.w.camps[0]; healthy.strength=7;
    group.w.camps.push_back(healthy); group.w.groups[0].camps.push_back(1);
    assert(group.near_worst(group.w.groups[0],{0,0})==12);
}
void route_checks() {
    Bot b;
    for(int i=0;i<3;++i) add_group(b,10,0);
    b.follow_value={0,0,1000}; b.follow_fixed={0,0,1}; b.follow_respawn={-1,-1,-1};
    b.w.pair_rows={{0,10,20},{10,0,1},{20,1,0}};
    Bot::RouteCand firsts[2]={{0,1,100,10,10,10},{1,1,60,20,3,20}};
    tune::kRouteDepth=2;
    assert(b.route_search(firsts,1)==0);
    assert(b.route_search(firsts,2)==1);
    // Both single scores are exact: a route candidate threshold can still
    // discard the best first leg. This is separate from bound admissibility.
    std::printf("route_candidate_fixture limited_first=0 expanded_first=1\n");
}
int main(int argc,char** argv) {
    const auto start=std::chrono::steady_clock::now();
    focused(); camp_checks(); route_checks();
    std::mt19937 rng(3603);
    auto r=[&](int n){ return static_cast<int>(rng()%n); };
    constexpr int scenes=24000;
    for(int scene=0;scene<scenes;++scene) {
        Bot b;
        b.round=std::array<int,7>{1,50,59,60,61,150,300}[r(7)];
        b.items=std::array<float,4>{0,14.999f,15,30}[r(4)];
        b.dps=1+r(150); b.max_hp=100+r(1000); b.soak=r(10000);
        b.gold_w=r(3)==0 ? .1f : .18f; b.range=1+r(4);
        b.alive_them=r(7); b.siege_rate=r(3)==0 ? 10.f : 0;
        b.w.altar_ready=r(2); b.w.enemy_ready=r(2); b.w.belief=r(12);
        tune::kBootBonus=std::array<float,5>{0,.3329f,.8f,2,4}[r(5)];
        tune::kAltarBonus=r(3)==0 ? .75f : 0;
        tune::kNumbersBonus=r(3)==0 ? 1.25f : 0;
        tune::kStickiness=std::array<float,4>{.5f,1,1.523f,2.5f}[r(4)];
        tune::kWaitWeight=std::array<float,3>{0,1,3.2398f}[r(3)];
        tune::kTerritory=std::array<float,3>{0,.7f,1}[r(3)];
        tune::kCohesion=r(3)==0 ? .05f : 0;
        tune::kHpCost=r(3)==0 ? 4.f : 0;
        tune::kShareGold=r(2); tune::kSiege=r(2); tune::kIncumbent=r(2);
        tune::kCapStaff=r(3); tune::kBelief=r(2); tune::kBeliefW=r(2) ? .4f : 0;
        tune::kRouteDepth=r(2) ? 1 : 2;
        buddy_penalty=r(3)==0 ? .1f : 0;
        seed_value=r(2) ? scene+1 : 0;
        seed_eps=std::array<float,3>{.04f,.5f,1}[r(3)];
        bigcamp=r(2) ? 20 : 0; bigjob_mult=std::array<float,4>{.5f,1,6,12}[r(4)];
        const int n=1+r(50);
        for(int i=0;i<n;++i) {
            // Sequence RNG draws explicitly; C++ argument evaluation order is
            // otherwise unspecified and can differ between optimized variants.
            const auto gems=r(401);
            const auto dist=r(80);
            const auto state=static_cast<GroupState>(r(3));
            const auto harmless=r(2);
            add_group(b,gems,dist,state,harmless);
            auto& g=b.w.groups.back();
            g.gold=r(701); g.health=r(20000); g.respawn_round=b.round+r(101);
            g.enemy_round=r(3)==0 ? b.round-r(20) : -1000;
            g.need=1+r(7); g.lower=r(4); g.higher=r(4); g.covered=r(30)==0; g.poorer=r(30)==0;
            b.w.group_margin.back()=r(21)-10;
        }
        b.target_group=r(n+1)-1;
        if(r(2)) b.w.allies.push_back({{r(40),0},1});
        check(b,1+r(6));
    }
    auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    if(argc>1) {
        std::ofstream out(argv[1],std::ios::binary);
        out.write(reinterpret_cast<const char*>(choices.data()),static_cast<std::streamsize>(choices.size()*sizeof(choices[0])));
        assert(out.good());
    }
    std::printf("RESULT cases=%llu scored=%llu violations=%llu checksum=%llu elapsed_us=%lld\n",
        (unsigned long long)cases,(unsigned long long)candidates,(unsigned long long)violations,
        (unsigned long long)checksum,(long long)elapsed);
}
