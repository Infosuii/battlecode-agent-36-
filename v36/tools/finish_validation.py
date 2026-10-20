"""Exact materialized artifact parity, then the untouched final fixed suite."""
import sys
from evaluate import ROOT,PROJECT,freeze,execute,run_cmd
from final_validation import main as final
from audit_evidence import main as audit

def main():
    sid='release-final-parity'
    if not (ROOT/'schedules'/(sid+'.json')).exists():
        freeze(sid,'release-final',['baseline'],sorted((PROJECT/'maps').glob('*.map26a')),
            'Exact metadata-normalized source-default release parity prerequisite; no new policy tuning')
    execute(sid,3)
    print(run_cmd([sys.executable,ROOT/'tools/agent4/check_parity.py',ROOT/'results/baseline-official.json',
        ROOT/'results'/(sid+'.json'),'--out',ROOT/'evidence/release-final-parity.json']),flush=True)
    final();audit()

if __name__=='__main__':main()
