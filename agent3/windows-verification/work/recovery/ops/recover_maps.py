"""Recover archived replay geometry through the shipped engine MapBuilder API.

These are reconstructed historical layouts, not asserted current official-map
identities or independent unseen maps. Every output carries exact provenance.
"""
from pathlib import Path
import sys,struct,json,hashlib,importlib.util

ROOT=Path(__file__).resolve().parents[3]
ANALYTICS=Path('C:/projects/cambridge battlecode analytics/Replay Analytics')
SOURCE=ANALYTICS/'battlecode_analytics/engine.py'
OUT=ROOT/'analysis/v36/recovery'
MAPS=ROOT/'work/recovery/eval/maps'
sha=lambda b:hashlib.sha256(b).hexdigest()

def main():
    spec=importlib.util.spec_from_file_location('recovery_engine',SOURCE)
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    engine=mod.Engine()
    records=[]; failures=[]; seen_replay=set(); seen_layout={}
    replays=sorted(Path('C:/projects/cambridge battlecode analytics').rglob('*.repl26a'))
    for source in replays:
        data=source.read_bytes(); replay_sha=sha(data)
        if replay_sha in seen_replay:continue
        seen_replay.add(replay_sha)
        try:
            replay=mod.Replay(engine,data)
            terrain=replay.terrain(); camps,groups=replay.camp_groups()
            geometry={'width':replay.W,'height':replay.H,'symmetry':replay.symmetry,
                      'team_size':replay.team_size,'spawn_a':replay.spawnA,
                      'terrain':{k:sorted(v) for k,v in terrain.items()},'camps':sorted(camps)}
            layout_sha=sha(json.dumps(geometry,sort_keys=True,separators=(',',':')).encode())
            if layout_sha in seen_layout:
                seen_layout[layout_sha]['source_replays'].append({'path':str(source),'sha256':replay_sha})
                continue
            e=engine
            e.call('MapBuilder_Create',replay.W,replay.H,replay.symmetry,e.out); builder=e.u64(e.out)
            e.call('MapBuilder_SetNumChampsPerTeam',builder,replay.team_size)
            e.write(e.args,struct.pack('<4i',*replay.spawnA));e.call('MapBuilder_SetTeamASpawnArea',builder,e.args)
            for key,fn in [('walls','Wall'),('holes','Hole'),('bushes','Bush'),('altars','Altar')]:
                for x,y in sorted(terrain[key]):e.call('MapBuilder_Add'+fn,builder,x,y)
            for kind,x,y in camps:e.call('MapBuilder_AddCamp',builder,kind,x,y,e.out)
            e.call('MapBuilder_VerifyIntegrity',builder)
            e.call('MapBuilder_Encode',builder,e.out,e.out+4)
            encoded=e.read(e.u32(e.out),e.u32(e.out+4))
            # Decode the emitted file and compare every canonical camp record.
            # This catches accidental symmetry expansion or enum/layout drift.
            ptr=e.malloc(len(encoded));e.write(ptr,encoded)
            e.call('MapBuilder_FromFileBytes',ptr,len(encoded),e.out); decoded=e.u64(e.out)
            e.call('MapBuilder_GetNumCamps',decoded,e.out); count=e.u32(e.out)
            roundtrip=[]
            for i in range(count):
                e.call('MapBuilder_GetCampHandle',decoded,i,e.out); handle=e.u64(e.out)
                e.call('MapBuilder_GetCamp',decoded,handle,e.out)
                roundtrip.append(struct.unpack('<iii',e.read(e.out,12)))
            assert sorted(roundtrip)==sorted(camps),'camp geometry changed on encode/decode'
            e.f('MapBuilder_Destroy',decoded);e.f('Engine_Free',ptr)
            path=MAPS/f'recovered_{layout_sha[:16]}.map26a'
            path.write_bytes(encoded)
            entry={'path':str(path.relative_to(ROOT)),'sha256':sha(encoded),'layout_sha256':layout_sha,
                   'geometry':geometry,'source_replays':[{'path':str(source),'sha256':replay_sha}],
                   'classification':'reconstructed historical layout; canonical official identity unverified',
                   'camp_roundtrip_verified':True}
            records.append(entry);seen_layout[layout_sha]=entry
            print('Recovered',replay.W,replay.H,'groups',len(groups),'as',path.name,flush=True)
            e.f('MapBuilder_Destroy',builder)
        except Exception as error:
            failures.append({'path':str(source),'sha256':replay_sha,'error':str(error)})
            print('Excluded',source.name,str(error),flush=True)
        if len(seen_replay)>=80:break
    manifest={'engine_wasm_sha256':sha((ANALYTICS/'engine/engine.wasm').read_bytes()),
              'decoder_sha256':sha(SOURCE.read_bytes()),'unique_replays_examined':len(seen_replay),
              'maps':records,'failures':failures,'not_a_holdout':True}
    (OUT/'recovered_maps.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Recovered',len(records),'layouts from',len(seen_replay),'unique replays')

if __name__=='__main__':main()
