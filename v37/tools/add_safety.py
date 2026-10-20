"""Use current visible individual camp state in recall/recovery danger geometry."""
import shutil
from lab import ROOT,write
def main():
    c=ROOT/'candidate/src/common.h';c.write_text('#ifndef V37_CAMP_STATE\n#define V37_CAMP_STATE 0\n#endif\n'+c.read_text(),newline='\n')
    p=ROOT/'candidate/src/world.h';s=p.read_text().replace('    bool dead = false;',
        '#if V37_CAMP_STATE\n    int awake_state = -1;\n    int awake_round = -1;\n#endif\n    bool dead = false;')
    p.write_text(s,newline='\n')
    p=ROOT/'candidate/src/world.cpp';s=p.read_text()
    needle='                    camps[i].hp = cs.health;'
    s=s.replace(needle,'''#if V37_CAMP_STATE
                    std::uint32_t observed_state=0;
                    if (camp_visible[i] && d::sdk_camp_state(static_cast<std::uint32_t>(static_cast<bc::handle_camp_t>(camps[i].camp)), &observed_state)==0) {
                        camps[i].awake_state=static_cast<int>(observed_state);
                        camps[i].awake_round=round;
                    }
#endif
'''+needle)
    needle='            if (!c.dead && c.strength > 0 && (c.hp < c.max_hp || awake)) hot_zone.draw_chebyshev(c.pos, static_cast<float>(c.range));'
    assert needle in s
    s=s.replace(needle,'''            bool dangerous = c.hp < c.max_hp || awake;
#if V37_CAMP_STATE
            // Only current visible per-member state can clear an inferred danger.
            // Stale/unknown observations keep the original conservative fallback.
            if(c.awake_round==round) dangerous=c.awake_state==1;
#endif
            if (!c.dead && c.strength > 0 && dangerous) hot_zone.draw_chebyshev(c.pos, static_cast<float>(c.range));''')
    # awake_camps_near must agree with hot_zone, including undamaged awake members.
    old='        if (!c.dead && c.strength > 0 && c.hp < c.max_hp && cheb(c.pos, q) <= c.range) out.push_back(static_cast<int>(ci));'
    s=s.replace(old,'''        bool awake = c.hp < c.max_hp;
#if V37_CAMP_STATE
        if(c.awake_round>=0 && c.awake_round==groups[static_cast<std::size_t>(camp_group[ci])].seen_round) awake=c.awake_state==1;
#endif
        if (!c.dead && c.strength > 0 && awake && cheb(c.pos, q) <= c.range) out.push_back(static_cast<int>(ci));''')
    p.write_text(s,newline='\n')
    dst=ROOT/'safe-state';shutil.copytree(ROOT/'candidate',dst)
    p=dst/'src/common.h';p.write_text('#define V37_CAMP_STATE 1\n'+p.read_text(),newline='\n')
    write(ROOT/'registry/safe-state-v1.json',dict(hypothesis='Recall/recovery positions should use visible individual awake state rather than damage proxies; sleeping damaged camps and healthy awakened members differ.',
        action_gate='Current-engine actual attack/wake/sleep fixtures passed; legacy fallback on unknown/stale states. Camp state enums taken from SDK 3.0.2.',
        independent_switch='V37_CAMP_STATE',default=0,screen='24 fresh dev and 35 consumed official development groups, both sides, win points>=0.53 and secured margin>0; no faults or critical regressions.',
        multiple_comparisons='Second policy; any confirmation must use Bonferroni-adjusted 97.5% intervals for two screened hypotheses, each with a distinct frozen final test. No optional extension.',
        remaining='This is current-observation recovery geometry only, not a full next-phase damage/arrival predictor. No expanded simulator authority.'))
if __name__=='__main__':main()
