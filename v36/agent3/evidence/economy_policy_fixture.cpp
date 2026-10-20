
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <set>
#include <string>
#include <utility>
#include <vector>
#define LOG(...)
#define NOVA_SPAWN_HOT 1
#define NOVA_CI_RETREAT 1.0f
#define NOVA_CI_LINK 1
struct vec2{int x=0,y=0;};
int cheb(vec2 a,vec2 b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));}
namespace bc{
enum class champion_type{tavi,cassi,issac,other};
struct bitboard{
 std::set<std::pair<int,int>> cells;
 void clear(){cells.clear();}void set(vec2 p,bool yes){if(yes)cells.insert({p.x,p.y});else cells.erase({p.x,p.y});}
 void draw_chebyshev(vec2 p,float rad){for(int x=p.x-int(rad);x<=p.x+int(rad);++x)for(int y=p.y-int(rad);y<=p.y+int(rad);++y)set({x,y},true);}
};
bitboard operator~(const bitboard&a){bitboard b;for(int x=-15;x<=15;++x)for(int y=-15;y<=15;++y)if(!a.cells.count({x,y}))b.set({x,y},true);return b;}
bitboard operator&(const bitboard&a,const bitboard&b){bitboard c;for(auto p:a.cells)if(b.cells.count(p))c.cells.insert(p);return c;}
bitboard operator|(const bitboard&a,const bitboard&b){bitboard c=a;c.cells.insert(b.cells.begin(),b.cells.end());return c;}
struct Self{int recalls=0;bool fail=false;void recall(){++recalls;if(fail)throw std::exception();}};
Self self_;Self& self(){return self_;}
}
bool bit(const bc::bitboard&b,vec2 p){return b.cells.count({p.x,p.y});}
int fdist(int field,vec2){return field;}
namespace tune{float kRetreatHp=.1288f,kThreatRetreat=.4f;int kShopGold=126,kShopWalkDist=7;bool kAwakeZone=true,kBushRecall=true,kUnstick=true;}
enum class GroupState{alive,reward,respawning};
struct Group{GroupState state=GroupState::alive;bc::bitboard tiles;};
struct Camp{bool dead=false;float strength=20;vec2 pos;int range=3;};
struct Ally{bc::champion_type type=bc::champion_type::other;float hp=100;vec2 pos;};
struct Sighting{int round=0;vec2 pos;};
struct World{bc::bitboard ally_spawn,hot_zone,static_blocked,unit_tiles;std::vector<Group>groups;std::vector<Camp>camps;std::vector<int>camp_group;std::vector<Ally>allies,enemies;std::vector<Sighting>sightings;int spawn_field=20;};
struct Bot{
 World w;vec2 pos{0,0};float hp=100,max_hp=100,prev_hp=100;int gold=0,target_group=-1,round=100;
 bool heading_home=false,hidden=false,follow_success=true,walk_success=true;
 int walks=0,follows=0,breaks=0;const char*mode="idle";bc::bitboard last_goal;
 bc::champion_type type=bc::champion_type::other;
 bool walk_toward(const bc::bitboard&b){++walks;last_goal=b;return walk_success;}
 bool follow_field(int,const bc::bitboard&,bool){++follows;return follow_success;}
 bool break_out(){++breaks;return true;}
 bool hidden_here(){return hidden;}
 bool go_home_if_needed();
};
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


int main(){unsigned checks=0;
 auto reset=[](){bc::self_=bc::Self{};};
 {reset();Bot b;b.type=bc::champion_type::tavi;b.gold=1000;assert(!b.go_home_if_needed()&&bc::self_.recalls==0);++checks;}
 {reset();Bot b;b.type=bc::champion_type::tavi;b.hp=5;b.prev_hp=5;assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
 {reset();Bot b;b.hp=5;b.prev_hp=5;assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
 {reset();Bot b;b.gold=200;b.target_group=0;b.w.groups.resize(1);b.w.groups[0].state=GroupState::reward;b.w.groups[0].tiles.set(b.pos,true);assert(!b.go_home_if_needed()&&bc::self_.recalls==0);++checks;}
 {reset();Bot b;b.gold=200;b.hp=5;b.prev_hp=5;b.target_group=0;b.w.groups.resize(1);b.w.groups[0].state=GroupState::reward;b.w.groups[0].tiles.set(b.pos,true);assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
 {reset();Bot b;b.w.ally_spawn.set(b.pos,true);b.heading_home=true;b.hp=99;assert(b.go_home_if_needed()&&!b.heading_home&&bc::self_.recalls==0);b.hp=100;assert(!b.go_home_if_needed()&&!b.heading_home);++checks;}
 {reset();Bot b;b.w.ally_spawn.set(b.pos,true);b.w.hot_zone.set(b.pos,true);b.hp=90;assert(!b.go_home_if_needed());++checks;}
 {reset();Bot b;b.gold=200;b.w.hot_zone.set(b.pos,true);assert(b.go_home_if_needed()&&b.walks==1&&bc::self_.recalls==0&&!bit(b.last_goal,b.pos));++checks;}
 {reset();Bot b;b.gold=200;b.hp=80;b.prev_hp=100;b.w.groups.resize(1);b.w.camps.push_back({false,20,{1,0},3});b.w.camp_group.push_back(0);assert(b.go_home_if_needed()&&b.walks==1&&bc::self_.recalls==0&&!bit(b.last_goal,b.pos));++checks;}
 {reset();Bot b;b.gold=200;b.w.enemies.push_back({bc::champion_type::other,100,{5,0}});assert(b.go_home_if_needed()&&b.follows==1&&bc::self_.recalls==0);++checks;}
 {reset();Bot b;b.gold=200;b.hidden=true;b.w.enemies.push_back({bc::champion_type::other,100,{5,0}});assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
 {reset();Bot b;b.gold=200;bc::self_.fail=true;assert(b.go_home_if_needed()&&bc::self_.recalls==1&&b.follows==1);++checks;}
 {reset();Bot b;b.gold=200;b.w.spawn_field=5;assert(b.go_home_if_needed()&&bc::self_.recalls==0&&b.follows==1);++checks;}
 {reset();Bot b;b.type=bc::champion_type::cassi;b.w.allies.push_back({bc::champion_type::issac,5,{4,0}});assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
 {reset();Bot b;b.gold=200;b.w.spawn_field=5;b.follow_success=false;b.walk_success=false;assert(b.go_home_if_needed()&&b.follows==2&&b.walks==1&&b.breaks==1);++checks;}
 std::printf("{\"actual_home_policy_scenarios\":%u}\n",checks);
}
