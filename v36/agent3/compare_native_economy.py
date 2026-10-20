"""Native clear/collection/submission timing for an immutable result schedule.

One collection is a distinct(group,input); collection-to-last-clear intervals are
observations, not inferred wasted time or proof of who caused the delay.
"""
import argparse,bisect,collections,hashlib,json,pathlib,statistics

def analyze(result):
    stat_path=pathlib.Path(result['statistics'])
    assert hashlib.sha256(stat_path.read_bytes()).hexdigest()==result['statistics_sha256']
    stat=json.loads(stat_path.read_text());events=[]
    for bucket in stat['events']:
        for entry in bucket['entries']:
            if bucket['kind'] in ('campClear','gemCollection','gemSubmission','death','gemLoss'):
                events.append(dict(kind=bucket['kind'],owner=bucket['owner'],entry=entry))
    events.sort(key=lambda e:int(e['entry']['timestamp']['ordinal']))
    clear_by_group=collections.defaultdict(list)
    for e in events:
        if e['kind']=='campClear':clear_by_group[e['entry']['target']['id']].append(e)
    teams=[]
    for side,name in [('teamA',result['a']),('teamB',result['b'])]:
        own=[e for e in events if e['owner'].get('team')==side]
        collected=collections.defaultdict(list)
        for e in own:
            if e['kind']=='gemCollection' and e['entry'].get('context')=='campGroup':
                collected[(e['entry']['target']['id'],e['entry']['timestamp']['input'])].append(e)
        captures=[]
        for (group,input_),evs in collected.items():
            e=evs[0];ts=e['entry']['timestamp'];ordinal=int(ts['ordinal'])
            prev=[c for c in clear_by_group[group] if int(c['entry']['timestamp']['ordinal'])<ordinal]
            last=prev[-1] if prev else None
            captures.append(dict(group=group,input=input_,round=ts['round'],gems=sum(float(x['entry']['value']) for x in evs),
                collector_body_ids=[x['owner']['id']for x in evs],
                clear_round=last['entry']['timestamp']['round'] if last else None,
                clear_team=last['owner'].get('team') if last else None,
                clear_to_collection_rounds=ts['round']-last['entry']['timestamp']['round']if last else None))
        submissions=[e for e in own if e['kind']=='gemSubmission']
        clears=[e for e in own if e['kind']=='campClear']
        through={}
        for cutoff in [50,100,150,300]:
            cap=[c for c in captures if c['round']<=cutoff]
            delay=[c['clear_to_collection_rounds']for c in cap if c['clear_to_collection_rounds']is not None]
            through[cutoff]=dict(captures=len(cap),collected_gems=sum(c['gems']for c in cap),
                submitted_gems=sum(float(e['entry']['value'])for e in submissions if e['entry']['timestamp']['round']<=cutoff),
                median_clear_collection_gap=statistics.median(delay)if delay else None,max_clear_collection_gap=max(delay)if delay else None)
        teams.append(dict(name=name,side=side,first_clear=min((e['entry']['timestamp']['round']for e in clears),default=None),
            first_collection=min((c['round']for c in captures),default=None),first_submission=min((e['entry']['timestamp']['round']for e in submissions),default=None),
            checkpoints=through,captures=captures,final_submitted_gems=int(stat['teams'][0 if side=='teamA'else 1]['submittedGems'])))
    return dict(job=result['id'],map=pathlib.Path(result['map']).stem,engine=result['engine'],map_sha256=result['map_sha256'],artifacts=[result['sha_a'],result['sha_b']],
        statistics=str(stat_path.resolve()),statistics_sha256=result['statistics_sha256'],teams=teams)

def main():
    p=argparse.ArgumentParser();p.add_argument('results');p.add_argument('--out',required=True);a=p.parse_args()
    rows=[analyze(r)for r in json.loads(pathlib.Path(a.results).read_text())]
    out=pathlib.Path(a.out);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(dict(matches=rows,limitations=['Native event association; no inferred exclusive downtime.','Same-map deterministic repeats are not additional independent observations.']),indent=2))
    print(json.dumps([dict(map=r['map'],teams=[{k:t[k]for k in ['name','side','first_clear','first_collection','first_submission','final_submitted_gems']}for t in r['teams']])for r in rows]))

if __name__=='__main__':main()
