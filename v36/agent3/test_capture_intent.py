"""Actual production capture-coverage helper, controlled positions/claims/identity."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];OUT=ROOT/'agent3/evidence';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'bot_v36/src/bot.cpp').read_text()
policy=source[source.index('bool Bot::rows_capture_intent()'):source.index('float Bot::my_items()')]
preamble=r'''
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
'''
tests=r'''
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
'''
cpp=OUT/'capture_intent_fixture.cpp';cpp.write_text(preamble+policy+tests)
sdk=next((ROOT.parent/'development').glob('wasi-sdk-*/bin/clang++.exe')).parent.parent
wasm=OUT/'capture_intent_fixture.wasm'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fwasm-exceptions','-mllvm','-wasm-use-legacy-eh=false','--no-wasm-opt','-L'+str(sdk/'share/wasi-sysroot/lib/wasm32-wasip1/eh'),'-lunwind',str(cpp),'-o',str(wasm)]
r=subprocess.run(cmd,capture_output=True,text=True);(OUT/'capture-intent-build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr)
runner=OUT/'run_capture_fixture.cjs'
runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const wasi=new WASI({version:'preview1',args:['fixture',process.argv[3]],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>process.exitCode=wasi.start(instance));")
results=[]
for scenario in range(5):
 r=subprocess.run(['node',str(runner),str(wasm),str(scenario)],capture_output=True,text=True)
 results.append(dict(scenario=scenario,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
evidence=dict(source_sha256=hashlib.sha256((ROOT/'bot_v36/src/bot.cpp').read_bytes()).hexdigest(),command=cmd,results=results,scope='Actual production helpers with controlled observations/claims and identity API result; strict classifier native fixtures are separate')
(OUT/'capture-intent-fixture.json').write_text(json.dumps(evidence,indent=2));print(json.dumps(evidence,indent=2))
assert all(r['returncode']==0 for r in results)
