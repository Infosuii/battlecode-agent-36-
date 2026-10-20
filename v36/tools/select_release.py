"""Apply the frozen fresh protocol once; do not tune from these results."""
import json, pathlib
from evaluate import ROOT, manifest, sha, summary, write
from paired_compare import compare

def read(path): return json.loads((ROOT/path).read_text())
def validated_rows(sid):
    schedule=read('schedules/'+sid+'.json'); rows=read('results/'+sid+'.json')
    jobs={j['id']:j for j in schedule['jobs']}
    if len(rows)!=schedule['games'] or {r['id'] for r in rows}!=set(jobs): raise ValueError('Incomplete or duplicate result grid')
    for row in rows:
        if any(row[k]!=jobs[row['id']][k] for k in ['a','b','sha_a','sha_b','map_sha256','group_id','side']):
            raise ValueError('Frozen job/result identity differs')
    return rows
def result(sid):
    return summary(validated_rows(sid),read('schedules/'+sid+'.json')['candidate'])
def main():
    protocol=read('FRESH_VALIDATION_PROTOCOL.json')
    receipt=read('evidence/fresh-freeze-receipt.json')
    if receipt['protocol_sha256']!=sha(ROOT/'FRESH_VALIDATION_PROTOCOL.json'):
        raise ValueError('Fresh protocol changed')
    for sid,h in receipt['schedule_sha256'].items():
        if sha(ROOT/'schedules'/(sid+'.json'))!=h: raise ValueError('Fresh schedule changed')
        validated_rows(sid)
    if any(manifest(n)['artifact_sha256']!=h for n,h in protocol['identities'].items()):
        raise ValueError('Frozen fresh artifact changed')
    parity=read('evidence/agent4/cache_off_parity_replay_check.json')
    if parity['games']!=34 or parity['exact_raw_equal']!=34 or not parity['all_score_round_fuel_fault_equal']:
        raise ValueError('Exact OFF parity prerequisite failed')
    for path in ['evidence/agent4/cache_identity_fixtures/draft_module_assertions.json',
                 'evidence/agent4/cache_fault_fixtures/draft_module_assertions.json']:
        if not read(path)['all_passed']: raise ValueError('Native cache prerequisite failed')
    writer=read('agent3/evidence/cache-writer-fixture.json')
    if writer['strategy_sha256']!=manifest('rows-cached')['source_tree']['files']['strategy.h'] or any(r['returncode'] for r in writer['results']) or sum(json.loads(r['stdout'])['writer_checks'] for r in writer['results'])!=110:
        raise ValueError('Actual draft-memory writer prerequisite failed')
    compare('stack-cached-fresh-pool','rows-cached-fresh-pool','fresh-governor-effect.json')
    compare('rows-cached-fresh-pool','baseline-fresh-pool','fresh-rows-effect.json')
    compare('stack-cached-fresh-pool','baseline-fresh-pool','fresh-stack-effect.json')
    official=validated_rows('rows-cached-official')
    outside=[r for r in official if pathlib.Path(r['map']).stem!='rows']
    if len(outside)!=32: raise ValueError('Outside-rows official grid differs')
    outside_summary=summary(outside,'rows-cached')
    def head_ok(sid):
        r=result(sid)
        return r['win_points']>=.5 and r['mean_submitted_gem_margin']>=0 and r['faults']==0
    def pool_ok(file):
        r=read('results/'+file)
        return all(r['paired_effect'][k]['mean']>=0 for k in ['points','gems'])
    def fault_free(candidate):
        for spec in protocol['schedules']:
            for row in read('results/'+spec['id']+'.json'):
                if candidate in [row['a'],row['b']]:
                    side='teamA' if row['a']==candidate else 'teamB'
                    if row['faults'][side]: return False
        return True
    rows_checks=dict(outside_rows=outside_summary['win_points']>=.5 and outside_summary['mean_submitted_gem_margin']>=0 and outside_summary['faults']==0,
        fresh_pool=pool_ok('fresh-rows-effect.json'),fresh_head=head_ok('rows-cached-fresh-head'),
        shift_head=head_ok('rows-cached-shift-validation'),zero_faults=fault_free('rows-cached'))
    rows_eligible=all(rows_checks.values())
    stack_checks=dict(rows_eligible=rows_eligible,pool_vs_rows=pool_ok('fresh-governor-effect.json'),
        pool_vs_baseline=pool_ok('fresh-stack-effect.json'),head_vs_rows=head_ok('stack-fresh-v-rows'),
        head_vs_baseline=head_ok('stack-cached-fresh-head'),shift_head=head_ok('stack-cached-shift-validation'),
        lower_native_pool_events=result('stack-cached-fresh-pool')['fuel_exhaustion_events']<result('rows-cached-fresh-pool')['fuel_exhaustion_events'],
        zero_faults=fault_free('stack-cached'))
    selected='stack-cached' if all(stack_checks.values()) else ('rows-cached' if rows_eligible else 'cache-source-off')
    decision=dict(protocol_sha256=receipt['protocol_sha256'],selected_build=selected,
        selection='baseline behavior (all new switches OFF)' if selected=='cache-source-off' else selected,
        selected_artifact_sha256=manifest(selected)['artifact_sha256'],rows_checks=rows_checks,stack_checks=stack_checks,
        outside_rows_summary=outside_summary,baseline_fallback_identity='Original baseline70139... remains immutable rollback; candidate OFF package must pass exact baseline replay parity.',
        caveat='Frozen point gates select one artifact; no significance or general-strength conclusion. Exact source-default artifact final validation remains required.')
    dest=ROOT/'RELEASE_SELECTION.json'
    if dest.exists() and json.loads(dest.read_text())!=decision: raise ValueError('One-time release selection changed')
    write(dest,decision)
    print(json.dumps(decision),flush=True)

if __name__=='__main__': main()
