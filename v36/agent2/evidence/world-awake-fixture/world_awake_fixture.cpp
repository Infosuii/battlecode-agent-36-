
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#define NOVA_WORLD_AWAKE 1
#define APEX_EVENTS 1
#define PROF(x)
struct vec2{int x=0,y=0;};
namespace bc {
using handle_camp_t=unsigned;
struct bitboard{std::vector<vec2> ps;bool is_empty()const{return ps.empty();}const std::vector<vec2>&positions()const{return ps;}
bitboard&operator&=(const bitboard&b){ps.erase(std::remove_if(ps.begin(),ps.end(),[&](vec2 a){return std::none_of(b.ps.begin(),b.ps.end(),[&](vec2 c){return a.x==c.x&&a.y==c.y;});}),ps.end());return *this;}};
}
namespace tune {int kAwakeZone=1,kCaptureTime=5;}
namespace d {struct camp_state{float health;};
unsigned group_state=1,life[2]={2,2};float health[2]={100,100};bool query[2]={true,false};
int sdk_camp_group_state(unsigned,unsigned*o){*o=group_state;return 0;}
int sdk_camp(unsigned i,camp_state*o){if(!query[i])return 1;o->health=health[i];return 0;}
int sdk_camp_state(unsigned i,unsigned*o){if(!query[i])return 1;*o=life[i];return 0;}
int sdk_camp_group_cooldown(unsigned,int*o){*o=10;return 0;}}
enum class GroupState:std::uint8_t{alive,reward,respawning};
struct CampRec{unsigned camp;float hp=100,max_hp=100,strength=10;bool dead=false;};
struct GroupRec{unsigned handle=0;GroupState state=GroupState::alive;std::vector<int>camps{0,1};int awake_state=-1,awake_seen=-1;bool awake_inferred=false;int available_since=0,respawn_round=0,reward_seen=-1,seen_round=-1,respawn=10;};
int find_camp(unsigned key){return key>0&&key<=2?static_cast<int>(key)-1:-1;}
struct World{
std::vector<CampRec>camps{{0},{1}};std::vector<GroupRec>groups{GroupRec{}};
std::vector<int>camp_group{0,0};std::vector<std::uint8_t>group_visible{0},camp_visible{0,0};
std::vector<int>pending_events;bc::bitboard visible{{{0,0}}},camp_tiles_all{{{0,0},{1,0}}};int width=16;
bool camp_awake(int)const;void observe_groups(int);void apply_event(int,int,int);};
bool World::camp_awake(int ci) const {
    const auto& c = camps[static_cast<std::size_t>(ci)];
    if (c.dead || c.strength <= 0) return false;
#if NOVA_WORLD_AWAKE
    const int gi = camp_group[static_cast<std::size_t>(ci)];
    if (gi < 0) return false;
    const auto& g = groups[static_cast<std::size_t>(gi)];
    if (g.state != GroupState::alive) return false;
    if (g.awake_state >= 0) return g.awake_state == 1;
    // Conservative legacy inference only while lifecycle has never been observed.
    if (tune::kAwakeZone)
        for (int member : g.camps)
            if (camps[member].dead || camps[member].hp < camps[member].max_hp) return true;
#endif
    return c.hp < c.max_hp;
}

void World::apply_event(int gi, int ready_round, int round) {
    if (gi < 0 || gi >= static_cast<int>(groups.size())) return;
    if (!group_visible.empty() && group_visible[static_cast<std::size_t>(gi)]) return; // our own eyes win
    auto& g = groups[static_cast<std::size_t>(gi)];
    if (ready_round > round) {
        if (g.state == GroupState::respawning && std::abs(g.respawn_round - ready_round) <= 1) return;
        g.state = GroupState::respawning;
        g.respawn_round = ready_round;
#if NOVA_WORLD_AWAKE
        g.awake_state = -1;
        g.awake_inferred = true;
#endif
        for (int i : g.camps) {
            camps[i].hp = camps[i].max_hp;
            camps[i].dead = false;
        }
    } else if (g.state != GroupState::alive && g.seen_round < ready_round) {
        g.state = GroupState::alive;
        g.available_since = ready_round;
#if NOVA_WORLD_AWAKE
        g.awake_state = -1;
        g.awake_inferred = true;
#endif
        for (int i : g.camps) {
            camps[i].hp = camps[i].max_hp;
            camps[i].dead = false;
        }
    }
}

