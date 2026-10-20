"""Release builds and immutable, resumable full-match schedules (Python 3.12)."""
import argparse, concurrent.futures, datetime, hashlib, json, math, os, pathlib, random, re, shutil, subprocess, time

ROOT = pathlib.Path(__file__).resolve().parents[1]
WORK = pathlib.Path(os.environ.get('V36_WORK_ROOT', str(ROOT.parent)))
PROJECT = pathlib.Path(os.environ.get('V36_PROJECT', str(WORK / 'development/current')))
CLI = pathlib.Path(os.environ.get('V36_CLI', str(WORK / 'runtime-latest/bin/cambc.exe')))
SDK = pathlib.Path(os.environ.get('V36_SDK', str(WORK / 'development/wasi-sdk-33.0-x86_64-windows')))
SWITCHES = dict(NOVA_RUNTIME_GOVERNOR=0, NOVA_RUNTIME_TELEMETRY=0, NOVA_TS_RELEVANCE=0,
    NOVA_TS_MAX_UNITS=8, NOVA_TS_SAFETY=0, NOVA_TS_SIM_FIXES=0, NOVA_FLEE_HIT=0,
    V36_FARM_BOUND=0, V36_FARM_DIAG=0, V36_FARM_EXHAUSTIVE=0, NOVA_STRICT_MAP_ID=0, NOVA_STRICT_MAP_CACHE=0, NOVA_WORLD_AWAKE=0, V36_ROWS_CAPTURE_INTENT=0)

def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(p, value):
    p=pathlib.Path(p); p.parent.mkdir(parents=True, exist_ok=True)
    tmp=p.with_suffix(p.suffix+'.tmp'); tmp.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8'); tmp.replace(p)
def tree(folder):
    rows={p.relative_to(folder).as_posix():sha(p) for p in sorted(pathlib.Path(folder).rglob('*')) if p.is_file()}
    digest=hashlib.sha256(''.join(f'{k}\0{v}\n' for k,v in rows.items()).encode()).hexdigest()
    return dict(algorithm='sha256(sorted relative path + NUL + lowercase file sha256 + LF)', sha256=digest, files=rows)
def run_cmd(cmd, **kw):
    r=subprocess.run([str(x) for x in cmd],capture_output=True,text=True,**kw)
    if r.returncode: raise RuntimeError(f'{cmd[0]} failed ({r.returncode}): '+(r.stdout+r.stderr)[-5000:])
    return r.stdout+r.stderr

def build(name, settings=None, source='bot_v36', materialized=False, normalize=False):
    dest=ROOT/'builds'/name
    if dest.exists() and (dest/'manifest.json').exists(): raise ValueError('Build IDs are immutable; use a new name: '+name)
    dest.mkdir(parents=True,exist_ok=True)
    if not (dest/'src').exists(): shutil.copytree(ROOT/source/'src',dest/'src')
    flags=dict(SWITCHES); flags.update(settings or {})
    if source=='bot_v34' and settings: raise ValueError('Frozen baseline does not accept settings')
    cpp=sorted((dest/'src').glob('*.cpp')); out=dest/(name+'.wasm')
    compiler=SDK/('bin/clang++.exe' if os.name=='nt' else 'bin/clang++')
    cmd=[compiler,'-O3','-DNDEBUG','-std=c++20','-msimd128','-mbulk-memory','-mbulk-memory-opt',
         '-fwasm-exceptions','-mllvm','-wasm-use-legacy-eh=false','--no-wasm-opt',
         '-I'+str(PROJECT/'sdk/cpp'),'-L'+str(SDK/'share/wasi-sysroot/lib/wasm32-wasip1/eh'),'-lunwind']
    if source!='bot_v34' and not materialized: cmd += [f'-D{k}={v}' for k,v in flags.items()]
    cmd += cpp+['-o',out]
    start=time.monotonic(); log=run_cmd(cmd,timeout=900); (dest/'build.log').write_text(log)
    normalization=None
    if normalize:
        from wasm_compare import sections
        raw=dest/(name+'.unstripped.wasm'); shutil.copyfile(out,raw)
        strip_tool=SDK/('bin/llvm-strip.exe' if os.name=='nt' else 'bin/llvm-strip')
        strip_cmd=[strip_tool,'--strip-all','-o',out,raw]
        run_cmd(strip_cmd,timeout=60)
        standard=lambda path:[r for r in sections(path) if r['kind']!=0]
        if standard(raw)!=standard(out): raise ValueError('Metadata normalization changed a standard Wasm section')
        normalization=dict(command=[str(x) for x in strip_cmd],raw_sha256=sha(raw),standard_sections_unchanged=True,
            purpose='Remove non-executable debug/name metadata for byte-for-byte rebuilds across output filenames')
    macros=run_cmd([compiler,'-std=c++20','-msimd128','-fwasm-exceptions','-I'+str(PROJECT/'sdk/cpp'),
        *([f'-D{k}={v}' for k,v in flags.items()] if source!='bot_v34' and not materialized else []),'-dM','-E','-x','c++',dest/'src/common.h'],timeout=60)
    if materialized:
        for key, value in flags.items():
            if not re.search(r'^#define '+re.escape(key)+r' '+str(value)+r'$', macros, re.M):
                raise ValueError('Packaged source default differs from selected configuration: '+key)
    (dest/'preprocessor.txt').write_text(macros)
    record=dict(id=name,created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,
        source_tree=tree(dest/'src'),sdk_tree=tree(PROJECT/'sdk/cpp'),switches=flags if source!='bot_v34' else {},
        all_apex_nova_v36_macros=[x for x in macros.splitlines() if re.match(r'#define (APEX_|NOVA_|V36_)',x)],
        compiler=run_cmd([compiler,'--version']),cli=run_cmd([CLI,'--version']).strip(),cli_sha256=sha(CLI),
        sdk_config=(PROJECT/'cambc.toml').read_text(),command=[str(x) for x in cmd],artifact=str(out),artifact_sha256=sha(out),
        release=True,materialized_source_defaults=materialized,metadata_normalization=normalization,
        fuel='runner default (no fuel override)',seconds=time.monotonic()-start)
    write(dest/'manifest.json',record); print(json.dumps(dict(build=name,sha256=record['artifact_sha256'],seconds=record['seconds'])),flush=True)
    return record

