"""Run production tactical-model invariants in WASI; this is not an engine oracle."""
import argparse, hashlib, json, pathlib, shutil, subprocess
parser=argparse.ArgumentParser();parser.add_argument('--fixes',type=int,choices=[0,1],default=0)
args=parser.parse_args()
ROOT=pathlib.Path(__file__).resolve().parents[1]
WORK=ROOT.parent
OUT=ROOT/'agent2/evidence'/('model-fixes' if args.fixes else 'model-off')
OUT.mkdir(parents=True,exist_ok=True)
sources={}
for name in ['tsearch.h','runtime_budget.h']:
    p=ROOT/'bot_v36/src'/name
    sources[name]=hashlib.sha256(p.read_bytes()).hexdigest()
    # Only replace the SDK umbrella include. All tested model bodies remain verbatim.
    (OUT/name).write_text(p.read_text().replace('#include "common.h"','#include "model_portable.h"'))
(OUT/'model_portable.h').write_text('''#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#define NOVA_QUARTIC 0
#define NOVA_TS_RELEVANCE 1
#define NOVA_TS_SIM_FIXES FIXES_VALUE
#define NOVA_RUNTIME_GOVERNOR 0
#define NOVA_RUNTIME_TELEMETRY 0
'''.replace('FIXES_VALUE',str(args.fixes)))
fixture=ROOT/'agent2_model_fixtures.h'
sources['fixture']=hashlib.sha256(fixture.read_bytes()).hexdigest()
shutil.copyfile(fixture,OUT/'agent2_model_fixtures.h')
(OUT/'model_fixture.cpp').write_text('#include "agent2_model_fixtures.h"\nint main(){ agent2_model_fixtures(); }\n')
sdk=WORK/'development/wasi-sdk-33.0-x86_64-windows'
cmd=[str(sdk/'bin/clang++.exe'),'-O2','-std=c++20','-fno-exceptions',str(OUT/'model_fixture.cpp'),'-o',str(OUT/'model_fixture.wasm')]
r=subprocess.run(cmd,capture_output=True,text=True)
(OUT/'build.log').write_text(r.stdout+r.stderr)
if r.returncode: raise RuntimeError(r.stderr)
runner=OUT/'run_fixture.cjs'
runner.write_text("const fs=require('node:fs');const {WASI}=require('node:wasi');const wasi=new WASI({version:'preview1',args:[],env:{},returnOnExit:true});WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{wasi_snapshot_preview1:wasi.wasiImport}).then(({instance})=>process.exitCode=wasi.start(instance));")
r=subprocess.run(['node',str(runner),str(OUT/'model_fixture.wasm')],capture_output=True,text=True)
record=dict(command=cmd,sha256=sources,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr,scope='Production model invariants with SDK umbrella replaced by standard headers; no official-engine equivalence or release fuel measurement')
(OUT/'model-fixture.json').write_text(json.dumps(record,indent=2))
print(json.dumps(record,indent=2))
if r.returncode: raise RuntimeError('Model fixture failed')
