"""Read charged farming diagnostics with official NUL/input stdout framing."""
import collections,hashlib,json,pathlib,re,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/agent4'))
from stdout_framing import frames
from replay_bundle import read_bundle
score_re=re.compile(r'FARM_SCORE R(\d+) slot(\d+) considered(\d+) pruned(\d+) work_capped(\d+) evaluated(\d+) scored(\d+) best(-?\d+) score([^\s]+) exhaustive(\d+)')
cover_re=re.compile(r'FARM_COVER R(\d+) slot(\d+) G(\d+) ally(\d+) ally_slot(-?\d+) ally_claim(-?\d+) target(-?\d+)')
turn_re=re.compile(r'FARM_TURN R(\d+) slot(\d+) mode(\S+) target(-?\d+) x(-?\d+) y(-?\d+) hp([^\s]+) gold(\d+) gems(\d+)')
rows=json.loads((ROOT/'results/bound-diagnostics.json').read_text())
reports=[]
for row in rows:
    side='a' if row['a']=='bound-diag' else 'b';team='team'+side.upper()
    path=ROOT/'runs'/row['id'];members=read_bundle(next(path.glob('*.repl26a')))
    raw=members['team-'+side+'.stdout'];parsed=frames(raw);scores=[];covers=[];turns=[];failures=[]
    for frame in parsed:
        context={'input_position':frame['position']}
        text=frame['text']
        for m in score_re.finditer(text):
            names=['round_label','slot','considered','pruned','work_capped','evaluated','scored','best','score','exhaustive']
            vals=[int(v) if i!=8 else float(v) for i,v in enumerate(m.groups())]
            scores.append(dict(zip(names,vals),**context))
        for m in cover_re.finditer(text):
            covers.append(dict(zip(['round_label','slot','group','ally','ally_slot','ally_claim','target'],map(int,m.groups())),**context))
        for m in turn_re.finditer(text):
            values=m.groups();turns.append(dict(round_label=int(values[0]),slot=int(values[1]),mode=values[2],target=int(values[3]),x=int(values[4]),y=int(values[5]),hp=float(values[6]),gold=int(values[7]),gems=int(values[8]),**context))
        if 'FARM_BOUND_FAILURE' in text:failures.append(dict(context,text=text))
    stats_path=path/'statistics.json';assert hashlib.sha256(stats_path.read_bytes()).hexdigest()==row['statistics_sha256']
    stats=json.loads(stats_path.read_text())
    events=[dict(owner=b['owner'],entry=e) for b in stats['events'] if b['kind']=='fuelExhausted' and b['owner'].get('team')==team for e in b['entries']]
    reports.append(dict(job=row['id'],map=pathlib.Path(row['map']).stem,team=team,artifact_sha256=row['sha_a'] if side=='a' else row['sha_b'],stdout_sha256=hashlib.sha256(raw).hexdigest(),stdout_bytes=len(raw),stdout_frames=len(parsed),
        score_records=len(scores),score_totals={key:sum(r[key] for r in scores) for key in ['considered','pruned','work_capped','evaluated','scored']},bound_failures=failures,native_faults=row['faults'][team],
        early_fuel_exhaustions=[e for e in events if e['entry']['timestamp']['round']<=60],
        scores=scores,covers=covers,turns=turns))
output=dict(scope='Charged SDK-state diagnostics; not strength/fuel-neutrality evidence. Labels are bot-stored round/slot; official NUL/input positions retained, awaiting-controller join not rederived here.',matches=reports,
    total_score_records=sum(r['score_records']for r in reports),total_final_scores_asserted=sum(r['score_totals']['scored']for r in reports),bound_failures=sum(len(r['bound_failures'])for r in reports),native_faults=sum(r['native_faults']for r in reports))
out=ROOT/'agent3/evidence/actual-state-diagnostics.json';out.write_text(json.dumps(output,indent=2))
print(json.dumps({key:value for key,value in output.items()if key!='matches'},indent=2))
for r in reports:
    if r['map']=='rows':
        print(json.dumps(dict(job=r['job'],team=r['team'],cover_records_26_27=[c for c in r['covers']if 26<=c['round_label']<=27],early_fuel_exhaustions=r['early_fuel_exhaustions'])))
