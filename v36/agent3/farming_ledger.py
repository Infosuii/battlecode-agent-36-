"""Descriptive ledger over Agent 4's official-engine compact replay schema.

No policy inputs, match-strength estimate or inferred mode/death-cause labels.
Usage: farming_ledger.py decoded.json.gz [...] --out ledger.json
"""
import argparse, collections, gzip, hashlib, json, pathlib

CHECKPOINTS=(50,100,150,300)

def attach_native(row, identity, execution_root):
    """Join hash-verified native statistics, preserving event input/ordinal identity."""
    assert row['decoded_sha256']==identity['sha256']
    assert row['raw_replay_sha256']==identity['replay_sha256']
    job=execution_root/'runs'/identity['baseline_job_id']
    result=json.loads((job/'result.json').read_text())
    stats_path=job/'statistics.json'
    assert hashlib.sha256(stats_path.read_bytes()).hexdigest()==result['statistics_sha256']
    stats=json.loads(stats_path.read_text())
    assert list(map(int,result['scores']))==[t['submitted_gems_final'] for t in row['teams']]
    native=collections.defaultdict(list)
    for bucket in stats['events']:
        kind=bucket['kind'];owner=bucket.get('owner',{})
        if kind not in ('campClear','gemCollection','gemSubmission','gemLoss','death'):continue
        for entry in bucket.get('entries',[]):
            native[(owner.get('team'),kind)].append(dict(owner=owner,entry=entry))
    for team,side in zip(row['teams'],('teamA','teamB')):
        clears=native[(side,'campClear')]
        collections_=[r for r in native[(side,'gemCollection')] if r['entry'].get('context')=='campGroup']
        submissions=native[(side,'gemSubmission')]
        losses=native[(side,'gemLoss')]
        def when(r):return r['entry']['timestamp']['round']
        def amount(r):return float(r['entry']['value'])
        def capture_key(r):
            e=r['entry'];return (e['target']['id'],e['timestamp']['input'])
        def capture_count(rows):return len({capture_key(r) for r in rows})
        team['native']=dict(
            first_camp_clear_round=min(map(when,clears),default=None),
            first_reward_collection_round=min(map(when,collections_),default=None),
            first_gem_submission_round=min(map(when,submissions),default=None),
            captures=capture_count(collections_),collected_camp_gems=sum(map(amount,collections_)),
            submitted_gems=sum(map(amount,submissions)),gem_loss_event_sum=sum(map(amount,losses)),
            camp_clear_events_raw=clears,reward_collection_events_raw=collections_,
            submission_events_raw=submissions,gem_loss_events_raw=losses,
        )
        # Native completion events must reconcile with the actual secured score.
        assert abs(team['native']['submitted_gems']-team['submitted_gems_final'])<.001
        for checkpoint in CHECKPOINTS:
            snapshot=team['checkpoints'][str(checkpoint)]
            if not snapshot['available']:continue
            at=[r for r in collections_ if when(r)<=checkpoint]
            snapshot['native_captures']=capture_count(at)
            snapshot['native_collected_camp_gems']=sum(map(amount,at))
            snapshot['native_submitted_gems']=sum(amount(r) for r in submissions if when(r)<=checkpoint)
            snapshot['native_camp_clear_event_count']=sum(when(r)<=checkpoint for r in clears)
    row['identity']=dict(map=identity['map'],job_id=identity['baseline_job_id'],
        artifact_names=[result['a'],result['b']],artifact_sha256=[result['sha_a'],result['sha_b']],
        map_sha256=result['map_sha256'],engine=result['engine'],statistics_sha256=result['statistics_sha256'])
    row['limitations'].append('Native captures deduplicate per team (campGroup ID,input) and sum per-body awarded gem values; zero-gem camp collections are retained.')
    return row

