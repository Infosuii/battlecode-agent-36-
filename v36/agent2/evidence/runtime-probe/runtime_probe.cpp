// Diagnostic bot only. Synthetic scene rollout fuel, not match-strength evidence.
#include "tsearch.h"
#include <cstdio>

int main() {
    while (true) {
        if (bc::is_draft()) {
            constexpr std::array picks{bc::champion_draft::varg,bc::champion_draft::ren,bc::champion_draft::stellar,
                bc::champion_draft::marcella,bc::champion_draft::ryth};
            const auto i=bc::draft::current_draft(bc::team::ally).size();
            if (i<picks.size()) bc::draft::pick(picks[i]);
        } else {
            const unsigned round=bc::round_number();
            if (round>=1 && round<=14) {
                ts::State s;
                s.n=2+static_cast<int>((round-1)%7);
                const int horizon=2+static_cast<int>((round-1)/7);
                for (int i=0;i<s.n;++i) {
                    auto& u=s.u[i];
                    u.flags=ts::U_ALIVE;u.team=i&1;u.x=12+(i&1)*2;u.y=12+i/2;
                    u.hp=u.max_hp=300.0f+50.0f*i;u.str=30.0f+5.0f*i;u.range=i%3==0?3:1;
                    u.sp[0].kind=ts::K_DAMAGE;u.sp[0].cd=0;u.sp[0].range=4;u.sp[0].value=50;
                    u.sp[1].kind=u.range==1?ts::K_RESET:ts::K_SHIELD;u.sp[1].cd=0;u.sp[1].value=40;
                }
                std::uint64_t blocked[16]={};
                ts::Map map;map.blocked=blocked;map.w=map.h=32;map.stride=4;
                map.home_x[0]=map.home_y[0]=0;map.home_x[1]=map.home_y[1]=31;
                std::uint64_t cost[2]={};float value[2]={};
                bool suspended=false;
                for (int plan=0;plan<2;++plan) {
                    const auto start=bc::fuel_remaining();
                    ts::State c=s;ts::Policy p;p.root_mode=plan?2:1;
                    ts::act(c,map,0,p);ts::rollout(c,map,p,horizon);value[plan]=ts::evaluate(c,ts::Weights{});
                    const auto end=bc::fuel_remaining();
                    suspended|=bc::round_number()!=round;
                    cost[plan]=start>=end && !suspended?start-end:0;
                }
                std::printf("MODELPROBE r%u n%d h%d fight%llu run%llu pair%llu suspended%d vf%.2f vr%.2f\n",round,s.n,horizon,
                    static_cast<unsigned long long>(cost[0]),static_cast<unsigned long long>(cost[1]),
                    static_cast<unsigned long long>(cost[0]+cost[1]),suspended,value[0],value[1]);
            }
        }
        bc::end_turn();
    }
}
