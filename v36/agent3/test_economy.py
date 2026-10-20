"""Run the actual home/recall policy on controlled SDK/state fixtures."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'agent3/evidence';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'bot_v36/src/bot.cpp').read_text()
policy=source[source.index('bool Bot::go_home_if_needed()'):source.index('bool Bot::tribute_if_needed()')]
preamble=r'''
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
'''
tests=r'''
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
'''
cpp=OUT/'economy_policy_fixture.cpp';cpp.write_text(preamble+policy+tests)
sdk=next((ROOT.parent/'development').glob('wasi-sdk-*/bin/clang++.exe')).parent.parent
wasm=OUT/'economy_policy_fixture.wasm'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fwasm-exceptions','-mllvm','-wasm-use-legacy-eh=false','--no-wasm-opt','-L'+str(sdk/'share/wasi-sysroot/lib/wasm32-wasip1/eh'),'-lunwind',str(cpp),'-o',str(wasm)]
r=subprocess.run(cmd,capture_output=True,text=True);(OUT/'economy-fixture-build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr)
runner=OUT/'run_economy_fixture.cjs'
runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const wasi=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>process.exitCode=wasi.start(instance));")
r=subprocess.run(['node',str(runner),str(wasm)],capture_output=True,text=True)
evidence=dict(command=cmd,source_sha256=hashlib.sha256((ROOT/'bot_v36/src/bot.cpp').read_bytes()).hexdigest(),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr,scope='Actual go_home_if_needed body with controlled SDK/action observations; no official engine legality or match evidence')
(OUT/'economy-fixture.json').write_text(json.dumps(evidence,indent=2));print(json.dumps(evidence,indent=2))
if r.returncode:raise RuntimeError('Economy fixture failed')