def manifest(name): return json.loads((ROOT/'builds'/name/'manifest.json').read_text())
def freeze(schedule_id, candidate, opponents, maps, purpose):
    dest=ROOT/'schedules'/(schedule_id+'.json')
    if dest.exists(): raise ValueError('Frozen schedule already exists')
    specs=[]; seen=set(); seen_jobs=set(); duplicates=0; cli_digest=sha(CLI)
    for m in maps:
        h=sha(m)
        if h in seen: raise ValueError('Duplicate map hash')
        seen.add(h)
        for opp in opponents:
            for side in [0,1]:
                a,b=(candidate,opp) if side==0 else (opp,candidate)
                ma,mb=manifest(a),manifest(b)
                key=[ma['artifact_sha256'],mb['artifact_sha256'],h,cli_digest,'release-default-full-v1']
                job_id=hashlib.sha256(json.dumps(key).encode()).hexdigest()
                if job_id in seen_jobs:
                    duplicates+=1
                    continue
                seen_jobs.add(job_id)
                specs.append(dict(id=job_id,a=a,b=b,
                    sha_a=key[0],sha_b=key[1],map=str(pathlib.Path(m).resolve()),map_sha256=h,
                    group_id=h,side=side,seed=None,variation='fixed deterministic map and side; no match seed option'))
    if sha(CLI)!=cli_digest: raise ValueError('Runner changed while freezing schedule')
    row=dict(id=schedule_id,purpose=purpose,created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),candidate=candidate,
        opponents=opponents,distinct_maps=len(seen),games=len(specs),duplicate_deterministic_jobs_removed=duplicates,independent_map_groups=len(seen),
        protocol='Full matches at default fuel in release. No deterministic rerun counted as new evidence.',jobs=specs)
    write(dest,row); return row

def fuel(s):
    return {side:sum(len(e.get('entries',[])) for e in s.get('events',[]) if e.get('kind')=='fuelExhausted' and e.get('owner',{}).get('team')==side)
            for side in ['teamA','teamB']}
def game(job):
    dest=ROOT/'runs'/job['id']; result=dest/'result.json'
    for n,h in [(job['a'],job['sha_a']),(job['b'],job['sha_b'])]:
        if sha(manifest(n)['artifact'])!=h: raise ValueError('Build changed')
    if sha(job['map'])!=job['map_sha256']: raise ValueError('Map changed')
    if result.exists():
        row=json.loads(result.read_text()); stat=dest/'statistics.json'
        if row['id']!=job['id'] or not stat.exists() or sha(stat)!=row['statistics_sha256']: raise ValueError('Corrupt completed result')
        if not list(dest.glob('*.repl26a')): raise ValueError('Completed result missing replay')
        # Artifact-keyed cached facts are contextualized with this schedule's names.
        if [row['sha_a'],row['sha_b'],row['map_sha256']] != [job['sha_a'],job['sha_b'],job['map_sha256']]: raise ValueError('Cached identity mismatch')
        return dict(row, a=job['a'], b=job['b'], side=job['side'])
    if dest.exists() and any(dest.iterdir()):
        quarantine=ROOT/'exclusions'/(job['id']+'-'+str(time.time_ns())); quarantine.parent.mkdir(exist_ok=True); dest.rename(quarantine)
    dest.mkdir(parents=True,exist_ok=True); stat=dest/'statistics.json'
    cmd=[CLI,'run',manifest(job['a'])['artifact'],manifest(job['b'])['artifact'],'--map',job['map'],
         '-o',dest,'--statistics',stat,'--json','--release']
    start=time.monotonic(); log=run_cmd(cmd,cwd=PROJECT,timeout=300); (dest/'runner.log').write_text(log)
    s=json.loads(stat.read_text()); match=s['match']
    if match.get('outcome') not in ['teamA','teamB','draw','tie']: raise ValueError('Unknown match outcome '+str(match.get('outcome')))
    if not list(dest.glob('*.repl26a')): raise ValueError('No replay')
    row=dict(job,command=[str(x) for x in cmd],seconds=time.monotonic()-start,scores=[int(t['submittedGems']) for t in s['teams']],
        outcome=match['outcome'],rounds=match['roundsStarted'],engine=match['engineBuildHash'],
        faults=s['diagnostics']['botFaults'],fuel_exhaustions=fuel(s),statistics=str(stat),statistics_sha256=sha(stat),
        raw_match=match)
    write(result,row); return row

