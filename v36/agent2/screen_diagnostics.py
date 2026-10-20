"""Read existing fixed screens; native fuel counts and body-death diagnostics, no games."""
import collections,json,pathlib,random,sys,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/agent4'))
from stdout_framing import fuel
def interval(groups,metric):
 values=[sum(r[metric] for r in rows)/len(rows) for rows in groups.values()]
 rng=random.Random(362020);boots=[]
 for _ in range(5000):boots.append(sum(rng.choices(values,k=len(values)))/len(values))
 boots.sort();return [boots[125],boots[4874]]
report=[]
for name in ['flee','model-fix','relevance5','governor']:
 rows=[]
 for i in range(1,5):rows+=json.loads((ROOT/'results'/(name+'-screen-'+str(i)+'.json')).read_text())
 assert len(rows)==400 and len({r['id'] for r in rows})==400
 groups=collections.defaultdict(list);games=[];missing=[]
 totals=dict(candidate_fuel_events=0,control_fuel_events=0,candidate_body_deaths=0,control_body_deaths=0,
  candidate_games_with_fuel_event=0,control_games_with_fuel_event=0,candidate_controller_turns=0,control_controller_turns=0,native_denominator_games=0)
 for row in rows:
  side='teamA' if row['a']==name else 'teamB';other='teamB' if side=='teamA' else 'teamA'
  stat=json.loads(pathlib.Path(row['statistics']).read_text())
  own=row['fuel_exhaustions'][side];opp=row['fuel_exhaustions'][other]
  deaths={t:sum(int(c['metrics']['deaths']) for c in stat['champions'] if c['team']==t) for t in [side,other]}
  game=dict(id=row['id'],candidate_side=side,map_group=row['group_id'],candidate_fuel_events=own,control_fuel_events=opp,
   fuel_difference=own-opp,candidate_body_deaths=deaths[side],control_body_deaths=deaths[other],death_difference=deaths[side]-deaths[other])
  bundle=list(pathlib.Path(row['statistics']).parent.glob('*.repl26a'))
  if len(bundle)==1 and zipfile.is_zipfile(bundle[0]):
   with zipfile.ZipFile(bundle[0]) as z:
    counts={}
    for team in [side,other]:
     member='team-'+('a' if team=='teamA' else 'b')+'.fuel'
     raw=z.read(member);info=fuel(raw)
     draft=sum(p['team']==team for p in stat['roster']['draft'])
     counts[team]=len(info['values'])-draft
     assert counts[team]>0
    game['candidate_controller_turns']=counts[side];game['control_controller_turns']=counts[other]
    totals['candidate_controller_turns']+=counts[side];totals['control_controller_turns']+=counts[other];totals['native_denominator_games']+=1
  else:missing.append(row['id'])
  totals['candidate_fuel_events']+=own;totals['control_fuel_events']+=opp
  totals['candidate_games_with_fuel_event']+=own>0;totals['control_games_with_fuel_event']+=opp>0
  totals['candidate_body_deaths']+=deaths[side];totals['control_body_deaths']+=deaths[other]
  games.append(game);groups[row['group_id']].append(game)
 out=dict(candidate=name,games=len(rows),map_groups=len(groups),totals=totals,
  mean_fuel_event_difference=(totals['candidate_fuel_events']-totals['control_fuel_events'])/len(rows),fuel_difference_cluster95=interval(groups,'fuel_difference'),
  mean_body_death_difference=(totals['candidate_body_deaths']-totals['control_body_deaths'])/len(rows),death_difference_cluster95=interval(groups,'death_difference'),
  candidate_fuel_event_rate=totals['candidate_fuel_events']/totals['candidate_controller_turns'] if not missing else None,
  control_fuel_event_rate=totals['control_fuel_events']/totals['control_controller_turns'] if not missing else None,
  missing_native_bundle_games=missing,receipts=games)
 report.append(out)
outpath=ROOT/'agent2/evidence/screen-diagnostics.json';outpath.write_text(json.dumps(dict(screens=report,
 scope='Fixed same-source paired map-side screens. Opposing control shares each candidate game and reacts to candidate policy; this is not a separate baseline self-play counterfactual. Body deaths count linked bodies separately, not incidents. Native controller denominators use verified77byte fuel header+uint64 records minus actual draft inputs. Repeated selection requires fresh validation.'),indent=2))
print(json.dumps([{k:v for k,v in x.items() if k!='receipts'} for x in report],indent=2))
