"""Compare two matched schedules, retaining map clusters across opponents/sides."""
import argparse,collections,json,pathlib,random,statistics
from evaluate import ROOT,write

def boot(values,seed=361020,reps=10000):
    if len(values)<2:return None
    r=random.Random(seed);v=sorted(sum(r.choices(values,k=len(values)))/len(values) for _ in range(reps))
    return [v[250],v[9749]]
def normal(rows,name):
    out={}
    for row in rows:
        if name not in [row['a'],row['b']]:raise ValueError('Unrelated result')
        i=0 if row['a']==name else 1;opponent=row['b'] if i==0 else row['a'];key=(row['map_sha256'],opponent,i)
        if key in out:raise ValueError('Duplicate stratum')
        point=.5 if row['outcome'] not in ['teamA','teamB'] else float(row['outcome']==['teamA','teamB'][i])
        out[key]=dict(points=point,gems=int(row['scores'][i])-int(row['scores'][1-i]),fuel=row['fuel_exhaustions'][['teamA','teamB'][i]],
            engine=row['engine'],job=row['id'])
    return out
def compare(cand_schedule,baseline_schedule,outfile):
    cs=json.loads((ROOT/'schedules'/(cand_schedule+'.json')).read_text());bs=json.loads((ROOT/'schedules'/(baseline_schedule+'.json')).read_text())
    cr=json.loads((ROOT/'results'/(cand_schedule+'.json')).read_text());br=json.loads((ROOT/'results'/(baseline_schedule+'.json')).read_text())
    if len(cr)!=cs['games'] or len(br)!=bs['games']:raise ValueError('Incomplete schedule')
    for schedule, rows in [(cs,cr),(bs,br)]:
        jobs={j['id']:j for j in schedule['jobs']}
        if len(jobs)!=len(rows) or {r['id'] for r in rows}!=set(jobs): raise ValueError('Result IDs differ from frozen schedule')
        for row in rows:
            job=jobs[row['id']]
            if any(row[k]!=job[k] for k in ['a','b','sha_a','sha_b','map_sha256','group_id','side']):
                raise ValueError('Result identities differ from frozen job')
    c=normal(cr,cs['candidate']);b=normal(br,bs['candidate'])
    if c.keys()!=b.keys():raise ValueError('Unmatched schedules')
    clusters=collections.defaultdict(list);strata=collections.defaultdict(list)
    for key in sorted(c):
        if c[key]['engine']!=b[key]['engine']:raise ValueError('Engine mismatch')
        difference={k:c[key][k]-b[key][k] for k in ['points','gems','fuel']}
        clusters[key[0]].append(difference);strata[key[1]].append(dict(candidate=c[key],baseline=b[key],difference=difference))
    means={m:{k:statistics.mean(v[k] for v in rows) for k in ['points','gems','fuel']} for m,rows in sorted(clusters.items())}
    result=dict(candidate=cs['candidate'],baseline=bs['candidate'],candidate_schedule=cand_schedule,baseline_schedule=baseline_schedule,
        matched_games_per_build=len(c),independent_map_clusters=len(means),fixed_opponents=len(strata),
        paired_effect={k:dict(mean=statistics.mean(v[k] for v in means.values()),map_cluster_bootstrap95=boot([v[k] for v in means.values()])) for k in ['points','gems','fuel']},
        per_opponent=[dict(opponent=n,games=len(rows),candidate_win_points=statistics.mean(r['candidate']['points'] for r in rows),
            baseline_win_points=statistics.mean(r['baseline']['points'] for r in rows),delta=statistics.mean(r['difference']['points'] for r in rows),
            gem_delta=statistics.mean(r['difference']['gems'] for r in rows)) for n,rows in sorted(strata.items())],
        caveat='Conditional fixed local opponents and these maps; map-cluster resampling preserves all paired opponent/side comparisons. No Elo/general ladder-strength estimate.')
    write(ROOT/'results'/outfile,result);print(json.dumps(result),flush=True);return result

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('candidate');p.add_argument('baseline');p.add_argument('--out',required=True);a=p.parse_args();compare(a.candidate,a.baseline,a.out)
