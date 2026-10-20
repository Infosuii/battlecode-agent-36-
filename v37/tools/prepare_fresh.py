"""Reuse pinned map generator with explicit Golem support and fresh frozen seeds."""
import sys,os,shutil,random,json
from lab import ROOT,WORK,write,sha
def main():
    dest=ROOT/'tools/mapgen';dest.mkdir(parents=True,exist_ok=True)
    old=ROOT.parent/'v36/agent4/tools/mapgen'
    s=(old/'mapbuilder.py').read_text().replace("'monolith': 4}","'monolith': 4, 'golem': 5}")
    (dest/'mapbuilder.py').write_text(s,newline='\n')
    s=(old/'gen.py').read_text()
    s=s.replace("('elk', [(0, 0)]),", "('golem', [(0, 0)]), ('elk', [(0, 0)]),")
    s=s.replace("kinds = {'wolf':", "kinds = {'golem': 'golem', 'wolf':")
    (dest/'gen.py').write_text(s,newline='\n')
    protocol=dict(id='fresh-v37-1',generator_sha256=sha(dest/'gen.py'),builder_sha256=sha(dest/'mapbuilder.py'),
        builder_engine_sha256=sha(ROOT/'tools/engine-current.wasm'),stages=[
            dict(name='dev',count=24,seed=37102061),dict(name='confirm',count=128,seed=37102062),
            dict(name='final',count=128,seed=37102063)],
        evidence_rule='No suite presumed fresh by name. New deterministic seeds, physical hash dedup, inherited engine hashes excluded when available. Whole layout groups include mirrored sides.',
        approximate_power='128 independent groups only detects large effects (~12 percentage points at 80% power under worst-case binary variance); smaller effects remain inconclusive.',
        primary='paired full-match secured margin and win points vs unchanged selected baseline',
        screen='At least +3% win points and positive secured margin, no new faults; screen alone cannot promote.',
        confirm='Only one prespecified link-emergency policy; win points >=0.55 with lower 95% map-cluster bound >0.50 and secured-margin lower 95% bound >0; no critical subgroup/fuel regression.',
        final='Freeze exact source/WASM first; same criteria on independent final stage. No optional extension based on near misses.',
        opponents='selected v36 and historical v34 proxy available; calibrated pool requested but absent')
    write(ROOT/'registry/fresh-v37-1.json',protocol)
    sys.path[:0]=[str(dest),str(WORK/'runtime')]
    os.environ['BC_ENGINE_WASM']=str(ROOT/'tools/engine-current.wasm')
    from gen import build_one
    from mapbuilder import Engine,loads_in_game
    e=Engine();all_hash=set()
    for stage in protocol['stages']:
        out=ROOT/'maps'/stage['name'];out.mkdir(parents=True,exist_ok=True)
        assert not (out/'manifest.json').exists(),'Immutable map stage exists'
        rng=random.Random(stage['seed']);made=tries=0;rows=[]
        while made<stage['count'] and tries<stage['count']*20:
            tries+=1;data=build_one(e,rng,made)
            if not data or not loads_in_game(e,data):continue
            import hashlib
            h=hashlib.sha256(data).hexdigest()
            if h in all_hash:continue
            all_hash.add(h);p=out/f'{stage["name"]}-{made:04d}.map26a';p.write_bytes(data)
            rows.append(dict(path=str(p),sha256=h,layout_group=h));made+=1
        assert made==stage['count']
        write(out/'manifest.json',dict(**stage,generator=protocol['generator_sha256'],engine=protocol['builder_engine_sha256'],maps=rows,
            status='registered before policy screens; do not use confirm/final as development data'))
        print(json.dumps(dict(stage=stage['name'],maps=made,tries=tries)),flush=True)
if __name__=='__main__':main()
