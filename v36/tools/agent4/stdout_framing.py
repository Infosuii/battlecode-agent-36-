"""Official stdout framing plus replay-backed controller context for telemetry.

Qh in installed visualiser index-oTc0ysQc.js: UTF8, each NUL advances position,
including empties; nonempty strings have no synthetic length/header framing.
"""
import argparse,hashlib,json,pathlib,re,subprocess
from replay_bundle import read_bundle
V36=pathlib.Path(__file__).resolve().parents[2]
VISUALISER=pathlib.Path('C:/Users/ishan/AppData/Local/cambc/assets/9f2cf433a04e5a113a5488a236703e1473411d687bb65322214eec98b96fa13a/visualiser/assets/index-oTc0ysQc.js')
def frames(raw):
    return [dict(position=i,text=t) for i,t in enumerate(raw.decode('utf-8',errors='replace').split('\0')) if t]
def fuel(raw):
    import struct
    if len(raw)<77 or int.from_bytes(raw[:8],'little')!=28839526091863649 or raw[8:11]!=bytes([0,0,1]) or (len(raw)-77)%8:raise ValueError('Unsupported fuel encoding')
    return dict(version='0.0.1',languages=list(raw[11:13]),
        limits=[dict(zip(['firstDraftTurn','draftTurn','firstTurn','turn'],struct.unpack_from('<4Q',raw,off))) for off in (13,45)],
        values=[x[0] for x in struct.iter_unpack('<Q',raw[77:])])
def main():
    parser=argparse.ArgumentParser();parser.add_argument('results');parser.add_argument('--outdir',required=True);a=parser.parse_args()
    dest=pathlib.Path(a.outdir);dest.mkdir(parents=True,exist_ok=True);reports=[]
    for result in json.loads(pathlib.Path(a.results).read_text()):
        folder=dest/result['id'];folder.mkdir(exist_ok=True)
        bundle=next(pathlib.Path(result['statistics']).parent.glob('*.repl26a'));members=read_bundle(bundle)
        records=[]
        for side in (0,1):
            for row in frames(members.get(f'team-{"ab"[side]}.stdout',b'')):records.append(dict(row,team=side))
        raw=folder/'replay.repl26a';raw.write_bytes(members['replay.repl26a'])
        positions=folder/'positions.json';positions.write_text(json.dumps(sorted({r['position'] for r in records})))
        contexts=folder/'contexts.json'
        if not contexts.exists():
            proc=subprocess.run(['node',str(V36/'tools/agent4/stdout_contexts.mjs'),str(raw),str(positions),str(contexts)],capture_output=True,text=True,timeout=300)
            if proc.returncode:raise RuntimeError(proc.stderr[-4000:])
        mapping=json.loads(contexts.read_text());joined=[];ambiguous=0
        for frame in records:
            controller=mapping['contexts'].get(str(frame['position']))
            if controller and (controller.get('alternatives') or controller['team']!=frame['team']):ambiguous+=1;controller=None
            joined.append(dict(frame,controller=controller))
        report=dict(job_id=result['id'],map=pathlib.Path(result['map']).stem,frames=len(joined),
            joined_frames=sum(r['controller'] is not None for r in joined),ambiguous_frames=ambiguous,
            missing_positions=mapping['missing_positions'],controller_turns=mapping['controller_turns'],
            rollovers=mapping['rollovers'],records=joined,
            fuel={f'team{side}':fuel(members[f'team-{str(side).lower()}.fuel']) for side in ('A','B') if f'team-{str(side).lower()}.fuel' in members})
        (folder/'framed_stdout.json').write_text(json.dumps(report,indent=2));reports.append({k:report[k] for k in ['job_id','map','frames','joined_frames','ambiguous_frames','missing_positions']})
    summary=dict(source_visualiser=str(VISUALISER),source_visualiser_sha256=hashlib.sha256(VISUALISER.read_bytes()).hexdigest(),
        framing='Each NUL advances the replay input position; empty spans retained in position count. Context joined from actual awaiting-input champion snapshots.',matches=reports)
    (dest/'manifest.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary))
if __name__=='__main__':main()
