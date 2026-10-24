"""Build named immutable release inputs, retaining actual compiler commands/logs."""
from pathlib import Path
import sys,subprocess,json,hashlib
ROOT=Path(__file__).resolve().parents[3]
EVAL=ROOT/'work/recovery/eval'
CLI=ROOT/'work/recovery/.venv/Scripts/cambc.exe'
OUT=ROOT/'analysis/v36/recovery'
sha=lambda b:hashlib.sha256(b).hexdigest()
def main():
    names=sys.argv[1:] or ['baseline','bound_off_v3','bound_on_v3','bound_diag_v3']
    results=[]
    for name in names:
        output=ROOT/'work/recovery/artifacts'/f'{name}.wasm'
        command=[str(CLI),'build',name,'--release','--output',str(output)]
        run=subprocess.run(command,cwd=EVAL,text=True,capture_output=True,timeout=180)
        (OUT/f'build-{name}.log').write_text(run.stdout+run.stderr)
        if run.returncode:raise RuntimeError(run.stdout+run.stderr)
        source=EVAL/'bots'/name/'src'
        files={p.name:sha(p.read_bytes()) for p in sorted(source.iterdir()) if p.is_file()}
        commands=EVAL/'bots'/name/'.cambc/build-release/compile_commands.json'
        entry={'name':name,'command':command,'source_files':files,'artifact_sha256':sha(output.read_bytes()),
               'artifact_bytes':output.stat().st_size,'compile_commands':json.loads(commands.read_text())}
        results.append(entry)
        print(name,entry['artifact_sha256'],flush=True)
    (OUT/'builds-latest.json').write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
