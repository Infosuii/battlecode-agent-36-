// Controlled mechanics fixture; never a competitive opponent or release bot.
#include <cambc.h>
#include <algorithm>
#include <cstdio>
namespace bc=cambc;
inline constexpr int probe_mode=0; // 0 Ren, 1 Varg splash, 2 idle
int main() {
    int hit_round=-1;
    while(true) {
        try {
            if(bc::is_draft()) {
                const bc::champion_draft picks[]={bc::champion_draft::ren,bc::champion_draft::varg,bc::champion_draft::tavi};
                bc::draft::pick(picks[bc::draft::current_draft(bc::team::ally).size()]);
            } else {
                auto& me=bc::self();const int round=static_cast<int>(bc::round_number());
                const auto active=probe_mode==0 ? bc::champion_type::ren : bc::champion_type::varg;
                if(probe_mode!=2 && me.champion_name()==active && round<=30 && !me.state().is_dead()) {
                    auto pos=me.position();
                    std::printf("CAMP_PROBE R%d type=%d pos=%d,%d hp=%.3f recalling=%d sleep_time=%d\n",
                        round,static_cast<int>(active),pos.x,pos.y,me.health(),
                        me.state().contains(bc::champion_state::recalling),bc::constants::camp_sleep_time);
                    for(const auto& c:bc::vision::camps()) {
                        auto p=c.position();
                        if(p.x<10 && p.y<10)std::printf("CAMP_STATE R%d x=%d y=%d state=%u hp=%.3f max=%.3f\n",
                            round,p.x,p.y,static_cast<unsigned>(c.state()),c.health(),c.max_health());
                    }
                    if(me.state().contains(bc::champion_state::recalling)) {
                    } else if(round==20 && hit_round>0) {
                        me.recall();std::printf("RECALL_STARTED R%d\n",round);
                    } else if(hit_round<0) {
                        if(pos==bc::vec2{5,2}) {
                            for(const auto& c:bc::vision::camps())if(c.position()==bc::vec2{6,2}) {
                                me.attack(c);hit_round=round;std::printf("HIT R%d\n",round);break;
                            }
                        } else me.move_towards({5,2});
                    } else if(round>=hit_round+4 && round<20) me.move_towards({1,4});
                }
            }
        } catch(const std::exception& e) {std::printf("PROBE_ERROR %s\n",e.what());}
        bc::end_turn();
    }
}
