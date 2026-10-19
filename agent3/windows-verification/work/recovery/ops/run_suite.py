"""Resumable real release matches; no fabricated opponents or truncated games."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,subprocess,time,shutil

ROOT=Path(__file__).resolve().parents[3]
EVAL=ROOT/'work/recovery/eval'
OUT=ROOT/'analysis/v36/recovery'
CLI=ROOT/'work/recovery/.venv/Scripts/cambc.exe'
RUNS=ROOT/'work/recovery/results'
sha=lambda b:hashlib.sha256(b).hexdigest()

def compact(stats):
    fuel={'teamA':0,'teamB':0}
    for stream in stats['events']:
        if stream['kind']=='fuelExhausted':
            owner=stream['owner']; team=owner.get('team') if isinstance(owner,dict) else owner
            if team in fuel:fuel[team]+=len(stream['entries'])
    totals={team:{'earnedGold':0,'collectedGems':0,'deaths':0,'monsterDamageTaken':0} for team in fuel}
    for champion in stats['champions']:
        for key in totals[champion['team']]:
            totals[champion['team']][key]+=float(champion['metrics'][key])
    return {'match':stats['match'],'gems':{t['team']:int(t['submittedGems']) for t in stats['teams']},
            'fuel_exhausted':fuel,'faults':stats['diagnostics']['botFaults'],'totals':totals}

def execute(job):
    folder=RUNS/job['id'];folder.mkdir(parents=True,exist_ok=True)
    summary=folder/'stats.json';receipt=folder/'receipt.json'
    if receipt.exists() and json.loads(receipt.read_text()).get('status')=='failed':
        attempt=1
        while (folder/f'failed-attempt-{attempt}.json').exists():attempt+=1
        shutil.copy2(receipt,folder/f'failed-attempt-{attempt}.json')
        if (folder/'runner.log').exists():shutil.copy2(folder/'runner.log',folder/f'failed-attempt-{attempt}.log')
        if summary.exists():raise RuntimeError(f'Failed job has partial stats needing review: {folder}')
    if receipt.exists() and summary.exists():
        old=json.loads(receipt.read_text())
        if old.get('job')==job and old.get('stats_sha256')==sha(summary.read_bytes()) and old.get('status')=='complete':
            return old
        raise RuntimeError(f'Existing result identity mismatch: {folder}')
    command=[str(CLI),'run',job['a'],job['b'],'--map',str(ROOT/job['map']),
             '--release','--json','--statistics',str(summary),'--out-dir',str(folder)]
    start=time.monotonic()
    run=subprocess.run(command,cwd=EVAL,text=True,capture_output=True,timeout=90)
    (folder/'runner.log').write_text(run.stdout+run.stderr)
    result={'job':job,'command':command,'elapsed_seconds':time.monotonic()-start,
            'returncode':run.returncode,'status':'failed'}
    if run.returncode==0 and summary.exists():
        stats=json.loads(summary.read_text())
        assert stats['match']['outcome'] in ['teamA','teamB','draw','tie'],stats['match']
        assert stats['match']['engineBuildHash']=='d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5'
        result.update(status='complete',stats_sha256=sha(summary.read_bytes()),summary=compact(stats))
    receipt.write_text(json.dumps(result,indent=2)+'\n')
    print(job['id'],result['status'],flush=True)
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--suite',default='recovered-development-01');p.add_argument('--jobs',type=int,default=1);p.add_argument('--profile-suffix',default='')
    args=p.parse_args()
    maps=json.loads((OUT/'recovered_maps.json').read_text())['maps']
    profiles=['baseline']+[name+args.profile_suffix for name in ['bound_off','bound_on','bound_diag']]
    hashes={name:sha((EVAL/'bots'/name/'.cambc/build-release/bot-injected.wasm').read_bytes()) for name in profiles}
    jobs=[]
    for i,m in enumerate(maps):
        assert sha((ROOT/m['path']).read_bytes())==m['sha256']
        for tag,a,b in [('base','baseline','baseline'),('offA','bound_off','baseline'),('offB','baseline','bound_off'),
                        ('onA','bound_on','bound_off'),('onB','bound_off','bound_on'),
                        ('diagA','bound_diag','bound_on'),('diagB','bound_on','bound_diag')]:
            if a!='baseline':a+=args.profile_suffix
            if b!='baseline':b+=args.profile_suffix
            jobs.append({'id':f'{args.suite}-{i:02d}-{tag}','map_index':i,'tag':tag,'a':a,'b':b,'map':m['path'],
                         'map_sha256':m['sha256'],'artifact_a_sha256':hashes[a],'artifact_b_sha256':hashes[b]})
    schedule={'id':args.suite,'kind':'development only; reconstructed historical layouts',
              'acceptance':'disabled parity first; corrected bound cannot be promoted from this small baseline-only screen',
              'maps':len(maps),'games':len(jobs),'jobs':jobs,'seed_control':'CLI exposes no seed option; record actual native seeds',
              'stop_rule':'complete fixed schedule; failures excluded explicitly; never use as untouched holdout'}
    path=OUT/(args.suite+'-schedule.json')
    if path.exists():assert json.loads(path.read_text())==schedule,'Schedule changed'
    else:path.write_text(json.dumps(schedule,indent=2)+'\n')
    records=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for result in pool.map(execute,jobs):records.append(result)
    (OUT/(args.suite+'-results.json')).write_text(json.dumps({'schedule':schedule['id'],'results':records},indent=2)+'\n')
    assert all(r['status']=='complete' for r in records),'Some matches failed'
    print('Completed',len(records),'full release games on',len(maps),'layouts')

if __name__=='__main__':main()
