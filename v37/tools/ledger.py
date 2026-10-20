"""Extend the inherited farming ledger with native 1.0.1 accounting and quality gates.

Native events are the authoritative resource ledger. Per-body replay observations
are descriptive; linked bodies keep distinct wallets and share incident identity.
"""
import argparse, collections, gzip, json, pathlib, subprocess, zipfile
from lab import ROOT, WS, WORK, ENGINE, sha, write
from replay_bundle import read_bundle
from legacy_farming_ledger import ledger as replay_ledger
NODE=pathlib.Path('C:/Users/ishan/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe')
def analyze(schedule,decode=()):
    results=json.loads((ROOT/'results'/(schedule+'.json')).read_text())
    rows=[];quality=[];losses=collections.Counter();fuel=collections.Counter();index=[]
    for receipt in results:
        j=receipt['assignment'];folder=ROOT/'runs'/j['id'];sp=folder/'statistics.json'
        assert sha(sp)==receipt['raw_hashes']['statistics.json']
        s=json.loads(sp.read_text());assert s['match']['engineBuildHash']==ENGINE
        roster={r['id']:r for r in s['roster']['champions']}
        assert len(roster)==len(s['roster']['champions'])==len(s['champions'])
        assert all(roster[c['id']]['team']==c['team'] and roster[c['id']]['name']==c['name'] for c in s['champions'])
        events=[];seen=set()
        for b in s['events']:
            for e in b['entries']:
                event=dict(kind=b['kind'],owner=b['owner'],**e)
                t=e['timestamp'];assert 0<=t['round']<=s['match']['roundsStarted']
                identity=(b['kind'],b['owner'].get('team'),b['owner'].get('id'),t['input'],t['ordinal'])
                assert identity not in seen,'Duplicate event';seen.add(identity)
                if b['owner'].get('kind')=='champion':assert b['owner']['id'] in roster
                events.append(event)
        events.sort(key=lambda e:(e['timestamp']['round'],e['timestamp']['input'],e['timestamp']['substep'],e['timestamp']['ordinal']))
        teams=[]
        for side in ('teamA','teamB'):
            ev=[e for e in events if e['owner'].get('team')==side]
            champions=[c for c in s['champions'] if c['team']==side]
            def of(kind):return [e for e in ev if e['kind']==kind]
            def amount(kind):return sum(int(e['value']) for e in of(kind))
            def first(kind):return min((e['timestamp']['round'] for e in of(kind)),default=None)
            score=next(int(t['submittedGems']) for t in s['teams'] if t['team']==side)
            assert score==amount('gemSubmission')==sum(int(c['metrics']['submittedGems']) for c in champions)
            collected=amount('gemCollection');lost=amount('gemLoss');carried=sum(int(c['metrics']['gems']) for c in champions)
            residual=collected-score-lost-carried
            assert residual==0,('Resource reconciliation',side,residual)
            for c in champions:
                body=[e for e in ev if e['owner'].get('kind')=='champion' and e['owner'].get('id')==c['id']]
                for kind,metric in [('gemCollection','collectedGems'),('gemSubmission','submittedGems'),('death','deaths')]:
                    assert sum(int(e['value']) for e in body if e['kind']==kind)==int(c['metrics'][metric]),(c['id'],kind)
            native_captures=[e for e in of('gemCollection') if e['context']=='campGroup']
            capture_ids={(e['target']['id'],e['timestamp']['input']) for e in native_captures}
            # Two linked bodies dying in the same engine input is one linked incident, two physical deaths.
            linked=collections.defaultdict(set)
            for e in of('death'):
                c=roster[e['owner']['id']]
                linked[(c['draftPick'],e['timestamp']['input'])].add(c['id'])
            checkpoints={}
            for rd in (50,100,150,300,s['match']['roundsCompleted']):
                at=[e for e in ev if e['timestamp']['round']<=rd]
                checkpoints[str(rd)]=dict(complete_round=rd<=s['match']['roundsCompleted'],
                    collected_gems=sum(int(e['value']) for e in at if e['kind']=='gemCollection'),
                    secured_gems=sum(int(e['value']) for e in at if e['kind']=='gemSubmission'),
                    lost_gems=sum(int(e['value']) for e in at if e['kind']=='gemLoss'),
                    gold_earned=None,gold_missing_reason='Native earnedGold has only final aggregate; replay needed')
            deaths=[dict(body_id=e['owner']['id'],name=e['owner']['name'],cause=e['context'],
                target=e['target'],timestamp=e['timestamp']) for e in of('death')]
            losses['lost_gems']+=lost;losses['end_unsecured_gems']+=carried
            losses['camp_body_deaths']+=sum(e['context']=='camp' for e in of('death'))
            fuel[pathlib.Path(j['map']).stem]+=len(of('fuelExhausted'))
            teams.append(dict(team=side,collected=collected,secured=score,lost=lost,end_bag=carried,residual=residual,
                first_clear=first('campClear'),first_collection=first('gemCollection'),first_secured=first('gemSubmission'),
                capture_count=len(capture_ids),zero_gem_collection_events=sum(int(e['value'])==0 for e in native_captures),
                body_deaths=len(of('death')),death_incidents=len(linked),linked_incidents=sum(len(v)>1 for v in linked.values()),
                checkpoints=checkpoints,death_examples=deaths[:8],
                gold_earned_final=sum(int(c['metrics']['earnedGold']) for c in champions),
                gold_spent_final=sum(int(c['metrics']['spentGold']) for c in champions)))
        row=dict(match=j['id'],map=j['map'],map_sha256=j['group'],engine=s['match'],teams=teams,
                 replay_ledger=None,statistics_sha256=sha(sp))
        if pathlib.Path(j['map']).stem in decode:
            bundle=next(p for p in folder.glob('*.repl26a') if zipfile.is_zipfile(p));raw=read_bundle(bundle)['replay.repl26a']
            rawpath=folder/'raw.repl26a';rawpath.write_bytes(raw)
            dest=ROOT/'decoded'/(j['id']+'.json.gz')
            r=subprocess.run([str(NODE),str(ROOT/'tools/decode.mjs'),str(rawpath),str(dest)],capture_output=True,text=True,timeout=180)
            if r.returncode:raise RuntimeError(r.stderr)
            data=json.loads(gzip.decompress(dest.read_bytes()));assert data['scores']==[t['secured'] for t in teams]
            row['replay_ledger']=replay_ledger(dest)
            assert data['rounds']==s['match']['roundsStarted']
            quality.append(dict(match=j['id'],decoder_scores_and_rounds_match_native=True,decoded_sha256=sha(dest)))
        rows.append(row)
        index.append(dict(match=j['id'],folder=str(folder),raw_hashes=receipt['raw_hashes']))
    write(ROOT/'evidence'/(schedule+'-ledger.json'),dict(schema=2,matches=rows))
    report=dict(matches=len(rows),native_resource_reconciliation='passed every body/team/match',
        duplicate_events='none',team_body_roster_join='passed',partial_final_rounds=sum(r['engine']['finalRoundPartial'] for r in rows),
        losses=dict(losses),fuel_exhaustions_by_map=dict(fuel.most_common()),replay_checks=quality,
        limitations=['Snapshots are omniscient labels only, never decision features.',
            'Raw map file hash differs from canonical engine mapHash; mirrored/name-independent identity still needs physical geometry normalization.',
            'Travel/staging/healing/idle and abandoned jobs require telemetry; not inferred as zero.',
            'Helper claims are not counted as actual participation.',
            'Lost gems are resource loss, not necessarily destroyed: killers may recover bags.',
            'Final checkpoint is last COMPLETE round; match end score includes partial final round separately.',
            'Linked incidents currently group same draftPick/input; event-phase linked causality requires oracle checks.'])
    write(ROOT/'evidence'/(schedule+'-quality.json'),report)
    write(ROOT/'evidence'/(schedule+'-raw-index.json'),index)
    print(json.dumps(report),flush=True)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('schedule');ap.add_argument('--decode',nargs='*',default=[])
    a=ap.parse_args();analyze(a.schedule,a.decode)
