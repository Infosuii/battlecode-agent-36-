"""Close local release evidence from completed frozen results, without new selection."""
import json
from evaluate import ROOT,manifest,sha,tree,write
from paired_compare import normal
from audit_evidence import main as audit
from wasm_compare import sections

def read(p):return json.loads((ROOT/p).read_text())
def main():
    audit();index=read('EXPERIMENT_INDEX.json');choice=read('RELEASE_SELECTION.json');release=read('CANDIDATE_MANIFEST.json')
    final=manifest('release-final');selected=manifest(choice['selected_build']);base=manifest('baseline')
    parity=read('evidence/release-final-parity.json');pool=read('results/final-pool-effect.json')
    head=read('results/release-final-head-summary.json');shift=read('results/release-final-shift-summary.json')
    standard=lambda p:[r for r in sections(p) if r['kind']!=0]
    c=normal(read('results/release-final-pool.json'),'release-final');b=normal(read('results/baseline-final-pool.json'),'baseline')
    identical_strata=c.keys()==b.keys() and all(all(c[k][m]==b[k][m] for m in ['points','gems','fuel']) for k in c)
    old={x.split(' ',2)[1]:x.split(' ',2)[2] for x in base['all_apex_nova_v36_macros'] if len(x.split(' ',2))==3}
    new={x.split(' ',2)[1]:x.split(' ',2)[2] for x in final['all_apex_nova_v36_macros'] if len(x.split(' ',2))==3}
    checks=dict(source_defaults_match_selection=final['switches']==selected['switches'],
        all_new_policy_flags_off=all(v==0 for k,v in final['switches'].items() if k!='NOVA_TS_MAX_UNITS'),
        baseline_tuning_macros_preserved=all(new.get(k)==v for k,v in old.items()),
        no_CAMBC_DEBUG='#define CAMBC_DEBUG' not in (ROOT/'builds/release-final/preprocessor.txt').read_text(),
        selected_executable_sections_equal=standard(final['artifact'])==standard(selected['artifact']),
        archive_rebuild_byte_equal=release['archive_rebuild']==final['artifact_sha256']==sha(ROOT/'release/bot_v36.wasm'),
        original_rollback_unchanged=sha(ROOT/'release/rollback/baseline.wasm')==base['artifact_sha256'],
        exact_official_raw_parity=parity['games']==parity['exact_raw_equal']==34 and parity['all_score_round_fuel_fault_equal'],
        final_head_gate=head['win_points']>=.5 and head['mean_submitted_gem_margin']>=0 and head['faults']==0,
        final_shift_gate=shift['win_points']>=.5 and shift['mean_submitted_gem_margin']>=0 and shift['faults']==0,
        final_pool_gate=all(pool['paired_effect'][k]['mean']>=0 for k in ['points','gems']),
        all320_pool_point_gem_fuel_strata_equal_baseline=identical_strata,
        all_completed_fullmatch_native_fault_entries_zero=all(v==0 for v in index['native_fault_entries'].values()),
        no_incomplete_active_schedules=not index['incomplete_schedules'])
    if not all(checks.values()):raise ValueError('Final review failed: '+str(checks))
    review=dict(checks=checks,selected_build=choice['selected_build'],release_build='release-final',
        artifact_sha256=final['artifact_sha256'],source_tree_sha256=final['source_tree']['sha256'],
        archive_sha256=release['archive_sha256'],unique_fullmatch_jobs=index['unique_completed_full_match_jobs'],
        final_policy='Retain baseline behavior; experimental changes OFF. No measured upgrade claimed.',
        review_scope='Root verified exact package after peer source/interface/protocol reviews. Child reports are development reviews; no unperformed child final-artifact approval is implied.',
        external_gaps=['Original calibrated seven-opponent pool','Authenticated current live pool and submission windows','Historical maps_real/maps_rdev','Competition-family independent holdout','Live ladder A/B'])
    write(ROOT/'evidence/FINAL_RELEASE_REVIEW.json',review)
    text=(ROOT/'VALIDATION_v36.md').read_text()
    text=text.replace('# v36 validation — release selection pending','# v36 validation — baseline behavior retained')
    text=text.replace('This report records local evidence. No candidate has yet passed fresh selection or final exact-artifact holdout. No new live bot was submitted. The user canceled cloud continuation; execution remains local.',
        'Local release preparation is complete. Fresh selection rejected both shortlisted improvements, so all new policy switches remain OFF. The exact source-default package passed final parity and holdout checks and matches baseline results on the measured suite. This is a validated fallback, not a demonstrated upgrade. No live bot was submitted; GitHub code/results publication was explicitly authorized on20 October2026.')
    text=text.replace('Later source freezes require their own parity receipts.','The corrected cache freeze and exact final release also each pass34/34 raw replay checks.')
    text=text.replace('Cached full-match comparisons and fresh selection are pending.','Cached development, one-time fresh selection and exact final evaluation are complete below.')
    marker='## Remaining release gates'
    if marker in text:text=text.split(marker)[0]
    if '## Fresh selection and final exact artifact' in text:text=text.split('## Fresh selection and final exact artifact')[0]
    text+='''## Fresh selection and final exact artifact

The frozen fresh protocol completed760 full games across eight schedules before once-only selection. Rows-only won118/160 local proxy-pool games versusbaseline119/160: matched−0.625 percentage points, map-cluster95% interval[−1.875,0], and mean gem-margin effect−5.2[−17.0875,+1.5625]. The governor stack won114/160; versusbaseline its matched effect was−3.125pp[−11.25,+5.0] and−13.6375g[−69.8125,+39.4875]. It lost its fresh baseline head28–52 and its rows-only head27–53. Both candidates fail the predeclared promotion rules. No threshold was changed or fresh selection repeated.

The official rows mechanism remains an identified improvement on one known map, but is not promoted after the complete candidate fails fresh validation. Synthetic gains must not be attributed to rows-specific intent: all generated layouts are physically distinct from official rows, and added code/checks can affect fuel despite an inactive policy branch. A static engine-geometry audit found327 distinct physical signatures across200 screen,40 fresh,10 shifted fresh,40 final,20 shifted final and17 local official maps; none were exact physical duplicates. Shared templates still limit independence and generalization.

The selected development OFF artifact is `032c963b6311c8ee28a40df307448e481ee6df3ade892f084dc4a7aa9c3a4093`. Source-default packaging initially changed only custom Wasm metadata under different output filenames. All standard sections were identical. The final build uses the recorded SDK llvm-strip step to remove non-executable debug/name metadata; raw and normalized standard sections are checked equal. Source and re-extracted official CLI bundle now reproduce the exact normalized artifact byte-for-byte.

- Final Wasm: `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1`.
- Legal source ZIP: `6622fb3f57f26f56cec83e03f9e64a9d1638fe907574bef0da3602311c7427ce`.
- All new policy and diagnostic flags: OFF. The inactive unit-count parameter remains8; original baseline tuning macros are unchanged.
- Exact final local official parity:34/34 raw replays, scores, rounds, native fuel incidents and faults match baseline.
- Untouched primary final suite:40 maps/80 games,40W40L, zero mean gem margin and zero native faults.
- Untouched structural final suite:20 maps/40 games,20W20L, zero mean gem margin and zero native faults.
- Final fixed proxy pool:40 maps×4 opponents×2 sides=320 games per build. Both final and baseline win233/320, with identical point/gem/fuel values in every matched stratum. Paired effects and conditional intervals are exactly0. This degenerate measured equality is not proof of universal equivalence or general strength.
- Final pool native fuel exhaustion remains1097 entries in320 team-games for each build. Retaining baseline behavior retains its suspensions; no near-zero-fuel claim is made.

Final holdout is760 games including the matched baseline pool; the exact-artifact34-game parity prerequisite is separate. No final map was used to retune policies. The package includes original source ZIP, baseline source and original baseline Wasm for rollback. See `evidence/FINAL_RELEASE_REVIEW.json`, `CANDIDATE_MANIFEST.json`, `BUILD_AND_ROLLBACK.md`, and the immutable result/schedule matrices.

## Completion and external limitations

The source, research changes, resumable harness, experiment registry, legal source/Wasm package, rollback and local results are ready. Required original calibrated opponents, authenticated live version/windows and independent competition-family validation remain unavailable. These missing external results prevent claiming full ladder validation or a competitive upgrade. GitHub publication does not replace or submit a live bot. Parent issue3 and workstream issues4–7 are retained for the durable record; final publication identity is recorded separately.
'''
    (ROOT/'VALIDATION_v36.md').write_text(text,encoding='utf-8')
    print(json.dumps(review),flush=True)

if __name__=='__main__':main()
