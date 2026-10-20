"""Reduce trace formatting fuel after the first instrumented audit failed."""
import shutil
from lab import ROOT,write,sha
def main():
    source=(ROOT/'candidate/src/bot.cpp').read_text()
    source=source.replace('    const bool tracing = round % 10 == 0;',
        '    static int traced_round=-1;\n    const bool tracing = round % 25 == 0 && traced_round!=round;\n    if(tracing) traced_round=round;')
    # Floating printf costs enough WASM fuel to materially perturb matches. Integer scaling is explicit.
    source=source.replace('%.7g','%d')
    source=source.replace('pos.x,pos.y,hp,max_hp,gems,gold,target_group',
        'pos.x,pos.y,static_cast<int>(hp*100),static_cast<int>(max_hp*100),gems,gold,target_group')
    source=source.replace('a.pos.x,a.pos.y,a.hp,a.max_hp,a.range',
        'a.pos.x,a.pos.y,static_cast<int>(a.hp*100),static_cast<int>(a.max_hp*100),a.range')
    source=source.replace('e.pos.x,e.pos.y,e.hp,e.range','e.pos.x,e.pos.y,static_cast<int>(e.hp*100),e.range')
    source=source.replace('round-tg.seen_round,tg.hp_left(w.camps)',
        'round-tg.seen_round,static_cast<int>(tg.hp_left(w.camps)*100)')
    source=source.replace('gi,score,value,cost,drain,approach',
        'gi,static_cast<int>(score*10000),static_cast<int>(value*100),static_cast<int>(cost*100),static_cast<int>(drain*100),approach')
    source=source.replace('best,best_score,best_need,n_order',
        'best,static_cast<int>(best_score*10000),best_need,n_order')
    source=source.replace('pos.x,pos.y,hp,gems,gold,heading_home',
        'pos.x,pos.y,static_cast<int>(hp*100),gems,gold,heading_home')
    (ROOT/'candidate/src/bot.cpp').write_text(source,newline='\n')
    dest=ROOT/'trace2';shutil.copytree(ROOT/'candidate',dest)
    p=dest/'src/common.h';p.write_text('#define V37_TRACE 1\n'+p.read_text(),newline='\n')
    # Correct the inherited descriptive engine label using an exact string replacement.
    p=ROOT/'tools/decode.mjs';s=p.read_text().replace("engine:'0.0.3'","engine:'1.0.1',snapshot_bytes:snapshotSize")
    p.write_text(s,newline='\n')
    write(ROOT/'registry/trace-6-v2.json',dict(id='trace-6-v2',reason='v1 floating formatting caused 9559 exhaustion events; rejected as production-neutral instrumentation',
        hypothesis='integer-only formatting and one scoring trace per body/25 rounds reduce perturbation',
        scales=dict(health=100,score=10000,value=100,cost=100,drain=100),
        promotion='none; even lower-overhead telemetry must pass parity analysis before policy activation'))
if __name__=='__main__':main()