def ledger(path):
    path=pathlib.Path(path)
    data=json.loads(gzip.decompress(path.read_bytes()) if path.suffix=='.gz' else path.read_text())
    assert data['schema']==1 and data['abi']==1, 'Unsupported decode schema'
    assert len(data['metrics'])==2 and len(data['scores'])==2
    for row in data['round_states']:
        assert len(row)==20 and row[2] in (0,1), 'Unexpected champion observation schema'
    events=data['events'];transitions=data['group_transitions'];deaths=data['deaths']
    grouped=collections.defaultdict(list)
    for row in data['round_states']:grouped[row[0]].append(row)
    rounds=sorted(grouped)
    teams=[]
    for side in (0,1):
        m=data['metrics'][side]
        own_events=[e for e in events if e[3]==1 and e[5]==side]
        own_deaths=[d for d in deaths if d[2]==side]
        # Group-state capture_side is retained raw. Association needs verified enum
        # semantics before calling a transition a particular team's capture.
        curve=m['banked_curve']
        checkpoints={}
        for checkpoint in CHECKPOINTS:
            if data['rounds']<checkpoint:
                checkpoints[str(checkpoint)]={'available':False,'reason':'match ended before checkpoint'}
                continue
            r=max((r for r in rounds if r<=checkpoint),default=None)
            if r is None:
                checkpoints[str(checkpoint)]={'available':False,'reason':'no preceding observed round'}
                continue
            bodies=[dict(id=s[1],type=s[3],x=s[4],y=s[5],hp=s[6],max_hp=s[7],alive=bool(s[8]),gold=s[9],gems=s[10],channel_type=s[14],channel_timer=s[15]) for s in grouped[r] if s[2]==side]
            bank=max((row for row in curve if row[0]<=checkpoint),default=None,key=lambda row:row[0])
            checkpoints[str(checkpoint)]={
                'available':True,'observation_round':r,'submitted_gems':bank[1] if bank else None,
                'bodies':bodies,'body_carried_gem_sum':sum(s['gems'] for s in bodies),
                'body_gold_sum':sum(s['gold'] for s in bodies),
                'body_deaths_through_checkpoint':sum(d[0]<=checkpoint for d in own_deaths),
                'recall_action_events':sum(e[2]==11 and e[1]<=checkpoint for e in own_events),
                'shop_action_events':sum(e[2]==13 and e[1]<=checkpoint for e in own_events),
                'tribute_completion_events':sum(e[2]==18 and e[1]<=checkpoint for e in own_events),
            }
        camp_hits=[e for e in own_events if e[15]]
        positive_flows=[f for f in data['gem_flows'] if f[2]==side and f[3]>0]
        tribute_starts=[e for e in own_events if e[2]==12]
        tribute_completions=[e for e in own_events if e[2]==18]
        teams.append(dict(
            side=side,submitted_gems_final=data['scores'][side],checkpoints=checkpoints,
            first_direct_camp_damage_round=min((e[1] for e in camp_hits),default=None),
            first_tribute_start_round=min((e[1] for e in tribute_starts),default=None),
            first_tribute_completion_round=min((e[1] for e in tribute_completions),default=None),
            first_positive_carried_gem_flow_round=min((f[0] for f in positive_flows),default=None),
            body_deaths=len(own_deaths),body_deaths_at_camp_actor_event=sum(d[6]==2 for d in own_deaths),
            gems_carried_at_death=sum(d[5] for d in own_deaths),deaths_raw=own_deaths,
            resource_counters_from_decoder={key:m.get(key) for key in ['captures','captured_gems','camp_damage_taken','champion_damage_taken','gems_lost_on_death']},
            purchases_raw=[p for p in data['purchases'] if p[2]==side],
            actions_by_kind=dict(collections.Counter(str(e[2]) for e in own_events)),
        ))
    return dict(
        decoded_file=str(path.resolve()),decoded_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        raw_replay_sha256=data['sha256'],decode_engine_label=data.get('engine'),rounds=data['rounds'],
        teams=teams,
        group_clear_transitions_raw=[t for t in transitions if t[2]==1 and t[3]==2],
        reward_collection_transitions_raw=[t for t in transitions if t[2]==2 and t[3]==3],
        limitations=[
            'Descriptive omniscient engine replay; artifact/map/team identities must be joined separately.',
            'Body resource sums retain per-body observations and do not deduplicate any linked/shared resource semantics.',
            'Group transitions have round but no event step; capture side enum retained raw until verified.',
            'Positive gem flows include sources other than camp captures; first positive flow is not first capture.',
            'Tribute start kind12 and completion kind18 are distinct; score curve is secured resource evidence.',
            'Camp-actor death-event count is a raw association; linked incident and causal damage attribution need event/state oracle.',
            'Action-event counts are not rounds of downtime; travel/staging/healing/idle/claims require bot telemetry or further observation reconstruction.',
            'Decoder historical aggregate metrics are retained with provenance and have not been independently rederived here.',
        ],
    )

def main():
    p=argparse.ArgumentParser();p.add_argument('decoded',nargs='*');p.add_argument('--out',required=True)
    p.add_argument('--identity-manifest')
    a=p.parse_args()
    if a.identity_manifest:
        identities=json.loads(pathlib.Path(a.identity_manifest).read_text())['maps']
        execution_root=pathlib.Path(__file__).resolve().parents[1]
        rows=[attach_native(ledger(r['decoded']),r,execution_root) for r in identities]
    else:
        if not a.decoded:p.error('provide decoded files or --identity-manifest')
        rows=[ledger(x) for x in a.decoded]
    out=pathlib.Path(a.out);out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(dict(schema=1,matches=rows),indent=2))
    print(json.dumps(dict(matches=len(rows),output=str(out),checkpoint_rows=sum(sum(c['available'] for c in t['checkpoints'].values()) for r in rows for t in r['teams']))))

if __name__=='__main__':main()
