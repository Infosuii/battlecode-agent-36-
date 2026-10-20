"""Compile/run real codec in a small WASI program, without engine or bot mocks."""
import hashlib,json,pathlib,subprocess,time
V36=pathlib.Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    root=V36/'evidence/agent4/cache_codec';root.mkdir(exist_ok=True);source=V36/'tools/agent4/map_cache_codec_fixture.cpp';header=V36/'bot_v36/src/draft_map_cache.h';artifact=root/'fixture.wasm'
    compiler=V36.parent/'development/wasi-sdk-33.0-x86_64-windows/bin/clang++.exe'
    command=[str(compiler),'-O2','-std=c++20','-I'+str(header.parent),str(source),'-o',str(artifact)]
    compiled=subprocess.run(command,capture_output=True,text=True,timeout=120)
    if compiled.returncode:raise RuntimeError(compiled.stderr[-3000:])
    launcher="import fs from'node:fs';import{WASI}from'node:wasi';const w=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});const m=await WebAssembly.compile(fs.readFileSync(process.argv[1]));const i=await WebAssembly.instantiate(m,w.getImportObject());process.exitCode=w.start(i);"
    result=subprocess.run(['node','--input-type=module','-e',launcher,str(artifact)],capture_output=True,text=True,timeout=60)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    report=dict(result=json.loads(result.stdout),source_sha256=sha(source),codec_sha256=sha(header),artifact_sha256=sha(artifact),command=command,
        scope='Substantive binary-format invariants against actual codec: every bit, truncated record, changed live context, unsupported schema, prefix preservation and failed-encode nonmutation. No policy performance claim.')
    (root/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
if __name__=='__main__':main()
