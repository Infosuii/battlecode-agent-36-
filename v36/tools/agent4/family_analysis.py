"""Independent sampling audit plus conservative synthetic-template sensitivity.

Reads screen/validation manifests only. Final holdout data/maps are never read.
Intervals describe this declared generator, not competition-map generalisation.
"""
import argparse,collections,hashlib,json,pathlib,random,statistics
V36=pathlib.Path(__file__).resolve().parents[2]
OUT=V36/'evidence/agent4'
def boot(means,reps=10000):
    if len(means)<2:return None
    rng=random.Random(361020);estimates=sorted(statistics.mean(rng.choices(means,k=len(means))) for _ in range(reps))
    return [estimates[int(.025*reps)],estimates[int(.975*reps)-1]]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('schedule');parser.add_argument('--out',required=True);a=parser.parse_args()
    schedule=json.loads((V36/'schedules'/f'{a.schedule}.json').read_text());results=json.loads((V36/'results'/f'{a.schedule}.json').read_text())
    expected={r['id'] for r in schedule['jobs']};actual=[r['id'] for r in results]
    assert len(actual)==len(set(actual)) and set(actual)==expected,'Incomplete/duplicate results are not eligible for final intervals'
    mapping={};stagefamilies={};stagehashes={};stageseeds={}
    for stage in ['screen','validation']:
        meta=json.loads((V36/'maps'/stage/'manifest.json').read_text());families=collections.Counter();hashes=set();seeds=set()
        for row in meta['maps']:
            family=f"{row['width']}x{row['height']}/picks{row['champions']}/{row['style']}"
            mapping[row['sha256']]=dict(family=family,stage=stage,seed=row['seed']);families[family]+=1;hashes.add(row['sha256']);seeds.add(row['seed'])
        stagefamilies[stage]=dict(families);stagehashes[stage]=hashes;stageseeds[stage]=seeds
    candidate=schedule['candidate'];maps=collections.defaultdict(list);strata=collections.defaultdict(list);engines=set();signatures=collections.defaultdict(list)
    for result in results:
        assert candidate in (result['a'],result['b']),'Candidate alias mismatch'
        side=0 if result['a']==candidate else 1;opponent=result['b'] if side==0 else result['a'];engines.add(result['engine'])
        point=.5 if result['outcome'] not in ['teamA','teamB'] else float(result['outcome']==['teamA','teamB'][side])
        margin=int(result['scores'][side])-int(result['scores'][1-side]);m=result['map_sha256'];maps[m].append((point,margin))
        signatures[m].append((opponent,side));strata[(opponent,side)].append((point,margin))
    assert len(engines)==1,'Different engine strata must be analysed separately'
    balance={tuple(sorted(v)) for v in signatures.values()};balanced=len(balance)==1
    mapmeans={k:(statistics.mean(v[0] for v in rows),statistics.mean(v[1] for v in rows)) for k,rows in maps.items()}
    families=collections.defaultdict(list)
    for m,means in mapmeans.items():families[mapping[m]['family'] if m in mapping else 'official:'+m].append((m,*means))
    familyrows=[]
    for family,rows in sorted(families.items()):
        familyrows.append(dict(family=family,maps=len(rows),games=sum(len(maps[r[0]]) for r in rows),win_points=statistics.mean(r[1] for r in rows),mean_gem_margin=statistics.mean(r[2] for r in rows)))
    fsizes={r['maps'] for r in familyrows};fpoint=[r['win_points'] for r in familyrows];fgem=[r['mean_gem_margin'] for r in familyrows]
    report=dict(schedule=a.schedule,candidate=candidate,complete=True,games=len(results),distinct_map_clusters=len(maps),template_strata=len(families),
        balanced_map_opponent_side_strata=balanced,equal_maps_per_template=len(fsizes)==1,engines=list(engines),
        map_win_points=statistics.mean(v[0] for v in mapmeans.values()),map_win_points_bootstrap95=boot([v[0] for v in mapmeans.values()]),
        map_mean_submitted_gem_margin=statistics.mean(v[1] for v in mapmeans.values()),map_gem_bootstrap95=boot([v[1] for v in mapmeans.values()]),
        template_equal_weight_win_points=statistics.mean(fpoint),template_bootstrap95_sensitivity=boot(fpoint),
        template_equal_weight_mean_gem_margin=statistics.mean(fgem),template_gem_bootstrap95_sensitivity=boot(fgem),
        leave_one_template_out_win_points=[statistics.mean([x for j,x in enumerate(fpoint) if j!=i]) for i in range(len(fpoint))] if len(fpoint)>1 else [],
        per_template=familyrows,per_opponent_side=[dict(opponent=k[0],candidate_side=k[1],games=len(v),win_points=statistics.mean(x[0] for x in v),mean_gem_margin=statistics.mean(x[1] for x in v)) for k,v in strata.items()],
        leakage_audit=dict(screen_validation_shared_map_hashes=len(stagehashes['screen']&stagehashes['validation']),
          screen_validation_shared_geometry_seeds=len(stageseeds['screen']&stageseeds['validation']),
          screen_validation_shared_template_strata=sorted(set(stagefamilies['screen'])&set(stagefamilies['validation'])),
          final_holdout='Not opened/read/performance tested by this analysis; preparation source declares the same template list and generator.'),
        interpretation=['Map bootstrap conditions on this fixed separated-camp generator and its declared size mixture.',
          'Template grouping is a conservative sensitivity to shared dimensions/pick count/style, not a proof of ten independent competition-map families.',
          'Screen and fresh validation use different seeds/hashes but share all ten templates, same spawn/camp/altar placement mechanism and wall generator; this is within-generator validation, not unseen-family validation.',
          'Repeated side games are coupled within the map; deterministic reruns and aliases do not increase sample size.',
          'Direct candidate-vs-OFF head-to-head points do not establish a varied-pool improvement. Selection among several variants requires once-only fresh validation.',
          'Geometry seed is not a runner match seed. No historical maps_rdev/maps_real or requested seven calibrated opponents have been recovered.'])
    pathlib.Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['schedule','games','distinct_map_clusters','template_strata','balanced_map_opponent_side_strata','map_win_points','map_win_points_bootstrap95','template_bootstrap95_sensitivity','template_gem_bootstrap95_sensitivity']}))
if __name__=='__main__':main()