def summary(rows,candidate):
    if rows and all(r['sha_a']==r['sha_b'] for r in rows):
        return dict(kind='self-play diagnostics only',games=len(rows),independent_map_groups=len({r['group_id'] for r in rows}),
            strength_estimate=None, fuel_exhaustion_events={side:sum(r['fuel_exhaustions'][side] for r in rows) for side in ['teamA','teamB']},
            faults={side:sum(r['faults'][side] for r in rows) for side in ['teamA','teamB']},engines=sorted({r['engine'] for r in rows}))
    clusters={}; points=[]; margins=[]
    for r in rows:
        if candidate not in [r['a'],r['b']]: raise ValueError('Candidate absent from result assignment')
        i=0 if r['a']==candidate else 1
        p=.5 if r['outcome'] not in ['teamA','teamB'] else float(r['outcome']==['teamA','teamB'][i])
        margin=int(r['scores'][i])-int(r['scores'][1-i]); points.append(p); margins.append(margin)
        clusters.setdefault(r['group_id'],[]).append((p,margin))
    means=[(sum(x[0] for x in v)/len(v),sum(x[1] for x in v)/len(v)) for _,v in sorted(clusters.items())]
    avg=sum(points)/len(points) if points else None; gem=sum(margins)/len(margins) if margins else None
    ci=None; gemci=None
    if len(means)>=2:
        rng=random.Random(361019); boot=[]; boots=[]
        for _ in range(5000):
            sample=rng.choices(means,k=len(means)); boot.append(sum(x[0] for x in sample)/len(sample)); boots.append(sum(x[1] for x in sample)/len(sample))
        boot.sort(); boots.sort(); ci=[boot[125],boot[4874]]; gemci=[boots[125],boots[4874]]
    return dict(candidate=candidate,games=len(rows),map_clusters=len(clusters),wins=points.count(1),ties=points.count(.5),losses=points.count(0),
        win_points=avg,cluster_bootstrap95=ci,mean_submitted_gem_margin=gem,gem_margin_cluster_bootstrap95=gemci,
        interpretation='Descriptive matched-map clusters; no significance or general-strength claim. A single map has no interval.',
        fuel_exhaustion_events=sum(r['fuel_exhaustions']['teamA' if r['a']==candidate else 'teamB'] for r in rows),
        faults=sum(r['faults']['teamA' if r['a']==candidate else 'teamB'] for r in rows),engines=sorted({r['engine'] for r in rows}))

def execute(schedule_id, workers=3):
    sched=json.loads((ROOT/'schedules'/(schedule_id+'.json')).read_text()); rows=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures=[pool.submit(game,j) for j in sched['jobs']]
        for f in concurrent.futures.as_completed(futures):
            rows.append(f.result()); write(ROOT/'results'/(schedule_id+'.json'),rows)
            if len(rows)%10==0: print(json.dumps(dict(schedule=schedule_id,completed=len(rows),total=sched['games'])),flush=True)
    if len(rows)!=sched['games'] or {r['id'] for r in rows}!={j['id'] for j in sched['jobs']}: raise ValueError('Incomplete/duplicate schedule')
    report=summary(rows,sched['candidate']); write(ROOT/'results'/(schedule_id+'-summary.json'),report); print(json.dumps(report),flush=True)
    return rows

if __name__=='__main__':
    p=argparse.ArgumentParser(); sub=p.add_subparsers(dest='command',required=True)
    b=sub.add_parser('build'); b.add_argument('name'); b.add_argument('--source',default='bot_v36'); b.add_argument('--settings',default='{}')
    r=sub.add_parser('run'); r.add_argument('schedule'); r.add_argument('--workers',type=int,default=3)
    a=p.parse_args()
    if a.command=='build': build(a.name,json.loads(a.settings),a.source)
    else: execute(a.schedule,a.workers)
