
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
bool Bot::ts_snapshot(ts::State& s, ts::Map& m, const EnemyRec** eref) {
    runtime::Probe probe(runtime::Stage::Snapshot);
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision = ts::DecisionInfo{};
#endif
    m.blocked = w.static_blocked.ptr();
    m.w = w.width;
    m.h = w.height;
    m.stride = static_cast<int>(bc::detail::layout().chunk_stride);
    if (ts_home[0].x < 0) {
        auto centre = [](const bc::bitboard& b) {
            long sx = 0, sy = 0, n = 0;
            for (vec2 p : b.positions()) sx += p.x, sy += p.y, ++n;
            return n ? vec2{static_cast<int>(sx / n), static_cast<int>(sy / n)} : vec2{0, 0};
        };
        ts_home[0] = centre(w.ally_spawn);
        ts_home[1] = centre(w.enemy_spawn);
    }
    m.home_x[0] = static_cast<std::int16_t>(ts_home[0].x);
    m.home_y[0] = static_cast<std::int16_t>(ts_home[0].y);
    m.home_x[1] = static_cast<std::int16_t>(ts_home[1].x);
    m.home_y[1] = static_cast<std::int16_t>(ts_home[1].y);

    // Unit 0 is us; then allies within 7 and enemies within 8 (nearest first by insertion order).
    const int my_cd[2] = {cooldown[0], cooldown[1]};
    fill_unit(s.u[0], 0, type, pos, hp, max_hp, strength, focus, range, gems, my_id, false, my_cd);
#if NOVA_TS_SIM_FIXES
    s.u[0].cast_used = cast_done;
    s.u[0].walked = walked;
    s.u[0].move_left = bc::self().can_move();
    s.u[0].attack_left = bc::self().can_attack();
#endif
    s.n = 1;
    int ci_idx[2][2] = {{-1, -1}, {-1, -1}}; // [team][cassi, issac]
    auto note_ci = [&](int team, bc::champion_type t, int idx) {
        if (t == bc::champion_type::cassi) ci_idx[team][0] = idx;
        if (t == bc::champion_type::issac) ci_idx[team][1] = idx;
    };
    note_ci(0, type, 0);
#if NOVA_TS_RELEVANCE
    // Stable relevance selection; a linked observed partner enters as one dependency group.
    struct Candidate { const AllyRec* ally; const EnemyRec* enemy; std::uint32_t id; bc::champion_type type; int team, distance; float priority; bool chosen; };
    Candidate cs[24];
    int count = 0;
    auto add = [&](const AllyRec* a, const EnemyRec* e) {
        const vec2 p = a ? a->pos : e->pos;
        const int dd = cheb(p, pos), tm = a ? 0 : 1;
        const auto ct = a ? a->type : e->type;
        const bool linked = ct == bc::champion_type::cassi || ct == bc::champion_type::issac;
        if (dd > (tm ? 8 : 7) && !linked) return;
        if (dd <= (tm ? 8 : 7)) {
            if (tm) ++ts::last_decision.eligible_enemies;
            else ++ts::last_decision.eligible_allies;
        }
        if (count >= 24) { ts::last_decision.omitted_enemy |= tm != 0; return; }
        const float ehp = a ? a->hp : e->hp;
        const float power = a ? a->strength : e->strength;
        const float foc = a ? a->focus : e->focus;
        const int reach = a ? a->range : e->range;
        // Close reachable burst dominates; distance then vulnerability break broad threat bands.
        const float burst = power + foc;
        const float pr = (tm ? 1000.0f : 0.0f) + (dd <= reach + 2 ? 2000.0f + burst : 0.0f)
            + (dd <= range + 1 ? 400.0f : 0.0f) - 100.0f * dd + 100.0f / std::max(1.0f, ehp);
        cs[count++] = Candidate{a, e, a ? a->id : e->id, ct, tm, dd, pr, false};
    };
    ts::last_decision.eligible_allies = 1;
    for (const auto& a : w.allies) if (a.hp > 0) add(&a, nullptr);
    for (const auto& e : w.enemies) if (e.hp > 0) add(nullptr, &e);
    std::sort(cs, cs + count, [](const Candidate& a, const Candidate& b) {
        return a.priority != b.priority ? a.priority > b.priority : a.id < b.id;
    });
    const int cap = std::clamp(NOVA_TS_MAX_UNITS, 2, ts::kMaxUnits);
    ts::last_decision.selected[0] = my_id;
    auto partner = [&](int i) {
        const auto ct = cs[i].type;
        if (ct != bc::champion_type::cassi && ct != bc::champion_type::issac) return -2;
        const auto wanted = ct == bc::champion_type::cassi ? bc::champion_type::issac : bc::champion_type::cassi;
        if (cs[i].team == 0 && type == wanted) return -1; // self is already retained
        for (int j = 0; j < count; ++j) if (cs[j].team == cs[i].team && cs[j].type == wanted) return j;
        return -3; // hidden/outside observation: never fabricate a partner
    };
    auto take = [&](int i) {
        Candidate& c = cs[i];
        if (c.chosen || s.n >= cap) return;
        if (c.ally) {
            const auto& a = *c.ally;
            d::champion_info info{};
            if (d::sdk_champion(a.id, &info) != 0) return;
            std::uint32_t g = 0;
            d::sdk_champion_gems(a.id, &g);
            fill_unit(s.u[s.n], 0, a.type, a.pos, a.hp, a.max_hp, a.strength, a.focus, a.range, static_cast<int>(g), a.id,
                (static_cast<std::uint32_t>(info.state) & (1u << 5)) != 0, nullptr);
        } else {
            const auto& e = *c.enemy;
            fill_unit(s.u[s.n], 1, e.type, e.pos, e.hp, e.max_hp, e.strength, e.focus, e.range, e.gems, e.id,
                (e.state & (1u << 5)) != 0, nullptr);
            if (e.tributing() || e.recalling()) s.u[s.n].flags |= ts::U_ACTED;
            eref[s.n] = &e;
        }
        note_ci(c.team, c.type, s.n);
        ts::last_decision.selected[s.n] = c.id;
        ++s.n;
        c.chosen = true;
    };
    // Self's observed linked body has first claim on a slot, even beyond the usual radius.
    if (type == bc::champion_type::cassi || type == bc::champion_type::issac) {
        bool found = false;
        for (int i = 0; i < count; ++i) if (partner(i) == -1) { take(i); found = cs[i].chosen; }
        ts::last_decision.incomplete_link |= !found;
    }
    for (int i = 0; i < count; ++i) {
        if (cs[i].chosen || cs[i].distance > (cs[i].team ? 8 : 7)) continue;
        const int p = partner(i);
        const int need = 1 + (p >= 0 && !cs[p].chosen ? 1 : 0);
        if (s.n + need > cap) continue;
        take(i);
        if (p >= 0) take(p);
        if (cs[i].chosen && (p == -3 || (p >= 0 && !cs[p].chosen))) ts::last_decision.incomplete_link = true;
    }
    int enemies = 0, near_d = 99;
    for (int i = 0; i < count; ++i) {
        const Candidate& c = cs[i];
        if (!c.chosen && c.distance <= (c.team ? 8 : 7)) {
            ++ts::last_decision.dropped;
            ts::last_decision.omitted_enemy |= c.team != 0;
#if NOVA_RUNTIME_TELEMETRY
            std::printf("TSDROP r%d id%u team%d d%d reasoncapacity\n", round, c.id, c.team, c.distance);
#endif
        }
        if (c.chosen && c.team) { ++enemies; near_d = std::min(near_d, c.distance); }
    }
    m.observed_occupancy = w.unit_tiles.ptr();
    m.modeled = s.n;
    for (int i = 0; i < s.n; ++i) { m.origin_x[i] = s.u[i].x; m.origin_y[i] = s.u[i].y; }
#else
    for (const auto& a : w.allies) {
        if (s.n >= 4 || cheb(a.pos, pos) > 7) continue;
        d::champion_info info{};
        if (d::sdk_champion(a.id, &info) != 0) continue;
        std::uint32_t g = 0;
        d::sdk_champion_gems(a.id, &g);
        const bool acted = (static_cast<std::uint32_t>(info.state) & (1u << 5)) != 0;
        fill_unit(s.u[s.n], 0, a.type, a.pos, a.hp, a.max_hp, a.strength, a.focus, a.range, static_cast<int>(g), a.id, acted, nullptr);
        note_ci(0, a.type, s.n);
        ++s.n;
    }
    int enemies = 0, near_d = 99;
    for (const auto& e : w.enemies) {
        if (s.n >= ts::kMaxUnits) break;
        const int dd = cheb(e.pos, pos);
        if (dd > 8 || e.hp <= 0) continue;
        near_d = std::min(near_d, dd);
        const bool acted = (e.state & (1u << 5)) != 0;
        fill_unit(s.u[s.n], 1, e.type, e.pos, e.hp, e.max_hp, e.strength, e.focus, e.range, e.gems, e.id, acted, nullptr);
        if (e.tributing() || e.recalling()) s.u[s.n].flags |= ts::U_ACTED; // channelling: it will not fight back now
        note_ci(1, e.type, s.n);
        eref[s.n] = &e;
        ++s.n;
        ++enemies;
    }
#endif
#if NOVA_TS_RELEVANCE || NOVA_TS_SAFETY || NOVA_RUNTIME_TELEMETRY
    ts::last_decision.retained = s.n;
    if (!NOVA_TS_RELEVANCE) {
        ts::last_decision.eligible_allies = 1;
        for (const auto& a : w.allies) ts::last_decision.eligible_allies += a.hp > 0 && cheb(a.pos, pos) <= 7;
        for (const auto& e : w.enemies) ts::last_decision.eligible_enemies += e.hp > 0 && cheb(e.pos, pos) <= 8;
        ts::last_decision.omitted_enemy = ts::last_decision.eligible_enemies > enemies;
        ts::last_decision.dropped = ts::last_decision.eligible_allies + ts::last_decision.eligible_enemies - s.n;
        ts::last_decision.selected[0] = my_id;
        for (int i = 1; i < s.n; ++i) if (eref[i]) ts::last_decision.selected[i] = eref[i]->id;
        ts::last_decision.incomplete_link = (ci_idx[0][0] >= 0) != (ci_idx[0][1] >= 0)
            || (ci_idx[1][0] >= 0) != (ci_idx[1][1] >= 0);
    }
    // Conservative risk guard: a nearby dangerous group may wake during the comparison.
    for (const auto& c : w.camps)
        if (!c.dead && c.hp > 0 && c.strength > 0 && cheb(c.pos, pos) <= c.range + 2) ts::last_decision.camp_risk = true;
#endif
    if (!enemies || near_d > tune::kTsRange) return false;
    s.nc = 0;
    if (tune::kTsCamps)
        for (const auto& g : w.groups) {
            if (g.state != GroupState::alive) continue;
            for (int ci : g.camps) {
                const CampRec& c = w.camps[ci];
                if (c.dead || c.hp <= 0 || c.strength <= 0 || cheb(c.pos, pos) > 7 || s.nc >= ts::kMaxCamps) continue;
                s.camps[s.nc++] = ts::Camp{c.strength, static_cast<std::int16_t>(c.pos.x), static_cast<std::int16_t>(c.pos.y),
                    static_cast<std::int8_t>(c.range)};
            }
        }
    for (int t = 0; t < 2; ++t)
        if (ci_idx[t][0] >= 0 && ci_idx[t][1] >= 0) {
            s.u[ci_idx[t][0]].link = static_cast<std::int8_t>(ci_idx[t][1]);
            s.u[ci_idx[t][1]].link = static_cast<std::int8_t>(ci_idx[t][0]);
        }
    return true;
}


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
