"""Production ts_snapshot body against controlled observation helpers; not engine equivalence."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];WORK=ROOT.parent
OUT=ROOT/'agent2/evidence/snapshot-fixture';OUT.mkdir(parents=True,exist_ok=True)
src=(ROOT/'bot_v36/src/tsearch.cpp').read_text()
body=src[src.index('bool Bot::ts_snapshot('):src.index('bool Bot::tactical_search(')]
(OUT/'tsearch.h').write_text((ROOT/'bot_v36/src/tsearch.h').read_text().replace('#include "common.h"','#include "portable.h"'))
(OUT/'portable.h').write_text('''#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#define NOVA_QUARTIC 0
#define NOVA_TS_RELEVANCE 1
#define NOVA_TS_MAX_UNITS 5
#define NOVA_TS_SAFETY 1
#define NOVA_TS_SIM_FIXES 0
#define NOVA_RUNTIME_TELEMETRY 0
''')
preamble=r'''
#include "tsearch.h"
#include <cassert>
#include <numeric>
struct vec2{int x=0,y=0;};
int cheb(vec2 a,vec2 b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));}
namespace bc {
enum class champion_type {ryth=0,cassi=3,issac=4};
struct bitboard{std::uint64_t bits[4]={};const std::uint64_t* ptr()const{return bits;}std::vector<vec2> positions()const{return {};}};
namespace detail {struct Layout{int chunk_stride=2;};Layout layout(){return {};}}
}
namespace d {struct champion_info {unsigned state=0;};int sdk_champion(unsigned,champion_info*){return 0;}int sdk_champion_gems(unsigned,unsigned*g){*g=0;return 0;}}
namespace runtime {enum class Stage{Snapshot};struct Probe{Probe(Stage){}};}
namespace tune {int kTsRange=6,kTsCamps=0;}
enum class GroupState{alive};
struct CampRec{bool dead=false;float hp=100,strength=0;vec2 pos;int range=3;};
struct Group{GroupState state=GroupState::alive;std::vector<int>camps;};
struct AllyRec{unsigned id;bc::champion_type type;vec2 pos;float hp=100,max_hp=100,strength=20,focus=20;int range=1;};
struct EnemyRec:AllyRec{int gems=0;unsigned state=0;bool tributing()const{return false;}bool recalling()const{return false;}};
struct World{bc::bitboard static_blocked,ally_spawn,enemy_spawn,unit_tiles;int width=16,height=16;std::vector<AllyRec>allies;std::vector<EnemyRec>enemies;std::vector<CampRec>camps;std::vector<Group>groups;};
void fill_unit(ts::Unit&u,int team,bc::champion_type,vec2 p,float hp,float mh,float st,float,int range,int gems,unsigned,bool acted,const int*){
u=ts::Unit{};u.team=team;u.x=p.x;u.y=p.y;u.hp=hp;u.max_hp=mh;u.str=st;u.range=range;u.gems=gems;u.flags=ts::U_ALIVE|(acted?ts::U_ACTED:0);}
struct Bot{World w;vec2 pos{4,4},ts_home[2]={{0,0},{15,15}};int cooldown[2]={},my_id=1,round=10,range=1,gems=0;float hp=100,max_hp=100,strength=20,focus=20;bc::champion_type type=bc::champion_type::ryth;bool ts_snapshot(ts::State&,ts::Map&,const EnemyRec**);};
'''
tests=r'''
EnemyRec enemy(unsigned id,int x,int y,bc::champion_type type=bc::champion_type::ryth){EnemyRec e;e.id=id;e.type=type;e.pos={x,y};return e;}
std::vector<unsigned> pick(Bot&b,ts::State*output=nullptr){ts::State s;ts::Map m;const EnemyRec*refs[8]={};assert(b.ts_snapshot(s,m,refs));if(output)*output=s;return {ts::last_decision.selected,ts::last_decision.selected+s.n};}
int main(){int checks=0;
 {Bot base;for(int i=0;i<6;++i)base.w.enemies.push_back(enemy(100+i,5+i,4));
  auto expected=pick(base);assert(expected.size()==5 && ts::last_decision.eligible_enemies==6 && ts::last_decision.dropped==2);++checks;
  assert(ts::last_decision.omitted_enemy && !ts::last_decision.allow_fight_override());++checks;
  std::vector<int> order(6);std::iota(order.begin(),order.end(),0);
  do{Bot b;for(int i:order)b.w.enemies.push_back(base.w.enemies[i]);assert(pick(b)==expected);++checks;}while(std::next_permutation(order.begin(),order.end()));
 }
 {Bot b;b.type=bc::champion_type::cassi;
  AllyRec a;a.id=2;a.type=bc::champion_type::issac;a.pos={12,4};b.w.allies.push_back(a);
  b.w.enemies.push_back(enemy(10,5,4));ts::State s;auto ids=pick(b,&s);
  assert(ids.size()==3 && ids[1]==2 && !ts::last_decision.incomplete_link && s.u[0].link==1 && s.u[1].link==0);++checks;
 }
 {Bot b;b.type=bc::champion_type::cassi;b.w.enemies.push_back(enemy(10,5,4));auto ids=pick(b);
  assert(ids.size()==2 && ts::last_decision.incomplete_link && !ts::last_decision.allow_fight_override());++checks;
 }
 {Bot b;b.w.enemies={enemy(10,5,4),enemy(11,6,4,bc::champion_type::cassi),enemy(12,12,4,bc::champion_type::issac)};
  ts::State s;auto ids=pick(b,&s);auto a=std::find(ids.begin(),ids.end(),11),c=std::find(ids.begin(),ids.end(),12);
  assert((a==ids.end())==(c==ids.end()));assert(!ts::last_decision.incomplete_link);++checks;
 }
 {Bot b;b.w.enemies={enemy(10,5,4,bc::champion_type::cassi)};pick(b);
  assert(ts::last_decision.incomplete_link && !ts::last_decision.allow_fight_override());++checks;
 }
 {Bot b;b.w.enemies={enemy(10,5,4)};CampRec c;c.pos={6,4};c.strength=10;b.w.camps.push_back(c);pick(b);
  assert(ts::last_decision.camp_risk && !ts::last_decision.allow_fight_override());++checks;
 }
 std::printf("{\"production_snapshot_property_checks\":%d,\"scope\":\"controlled observation helpers, no engine policy claim\"}\n",checks);
}
'''
cpp=OUT/'snapshot_fixture.cpp';cpp.write_text(preamble+body+tests)
sdk=WORK/'development/wasi-sdk-33.0-x86_64-windows';artifact=OUT/'snapshot_fixture.wasm'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fno-exceptions',str(cpp),'-o',str(artifact)]
r=subprocess.run(cmd,capture_output=True,text=True);(OUT/'build.log').write_text(r.stdout+r.stderr)
if r.returncode:raise RuntimeError(r.stderr[-4000:])
runner=OUT/'run.cjs';runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const w=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:w.wasiImport}).then(({instance})=>process.exitCode=w.start(instance));")
r=subprocess.run(['node',str(runner),str(artifact)],capture_output=True,text=True)
record=dict(source_sha256=hashlib.sha256(src.encode()).hexdigest(),tsearch_header_sha256=hashlib.sha256((ROOT/'bot_v36/src/tsearch.h').read_bytes()).hexdigest(),command=cmd,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr)
(OUT/'snapshot-fixture.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
if r.returncode:raise RuntimeError('Snapshot fixture failed')
