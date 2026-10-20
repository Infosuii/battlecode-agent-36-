
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
inline constexpr std::uint8_t kMagic = 0xA5;

inline int map_class(int w, int h, int team_size) {
    const int t = std::clamp(team_size, 3, 5);
    return (t - 3) * 2 + (w * h > 2000 ? 1 : 0);
}
inline bool is_caster(bc::champion_draft c) {
    return c == bc::champion_draft::cassi_and_issac || c == bc::champion_draft::stellar ||
        c == bc::champion_draft::prism || c == bc::champion_draft::tavi || c == bc::champion_draft::willow;
}

// Draft side: record what the champions need to know (called on every draft turn).
// max_group = gem value of the biggest camp group on the map (from map facts; 0 = unknown), so
// champions can tell strip-carry maps from sparse bank-early maps (NOVA_BANK_FIX).
// flags: bit0 = race map (dense pool: total gems/area >= NOVA_RACE_DENS), bit1 = monolith map.
inline void record_draft(unsigned team_size, int max_group = 0, unsigned flags = 0) {
    if (!APEX_BRAIN) return;
    int casters = 0;
    for (auto c : bc::draft::current_draft(team::enemy)) casters += is_caster(c);
    const int cls = map_class(bc::map::width(), bc::map::height(), static_cast<int>(team_size));
    const std::uint8_t cl8 = static_cast<std::uint8_t>(cls), ts8 = static_cast<std::uint8_t>(team_size),
        cs8 = static_cast<std::uint8_t>(std::min(casters, 255)), mg8 = static_cast<std::uint8_t>(std::min(std::max(max_group, 0), 255)),
        fl8 = static_cast<std::uint8_t>(flags & 3);
#if NOVA_STRICT_MAP_CACHE && NOVA_STRICT_MAP_ID
    if (bc::constants::draft_memory_bytes >= 16) {
        // Versioned identity extension; the existing six strategy bytes retain
        // their exact layout and meaning. Short SDK writes leave old bytes, so
        // failed facts must explicitly invalidate a previous cache extension.
        std::array<std::uint8_t, 16> mem{kMagic, ts8, cs8, cl8, mg8, fl8};
        if (!nova_record_map_cache(mem, team_size)) std::fill(mem.begin() + 6, mem.end(), 0);
        bc::draft_memory = mem;
        return;
    }
#endif
    if (bc::constants::draft_memory_bytes >= 6) {
        const std::array<std::uint8_t, 6> mem{kMagic, ts8, cs8, cl8, mg8, fl8};
        bc::draft_memory = mem;
    } else if (bc::constants::draft_memory_bytes >= 5) {
        const std::array<std::uint8_t, 5> mem{kMagic, ts8, cs8, cl8, mg8};
        bc::draft_memory = mem;
    } else {
        const std::array<std::uint8_t, 4> mem{kMagic, ts8, cs8, cl8};
        bc::draft_memory = mem;
    }
}


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
