"""Add compile-time-disabled decision telemetry to the exact selected source.

Trace builds are explicitly not production measurements. Unselected outcomes stay absent.
"""
from lab import ROOT, write, sha
def main():
    path=ROOT/'candidate/src/bot.cpp'
    source=path.read_text()
    assert 'V37_TRACE' not in source
    source='#ifndef V37_TRACE\n#define V37_TRACE 0\n#endif\n'+source
    start=source.index('int Bot::score_groups(')
    end=source.index('\nvoid Bot::refresh_follow_ups()',start)
    f=source[start:end]
    f=f.replace('    PROF(score);', '''    PROF(score);
#if V37_TRACE
    static unsigned trace_sequence = 0;
    const unsigned trace_call = ++trace_sequence;
    const bool tracing = round % 10 == 0;
    if (tracing) {
        std::printf("V37D,%d,%u,%u,%d,%d,%d,%.7g,%.7g,%d,%d,%d,%zu,%zu\\n",
            round,my_id,trace_call,static_cast<int>(type),pos.x,pos.y,hp,max_hp,gems,gold,target_group,w.allies.size(),w.enemies.size());
        for (const auto& a:w.allies) std::printf("V37A,%d,%u,%u,%u,%d,%d,%.7g,%.7g,%d\\n",round,my_id,trace_call,a.id,a.pos.x,a.pos.y,a.hp,a.max_hp,a.range);
        for (const auto& e:w.enemies) std::printf("V37E,%d,%u,%u,%d,%d,%.7g,%d\\n",round,my_id,trace_call,e.pos.x,e.pos.y,e.hp,e.range);
    }
#endif''')
    f=f.replace('        const int dist = order[oi].first;', '''        const int dist = order[oi].first;
#if V37_TRACE
        const char* trace_reason="excluded_unclassified";
        auto trace_exit=[&](void*) {
            const int tgi=order[oi].second;
            const auto& tg=w.groups[static_cast<std::size_t>(tgi)];
            if(tracing) std::printf("V37C,%d,%u,%u,%d,%s,%d,%d,%u,%u,%d,%.7g\\n",round,my_id,trace_call,tgi,trace_reason,dist,static_cast<int>(tg.state),tg.gems,tg.gold,round-tg.seen_round,tg.hp_left(w.camps));
        };
        std::unique_ptr<void,decltype(trace_exit)> trace_guard(this,trace_exit);
#endif''')
    # Preserve every original branch and all work caps. Reason tags are descriptive only.
    f=f.replace('                continue;\n            }\n        }\n        ++evaluated;', '''#if V37_TRACE
                trace_reason="bound_pruned";
#endif
                continue;
            }
        }
        ++evaluated;''')
    f=f.replace('        if (score > best_score) {', '''#if V37_TRACE
        trace_reason="eligible";
        if(tracing) std::printf("V37F,%d,%u,%u,%zu,%.7g,%.7g,%.7g,%.7g,%d,%d,%d,%d,%d,%d,%d,%d,%d\\n",round,my_id,trace_call,gi,score,value,cost,drain,approach,wait,participants,need,lower,higher,static_cast<int>(siege),static_cast<int>(initiating),static_cast<int>(solo_job));
#endif
        if (score > best_score) {''')
    f=f.replace('    return best;','''#if V37_TRACE
    if(tracing) std::printf("V37S,%d,%u,%u,%d,%.7g,%d,%d,%zu\\n",round,my_id,trace_call,best,best_score,best_need,n_order,w.groups.size());
#endif
    return best;''')
    source=source[:start]+f+source[end:]
    source=source.replace('    PROF_CHECK("decide");','''#if V37_TRACE
    std::printf("V37M,%d,%u,%s,%d,%d,%d,%.7g,%d,%d,%d\\n",round,my_id,mode,target_group,pos.x,pos.y,hp,gems,gold,heading_home?1:0);
#endif
    PROF_CHECK("decide");''')
    path.write_text(source,newline='\n')
    # A separate named instrumented copy, never mistaken for source-default production.
    import shutil
    trace=ROOT/'trace';shutil.copytree(ROOT/'candidate',trace)
    p=trace/'src/common.h';p.write_text('#define V37_TRACE 1\n'+p.read_text(),newline='\n')
    write(ROOT/'evidence/trace-contract.json',dict(schema=1,sampling='scorer every 10th round; live decision-end mode each round',
        source_sha256=sha(path),default=0,production_behavior_intended='unchanged',
        missing=['body shield/status details','camp individual awake state','full excluded-candidate reason classification','pre-order avoided/unreachable/cap-excluded candidate rows'],
        limitations='Preparation dataset only until completeness and instrumentation parity gates pass. Future outcomes never emitted as inputs.'))
if __name__=='__main__':main()
