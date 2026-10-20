"""One authorized bounded official-runner release diagnostic, not full-match evidence."""
import hashlib,json,pathlib,shutil,subprocess,time,zipfile,re
ROOT=pathlib.Path(__file__).resolve().parents[1];WORK=ROOT.parent
OUT=ROOT/'agent2/evidence/runtime-probe'
OUT.mkdir(parents=True,exist_ok=True)
SDK=WORK/'development/wasi-sdk-33.0-x86_64-windows'
PROJECT=WORK/'development/current';CLI=WORK/'runtime-latest/bin/cambc.exe'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
sources={}
for p in [ROOT/'agent2/runtime_probe.cpp',ROOT/'bot_v36/src/tsearch.h',ROOT/'bot_v36/src/common.h']:
    q=OUT/p.name;shutil.copyfile(p,q);sources[p.name]=sha(q)
artifact=OUT/'runtime-probe.wasm'
cmd=[str(SDK/'bin/clang++.exe'),'-O3','-DNDEBUG','-std=c++20','-msimd128','-mbulk-memory','-mbulk-memory-opt',
    '-fwasm-exceptions','-mllvm','-wasm-use-legacy-eh=false','--no-wasm-opt','-I'+str(PROJECT/'sdk/cpp'),
    '-I'+str(OUT),'-L'+str(SDK/'share/wasi-sysroot/lib/wasm32-wasip1/eh'),'-lunwind',str(OUT/'runtime_probe.cpp'),'-o',str(artifact)]
t=time.monotonic();r=subprocess.run(cmd,capture_output=True,text=True,timeout=300)
(OUT/'build.log').write_text(r.stdout+r.stderr)
record=dict(source_sha256=sources,build_command=cmd,compile_seconds=time.monotonic()-t,build_code=r.returncode)
if r.returncode:raise RuntimeError(r.stderr[-4000:])
map_path=PROJECT/'maps/collision.map26a';run=OUT/'run';run.mkdir(exist_ok=True)
cmd=[str(CLI),'run',str(artifact),str(artifact),'--map',str(map_path),'-o',str(run),'--rounds','14','--release','--statistics',str(run/'statistics.json'),'--json']
t=time.monotonic();r=subprocess.run(cmd,cwd=PROJECT,capture_output=True,text=True,timeout=120)
(OUT/'runner.log').write_text(r.stdout+r.stderr)
record.update(artifact_sha256=sha(artifact),map_sha256=sha(map_path),command=cmd,run_seconds=time.monotonic()-t,run_code=r.returncode,
    scope='Charged release fuel in synthetic tactical scenes, 14-round diagnostic; not full-match, broad worst-case bound or competitive evidence')
if r.returncode:raise RuntimeError(r.stderr[-4000:])
rows=[]
for bundle in run.glob('*.repl26a'):
    with zipfile.ZipFile(bundle) as z:
        for side in ['a','b']:
            raw=z.read('team-'+side+'.stdout').decode('utf-8','replace')
            (OUT/('team-'+side+'.stdout.txt')).write_text(raw)
            for match in re.finditer(r'MODELPROBE r(\d+) n(\d+) h(\d+) fight(\d+) run(\d+) pair(\d+) suspended(\d+)',raw):
                vals=[int(x) for x in match.groups()]
                rows.append(dict(side=side,round=vals[0],units=vals[1],horizon=vals[2],fight_cost=vals[3],run_cost=vals[4],pair_cost=vals[5],suspended=bool(vals[6])))
record['samples']=rows
record['statistics']=json.loads((run/'statistics.json').read_text()) if (run/'statistics.json').exists() else None
record['statistics_status']='Bounded unfinished diagnostics do not export completed-match statistics.'
(OUT/'runtime-probe.json').write_text(json.dumps(record,indent=2))
table=[]
for n in range(2,9):
    for h in [2,3]:
        cells=[x for x in rows if x['units']==n and x['horizon']==h and not x['suspended']]
        table.append(dict(units=n,horizon=h,records=len(cells),pair_costs=sorted({x['pair_cost'] for x in cells})))
print(json.dumps(dict(build_sha256=record['artifact_sha256'],sample_count=len(rows),table=table,scope=record['scope']),indent=2))
