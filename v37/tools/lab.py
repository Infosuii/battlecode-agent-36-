"""v37 extension of v36/tools/evaluate.py: immutable artifact keys and strict engine gates.

Uses the official CLI's production build, full matches, and native statistics.
No --rounds or --fuel overrides. Identical deterministic jobs share cached evidence.
"""
import argparse, concurrent.futures, hashlib, json, os, pathlib, random, shutil, subprocess, time, zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
WORK=ROOT.parents[1]
CLI=pathlib.Path(os.environ.get('V37_CLI',str(WORK/'runtime-3.0.2/bin/cambc.exe')))
WS=ROOT/'workspace'
ENGINE='c3e9ffc8d98c106f86660c58344dc26ced9459a504e2f0d90d88b4c8594c7765'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def digest(x):return hashlib.sha256(json.dumps(x,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def tree(p):return {str(f.relative_to(p)).replace('\\','/'):sha(f) for f in sorted(pathlib.Path(p).rglob('*')) if f.is_file()}
def write(p,x):
    p=pathlib.Path(p);p.parent.mkdir(parents=True,exist_ok=True)
    tmp=p.with_suffix(p.suffix+'.tmp');tmp.write_text(json.dumps(x,indent=2)+'\n');tmp.replace(p)
def run(cmd,cwd=WS,timeout=600):
    env=dict(os.environ,LOCALAPPDATA=os.environ.get('V37_LOCALAPPDATA',str(pathlib.Path.home()/'AppData/Local')))
    r=subprocess.run([str(x) for x in cmd],cwd=cwd,env=env,capture_output=True,text=True,timeout=timeout)
    if r.returncode:raise RuntimeError((r.stdout+r.stderr)[-5000:])
    return r.stdout+r.stderr
def build(name,source):
    dest=WS/'bots'/name
    record=ROOT/'builds'/name/'manifest.json'
    if record.exists():raise ValueError('Immutable build exists')
    if not dest.exists():shutil.copytree(source,dest)
    expected=tree(pathlib.Path(source)/'src')
    assert tree(dest/'src')==expected
    log=run([CLI,'build','--release',name])
    out=dest/'.cambc/build-release/bot-injected.wasm'
    record.parent.mkdir(parents=True,exist_ok=True)
    (record.parent/'build.log').write_text(log)
    commands=json.loads((dest/'.cambc/build-release/compile_commands.json').read_text())
    compiler=pathlib.Path(commands[0]['command'].split(' --target=')[0])
    data=dict(name=name,source=str(pathlib.Path(source).resolve()),source_files=expected,
        source_sha256=digest(expected),artifact=str(out),wasm_sha256=sha(out),cli_sha256=sha(CLI),
        cli_version=run([CLI,'--version']).strip(),sdk_files=tree(WS/'sdk/cpp'),
        sdk_config=(WS/'cambc.toml').read_text(),compiler_sha256=sha(compiler),
        compiler_version=run([compiler,'--version']).strip(),compile_commands=commands,
        full_match_rules='release; default fuel; no round cap',production='trace' not in name)
    write(record,data);print(json.dumps(dict(build=name,wasm=data['wasm_sha256'])),flush=True)
    return data
def manifest(name):
    m=json.loads((ROOT/'builds'/name/'manifest.json').read_text())
    assert sha(m['artifact'])==m['wasm_sha256'] and sha(CLI)==m['cli_sha256']
    return m
def freeze(name,candidate,opponents,maps,purpose):
    dest=ROOT/'schedules'/(name+'.json')
    assert not dest.exists(),'Schedule already exists'
    jobs=[];seen=set()
    for mp in maps:
        mp=pathlib.Path(mp).resolve();mh=sha(mp)
        if mh in seen:raise ValueError('Duplicate physical map file')
        seen.add(mh)
        for opp in opponents:
            for side in (0,1):
                names=[candidate,opp] if side==0 else [opp,candidate]
                ms=[manifest(n) for n in names]
                key=dict(wasm=[m['wasm_sha256'] for m in ms],source=[m['source_sha256'] for m in ms],
                    map_sha256=mh,engine=ENGINE,cli=sha(CLI),rules='release-full-default-v1',log_limit=33554432)
                jobs.append(dict(id=digest(key),key=key,bots=names,map=str(mp),side=side,group=mh))
    # Selfplay sides are duplicate executions, never independent samples.
    unique={j['id']:j for j in jobs}
    data=dict(name=name,candidate=candidate,opponents=opponents,purpose=purpose,
        jobs=list(unique.values()),duplicates_removed=len(jobs)-len(unique),frozen_ns=time.time_ns())
    data['schedule_sha256']=digest(data);write(dest,data);return data
def game(j):
    assert sha(j['map'])==j['key']['map_sha256']
    ms=[manifest(n) for n in j['bots']]
    assert [m['wasm_sha256'] for m in ms]==j['key']['wasm']
    dest=ROOT/'runs'/j['id'];receipt=dest/'receipt.json'
    if receipt.exists():
        r=json.loads(receipt.read_text());assert r['key']==j['key']
        assert all(sha(dest/p)==h for p,h in r['raw_hashes'].items())
        assert r['match']['engineBuildHash']==ENGINE
        return dict(r,assignment=j)
    if dest.exists():
        quarantine=ROOT/'quarantine'/(j['id']+'-'+str(time.time_ns()))
        quarantine.parent.mkdir(parents=True,exist_ok=True);dest.rename(quarantine)
    dest.mkdir(parents=True)
    cmd=[CLI,'run',*[m['artifact'] for m in ms],'--map',j['map'],'--release',
        '-o',dest,'--statistics',dest/'statistics.json','--json','--log-limit','33554432']
    start=time.monotonic();log=run(cmd);(dest/'runner.log').write_text(log)
    s=json.loads((dest/'statistics.json').read_text())
    assert s['match']['engineVersion']=='1.0.1' and s['match']['engineBuildHash']==ENGINE
    assert s['diagnostics']['detailedCaptureComplete']
    assert s['match']['outcome'] in ('teamA','teamB','draw','tie')
    bundles=list(dest.glob('*.repl26a'));assert len(bundles)==1
    with zipfile.ZipFile(bundles[0]) as z:
        assert len(z.namelist())==len(set(z.namelist()))
        assert {'replay.repl26a','team-a.fuel','team-b.fuel'}<=set(z.namelist())
        for n in ('team-a.fuel','team-b.fuel'):
            raw=z.read(n);assert raw.startswith(b'a62leuf'), 'Unknown fuel format'
    r=dict(key=j['key'],match=s['match'],scores={t['team']:int(t['submittedGems']) for t in s['teams']},
        faults=s['diagnostics']['botFaults'],seconds=time.monotonic()-start,command=[str(c) for c in cmd],
        fuel_exhaustions={t:sum(len(e['entries']) for e in s['events'] if e['kind']=='fuelExhausted' and e['owner'].get('team')==t) for t in ('teamA','teamB')},
        raw_hashes={p.name:sha(p) for p in dest.iterdir() if p.is_file()})
    write(receipt,r);return dict(r,assignment=j)
def execute(name):
    sc=json.loads((ROOT/'schedules'/(name+'.json')).read_text());expected=sc.pop('schedule_sha256')
    assert digest(sc)==expected,'Schedule mutated'
    rows=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for r in pool.map(game,sc['jobs']):rows.append(r)
    write(ROOT/'results'/(name+'.json'),rows)
    groups={};points=[];faults=exhaustions=0
    for r in rows:
        j=r['assignment'];side=j['side'];team=('teamA','teamB')[side];other=('teamB','teamA')[side]
        point=.5 if r['match']['outcome'] not in ('teamA','teamB') else float(r['match']['outcome']==team)
        margin=r['scores'][team]-r['scores'][other]
        groups.setdefault(j['group'],[]).append((point,margin));points.append(point)
        faults+=r['faults'][team];exhaustions+=r['fuel_exhaustions'][team]
    means=[(sum(x[0] for x in v)/len(v),sum(x[1] for x in v)/len(v)) for v in groups.values()]
    rng=random.Random(371020);boot=[]
    for _ in range(4000):
        sample=rng.choices(means,k=len(means));boot.append((sum(x[0] for x in sample)/len(sample),sum(x[1] for x in sample)/len(sample)))
    intervals=[[sorted(x[i] for x in boot)[k] for k in (100,3899)] for i in (0,1)]
    report=dict(schedule=name,schedule_sha256=expected,games=len(rows),independent_file_groups=len(groups),
        wins=points.count(1),ties=points.count(.5),losses=points.count(0),
        win_points=sum(points)/len(points),mean_secured_margin=sum(x[1] for x in means)/len(means),
        clustered95=intervals,faults=faults,exhaustions=exhaustions,
        selfplay=all(r['key']['wasm'][0]==r['key']['wasm'][1] for r in rows),
        note='File groups only: related/mirrored generated layouts require a higher family grouping before promotion. Selfplay has no strength interpretation.')
    write(ROOT/'results'/(name+'-summary.json'),report);print(json.dumps(report),flush=True)
    return rows
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('command',choices=['build','run','freeze-diagnostics']);ap.add_argument('name');ap.add_argument('--source')
    a=ap.parse_args()
    if a.command=='build':build(a.name,pathlib.Path(a.source))
    elif a.command=='run':execute(a.name)
    else:freeze(a.name,'v37-baseline',['v37-baseline'],sorted((WS/'maps').glob('*.map26a')),'BASE/DATA diagnostic selfplay; all official maps development data')
