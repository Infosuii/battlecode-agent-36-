"""Freeze initial diagnostic/instrumentation protocol before reading treatment outcomes."""
from lab import ROOT,WS,freeze,manifest,write,sha
def main():
    names=['00rows','00lanes','00corners','00expanse','01boss-battle','01gold-rush']
    maps=[WS/'maps'/(n+'.map26a') for n in names]
    baseline=manifest('v37-baseline');off=manifest('v37-off')
    assert baseline['wasm_sha256']==off['wasm_sha256']
    write(ROOT/'evidence/disabled-parity.json',dict(baseline_wasm=baseline['wasm_sha256'],off_wasm=off['wasm_sha256'],byte_identical=True,
        conclusion='Disabled trace compiles to exact production module. No duplicate games needed.'))
    protocol=dict(id='trace-6-v1',hypothesis='Instrumentation may affect fuel and decisions; never pool trace results as production evidence.',
        primary='decision episode labels and mode resource diagnosis',maps=names,both_sides=True,full_match=True,
        correctness=['native resource reconciliation','engine build gate','source-default OFF WASM identity'],
        promotion='none; trace builds cannot be promoted',stop='malformed trace, resource mismatch, log truncation, engine drift',
        sample_groups=6,limitations='Observational selected actions; unselected outcomes absent; all six maps are development data.')
    write(ROOT/'registry/trace-6-v1.json',protocol)
    freeze('trace-6-v1','v37-trace',['v37-baseline'],maps,'Instrumentation audit and observational training preparation')
    print('Registered trace-6-v1; exact disabled-WASM parity passed')
if __name__=='__main__':main()
