"""Exact release artifact final holdout; no tuning or second selection here."""
import json
from evaluate import ROOT, freeze, execute, manifest, sha, write
from paired_compare import compare
from wasm_compare import sections

def main():
    selection=json.loads((ROOT/'RELEASE_SELECTION.json').read_text())
    exact=manifest('release-final'); selected=manifest(selection['selected_build'])
    release=json.loads((ROOT/'CANDIDATE_MANIFEST.json').read_text())
    if release['exact_build']['artifact_sha256']!=exact['artifact_sha256'] or release['archive_rebuild']!=exact['artifact_sha256']:
        raise ValueError('Archive reproduction gate failed')
    if exact['switches']!=selected['switches']: raise ValueError('Release selected configuration changed')
    if selection['selected_artifact_sha256']!=selected['artifact_sha256']:
        raise ValueError('Pinned selected artifact changed')
    standard=lambda path:[r for r in sections(path) if r['kind']!=0]
    if standard(exact['artifact'])!=standard(selected['artifact']):
        raise ValueError('Materialization changed executable Wasm sections')
    parity=json.loads((ROOT/'evidence/release-final-parity.json').read_text())
    if parity['games']!=34 or parity['exact_raw_equal']!=34 or not parity['all_score_round_fuel_fault_equal']:
        raise ValueError('Exact final artifact parity gate failed')
    primary=sorted((ROOT/'maps/final_holdout').glob('*.map26a'))
    shift=sorted((ROOT/'maps/shift_final').glob('*.map26a'))
    if len(primary)!=40 or len(shift)!=20: raise ValueError('Predeclared final map counts differ')
    pool=['spar_fish','spar_mite','spar_bush','live_platform_v35']
    specs=[('release-final-head','release-final',['baseline'],primary),
           ('release-final-shift','release-final',['baseline'],shift),
           ('release-final-pool','release-final',pool,primary),
           ('baseline-final-pool','baseline',pool,primary)]
    protocol=dict(selected_build=selection['selected_build'],selected_sha256=selected['artifact_sha256'],
        exact_release_sha256=exact['artifact_sha256'],source_defaults_verified=True,archive_rebuild_equal=True,
        release_selection_sha256=sha(ROOT/'RELEASE_SELECTION.json'),
        acceptance='Report final head and structural-shift W/T/L, submitted gems, native faults and paired fixed-pool effects. Candidate is supported only if both head suites have >=0.50 win points and >=0 mean gem margin, fixed-pool paired point AND gem effects >=0, and zero faults. If any fail, retain packaged candidate as a rejected experiment and recommend immutable baseline rollback; do not tune or reuse final maps as pristine evidence.',
        caveat='40 primary maps share generator templates with development;20 shift maps change geometry style. No unseen competition-family claim. Four fixed uncalibrated local proxy opponents.',
        schedules=[dict(id=s,candidate=c,opponents=o,map_hashes=[sha(m) for m in maps]) for s,c,o,maps in specs])
    path=ROOT/'FINAL_VALIDATION_PROTOCOL.json'
    if path.exists():
        if json.loads(path.read_text())!=protocol: raise ValueError('Immutable final protocol changed')
    else:
        if any((ROOT/'results'/(s+'.json')).exists() for s,_,_,_ in specs): raise ValueError('Final results predate protocol')
        write(path,protocol)
    for sid,c,op,maps in specs:
        p=ROOT/'schedules'/(sid+'.json')
        if not p.exists(): freeze(sid,c,op,maps,'Final exact artifact protocol: '+sha(path))
        saved=json.loads(p.read_text())
        expected={(sha(m),o,i) for m in maps for o in op for i in [0,1]}
        actual={(j['map_sha256'],j['b'] if j['a']==c else j['a'],0 if j['a']==c else 1) for j in saved['jobs']}
        if saved['candidate']!=c or saved['opponents']!=op or actual!=expected or len(saved['jobs'])!=len(expected) or saved['games']!=len(expected) or len({j['id'] for j in saved['jobs']})!=len(expected):
            raise ValueError('Frozen final job grid differs')
        if any(j['sha_a']!=manifest(j['a'])['artifact_sha256'] or j['sha_b']!=manifest(j['b'])['artifact_sha256'] for j in saved['jobs']):
            raise ValueError('Frozen final artifacts differ')
    receipt=dict(protocol_sha256=sha(path),schedule_sha256={s:sha(ROOT/'schedules'/(s+'.json')) for s,_,_,_ in specs})
    rp=ROOT/'evidence/final-freeze-receipt.json'
    if rp.exists() and json.loads(rp.read_text())!=receipt: raise ValueError('Immutable final freeze receipt changed')
    write(rp,receipt)
    for sid,_,_,_ in specs: execute(sid,3)
    compare('release-final-pool','baseline-final-pool','final-pool-effect.json')

if __name__=='__main__': main()
