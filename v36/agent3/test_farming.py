"""Compile the production score_groups body against synthetic observations, run via WASI.

The scoring algorithm is extracted verbatim, not translated into a test implementation.
SDK observation/eligibility helpers are controlled fixtures; this is not an engine oracle.
"""
import hashlib, json, pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORK = ROOT.parent
OUT = ROOT / 'agent3' / 'evidence'
OUT.mkdir(parents=True, exist_ok=True)
src = (ROOT / 'bot_v36/src/bot.cpp').read_text()
body = src[src.index('int Bot::score_groups('):src.index('// Follow-up table for one champion')]
camp_methods = src[src.index('bool Bot::camp_targets_me('):src.index('// Damage taken per unit of team damage-per-turn')]
camp_methods += src[src.index('float Bot::group_soak('):src.index('int Bot::champions_needed(')]
# Diagnostics assert every computed final production score against its actual bound.
# Suppress successful per-call logs only; failures still trap and fail the WASI process.
body = body.replace('std::printf("FARM_SCORE', 'quiet_printf("FARM_SCORE')
preamble = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <tuple>
#include <vector>
#include "farming_bound.h"
#define NOVA_LEARNER 1
#define NOVA_LEARNER_W 12.0f,-5.0f,3.0f
#include "learner.h"
#define PROF(x)
#define LOG(...)
#define V36_FARM_BOUND bound_on
#define V36_FARM_DIAG 1
#define V36_FARM_EXHAUSTIVE exhaustive
#define NOVA_BIGJOB 0.6f
#define NOVA_BIGJOB_FROM 60
#define NOVA_BIGJOB_MULT 6.0f
#define NOVA_BIGCAMP 0
#define NOVA_DUO 0
#define APEX_BOOT_ITEMS 15
#define NOVA_BUDDY 0.0f
#define NOVA_BUDDY_DIST 3
#define NOVA_SEED seed
#define NOVA_SEED_EPS 0.25f
bool bound_on=true,exhaustive=false; int seed=0;
void quiet_printf(const char*,...) {}
struct vec2 {int x=0,y=0;};
int cheb(vec2 a,vec2 b) {return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));}
int field_index(vec2) {return 0;}
constexpr int kUnreach=65535;
namespace bc {inline std::uint64_t fuel_remaining(){return 1000000000;}}
namespace runtime {enum class Stage {FarmScore,Route}; bool admit(Stage,int){return true;} struct Probe {Probe(Stage){}};}
namespace tune {
int kRouteDepth=1,kRouteK=4,kDetScoreN=16,kDetSafety=22000,kScoreFloor=30000,kCaptureTime=5;
float kRouteSlack=0.5f,kWaitWeight=1,kStickiness=1.523f,kBootBonus=.3329f,kAltarBonus=0,kNumbersBonus=.5f;
bool kDet=true,kShareGold=false,kSiege=false,kInitiateJobs=true,kIncumbent=false,kBelief=false,kPlan=false;
int kCapStaff=0,kNumbersInvade=2,kTerritoryUntil=150,kTerritoryMargin=0,kCohesionDist=3,kRouteFuel=40000;
float kTerritory=1,kCohesion=0,kHpCost=0,kBeliefW=0,kBeliefAlly=1;
}
enum class GroupState {alive,reward,respawning};
struct Camp {vec2 pos;float max_hp=100,hp=100,strength=10;int range=3;bool dead=false;};
struct GroupRec {
 int gems=0,gold=0,respawn_round=0,enemy_round=-1000,need=1,lower=0,higher=0;
 bool harmless=true; GroupState state=GroupState::reward; std::vector<int> camps{0};
 float hp=0,soak=0; float hp_left(const std::vector<Camp>&)const{return hp;}
};
struct Ally {int id=0;vec2 pos;};
struct World {
 std::vector<GroupRec> groups;std::vector<Camp> camps{{{0,0}}}; std::vector<Ally> enemies;
 std::vector<const std::uint16_t*> field_ptr; std::vector<int> group_margin;
 std::vector<Ally> allies;bool altar_ready=true,enemy_ready=true;int altar_field=0;
 float expected_enemies(vec2,int){return 0;}
};
int fdist(int,vec2){return 0;}
struct Bot {
 struct RouteCand {int gi,need;float value,cost,score;int finish;};
 World w;vec2 pos;std::vector<int> group_order,avoid_until;std::vector<char> order_seen,big_group;
 int target_group=-1,round=1,range=1,slot=0,duo_ignore_slot=-1,team_size=6,call_ban_until=0;
 int alive_us=6,alive_them=4,plan_bank_round=0,my_id=10;bool no_initiate=false,in_role_pack=false;
 std::array<int,6> ally_handle{};float max_hp=1000,camp_taken_mult=1,items=0;
 float my_dps(const GroupRec*)const{return 50;}
 float gold_weight()const{return .25f;}
 float my_items()const{return items;}
 bool group_capture_covered(const GroupRec&)const{return false;}
 bool poorer_ally_takes(const GroupRec&,int,bool)const{return false;}
 std::pair<int,int> claims_split(int gi)const{return {w.groups[gi].lower,w.groups[gi].higher};}
 int champions_needed(const GroupRec&g,float)const{return g.need;}
 float siege_dps(const GroupRec&)const{return 0;}
 bool open_call_by_lower()const{return false;}
 int senior_claims(int gi)const{return w.groups[gi].lower;}
 int claim_of(int)const{return -1;}
 vec2 buddy_anchor()const{return {-1,-1};}
 float soak_coefficient(int gi,float)const{return w.groups[gi].soak;}
 int plan_search(RouteCand*,int,bool&){return -1;}
 int route_search(RouteCand*,int){return -1;}
 int score_groups(int,int&,float&);
 bool camp_targets_me(int,vec2)const;
 float incoming_from_group(const GroupRec&,vec2)const;
 float group_soak(const GroupRec&,float)const;
};
'''
tests = r'''
static std::uint32_t rng=0x369bd1u;
std::uint32_t rnd(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(){
 unsigned checks=0,old_failures=0,changed_choices=0;
 unsigned camp_scenarios=0;
 { // Actual-position tanking, range, low-ID tie, tank departure and dead camp.
  Bot b;b.pos={1,0};b.w.allies.push_back({9,{1,1}});
  assert(!b.camp_targets_me(0,b.pos));++camp_scenarios;
  b.w.allies[0].id=11;assert(b.camp_targets_me(0,b.pos));++camp_scenarios;
  b.w.allies[0].pos={0,1};b.pos={2,0};assert(!b.camp_targets_me(0,b.pos));++camp_scenarios;
  b.w.allies[0].pos={9,9};assert(b.camp_targets_me(0,b.pos));++camp_scenarios;
  b.w.enemies.push_back({1,{1,0}});assert(!b.camp_targets_me(0,b.pos));++camp_scenarios;
  b.w.enemies.clear();assert(!b.camp_targets_me(0,{4,0}));++camp_scenarios;
  GroupRec g;g.camps={0};assert(b.incoming_from_group(g,b.pos)==10);++camp_scenarios;
  b.w.camps[0].dead=true;assert(b.incoming_from_group(g,b.pos)==0);++camp_scenarios;
 }
 { // Dense-scene regression characterization: the 13th harmful camp is omitted.
  Bot b;GroupRec g;g.state=GroupState::alive;g.camps.clear();b.w.camps.assign(13,Camp{});
  for(int i=0;i<13;++i)g.camps.push_back(i);
  const float uncapped=10.f*100.f*(13.f*14.f/2.f);
  const float capped=b.group_soak(g,1.f);
  assert(capped==78000.f && uncapped==91000.f && capped<uncapped);++camp_scenarios;
  g.camps.pop_back();assert(b.group_soak(g,1.f)==78000.f);++camp_scenarios;
 }
 // Minimal concrete regression: the farther harmless reward is incorrectly pruned
 // by the old bound even though the production scorer's runtime boot bonus wins.
 {
  Bot b;b.w.groups.resize(2);b.avoid_until.assign(2,0);b.w.group_margin.assign(2,1);
  std::uint16_t d[2]={1,2};b.w.field_ptr={&d[0],&d[1]};
  b.w.groups[0].gems=100;b.w.groups[0].harmless=false;
  b.w.groups[1].gems=100;tune::kBootBonus=4;tune::kStickiness=.5f;
  int need;float score;exhaustive=true;int truth=b.score_groups(1,need,score);
  exhaustive=false;bound_on=false;int old=b.score_groups(1,need,score);
  bound_on=true;int fixed=b.score_groups(1,need,score);
  assert(truth==1 && old==0 && fixed==truth);++old_failures;++checks;
 }
 // Randomized actual-score checks across state, respawn, rewards, claims, participants,
 // round 60 and dominance boundaries, item threshold, runtime bonuses and discounts.
 for(int t=0;t<25000;++t){
  Bot base;int n=2+rnd()%7;base.w.groups.resize(n);base.avoid_until.assign(n,0);
  base.w.group_margin.resize(n);std::vector<std::uint16_t> distances(n);
  base.round=(t%5==0?59:(t%5==1?60:1+rnd()%300));
  base.items=(t%3==0?14.999f:(t%3==1?15.0f:30.0f));
  base.target_group=(rnd()%3==0?int(rnd()%n):-1);
  tune::kStickiness=(rnd()%4==0?.4f:1.f+(rnd()%200)/100.f);
  tune::kBootBonus=(rnd()%6)/2.f;tune::kAltarBonus=(rnd()%4)/2.f;
  tune::kNumbersBonus=(rnd()%4)/2.f;tune::kTerritory=.2f+(rnd()%9)/10.f;
  tune::kCohesion=(rnd()%4)/10.f;tune::kHpCost=(rnd()%4)*5.f;
  seed=rnd()%2?1+rnd()%1000:0;
  for(int i=0;i<n;++i){auto&g=base.w.groups[i];
   g.gems=rnd()%201;g.gold=rnd()%301;g.harmless=rnd()%2;g.state=GroupState(rnd()%3);
   g.hp=g.state==GroupState::reward?0:rnd()%4001;g.soak=rnd()%4001;
   g.need=1+rnd()%6;g.lower=rnd()%3;g.higher=rnd()%3;
   g.enemy_round=base.round-int(rnd()%25);g.respawn_round=base.round+int(rnd()%100);
   base.w.group_margin[i]=rnd()%2?1:-1;distances[i]=rnd()%60;
   base.w.field_ptr.push_back(&distances[i]);
  }
  int ne,np;float se,sp;exhaustive=true;bound_on=true;Bot exact=base;
  int e=exact.score_groups(1+rnd()%6,ne,se);
  // Same participant hint for both calls is necessary for matched objective.
  const int hint=1+rnd()%6;exact=base;e=exact.score_groups(hint,ne,se);
  exhaustive=false;Bot pruned=base;int p=pruned.score_groups(hint,np,sp);
  assert(e==p && ne==np && se==sp);++checks;
  if(t<2000){Bot old=base;bound_on=false;int o=old.score_groups(hint,np,sp);
   old_failures+=o!=e;changed_choices+=o!=p;bound_on=true;}
 }
 // Unsupported/overflow configurations are finite and unprunable.
 const float mx=std::numeric_limits<float>::max();
 assert(farming::score_upper_bound(1,-1,1,1,1,1,1,0,1)==mx);
 assert(farming::score_upper_bound(mx,.1f,3,6,4,2,2,0,2)==mx);
 assert(farming::score_upper_bound(1,1,-1,1,1,1,1,0,1)==mx);
 std::printf("{\"actual_scorer_matched_choices\":%u,\"old_bound_choice_failures\":%u,\"changed_random_choices\":%u,\"camp_scenarios\":%u,\"dense_soak_13_camps_underestimate\":13000,\"seed\":3578833}\n",checks,old_failures,changed_choices,camp_scenarios);
}
'''
cpp=OUT/'farming_scorer_fixture.cpp'
cpp.write_text(preamble+body+camp_methods+tests)
sdk=next((WORK/'development').glob('wasi-sdk-*/bin/clang++.exe')).parent.parent
wasm=OUT/'farming_scorer_fixture.wasm'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fno-exceptions','-I'+str(ROOT/'bot_v36/src'),str(cpp),'-o',str(wasm)]
result=subprocess.run(cmd,capture_output=True,text=True)
(OUT/'fixture-build.log').write_text(result.stdout+result.stderr)
if result.returncode:raise RuntimeError(result.stderr)
runner=OUT/'run_fixture.cjs'
runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const wasi=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>process.exitCode=wasi.start(instance));")
result=subprocess.run(['node',str(runner),str(wasm)],capture_output=True,text=True)
evidence=dict(command=cmd,source_sha256=hashlib.sha256((ROOT/'bot_v36/src/bot.cpp').read_bytes()).hexdigest(),helper_sha256=hashlib.sha256((ROOT/'bot_v36/src/farming_bound.h').read_bytes()).hexdigest(),returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,scope='Production scorer body with controlled observation/eligibility helpers; not engine validation')
(OUT/'scorer-fixture.json').write_text(json.dumps(evidence,indent=2))
print(json.dumps(evidence,indent=2))
if result.returncode:raise RuntimeError('Fixture failed')
