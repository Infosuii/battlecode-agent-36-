"""Join verified official NUL log positions to actual controller turns; no match execution."""
import argparse,bisect,collections,json,pathlib,re
parser=argparse.ArgumentParser();parser.add_argument('--schedule',default='runtime-telemetry');parser.add_argument('--candidate',default='telemetry');parser.add_argument('--frames',default='stdout_framing');args=parser.parse_args()
ROOT=pathlib.Path(__file__).resolve().parents[1]
FRAMES=ROOT/'evidence/agent4'/args.frames
OUT=ROOT/'agent2/evidence'/('telemetry' if args.schedule=='runtime-telemetry' else args.schedule);OUT.mkdir(parents=True,exist_ok=True)
results=json.loads((ROOT/'results'/(args.schedule+'.json')).read_text())
RB=re.compile(r'RB r(\d+) stage(\w+) cost(\d+) left(\d+) suspended([01])')
def dist(values):
 v=sorted(values)
 return dict(records=len(v),p50=v[len(v)//2],p90=v[min(len(v)-1,int(.9*len(v)))],p99=v[min(len(v)-1,int(.99*len(v)))],max=v[-1],min=v[0]) if v else dict(records=0)
games=[];all_fuel=[];all_stages=[];all_refusals=[]
for row in results:
 d=json.loads((FRAMES/row['id']/'framed_stdout.json').read_text())
 team=0 if row['a']==args.candidate else 1;team_name=['teamA','teamB'][team]
 stat=json.loads(pathlib.Path(row['statistics']).read_text())
 turns=[x for x in d['controller_turns'] if x['team']==team]
 keys={(x['champion'],x['round']) for x in turns}
 records=[];suspended=[];scopes=collections.defaultdict(list);route_farm=[];refusals=[];cross_frame_lines=0;refusal_prefixes=0
 # A single printf can suspend in the middle of a number. Reassemble only each
 # champion's own ordered output stream and retain both endpoint frame contexts.
 by_body=collections.defaultdict(list)
 for original in d['records']:
  assert original['controller'] is not None and original['controller']['team']==team
  by_body[original['controller']['champion']].append(original)
 for body_frames in by_body.values():
  body_frames.sort(key=lambda f:f['position'])
  starts=[];parts=[];offset=0
  for original in body_frames:
   starts.append(offset);parts.append(original['text']);offset+=len(original['text'])
  frame=dict(text=''.join(parts))
  def locate(offset):return body_frames[bisect.bisect_right(starts,offset)-1]
  refusal_prefixes+=frame['text'].count('RBSKIP ')
  for m in re.finditer(r'RBSKIP r(\d+) stage(\w+) cost(\d+) reserve(\d+)',frame['text']):
   first,last=locate(m.start()),locate(m.end()-1);c=first['controller']
   cross_frame_lines+=first['position']!=last['position']
   start,stage,cost,reserve=m.groups()
   refusals.append(dict(game=row['id'],champion=c['champion'],controller_round=c['round'],reported_round=int(start),stage=stage,cost=int(cost),reserve=int(reserve),position=first['position'],end_position=last['position'],end_controller_round=last['controller']['round']))
  nested_route=[]
  for m in RB.finditer(frame['text']):
   first,last=locate(m.start()),locate(m.end()-1);c=first['controller']
   cross_frame_lines+=first['position']!=last['position']
   start,stage,cost,left,susp=m.groups()
   p=dict(champion=c['champion'],controller_round=c['round'],start_round=int(start),stage=stage,cost=int(cost),left=int(left),suspended=bool(int(susp)),position=first['position'],end_position=last['position'],end_controller_round=last['controller']['round'])
   records.append(p);all_stages.append(p)
   if p['suspended']:suspended.append(p)
   if stage=='turn':scopes[(p['champion'],p['start_round'])].append(p)
   if stage=='route' and not p['suspended']:nested_route.append(p)
   if stage=='farm_score':
    children=[x for x in nested_route if x['start_round']==p['start_round']]
    if not p['suspended'] and children:
     route_farm.append(dict(round=p['start_round'],champion=p['champion'],score_inclusive=p['cost'],route=sum(x['cost'] for x in children),score_excluding_nested_route=p['cost']-sum(x['cost'] for x in children)))
    nested_route=[]
 official={(e['owner']['id'],v['timestamp']['round']) for e in stat['events'] if e['kind']=='fuelExhausted' and e['owner'].get('team')==team_name for v in e['entries']}
 nested_keys={(p['champion'],p['start_round']) for p in suspended}
 whole_keys={(p['champion'],p['start_round']) for p in suspended if p['stage']=='turn'}
 covered=set();outside=[];unexplained=[]
 for key in sorted(official):
  if any(p['champion']==key[0] and p['start_round']<=key[1]<p['controller_round'] for p in suspended if p['stage']=='turn'):
   covered.add(key)
  elif key in scopes and all(not p['suspended'] for p in scopes[key]):
   outside.append(dict(champion=key[0],round=key[1],completed_probe=scopes[key]))
  else:unexplained.append(key)
 fuel=d['fuel'][team_name]
 draft_count=sum(x['team']==team_name for x in stat['roster']['draft'])
 assert len(fuel['values'])==len(turns)+draft_count
 game_fuel=fuel['values'][draft_count:]
 mapped={(x['champion'],x['round']):v for x,v in zip(turns,game_fuel)}
 assert all(mapped[x]==fuel['limits'][team]['turn'] for x in official)
 external=[];external_first=[];negative_external=[]
 for key,ps in scopes.items():
  if key not in mapped:continue
  for p in ps:
   if p['suspended']:continue
   delta=mapped[key]-p['cost']
   if delta<0:negative_external.append(dict(key=key,delta=delta))
   elif key[1]==1:external_first.append(delta)
   else:external.append(delta)
 all_fuel.extend(game_fuel)
 all_refusals.extend(refusals)
 refusal_stages={stage:dict(logged_attempts=sum(p['stage']==stage for p in refusals),
  unique_controller_turns=len({(p['champion'],p['controller_round']) for p in refusals if p['stage']==stage}),
  unique_reported_round_body=len({(p['champion'],p['reported_round']) for p in refusals if p['stage']==stage})) for stage in sorted({p['stage'] for p in refusals})}
 participation={stage:dict(scope_calls=sum(p['stage']==stage for p in records),
  unique_controller_turns=len({(p['champion'],p['controller_round']) for p in records if p['stage']==stage})) for stage in sorted({p['stage'] for p in records})}
 games.append(dict(job=row['id'],candidate_side=team_name,joined_frames=d['joined_frames'],missing_positions=d['missing_positions'],
  actual_controller_turns=len(turns),completed_whole_turn_probes=sum(p['stage']=='turn' for p in records),
  official_exhaustion_events=len(official),official_exhaustion_rate=len(official)/len(turns),
  detected_suspended_invocations=len(whole_keys),nested_suspension_scopes=len(suspended),unique_nested_start_keys=len(nested_keys),
  official_events_covered_by_whole_turn_span=len(covered),official_events_after_completed_probe=len(outside),
  unclassified_official_events=unexplained,after_completed_probe=outside,
  native_gameplay_fuel=dist(game_fuel),native_fuel_limits=fuel['limits'][team],
  native_minus_play_turn_probe=dist(external),first_turn_native_minus_probe=dist(external_first),negative_external_cases=negative_external,
  refusals_by_stage=refusal_stages,participation_by_stage=participation,
  reconstructed_cross_frame_lines=cross_frame_lines,refusal_prefixes=refusal_prefixes,unparsed_refusal_prefixes=refusal_prefixes-len(refusals),
  unique_controller_turns_with_any_refusal=len({(p['champion'],p['controller_round']) for p in refusals}),
  nested_route_farm_records=len(route_farm),farm_excluding_nested_route=dist([x['score_excluding_nested_route'] for x in route_farm])))
total_turns=sum(x['actual_controller_turns'] for x in games);exhaust=sum(x['official_exhaustion_events'] for x in games)
report=dict(games=games,totals=dict(actual_controller_turns=total_turns,official_exhaustion_events=exhaust,
 official_exhaustion_rate=exhaust/total_turns,detected_suspended_invocations=sum(x['detected_suspended_invocations'] for x in games),
 official_events_after_completed_probe=sum(x['official_events_after_completed_probe'] for x in games)),
 native_gameplay_fuel=dist(all_fuel),stage_costs={s:dist([p['cost'] for p in all_stages if p['stage']==s and not p['suspended']]) for s in sorted({p['stage'] for p in all_stages})},
 refusals=dict(logged_attempts=len(all_refusals),unique_stage_controller_turns=len({(p['game'],p['stage'],p['champion'],p['controller_round']) for p in all_refusals}),
  unique_stage_reported_body_rounds=len({(p['game'],p['stage'],p['champion'],p['reported_round']) for p in all_refusals}),
  per_stage={stage:dict(logged_attempts=sum(p['stage']==stage for p in all_refusals),unique_controller_turns=len({(p['game'],p['champion'],p['controller_round']) for p in all_refusals if p['stage']==stage}),
  unique_reported_body_rounds=len({(p['game'],p['champion'],p['reported_round']) for p in all_refusals if p['stage']==stage})) for stage in sorted({p['stage'] for p in all_refusals})}),
 interpretation=['Official visualiser NUL framing plus actual engine awaiting-controller contexts is independently supplied by Agent 4.',
 'Nested suspension scopes are deduplicated by champion/start round. Whole-turn spans may cover multiple official events; do not sum either counter.',
 'Exhaustion after a completed play_turn probe is outside its reported scope, potentially printf/flush/end_turn work; exact substage remains uninstrumented.',
 'Native per-turn fuel mapping verified by draft offset and every official exhausted invocation reading its exact100000 budget.',
 'Instrumented logs affect execution, full-turn native fuel includes work outside the play_turn probe; all first-turn200000 records retained.',
 'Farm-minus-route is a nested-scope subtraction, not an independent worst-case score bound; child diagnostic print overhead stays in parent.',
 'Refusals counted at attempt level and distinct stage/body/controller round; reported admission round is preserved separately for buffered output.',
 'Champion output streams are reassembled across input frames to recover interrupted printf lines; start and end frame contexts are retained.',
 'Accepted admission requests are not all logged, so an attempt-level refusal rate is unavailable. Scope participation includes no-work/refused calls and is not a count of useful completed plans.',
 'Outer farm refusal can coincide with baseline low-field/search fallback; refusal count is not incremental loss of planning.'])
(OUT/'framed-telemetry.json').write_text(json.dumps(report,indent=2))
print(json.dumps(dict(totals=report['totals'],refusals=report['refusals'],games=[{k:g[k] for k in ['job','actual_controller_turns','official_exhaustion_events','detected_suspended_invocations','official_events_after_completed_probe','unclassified_official_events','participation_by_stage']} for g in games],native_gameplay_fuel=report['native_gameplay_fuel']),indent=2))
