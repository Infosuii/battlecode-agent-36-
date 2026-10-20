
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <set>
#include <vector>
#define V36_FARM_DIAG 0
#define V36_ROWS_CAPTURE_INTENT policy_on
#define NOVA_STRICT_MAP_ID strict_on
bool policy_on=true,strict_on=true,missing_metadata=false;int map_id=13;
struct vec2{int x,y;};
struct Board{std::set<std::pair<int,int>>cells;};
bool bit(const Board&b,vec2 p){return b.cells.count({p.x,p.y});}
namespace bc{namespace draft{unsigned team_size(){if(missing_metadata)throw std::exception();return 4;}}}
int nova_official_map_id(int,int,unsigned,int){return map_id;}
namespace tune{int kShareGold=1;}
float items_of(int,float items,float,float){return items;}
enum class GroupState{alive,reward,respawning};
struct GroupRec{GroupState state=GroupState::reward;Board tiles;};
struct Ally{unsigned id;vec2 pos;int type=0;float max_hp=0,strength=0,focus=0;};
struct World{int width=50,height=50;std::vector<GroupRec>groups;std::vector<Ally>allies;};
constexpr int kClaimSlots=6;
struct Bot{
 World w;int team_size=5,target_group=0,slot=0,round=27;std::array<unsigned,6>ally_handle{10,11,12,13,14,15};
 std::array<int,6>claims{0,2,0,0,0,0};float items=0;
 float my_items()const{return items;}
 int claim_of(int s)const{return claims[s];}
 bool rows_capture_intent()const;
 bool group_capture_covered(const GroupRec&)const;
};
bool Bot::rows_capture_intent() const {
    if (!V36_ROWS_CAPTURE_INTENT || !NOVA_STRICT_MAP_ID) return false;
    static int rows = -1;
    if (rows < 0) {
        rows = 0;
        try {
            // Canonical layout identity, actual draft picks (C&I is one pick).
            // Failed facts stay unknown and leave the baseline policy in place.
            rows = nova_official_map_id(w.width, w.height, bc::draft::team_size(), static_cast<int>(w.groups.size())) == 13;
        } catch (...) {
        }
    }
    return rows == 1;
}

bool Bot::group_capture_covered(const GroupRec& g) const {
    if (g.state != GroupState::reward) return false;
    const bool require_intent = rows_capture_intent();
    const int group = static_cast<int>(&g - w.groups.data());
    const float mine = tune::kShareGold ? my_items() : 0.0f;
    for (const auto& a : w.allies) {
        if (!bit(g.tiles, a.pos)) continue;
        if (require_intent) {
            int ally_slot = -1;
            for (int s = 0; s < std::min(team_size, kClaimSlots); ++s)
                if (ally_handle[static_cast<std::size_t>(s)] == a.id) ally_slot = s;
            if (ally_slot < 0 || claim_of(ally_slot) != group) continue;
        }
        // A clearly richer ally on the region does not cover it: we take over and it hands over.
        if (tune::kShareGold && items_of(a.type, a.max_hp, a.strength, a.focus) > mine + static_cast<float>(tune::kShareGold))
            continue;
#if V36_FARM_DIAG
        static int last_covered_round = -1;
        if (round <= 60 && last_covered_round != round) {
            int ally_slot = -1;
            for (int s = 0; s < std::min(team_size, kClaimSlots); ++s)
                if (ally_handle[static_cast<std::size_t>(s)] == a.id) ally_slot = s;
            std::printf("FARM_COVER R%d slot%d G%td ally%u ally_slot%d ally_claim%d target%d\n",
                round, slot, &g - w.groups.data(), a.id, ally_slot, ally_slot >= 0 ? claim_of(ally_slot) : -1, target_group);
            last_covered_round = round;
        }
#endif
        return true;
    }
    return false;
}


int main(int argc,char**argv){
 int scenario=argc>1?std::atoi(argv[1]):0;
 if(scenario==1)map_id=-1; if(scenario==2)map_id=3;
 if(scenario==3)missing_metadata=true;if(scenario==4)strict_on=false;
 const bool active=scenario==0;
 Bot b;b.w.groups.resize(3);b.w.groups[0].tiles.cells.insert({18,4});b.w.allies.push_back({11,{18,4}});
 unsigned checks=0;
 // Actual rows counterexample: ally stands on reward but routes to group 2.
 policy_on=false;assert(b.group_capture_covered(b.w.groups[0]));++checks;
 policy_on=true;assert(b.group_capture_covered(b.w.groups[0])==!active);++checks;
 // Same physical position, explicit capture intent: preserve incumbent coverage.
 b.claims[1]=0;assert(b.group_capture_covered(b.w.groups[0]));++checks;
 // Matching claim without physical arrival is not guaranteed collection/tanking.
 b.w.allies[0].pos={19,4};assert(!b.group_capture_covered(b.w.groups[0]));++checks;
 b.w.allies[0].pos={18,4};b.claims[1]=127;assert(b.group_capture_covered(b.w.groups[0])==!active);++checks;
 // Unknown ally slot cannot provide verified intent; baseline remains unchanged elsewhere.
 b.w.allies[0].id=999;assert(b.group_capture_covered(b.w.groups[0])==!active);++checks;
 b.w.allies[0].id=11;b.claims[1]=0;
 // Do not erase existing poorer-collector takeover semantics or reward-state guard.
 b.w.allies[0].max_hp=10;assert(!b.group_capture_covered(b.w.groups[0]));++checks;
 b.w.allies[0].max_hp=0;b.w.groups[0].state=GroupState::alive;assert(!b.group_capture_covered(b.w.groups[0]));++checks;
 // The group's actual index, not current target, determines coverage.
 b.w.groups[1].tiles.cells.insert({18,4});b.target_group=2;b.claims[1]=1;
 assert(b.group_capture_covered(b.w.groups[1]));++checks;
 b.claims[1]=2;assert(b.group_capture_covered(b.w.groups[1])==!active);++checks;
 std::printf("{\"scenario\":%d,\"checks\":%u,\"policy_active\":%d}\n",scenario,checks,active);
}
