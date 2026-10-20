"""One immutable fresh selection protocol, frozen before any fresh jobs run."""
import json
from evaluate import ROOT, freeze, execute, write, manifest, sha
from paired_compare import compare

def plan():
    maps=sorted((ROOT/'maps/validation').glob('*.map26a'))
    shift=sorted((ROOT/'maps/shift_validation').glob('*.map26a'))
    if len(maps)!=40 or len(shift)!=10: raise ValueError('Fresh map counts differ from predeclared protocol')
    pool=['spar_fish','spar_mite','spar_bush','live_platform_v35']
    specs=[]
    for c in ['rows-cached','stack-cached']:
        specs += [(c+'-fresh-head', c, ['baseline'], maps),
                  (c+'-shift-validation', c, ['baseline'], shift)]
    specs.append(('stack-fresh-v-rows','stack-cached',['rows-cached'],maps))
    for c in ['baseline','rows-cached','stack-cached']:
        specs.append((c+'-fresh-pool',c,pool,maps[:20]))
    return specs

def main():
    specs=plan()
    builds=sorted({n for _,c,op,_ in specs for n in [c]+op})
    protocol=dict(version=2, alternatives=['baseline','rows-cached','stack-cached'],
        identities={n:manifest(n)['artifact_sha256'] for n in builds},
        schedules=[dict(id=s,candidate=c,opponents=o,map_hashes=[sha(m) for m in maps]) for s,c,o,maps in specs],
        frozen_before_any_fresh_games=True,
        acceptance=dict(
            rows='Eligible only after native cached identity/writer checks, exact OFF parity, and isolated outside-rows official head winpoints>=0.50, mean submitted-gem margin>=0, zero faults. Then fresh pool matched point AND gem effects versus baseline must both be >=0; fresh head and shift head versus baseline must each have winpoints>=0.50 and mean gem margin>=0.',
            governor='Eligible only when rows is eligible. Fresh pool matched point AND gem effects versus rows AND baseline must be >=0; fresh head versus rows and baseline and shift head versus baseline must each have winpoints>=0.50 and mean gem margin>=0. Total native exhaustion entries in the identical 160-game fresh pool must be strictly below rows (equality fails). This is an event-count gate, not a per-turn fuel guarantee.',
            faults='Any native botFault entry in any completed fresh schedule for a proposed candidate rejects that candidate. Cached identity corruption/missing/context failures must already fail closed in native development fixtures. A fresh cache assertion failure also rejects it.',
            fallback='If rows fails, select frozen baseline behavior. If rows passes but governor fails, select rows. If both pass, select stack. Zero point/gem effects pass; exhaustion equality fails. No adaptive threshold or repeat fresh selection.',
            final='Freeze one exact materialized source/archive build before final 40 primary+20 structural-shift maps. Report any final failure without tuning or treating reused holdout as pristine.'),
        caveat='Point gates are conservative selection rules, not statistical significance. Map-cluster intervals reported. Fixed uncalibrated local proxy opponents; shared generator templates; no competition-family generalization or calibrated historical pool claim.')
    dest=ROOT/'FRESH_VALIDATION_PROTOCOL.json'
    if dest.exists():
        if json.loads(dest.read_text())!=protocol: raise ValueError('Frozen fresh protocol differs')
    else:
        if any((ROOT/'results'/(s+'.json')).exists() for s,_,_,_ in specs):
            raise ValueError('Fresh results predate protocol')
        write(dest,protocol)
    for sid,c,op,maps in specs:
        path=ROOT/'schedules'/(sid+'.json')
        if not path.exists(): freeze(sid,c,op,maps,'Fresh protocol v2: '+sha(dest))
        saved=json.loads(path.read_text())
        if saved['candidate']!=c or saved['opponents']!=op or {j['map_sha256'] for j in saved['jobs']}!={sha(m) for m in maps}:
            raise ValueError('Frozen schedule contradicts fresh protocol')
        expected={(sha(m), opponent, side) for m in maps for opponent in op for side in [0,1]}
        actual={(j['map_sha256'], j['b'] if j['a']==c else j['a'], 0 if j['a']==c else 1) for j in saved['jobs']}
        if actual!=expected or len(saved['jobs'])!=len(expected) or len({j['id'] for j in saved['jobs']})!=len(expected):
            raise ValueError('Fresh schedule does not contain the exact paired job grid')
        for job in saved['jobs']:
            if job['sha_a']!=protocol['identities'][job['a']] or job['sha_b']!=protocol['identities'][job['b']]:
                raise ValueError('Fresh artifact differs')
    receipt=dict(protocol_sha256=sha(dest),schedule_sha256={s:sha(ROOT/'schedules'/(s+'.json')) for s,_,_,_ in specs})
    receipt_path=ROOT/'evidence/fresh-freeze-receipt.json'
    if receipt_path.exists():
        if json.loads(receipt_path.read_text())!=receipt: raise ValueError('Immutable fresh freeze receipt changed')
    else: write(receipt_path,receipt)
    for sid,_,_,_ in specs: execute(sid,3)
    compare('stack-cached-fresh-pool','rows-cached-fresh-pool','fresh-governor-effect.json')
    compare('rows-cached-fresh-pool','baseline-fresh-pool','fresh-rows-effect.json')
    compare('stack-cached-fresh-pool','baseline-fresh-pool','fresh-stack-effect.json')

if __name__=='__main__': main()
