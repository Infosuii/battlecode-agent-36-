"""Independent exact raw-replay parity check, including both assigned sides."""
import argparse,hashlib,json,pathlib
from replay_bundle import read_bundle
def raw_hash(result):
    bundle=next(pathlib.Path(result['statistics']).parent.glob('*.repl26a'))
    return hashlib.sha256(read_bundle(bundle)['replay.repl26a']).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('baseline');p.add_argument('candidate');p.add_argument('--out',required=True);a=p.parse_args()
    baseline=json.loads(pathlib.Path(a.baseline).read_text());candidates=json.loads(pathlib.Path(a.candidate).read_text())
    base={r['map_sha256']:(r,raw_hash(r)) for r in baseline};rows=[]
    for r in candidates:
        b,bh=base[r['map_sha256']];ch=raw_hash(r)
        rows.append(dict(map=pathlib.Path(r['map']).stem,candidate_side=r['side'],baseline_raw_sha256=bh,candidate_raw_sha256=ch,
            exact_input_replay_equal=bh==ch,scores_equal=list(map(int,b['scores']))==list(map(int,r['scores'])),rounds_equal=b['rounds']==r['rounds'],fuel_events_equal=b['fuel_exhaustions']==r['fuel_exhaustions'],faults_equal=b['faults']==r['faults']))
    report=dict(games=len(rows),distinct_maps=len(base),exact_raw_equal=sum(r['exact_input_replay_equal'] for r in rows),
        changed=[r for r in rows if not r['exact_input_replay_equal']],
        all_score_round_fuel_fault_equal=all(all(r[k] for k in ['scores_equal','rounds_equal','fuel_events_equal','faults_equal']) for r in rows),rows=rows,
        interpretation='Raw input replay byte equality establishes identical recorded map/input decisions under the real engine, stronger than matching final outcomes. Diagnostic stdout/fuel and bundle compression are excluded.')
    pathlib.Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['games','distinct_maps','exact_raw_equal','all_score_round_fuel_fault_equal','changed']}))
if __name__=='__main__':main()
