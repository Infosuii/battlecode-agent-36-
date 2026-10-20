"""Make a private-repository cloud checkpoint without credentials or bulk replay data."""
import hashlib, json, pathlib, shutil, zipfile
from evaluate import ROOT, WORK, PROJECT, sha, write

def main():
    dest=WORK/'v36-cloud-repo/v36';dest.mkdir(parents=True,exist_ok=True)
    text_ext={'.py','.md','.json','.cpp','.h','.mjs','.js','.txt','.csv'}
    selected=[]
    for p in ROOT.iterdir():
        if p.is_file() and p.suffix in text_ext:selected.append(p)
    for name in ['bot_v34','bot_v36','tools','prompts','agent2','agent3','evidence','results','schedules']:
        for p in (ROOT/name).rglob('*'):
            if p.is_file() and p.suffix in text_ext and '__pycache__' not in p.parts and p.stat().st_size<2_000_000:
                selected.append(p)
    for p in selected:
        q=dest/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
    archive=dest/'checkpoint-resources.zip';index=[];seen=set()
    def add(z,p,arc):
        arc=pathlib.PurePosixPath(arc).as_posix()
        if arc in seen:return
        seen.add(arc);z.write(p,arc);index.append(dict(path=arc,sha256=sha(p),bytes=p.stat().st_size))
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6,strict_timestamps=False) as z:
        for p in (ROOT/'maps').rglob('*'):
            if p.is_file():add(z,p,p.relative_to(ROOT))
        for folder in (ROOT/'builds').iterdir():
            if not (folder/'manifest.json').exists():continue
            for p in folder.rglob('*'):
                if p.is_file():add(z,p,p.relative_to(ROOT))
            receipt=json.loads((folder/'manifest.json').read_text())
            binary=pathlib.Path(receipt['artifact'])
            if binary.exists() and not binary.is_relative_to(folder):add(z,binary,f'builds/{folder.name}/{folder.name}.wasm')
        selected_runs=set()
        for name in ['baseline-official','runtime-telemetry']:
            for row in json.loads((ROOT/'results'/(name+'.json')).read_text()):selected_runs.add(row['id'])
        for folder in (ROOT/'runs').iterdir():
            if not (folder/'result.json').exists():continue
            for p in folder.iterdir():
                if p.is_file() and (p.suffix=='.json' or p.name=='runner.log' or (folder.name in selected_runs and p.suffix=='.repl26a')):
                    add(z,p,p.relative_to(ROOT))
        for name in ['agent2','agent3','evidence']:
            for p in (ROOT/name).rglob('*'):
                if p.is_file() and p.suffix in ['.gz','.wasm','.stdout','.stderr'] and 'runtime-probe' not in p.parts:
                    add(z,p,p.relative_to(ROOT))
        for p in (PROJECT/'sdk/cpp').rglob('*'):
            if p.is_file():add(z,p,'resources/project/sdk/cpp/'+p.relative_to(PROJECT/'sdk/cpp').as_posix())
        for p in (PROJECT/'maps').glob('*.map26a'):add(z,p,'resources/project/maps/'+p.name)
        add(z,PROJECT/'cambc.toml','resources/project/cambc.toml')
        add(z,WORK/'development/engine-current.wasm','resources/engine-current.wasm')
        add(z,pathlib.Path('C:/Users/ishan/Downloads/bot v34.zip'),'resources/bot-v34-original.zip')
    if archive.stat().st_size>90_000_000:raise RuntimeError('Checkpoint archive too large for conservative Git storage')
    write(dest/'CHECKPOINT_MANIFEST.json',dict(archive=archive.name,archive_sha256=sha(archive),archive_bytes=archive.stat().st_size,
        entries=index,selected_baseline_telemetry_replays_only=True,full_screen_replays_not_uploaded=True,
        source_files={p.relative_to(ROOT).as_posix():sha(p) for p in selected}))
    with zipfile.ZipFile(archive) as z:
        bad=z.testzip()
        if bad:raise RuntimeError('Corrupt checkpoint '+bad)
    (WORK/'v36-cloud-repo/CONTINUE_V36.md').write_text('Continue from v36/CLOUD_HANDOFF.md. This is a partial, hash-verified execution checkpoint; no release or strength claim.\n',encoding='utf-8')
    print(json.dumps(dict(text_files=len(selected),archive_files=len(index),archive_bytes=archive.stat().st_size,archive_sha256=sha(archive),destination=str(dest))),flush=True)

if __name__=='__main__':main()
