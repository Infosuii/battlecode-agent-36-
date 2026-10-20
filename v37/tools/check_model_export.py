"""Compare actual WASM inference to offline model, including OOD/missing inputs."""
import json,math,pathlib,subprocess,numpy as np
from lab import ROOT,run,write,sha
def main():
    m=json.loads((ROOT/'ml/model.json').read_text());rows=[json.loads(l) for l in (ROOT/'ml/decisions.jsonl').read_text().splitlines()]
    cases=[r['x'] for r in rows[::max(1,len(rows)//20)][:20]]
    cases.extend([[1e9]*24,[-1e9]*24,[float('nan')]+[0]*23])
    source=['#include "pilot_model.h"','#include <cstdio>','int main(){']
    expected=[]
    for i,x in enumerate(cases):
        xs=','.join('NAN' if not math.isfinite(v) else repr(float(v)) for v in x)
        source.append('{double x[]={'+xs+'};double a=v37_pilot::predict(x,v37_pilot::elapsed),b=v37_pilot::predict(x,v37_pilot::risk);std::printf("%d %.17g %.17g\\n",'+str(i)+',a,b);}')
        if all(math.isfinite(v) for v in x):
            z=np.clip((np.array(x)-np.array(m['normalization']['mean']))/np.array(m['normalization']['scale']),-5,5)
            expected.append([float(m['elapsed_log1p_weights'][0]+z@np.array(m['elapsed_log1p_weights'][1:])),float(m['interruption_logit_weights'][0]+z@np.array(m['interruption_logit_weights'][1:]))])
        else:expected.append(None)
    source.append('}')
    p=ROOT/'ml/export_fixture.cpp';p.write_text('\n'.join(source))
    compiler=pathlib.Path('C:/Users/ishan/AppData/Local/cambc/wasi-sdk-33/bin/clang++.exe');wasm=ROOT/'ml/export_fixture.wasm'
    cmd=[compiler,'-O2','-std=c++20',p,'-o',wasm];run(cmd)
    node=pathlib.Path('C:/Users/ishan/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe')
    output=run([node,ROOT/'tools/run_fixture.cjs',wasm]);observations=[]
    for line in output.splitlines():
        parts=line.split()
        if len(parts)!=3 or not parts[0].isdigit():continue
        index=int(parts[0]);values=[float(v) for v in parts[1:]];exp=expected[index]
        if exp is None:assert all(math.isnan(v) for v in values)
        else:assert all(math.isfinite(v) and abs(v-e)<1e-9 for v,e in zip(values,exp)),(index,values,exp)
        observations.append(dict(index=index,finite=exp is not None,max_abs_error=max(abs(v-e) for v,e in zip(values,exp)) if exp else None))
    assert len(observations)==len(cases)
    write(ROOT/'ml/export-parity.json',dict(passed=True,cases=len(cases),observations=observations,model_sha256=sha(ROOT/'ml/model.json'),
        header_sha256=sha(ROOT/'ml/pilot_model.h'),wasm_sha256=sha(wasm),command=[str(v) for v in cmd],
        clamp='standardized features clipped [-5,5]; logits finite for finite inputs; missing returns NaN requiring heuristic fallback',
        activation_weight=0,production_inference_fuel=None,production_inference_fuel_reason='Predictor not linked into production; performance and zero-weight decision gates remain pending'))
    print(json.dumps(dict(cases=len(cases),wasm_export_parity=True,activation_weight=0)))
if __name__=='__main__':main()
