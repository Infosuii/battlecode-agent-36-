"""Recover immutable runtime group definitions from actual installed SDK calls.

The editor's handle order differs after runtime mirrored-map expansion; never
mix editor group handles with runtime camp counts. This fixture serializes the
same checked SDK data used by the production strict comparator.
"""
import hashlib,json,pathlib,re,subprocess
from replay_bundle import read_bundle
V36=pathlib.Path(__file__).resolve().parents[2]
MAPS='ambush collision contest corners eight elkring expanse fragment interlock lanes monolith packs pilgrimage rows rushbait-v2 stampede swirl'.split()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    build=json.loads((V36/'builds/agent4-identity-sdk-dump/manifest.json').read_text());bot=pathlib.Path(build['artifact']);assert sha(bot)==build['artifact_sha256']
    cli=V36.parent/'runtime-latest/bin/cambc.exe';root=V36/'evidence/agent4/sdk_group_scenarios';root.mkdir(exist_ok=True);results=[]
    for name in MAPS:
        folder=root/name;folder.mkdir(exist_ok=True);mapfile=V36.parent/'development/current/maps'/f'{name}.map26a'
        command=[str(cli),'run',str(bot),str(bot),'--map',str(mapfile),'--rounds','2','--release','-o',str(folder)]
        if not list(folder.glob('*.repl26a')):
            run=subprocess.run(command,cwd=V36.parent/'development/current',capture_output=True,text=True,timeout=60);(folder/'runner.log').write_text(run.stdout+run.stderr)
            if run.returncode:raise RuntimeError(run.stdout+run.stderr)
        bundle=next(folder.glob('*.repl26a'));members=read_bundle(bundle);sides=[]
        for side in 'ab':
            text=members[f'team-{side}.stdout'].decode('utf-8',errors='replace')
            # Gameplay modules can suspend mid-dump and interleave in global
            # output order. Use only the first draft module's completed dump.
            prefix=text.split('IDENTITY_PROBE phase1',1)[0].replace('\0','')
            allrows=[tuple(map(int,m.group(1).split())) for m in re.finditer(r'SDK_GROUP((?: \d+)+)\n',prefix)]
            counts={int(m.group(1)) for m in re.finditer(r'IDENTITY_PROBE phase\d+ size\d+ groups(\d+)',text)}
            assert len(counts)==1
            count=next(iter(counts));rows=allrows[:count]
            assert len(rows)==count and len(allrows)%count==0,f'{name} incomplete SDK group dump {len(allrows)} vs {count}; inspect early terrain failures'
            assert len(allrows)==count,f'{name}: ambiguous first-draft dump'
            assert all(len(r)==4+3*r[3] for r in rows)
            sides.append(rows)
        assert sides[0]==sides[1],f'{name} team orientation changed static data'
        result=dict(map=name,map_sha256=sha(mapfile),artifact_sha256=sha(bot),cli_sha256=sha(cli),groups=sides[0],group_count=len(sides[0]),
            bundle=str(bundle),bundle_sha256=sha(bundle),command=command,round_limit=2,
            provenance='Actual SDK sdk_map_camp_groups, sdk_map_camp_group/camps/camp_info and sdk_bc_camp_group_max_respawn; canonical sorted [gems,gold,respawn,n,type,x,y,...].')
        (folder/'receipt.json').write_text(json.dumps(result,indent=2));results.append(result);print(json.dumps(dict(map=name,groups=len(sides[0]))),flush=True)
    (V36/'evidence/agent4/sdk_group_layouts.json').write_text(json.dumps(results,indent=2))
if __name__=='__main__':main()
