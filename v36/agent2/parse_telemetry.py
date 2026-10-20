"""Summarize bounded charged release instrumentation; nested scopes are not additive."""
import argparse,collections,json,pathlib,re,statistics,zipfile
parser=argparse.ArgumentParser();parser.add_argument('--schedule',default='runtime-telemetry');parser.add_argument('--candidate',default='telemetry');args=parser.parse_args()
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'agent2/evidence'/('telemetry' if args.schedule=='runtime-telemetry' else args.schedule);OUT.mkdir(parents=True,exist_ok=True)
rows=json.loads((ROOT/'results'/(args.schedule+'.json')).read_text())
rb=[];td=[];skips=[];games=[]
def dist(values):
 v=sorted(values)
 return dict(records=len(v),p50=v[len(v)//2],p90=v[min(len(v)-1,int(.9*len(v)))],p99=v[min(len(v)-1,int(.99*len(v)))],max=v[-1]) if v else dict(records=0)
for row in rows:
 side='a' if row['a']==args.candidate else 'b';team='teamA' if side=='a' else 'teamB'
 folder=ROOT/'runs'/row['id'];bundle=next(folder.glob('*.repl26a'))
 with zipfile.ZipFile(bundle) as z:raw=z.read('team-'+side+'.stdout')
 text=raw.decode('utf-8','replace')
 for m in re.finditer(r'RB r(\d+) stage(\w+) cost(\d+) left(\d+) suspended([01])',text):
  r,stage,cost,left,susp=m.groups();rb.append(dict(game=row['id'],round=int(r),stage=stage,cost=int(cost),left=int(left),suspended=bool(int(susp))))
 for m in re.finditer(r'TDV r(\d+) reason(\d+) original(\d+) kept(\d+) dropped(\d+) omit([01]) link([01]) camp([01]) models(\d+) horizon(\d+) pairs(\d+) prep(\d+) pair0(\d+) pair1(\d+)',text):
  v=[int(x) for x in m.groups()];td.append(dict(zip(['round','reason','original','retained','dropped','omitted','link','camp','models','horizon','pairs','prep','pair0','pair1'],v),game=row['id']))
 skips.extend(m.group(0) for m in re.finditer(r'RBSKIP [^\n]*',text))
 stats=json.loads(pathlib.Path(row['statistics']).read_text())
 official=[dict(champion=e['owner']['id'],round=x['timestamp']['round']) for e in stats['events'] if e['kind']=='fuelExhausted' and e['owner'].get('team')==team for x in e['entries']]
 n=sum(c['team']==team for c in stats['roster']['champions'])
 games.append(dict(job=row['id'],side=team,rounds=stats['match']['roundsStarted'],roster_bodies=n,
   champion_round_opportunities=n*stats['match']['roundsStarted'],official_exhaustion_entries=official,
   official_exhaustion_count=len(official),faults=row['faults'][team],stdout_member_bytes=len(raw),
   logfile_capped_suspected=len(raw)>=1048576))
coverage=[]
for n in sorted({d['original'] for d in td}):
 cells=[d for d in td if d['original']==n]
 coverage.append(dict(original_eligible=n,calls=len(cells),completed_comparisons=sum(d['pairs']>0 for d in cells),
  clear_fight=sum(d['reason']==5 for d in cells),clear_retreat=sum(d['reason']==6 for d in cells),
  reason_counts=dict(collections.Counter(d['reason'] for d in cells)),retained_counts=dict(collections.Counter(d['retained'] for d in cells))))
actual=[]
for n in sorted({d['retained'] for d in td if d['pairs']>0}):
 for h in sorted({d['horizon'] for d in td if d['retained']==n and d['pairs']>0}):
  cells=[d for d in td if d['retained']==n and d['horizon']==h and d['pairs']>0]
  costs=[d['pair0'] for d in cells if d['pair0']>0]+[d['pair1'] for d in cells if d['pairs']>1 and d['pair1']>0]
  actual.append(dict(retained=n,horizon=h,predicted_pair=2*2200*n*(h+1),observed_pair=dist(costs),prep=dist([d['prep'] for d in cells])))
report=dict(games=games,stage_costs={stage:dist([r['cost'] for r in rb if r['stage']==stage and not r['suspended']]) for stage in sorted({r['stage'] for r in rb})},
 detected_suspended_turn_scopes=sum(r['stage']=='turn' and r['suspended'] for r in rb),
 nested_suspension_scopes_by_stage=dict(collections.Counter(r['stage'] for r in rb if r['suspended'])),
 turn_scopes=sum(r['stage']=='turn' for r in rb),coverage_by_original_scene=coverage,actual_pair_costs=actual,
 stage_skips=len(skips),runtime_faults=sum(g['faults'] for g in games),
 limitations=['Instrumented same-map two-sided diagnostic, not strength evidence or generalization.',
 'Nested scopes overlap; never sum costs or suspension scopes. Whole-turn cost excludes end_turn.',
 'Only suspended turn scope count is deduplicated across nesting; champion framing join pending.',
 'Official exhaustion events and detected suspended invocations overlap; do not add them.',
 'Original=0 denotes admission refusal before snapshot, not an empty actual scene.',
 'Zero-cost cross-round scopes are excluded from distributions. Print overhead changes instrumentation costs.',
 'Champion-round opportunities includes dead/ineligible champions and final partial round; actual controller-turn denominator pending framing parser.',
 'Refusals are admission-attempt counts; governor-enabled runs require framing to deduplicate body turns and distinguish already-ineligible baseline work.'])
(OUT/'telemetry.json').write_text(json.dumps(report,indent=2))
(OUT/'rb-records.json').write_text(json.dumps(rb))
(OUT/'tdv-records.json').write_text(json.dumps(td))
print(json.dumps(dict(games=[{k:g[k] for k in ['job','official_exhaustion_count','stdout_member_bytes','logfile_capped_suspected']} for g in games],turn_scopes=report['turn_scopes'],stage_skip_attempts=report['stage_skips'],coverage_by_original_scene=coverage),indent=2))
