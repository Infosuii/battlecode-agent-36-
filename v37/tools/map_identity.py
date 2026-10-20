"""Canonical physical layout identities, independent of name and dihedral orientation."""
import sys,struct,json,collections
from lab import ROOT,WORK,WS,write,sha,digest
sys.path.insert(0,str(WORK/'runtime'))
from engine_abi import Engine,Replay
def identity(engine,path):
    e=engine;raw=path.read_bytes();p=e.malloc(len(raw));e.write(p,raw)
    e.call('Game_FromMapFileBytes',p,len(raw),e.out,0);h=e.u64(e.out);e.f('Engine_Free',p)
    obj=Replay.__new__(Replay);obj.e=e;obj.h=h
    try:
        e.call('Game_GetBitboardMetadata',h,e.out);obj.W,obj.H,obj.cw,obj.ch,obj.stride=struct.unpack('<5i',e.read(e.out,20))
        e.call('Game_GetNumChampsPerTeam',h,e.out);size=e.i32(e.out)
        e.call('Game_GetSymmetry',h,e.out);sym=e.i32(e.out)
        e.call('Game_GetTeamASpawnArea',h,e.out);spawn=struct.unpack('<4i',e.read(e.out,16))
        terrain=obj.terrain();camps,groups=obj.camp_groups();w,hh=obj.W,obj.H
        def mirror(x,y):return (w-1-x,y) if sym==0 else (x,hh-1-y) if sym==1 else (w-1-x,hh-1-y)
        # Engine spawn area is inclusive min/max corners, as used by official map editor.
        x0,y0,x1,y1=spawn
        spawns=[[(x0,y0),(x1,y1)],[mirror(x0,y0),mirror(x1,y1)]]
        representations=[]
        for swap in (False,True):
            for flipx in (False,True):
                for flipy in (False,True):
                    def tf(x,y):
                        if flipx:x=w-1-x
                        if flipy:y=hh-1-y
                        return (y,x) if swap else (x,y)
                    board={k:sorted(tf(*xy) for xy in cells) for k,cells in terrain.items()}
                    gs=sorted((g['gems'],g['gold'],g['cooldown'],sorted((kind,*tf(x,y)) for kind,x,y in g['camps'])) for g in groups)
                    representation=dict(width=hh if swap else w,height=w if swap else hh,team_size=size,
                        terrain=board,groups=gs,spawns=sorted(sorted(tf(*xy) for xy in corners) for corners in spawns))
                    representations.append(json.dumps(representation,sort_keys=True,separators=(',',':')))
        canonical=min(representations)
        import hashlib
        return dict(sha256=sha(path),physical_sha256=hashlib.sha256(canonical.encode()).hexdigest(),
            width=w,height=hh,team_size=size,golems=sum(kind==5 for kind,x,y in camps),groups=len(groups),spawn_raw=spawn)
    finally:e.call('Game_Destroy',h)
def main():
    e=Engine(ROOT/'tools/engine-current.wasm');rows=[];seen={}
    files=sorted((WS/'maps').glob('*.map26a'))+sorted((ROOT/'maps').glob('*/*.map26a'))
    for p in files:
        row=dict(path=str(p),**identity(e,p));rows.append(row)
        stage=p.parent.name
        old=seen.get(row['physical_sha256'])
        if old and old!=stage:raise ValueError('Related physical geometry crosses split: '+str(p))
        seen[row['physical_sha256']]=stage
    write(ROOT/'evidence/physical-map-identities.json',dict(schema=1,engine_sha256=sha(ROOT/'tools/engine-current.wasm'),
        normalization='lexicographic canonical serialization across all eight dihedral orientations; both team spawns unordered; all terrain and physical group members/rewards/respawn/team size included',
        files=rows,groups=len(seen),cross_split_duplicates=0,
        raw_to_engine_hash_note='Native mapHash equals file SHA256 on the 35 baseline official matches; orientation canonicalization is additional, never substituted for runner integrity checks.',
        limitations='Offline split/analysis identity only. Production draft classifier unchanged; unknown layouts keep the baseline fallback.'))
    print(json.dumps(dict(files=len(rows),physical_groups=len(seen),golem_maps=sum(r['golems']>0 for r in rows),cross_split_duplicates=0)))
if __name__=='__main__':main()
