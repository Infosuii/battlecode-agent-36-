"""Actual World lifecycle methods against controlled SDK observation failures/states."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];WORK=ROOT.parent
OUT=ROOT/'agent2/evidence/world-awake-fixture';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'bot_v36/src/world.cpp').read_text()
methods=source[source.index('bool World::camp_awake('):source.index('int World::group_to_group(')]
methods+=source[source.index('void World::apply_event('):source.index('void World::observe_champions(')]
preamble=r'''
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
'''
tests=r'''
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
'''
cpp=OUT/'world_awake_fixture.cpp';cpp.write_text(preamble+methods+tests)
sdk=WORK/'development/wasi-sdk-33.0-x86_64-windows';artifact=OUT/'world_awake_fixture.wasm'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fno-exceptions',str(cpp),'-o',str(artifact)]
r=subprocess.run(cmd,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr[-4000:])
runner=OUT/'run.cjs';runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const w=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:w.wasiImport}).then(({instance})=>process.exitCode=w.start(instance));")
r=subprocess.run(['node',str(runner),str(artifact)],capture_output=True,text=True)
record=dict(source_sha256=hashlib.sha256(source.encode()).hexdigest(),command=cmd,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr)
(OUT/'world-awake-fixture.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
if r.returncode:raise RuntimeError('World fixture failed')