void World::observe_groups(int round) {
    PROF(groups);
    // One intersection plus table lookups instead of a bitboard AND per group (that cost ~25k fuel).
    std::fill(group_visible.begin(), group_visible.end(), 0);
    std::fill(camp_visible.begin(), camp_visible.end(), 0);
    bc::bitboard seen = visible;
    seen &= camp_tiles_all;
    if (!seen.is_empty())
        for (vec2 p : seen.positions()) {
            const int ci = find_camp(static_cast<std::uint32_t>(p.y * width + p.x) + 1);
            if (ci < 0) continue;
            camp_visible[ci] = 1;
            group_visible[camp_group[ci]] = 1;
        }
    for (std::size_t gi = 0; gi < groups.size(); ++gi) {
        auto& g = groups[gi];
        if (!group_visible[gi]) {
            // Not in view: advance predictions.
            if (g.state == GroupState::respawning && round >= g.respawn_round) {
                g.state = GroupState::alive;
#if NOVA_WORLD_AWAKE
                g.awake_state = -1;
                g.awake_inferred = true;
#endif
                for (int i : g.camps) {
                    camps[i].hp = camps[i].max_hp;
                    camps[i].dead = false;
                }
            } else if (g.state == GroupState::reward && round - g.reward_seen > 12) {
                // Someone almost certainly captured it; assume the respawn clock started.
                g.state = GroupState::respawning;
                g.respawn_round = g.reward_seen + tune::kCaptureTime + g.respawn;
#if NOVA_WORLD_AWAKE
                g.awake_state = -1;
                g.awake_inferred = true;
#endif
            }
            continue;
        }
        g.seen_round = round;
        std::uint32_t st = 0;
        if (d::sdk_camp_group_state(g.handle, &st) != 0) continue;
        if (st == 1) {
            g.state = GroupState::alive;
#if NOVA_WORLD_AWAKE
            bool fresh_awake = false, fresh_asleep = false, new_attack_evidence = false;
#endif
            for (int i : g.camps) {
                d::camp_state cs{};
                const int code = d::sdk_camp(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camps[i].camp)), &cs);
                if (code == 0) {
#if NOVA_WORLD_AWAKE
                    new_attack_evidence |= cs.health < camps[i].hp || (cs.health <= 0 && !camps[i].dead);
#endif
                    camps[i].hp = cs.health;
                    camps[i].dead = cs.health <= 0;
#if NOVA_WORLD_AWAKE
                    std::uint32_t lifecycle = 0;
                    if (d::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camps[i].camp)), &lifecycle) == 0
                        && (lifecycle == 1 || lifecycle == 2)) {
                        fresh_awake |= lifecycle == 1;
                        fresh_asleep |= lifecycle == 2;
                    }
#endif
                } else if (camp_visible[i]) {
#if NOVA_WORLD_AWAKE
                    new_attack_evidence |= !camps[i].dead;
#endif
                    camps[i].dead = true;
                }
            }
#if NOVA_WORLD_AWAKE
            if (fresh_awake || fresh_asleep) {
                g.awake_state = fresh_awake ? 1 : 0;
                g.awake_seen = round;
                g.awake_inferred = false;
            } else if (new_attack_evidence) {
                // A new visible death/damage wakes unseen survivors, even when
                // the previous direct observation was asleep. Do not forge its age.
                g.awake_state = 1;
                g.awake_inferred = true;
            }
#endif
        } else if (st == 2) {
            g.state = GroupState::reward;
#if NOVA_WORLD_AWAKE
            g.awake_state = 0;
            g.awake_seen = round;
            g.awake_inferred = false;
#endif
            g.reward_seen = round;
        } else if (st == 3) {
#if NOVA_WORLD_AWAKE
            g.awake_state = 0;
            g.awake_seen = round;
            g.awake_inferred = false;
#endif
            std::int32_t timer = 0;
            d::sdk_camp_group_cooldown(g.handle, &timer);
            if (APEX_EVENTS && (g.state != GroupState::respawning || std::abs(g.respawn_round - (round + timer)) > 1))
                pending_events.push_back(static_cast<int>(gi));
            g.state = GroupState::respawning;
            g.respawn_round = round + timer;
            for (int i : g.camps) {
                camps[i].hp = camps[i].max_hp;
                camps[i].dead = false;
            }
        }
    }
}


int main(){unsigned checks=0;World w;
 w.observe_groups(1);assert(w.groups[0].awake_state==0&&w.groups[0].awake_seen==1&&!w.groups[0].awake_inferred);assert(!w.camp_awake(0)&&!w.camp_awake(1));++checks;
 d::health[0]=75;d::life[0]=1;w.observe_groups(2);assert(w.camp_awake(1)&&w.groups[0].awake_seen==2&&!w.groups[0].awake_inferred);++checks;
 d::life[0]=2;w.observe_groups(3);assert(!w.camp_awake(0)&&!w.camp_awake(1)&&w.camps[0].hp==75);++checks;
 d::health[0]=0;d::life[0]=0;w.observe_groups(4);assert(w.camp_awake(1)&&w.groups[0].awake_inferred&&w.groups[0].awake_seen==3);assert(!w.camp_awake(0));++checks;
 w.visible.ps={{1,0}};d::query[0]=false;d::query[1]=true;w.observe_groups(5);assert(!w.camp_awake(1)&&!w.groups[0].awake_inferred&&w.groups[0].awake_seen==5);++checks;
 d::life[1]=1;w.observe_groups(6);w.visible.ps.clear();w.observe_groups(7);assert(w.camp_awake(1)&&w.groups[0].awake_seen==6);++checks;
 w.groups[0].state=GroupState::respawning;w.groups[0].respawn_round=8;w.observe_groups(8);assert(w.groups[0].state==GroupState::alive&&w.groups[0].awake_state==-1&&w.groups[0].awake_seen==6&&w.groups[0].awake_inferred);assert(!w.camp_awake(0)&&!w.camp_awake(1));++checks;
 w.group_visible[0]=0;w.apply_event(0,20,9);assert(w.groups[0].state==GroupState::respawning&&w.groups[0].awake_seen==6&&w.groups[0].awake_state==-1);++checks;
 w.apply_event(0,20,21);assert(w.groups[0].state==GroupState::alive&&w.groups[0].awake_seen==6&&w.groups[0].awake_state==-1);++checks;
 w.visible.ps={{1,0}};d::group_state=2;w.observe_groups(22);assert(w.groups[0].state==GroupState::reward&&w.groups[0].awake_seen==22&&!w.camp_awake(1));++checks;
 std::printf("{\"production_world_lifecycle_checks\":%u,\"scope\":\"controlled observed/dead/hidden SDK states, not engine policy evidence\"}\n",checks);
}
