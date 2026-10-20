// Include in an official-engine harness. These test simulator invariants, not engine equivalence.
#pragma once
#include "tsearch.h"
#include "runtime_budget.h"
#include <cstdlib>
#include <limits>

inline void agent2_model_fixtures() {
    static_assert(!runtime::fits(21, 0, 22));
    static_assert(runtime::fits(22, 0, 22));
    static_assert(runtime::fits(100, 78, 22));
    static_assert(!runtime::fits(100, 79, 22));
    static_assert(!runtime::fits(100, std::numeric_limits<std::uint64_t>::max(), 22));
    static_assert(runtime::fits(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(), 0));
    auto require = [](bool x, const char* label) {
        if (!x) { std::printf("A2FAIL %s\n", label); std::abort(); }
    };
    auto unit = [](int team, int x, int y, float hp=100.0f) {
        ts::Unit u;
        u.team=static_cast<std::int8_t>(team); u.x=x; u.y=y; u.hp=u.max_hp=hp; u.str=20; u.flags=ts::U_ALIVE;
        return u;
    };
    std::uint64_t tiles[4] = {};
    ts::Map m;
    m.blocked=tiles; m.w=m.h=16; m.stride=2;
    {
        ts::State s;
        s.n=3; s.u[0]=unit(0,4,4); s.u[1]=unit(1,5,4); s.u[2]=unit(1,5,5);
        s.u[1].shield=30; s.u[1].gems=7; s.u[2].gems=11;
        s.u[1].link=2; s.u[2].link=1;
        ts::deal(s,0,1,40);
        require(s.u[1].shield==0 && s.u[1].hp==90,"shield_absorbs_before_health");
        ts::deal(s,0,1,100);
        require(!s.u[1].alive() && !s.u[2].alive() && s.u[0].gems==18,"linked_death_transfers_both_bags_once");
        ts::kill(s,0,2);
        require(s.u[0].gems==18,"death_idempotence");
        ts::stun(s,0); require(s.u[0].stunned==1,"normal_stun");
        s.u[0].stunned=0; s.u[0].flags|=ts::U_STUN_IMMUNE;
        ts::stun(s,0); require(s.u[0].stunned==0,"stun_immunity");
    }
    {
        ts::State s;
        s.n=2; s.u[0]=unit(0,4,4); s.u[1]=unit(1,5,4,200);
        s.u[0].sp[0].kind=ts::K_DAMAGE; s.u[0].sp[0].cd=0; s.u[0].sp[0].range=4; s.u[0].sp[0].value=10;
        s.u[0].sp[1].kind=ts::K_RESET; s.u[0].sp[1].cd=0;
        ts::Policy p; p.root_mode=1;
        ts::act(s,m,0,p);
        const bool two_casts=s.u[0].sp[0].cd>0 && s.u[0].sp[1].cd>0;
        std::printf("A2MODEL one_cast_counterexample two_casts%d enemy_hp%.0f\n",two_casts,s.u[1].hp);
#if NOVA_TS_SIM_FIXES
        require(!two_casts && s.u[1].hp==170,"one_successful_cast_across_damage_and_reset");
#else
        require(two_casts && s.u[1].hp==150,"baseline_counterexample_reproduced");
#endif
    }
#if NOVA_TS_SIM_FIXES
    {
        ts::State s;
        s.n=2; s.u[0]=unit(0,4,4); s.u[1]=unit(1,5,4,200);
        s.u[0].cast_used=true;
        s.u[0].sp[0].kind=ts::K_RESET; s.u[0].sp[0].cd=0;
        require(!ts::cast(s,0,0,-1,0,0),"spent_cast_not_replenished_by_snapshot");
        s.u[0].cast_used=false;
        ts::basic_attack(s,0,1);
        ts::basic_attack(s,0,1);
        require(s.u[1].hp==180,"only_one_successful_basic_before_reset");
        require(ts::cast(s,0,0,-1,0,0),"reset_is_legal_as_first_cast");
        ts::basic_attack(s,0,1);
        require(s.u[1].hp==160,"reset_restores_one_attack");
        require(ts::step(s,m,0,0,0,false),"melee_hit_and_escape_move");
        require(!ts::step(s,m,0,0,0,false),"only_one_ordinary_move");
        s.u[0]=unit(0,4,4); s.u[0].range=3; s.u[0].walked=true; s.u[0].move_left=false;
        const auto before=s.u[1].hp;
        ts::basic_attack(s,0,1);
        require(s.u[1].hp==before,"ranged_walk_excludes_basic");
        s.u[0]=unit(0,4,4); s.u[0].range=3;
        s.u[0].sp[0].kind=ts::K_DAMAGE; s.u[0].sp[0].cd=0; s.u[0].sp[0].range=4;
        s.u[0].sp[0].value=10; s.u[0].sp[0].flags=ts::SF_EXCL_WALK;
        require(ts::cast(s,0,0,1,0,0),"movement_exclusive_cast_from_still_tile");
        ts::basic_attack(s,0,1);
        require(s.u[1].hp==before-30,"movement_exclusive_cast_still_allows_basic");
        require(!ts::step(s,m,0,0,0,false),"exclusive_cast_blocks_walk");
    }
#endif
#if NOVA_TS_RELEVANCE
    {
        std::uint64_t occ[4]={};
        occ[0]=(std::uint64_t{1} << ((4<<3)|4)) | (std::uint64_t{1} << ((4<<3)|5));
        m.observed_occupancy=occ; m.modeled=1; m.origin_x[0]=4; m.origin_y[0]=4;
        ts::State s;
        s.n=1; s.u[0]=unit(0,3,4);
        require(!ts::free_tile(s,m,5,4),"omitted_visible_occupant_remains_blocker");
        require(ts::free_tile(s,m,4,4),"modeled_unit_can_vacate_original_tile");
        require(!ts::free_tile(s,m,-1,4),"outside_map_blocked");
    }
#endif
    std::puts("A2FIXTURES PASS (simulator invariants only; engine differential pending)");
}
