"""Two-round SDK identity scenarios; explicitly not full-match strength evidence."""
import argparse,hashlib,json,pathlib,re,subprocess,time
from replay_bundle import read_bundle
from stdout_framing import frames
V36=pathlib.Path(__file__).resolve().parents[2]
MAPS='ambush collision contest corners eight elkring expanse fragment interlock lanes monolith packs pilgrimage rows rushbait-v2 stampede swirl'.split()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--build',default='agent4-identity-probe-fixed2');parser.add_argument('--out-name',default='identity_fixtures_corrected');parser.add_argument('--maps',nargs='*');parser.add_argument('--game-unknown',action='store_true');a=parser.parse_args()
    build=json.loads((V36/'builds'/a.build/'manifest.json').read_text());artifact=pathlib.Path(build['artifact']);assert sha(artifact)==build['artifact_sha256']
    cli=V36.parent/'runtime-latest/bin/cambc.exe';maps=[(n,V36.parent/'development/current/maps'/f'{n}.map26a',i) for i,n in enumerate(MAPS)]
    maps += [(p.stem,p,-1) for p in sorted((V36/'evidence/agent4/identity_maps').glob('*.map26a'))]
    if a.maps:maps=[m for m in maps if m[0] in a.maps]
    root=V36/'evidence/agent4'/a.out_name;root.mkdir(exist_ok=True);receipts=[]
    for name,mapfile,expected in maps:
        folder=root/name;folder.mkdir(exist_ok=True);stat=folder/'statistics.json'
        command=[str(cli),'run',str(artifact),str(artifact),'--map',str(mapfile.resolve()),'--rounds','2','--release','-o',str(folder.resolve()),'--statistics',str(stat.resolve()),'--json']
        marker=folder/'receipt.json'
        if marker.exists():
            receipt=json.loads(marker.read_text());assert receipt['artifact_sha256']==sha(artifact) and receipt['map_sha256']==sha(mapfile)
        else:
            started=time.monotonic();proc=subprocess.run(command,cwd=V36.parent/'development/current',capture_output=True,text=True,timeout=60)
            (folder/'runner.log').write_text(proc.stdout+proc.stderr)
            if proc.returncode:raise RuntimeError(proc.stderr+proc.stdout)
            bundle=next(folder.glob('*.repl26a'));members=read_bundle(bundle);parsed={}
            for side in ['a','b']:
                spans=frames(members[f'team-{side}.stdout']);observations=[];lineups=[]
                for span in spans:
                    assert 'IDENTITY_ERROR' not in span['text'],span['text']
                    for m in re.finditer(r'IDENTITY_PROBE phase(\d+) size(\d+) groups(\d+) code(-?\d+) id(-?\d+)',span['text']):
                        values=list(map(int,m.groups()));observations.append(dict(zip(['draft_phase','picks','groups','sdk_code','id'],values),position=span['position']))
                    for m in re.finditer(r'NOVA_MAPINFO (-?\d+) (-?\d+) (\d+) (-?\d+) (-?\d+) (-?\d+)',span['text']):lineups.append(list(map(int,m.groups())))
                assert observations and any(o['draft_phase'] for o in observations) and any(not o['draft_phase'] for o in observations),f'{name}: missing draft/game identity'
                assert all(o['sdk_code']==0 and o['id']==(-1 if a.game_unknown and not o['draft_phase'] else expected) for o in observations),f'{name}: ID mismatch {observations}'
                assert lineups and all(l[4]==expected for l in lineups),f'{name}: missing actual draft policy trace'
                parsed[side]=dict(identity_observations=observations,lineup_trace=lineups,stdout_sha256=hashlib.sha256(members[f'team-{side}.stdout']).hexdigest())
            receipt=dict(name=name,expected_id=expected,artifact_sha256=sha(artifact),map_sha256=sha(mapfile),cli_sha256=sha(cli),command=command,
                seconds=time.monotonic()-started,round_limit=2,scope='Nonperformance actual SDK fixture; no full-match outcome/strength estimate',
                bundle=str(bundle),bundle_sha256=sha(bundle),raw_replay_sha256=hashlib.sha256(members['replay.repl26a']).hexdigest(),
                statistics=str(stat) if stat.exists() else None,parsed=parsed,passed=True)
            marker.write_text(json.dumps(receipt,indent=2))
        receipts.append(receipt);print(json.dumps(dict(map=name,expected_id=expected,passed=True)),flush=True)
    report=dict(known_layout_scenarios=sum(m[2]>=0 for m in maps),descriptor_collision_scenarios=sum(m[2]<0 for m in maps),runtime_gameplay_expected_unknown=a.game_unknown,round_limit=2,passed=True,receipts=receipts,
        limitations=['Actual absolute orientation and bush-only descriptor collisions tested; arbitrary rotation/transposition acceptance remains intentionally unsupported.',
          'Unavailable SDK query paths fail closed by source review; the live SDK is not deliberately fault-injected in these scenarios.',
          'These are two-round correctness fixtures, never strength evidence or final holdout data.'])
    (root/'report.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':main()
