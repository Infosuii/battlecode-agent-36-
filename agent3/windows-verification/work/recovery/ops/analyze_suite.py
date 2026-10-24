"""Audit native results and decode the bundled visualiser's fuel format v0.0.1."""
from pathlib import Path
import argparse,hashlib,json,math,random,re,statistics,struct,zipfile
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'analysis/v36/recovery'
VIS=Path('C:/Users/omdha/AppData/Local/cambc/assets/9f2cf433a04e5a113a5488a236703e1473411d687bb65322214eec98b96fa13a/visualiser/assets/index-oTc0ysQc.js')
sha=lambda b:hashlib.sha256(b).hexdigest()

def percentile(values,q):
    if not values:return None
    x=sorted(values);i=(len(x)-1)*q;a=math.floor(i);b=math.ceil(i)
    return x[a]+(x[b]-x[a])*(i-a)

def fuel(data,stats,team):
    assert len(data)>=77 and struct.unpack_from('<Q',data)[0]==28839526091863649
    assert data[8:11]==bytes([0,0,1]) and data[11:13]==bytes([0,0])
    assert (len(data)-77)%8==0
    both=struct.unpack_from('<8Q',data,13);limits=both[:4] if team=='teamA' else both[4:]
    assert limits==(1000000,100000,200000,100000)
    values=list(struct.unpack('<'+'Q'*((len(data)-77)//8),data[77:]))
    draft=sum(c['team']==team for c in stats['roster']['draft'])
    bodies=sum(c['team']==team for c in stats['roster']['champions'])
    assert draft+stats['match']['roundsCompleted']*bodies<=len(values)<=draft+stats['match']['roundsStarted']*bodies
    first=values[draft:draft+bodies];normal=values[draft+bodies:]
    assert all(v<=200000 for v in first) and all(v<=100000 for v in normal)
    native=sum(len(s['entries']) for s in stats['events'] if s['kind']=='fuelExhausted' and s['owner'].get('team')==team)
    saturated=sum(v==200000 for v in first)+sum(v==100000 for v in normal)
    assert saturated==native,(team,saturated,native)
    return {'draft_records':draft,'first_turn_records':len(first),'normal_turn_records':len(normal),
            'native_exhaustion_events':native,'saturated_fuel_records':saturated,
            'normal_fuel':normal,'first_fuel':first}

def main():
    p=argparse.ArgumentParser();p.add_argument('--suite',default='recovered-development-v3');args=p.parse_args()
    rows=json.loads((OUT/f'{args.suite}-results.json').read_text())['results']
    by={(r['job']['map_index'],r['job']['tag']):r for r in rows}
    n=len(set(r['job']['map_index'] for r in rows))
    parity=[];effects=[];native_faults=0;pergame=[]
    pooled={name:[] for name in ['baseline','off','on','diagnostic']}
    checkpoints=[];bound_violations=0
    for row in rows:
        folder=ROOT/'work/recovery/results'/row['job']['id']
        stats=json.loads((folder/'stats.json').read_text())
        assert sha((folder/'stats.json').read_bytes())==row['stats_sha256']
        native_faults+=sum(row['summary']['faults'].values())
        bundles=list(folder.glob('*.repl26a'));assert len(bundles)==1
        with zipfile.ZipFile(bundles[0]) as z:
            evidence={}
            for team,letter in [('teamA','a'),('teamB','b')]:
                f=fuel(z.read(f'team-{letter}.fuel'),stats,team)
                tag=row['job']['tag'];profile=row['job']['a' if team=='teamA' else 'b']
                cohort=None
                if tag=='base':cohort='baseline'
                elif tag in ['onA','onB']:cohort='on' if profile.startswith('bound_on') else 'off'
                elif tag in ['diagA','diagB'] and profile.startswith('bound_diag'):cohort='diagnostic'
                if cohort:pooled[cohort]+=f['normal_fuel']
                evidence[team]={k:v for k,v in f.items() if k not in ['normal_fuel','first_fuel']}
                evidence[team].update(normal_p50=percentile(f['normal_fuel'],.5),normal_p95=percentile(f['normal_fuel'],.95),
                                      normal_p99=percentile(f['normal_fuel'],.99),normal_max=max(f['normal_fuel']))
                text=z.read(f'team-{letter}.stdout').decode('utf-8',errors='replace')
                bound_violations+=text.count('FARM_BOUND_VIOLATION')
                for line in re.findall(r'FARM_LEDGER [^\r\n]*',text):
                    checkpoints.append({'job':row['job']['id'],'team':team,'line':line})
            pergame.append({'id':row['job']['id'],'bundle_sha256':sha(bundles[0].read_bytes()),'fuel':evidence})
    for i in range(n):
        base=by[i,'base']['summary']
        for tag in ['offA','offB']:
            other=by[i,tag]['summary'];parity.append({'map':i,'tag':tag,'exact_compact_statistics':base==other})
        a=by[i,'onA']['summary'];b=by[i,'onB']['summary']
        score_a=a['gems']['teamA']-a['gems']['teamB'];score_b=b['gems']['teamB']-b['gems']['teamA']
        won_a=a['match']['outcome']=='teamA';won_b=b['match']['outcome']=='teamB'
        on_fuel=a['fuel_exhausted']['teamA']+b['fuel_exhausted']['teamB']
        off_fuel=a['fuel_exhausted']['teamB']+b['fuel_exhausted']['teamA']
        effects.append({'map':i,'wins':int(won_a)+int(won_b),'games':2,'mean_secured_margin':(score_a+score_b)/2,
                        'on_exhaustion_events':on_fuel,'off_exhaustion_events':off_fuel})
    rng=random.Random(36032026)
    boots=[statistics.mean(rng.choice(effects)['mean_secured_margin'] for _ in effects) for _ in range(10000)]
    report={'suite':args.suite,'games':len(rows),'independent_layout_groups':n,'runtime_faults':native_faults,
            'parity':parity,'on_wins':sum(e['wins'] for e in effects),'on_games':2*n,'per_map':effects,
            'mean_secured_margin':statistics.mean(e['mean_secured_margin'] for e in effects),
            'descriptive_map_cluster_bootstrap_95':[percentile(boots,.025),percentile(boots,.975)],
            'interval_limitations':'Only nine reconstructed development layouts, baseline opponent, no final holdout/general-strength inference.',
            'on_exhaustion_events':sum(e['on_exhaustion_events'] for e in effects),
            'off_exhaustion_events':sum(e['off_exhaustion_events'] for e in effects),
            'fuel_format_reference':{'visualiser_sha256':sha(VIS.read_bytes()),'magic_u64':28839526091863649,'version':[0,0,1],
                'header_bytes':77,'values':'little endian u64; native budget exhaustion equality checked per team/match',
                'normal_denominator':'All recorded champion turns after each first play turn, including dead/shopping turns; no live-only claim'},
            'fuel_distributions':{name:{'turns':len(v),'mean':statistics.mean(v),'p50':percentile(v,.5),
                                       'p95':percentile(v,.95),'p99':percentile(v,.99),'max':max(v)} for name,v in pooled.items()},
            'per_game_fuel_validation':pergame,'diagnostic_bound_violations':bound_violations,
            'diagnostic_ledger_checkpoint_count':len(checkpoints),
            'decision':'Keep bound OFF: correctness passes, competitive improvement not demonstrated; runtime integration still unavailable.'}
    (OUT/f'{args.suite}-analysis.json').write_text(json.dumps(report,indent=2)+'\n')
    (OUT/f'{args.suite}-ledger-checkpoints.json').write_text(json.dumps(checkpoints,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ['games','independent_layout_groups','runtime_faults','on_wins','on_games','mean_secured_margin',
        'descriptive_map_cluster_bootstrap_95','on_exhaustion_events','off_exhaustion_events','diagnostic_bound_violations','fuel_distributions']},indent=2))
    assert all(p['exact_compact_statistics'] for p in parity),'Disabled parity failed'
    assert bound_violations==0
if __name__=='__main__':main()
