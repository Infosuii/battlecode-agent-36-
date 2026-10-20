"""Recover already-completed diagnostic output after missing-statistics parser failure. No execution."""
import hashlib,json,pathlib,re,zipfile
OUT=pathlib.Path(__file__).resolve().parent/'evidence/runtime-probe'
rows=[]
bundles=list((OUT/'run').glob('*.repl26a'))
for bundle in bundles:
 with zipfile.ZipFile(bundle) as z:
  for side in ['a','b']:
   raw=z.read('team-'+side+'.stdout').decode('utf-8','replace')
   for m in re.finditer(r'MODELPROBE r(\d+) n(\d+) h(\d+) fight(\d+) run(\d+) pair(\d+) suspended(\d+)',raw):
    v=[int(x) for x in m.groups()]
    rows.append(dict(side=side,round=v[0],units=v[1],horizon=v[2],fight_cost=v[3],run_cost=v[4],pair_cost=v[5],suspended=bool(v[6])))
table=[]
for n in range(2,9):
 for h in [2,3]:
  cells=[x for x in rows if x['units']==n and x['horizon']==h and not x['suspended']]
  table.append(dict(units=n,horizon=h,records=len(cells),pair_costs=sorted({x['pair_cost'] for x in cells})))
record=dict(source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.iterdir() if p.suffix in ['.h','.cpp','.wasm']},
 bundle_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in bundles},samples=rows,table=table,
 scope='One 14-round official-runner release diagnostic with synthetic scenes; 14 scenario cells, repeated same scene records are not independent observations; no completed-match score/fault statistics; no strength or worst-case bound claim',
 recovery='Compile and one bounded run succeeded. Original parser expected absent completed-game statistics; this recovery only reads existing bundle. Reproducible commands are in run_runtime_probe.py; no rerun was performed.')
(OUT/'runtime-probe.json').write_text(json.dumps(record,indent=2))
print(json.dumps(dict(table=table,records=len(rows),suspended=sum(x['suspended'] for x in rows)),indent=2))
