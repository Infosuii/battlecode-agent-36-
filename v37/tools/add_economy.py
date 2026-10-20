"""Scoped linked-body emergency policy, default OFF until fresh match promotion."""
import shutil
from lab import ROOT,WORK,write,sha
def main():
    p=ROOT/'candidate/src/bot.cpp';s=p.read_text()
    anchor='    const bool rich = gold >= tune::kShopGold;'
    change='''#if V37_LINK_EMERGENCY
    // Shared death, distinct health pools. A visible partner below its own
    // emergency fraction can kill both bodies; a claim or assumed arrival is irrelevant.
    if (ci_body) {
        const auto partner = type == bc::champion_type::cassi ? bc::champion_type::issac : bc::champion_type::cassi;
        for (const auto& a : w.allies)
            if (a.type == partner && a.hp < a.max_hp * tune::kRetreatHp) low = true;
    }
#endif
'''
    assert s.count(anchor)==1;s=s.replace(anchor,change+anchor)
    p.write_text(s,newline='\n')
    common=ROOT/'candidate/src/common.h'
    common.write_text('#ifndef V37_LINK_EMERGENCY\n#define V37_LINK_EMERGENCY 0\n#endif\n'+common.read_text(),newline='\n')
    dest=ROOT/'econ-link';shutil.copytree(ROOT/'candidate',dest)
    q=dest/'src/common.h';q.write_text('#define V37_LINK_EMERGENCY 1\n'+q.read_text(),newline='\n')
    # Reuse the inherited actual-policy fixture, replacing its old extracted function.
    template=(WORK/'v36-execution/agent3/evidence/economy_policy_fixture.cpp').read_text()
    start=template.index('bool Bot::go_home_if_needed()');end=template.index('\nint main()',start)
    policy=s[s.index('bool Bot::go_home_if_needed()'):s.index('bool Bot::tribute_if_needed()')]
    template=template[:start]+policy+'\n'+template[end:]
    template=template.replace('#define NOVA_CI_LINK 1','#define NOVA_CI_LINK 0\n#define V37_LINK_EMERGENCY 1')
    template=template.replace('float hp=100;vec2 pos;','float hp=100;vec2 pos;float max_hp=100;')
    extra='''
 {reset();Bot b;b.type=bc::champion_type::cassi;b.max_hp=1000;b.hp=1000;b.w.allies.push_back({bc::champion_type::issac,15,{4,0},100});assert(!b.go_home_if_needed());++checks;}
 {reset();Bot b;b.type=bc::champion_type::issac;b.max_hp=100;b.hp=100;b.w.allies.push_back({bc::champion_type::cassi,100,{4,0},1000});assert(b.go_home_if_needed()&&bc::self_.recalls==1);++checks;}
'''
    template=template.replace(' std::printf("{',extra+' std::printf("{')
    out=ROOT/'fixtures/economy.cpp';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(template,newline='\n')
    shutil.copyfile(WORK/'v36-execution/agent3/evidence/run_economy_fixture.cjs',ROOT/'tools/run_fixture.cjs')
    write(ROOT/'registry/econ-link-v1.json',dict(hypothesis='Visible critical linked partner triggers recovery before shared death; health fraction uses partner max HP.',
        motivation='248 camp-attributed physical deaths and 11353 lost gems across baseline official diagnostics; linked-wallet/shared-death mechanics verified by current engine oracle.',
        switch='V37_LINK_EMERGENCY',default=0,correctness=['actual extracted policy fixture','healthy rich capture preserved','healthy Tavi field shopping preserved','low-health emergency and failed recall fallback preserved','unequal max health partner tests'],
        action_gate='ON must change actions in registered full matches; fewer deaths alone cannot promote.',
        screen='24 new generated dev groups + 35 official groups, both sides; pass win points >=0.53 and secured margin >0 with no new faults.',
        confirmation='fresh-v37-1 protocol; no extension after failed screen; only one policy tested',
        limitations='Partner can be outside vision. Both recalling may interrupt profitable work; needs secured-resource evidence.'))
if __name__=='__main__':main()
