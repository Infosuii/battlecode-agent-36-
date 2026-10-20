"""Actual record_draft writer against SDK-equivalent short-write semantics."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1];OUT=ROOT/'agent3/evidence';OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'bot_v36/src/strategy.h').read_text()
writer=source[source.index('inline constexpr std::uint8_t kMagic'):source.index('struct State {')]
preamble=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "draft_map_cache.h"
#define APEX_BRAIN 1
enum class team{enemy};
namespace bc{
enum class champion_draft{cassi_and_issac,stellar,prism,tavi,willow,other};
namespace constants{unsigned draft_memory_bytes=32;}
namespace draft{std::vector<champion_draft> current_draft(team){return {champion_draft::stellar,champion_draft::other,champion_draft::tavi};}}
namespace map{int width(){return 50;}int height(){return 50;}}
struct Memory{
 std::vector<std::uint8_t> engine=std::vector<std::uint8_t>(64,0xA7);
 unsigned submitted=0,calls=0;
 template<std::size_t N>Memory&operator=(const std::array<std::uint8_t,N>&a){
  submitted=N;++calls;std::copy_n(a.begin(),std::min<unsigned>(N,constants::draft_memory_bytes),engine.begin());return *this;
 }
};Memory draft_memory;
}
bool helper_success=true;unsigned helper_calls=0;
bool nova_record_map_cache(std::array<std::uint8_t,16>&mem,unsigned picks){
 ++helper_calls;if(!helper_success)return false;return nova_cache::encode(mem,50,50,picks,9,13);
}
namespace brain{
'''
tests=r'''
}
int main(){unsigned checks=0;
 constexpr bool active=NOVA_STRICT_MAP_CACHE && NOVA_STRICT_MAP_ID;
 for(unsigned capacity:{4u,5u,6u,15u,16u,32u})for(bool success:{false,true})for(unsigned picks:{3u,4u,5u}){
  bc::constants::draft_memory_bytes=capacity;bc::draft_memory=bc::Memory{};helper_success=success;helper_calls=0;
  brain::record_draft(picks,1000,7);
  const std::array<std::uint8_t,6>prefix{0xA5,std::uint8_t(picks),2,std::uint8_t((picks-3)*2+1),255,3};
  assert(std::equal(prefix.begin(),prefix.begin()+std::min<unsigned>(capacity,6),bc::draft_memory.engine.begin()));
  const unsigned want=active&&capacity>=16?16:capacity>=6?6:capacity>=5?5:4;
  assert(bc::draft_memory.calls==1&&bc::draft_memory.submitted==want);
  assert(helper_calls==unsigned(active&&capacity>=16));
  if(active&&capacity>=16){
   const int decoded=nova_cache::decode(bc::draft_memory.engine,50,50,picks,9);
   assert(decoded==(success?13:-2));
   if(!success)assert(std::all_of(bc::draft_memory.engine.begin()+6,bc::draft_memory.engine.begin()+16,[](auto b){return b==0;}));
  }else assert(bc::draft_memory.engine[6]==0xA7);
  ++checks;
 }
 if(active){
  bc::constants::draft_memory_bytes=32;bc::draft_memory=bc::Memory{};helper_success=true;
  brain::record_draft(4,100,0);assert(nova_cache::decode(bc::draft_memory.engine,50,50,4,9)==13);++checks;
  // Same prefix, then unavailable facts: SDK short writes would retain the old
  // valid cache. The production writer must submit all16 and clear extension.
  helper_success=false;brain::record_draft(4,100,0);
  assert(bc::draft_memory.submitted==16&&nova_cache::decode(bc::draft_memory.engine,50,50,4,9)==-2);++checks;
 }
 std::printf("{\"cache\":%d,\"strict\":%d,\"writer_checks\":%u}\n",NOVA_STRICT_MAP_CACHE,NOVA_STRICT_MAP_ID,checks);
}
'''
cpp=OUT/'cache_writer_fixture.cpp';cpp.write_text(preamble+writer+tests)
sdk=next((ROOT.parent/'development').glob('wasi-sdk-*/bin/clang++.exe')).parent.parent
runner=OUT/'run_cache_writer.cjs'
runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const wasi=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>process.exitCode=wasi.start(instance));")
results=[]
for cache,strict in [(1,1),(0,1),(1,0)]:
 wasm=OUT/f'cache_writer_{cache}_{strict}.wasm'
 cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fno-exceptions',f'-DNOVA_STRICT_MAP_CACHE={cache}',f'-DNOVA_STRICT_MAP_ID={strict}','-I'+str(ROOT/'bot_v36/src'),str(cpp),'-o',str(wasm)]
 r=subprocess.run(cmd,capture_output=True,text=True)
 if r.returncode:raise RuntimeError(r.stderr)
 r=subprocess.run(['node',str(runner),str(wasm)],capture_output=True,text=True)
 results.append(dict(cache=cache,strict=strict,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr,command=cmd))
evidence=dict(strategy_sha256=hashlib.sha256((ROOT/'bot_v36/src/strategy.h').read_bytes()).hexdigest(),results=results,scope='Actual production writer and actual cache codec; controlled encoder result and SDK-documented short-write semantics. Native whole-bot cache gate is separate.')
(OUT/'cache-writer-fixture.json').write_text(json.dumps(evidence,indent=2));print(json.dumps(evidence,indent=2));assert all(r['returncode']==0 for r in results)
