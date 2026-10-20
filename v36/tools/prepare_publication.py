"""Export reviewed code/results and pinned artifacts, excluding raw runtime datasets."""
import hashlib,json,pathlib,shutil,zipfile
from evaluate import ROOT,WORK,manifest,sha,write

DEST=WORK/'v36-publish/v36'

def main():
    if DEST.exists(): raise ValueError('Publication export already exists; inspect it before replacement')
    DEST.mkdir(parents=True)
    exported={};omitted=[]
    def copy(p,relative=None):
        p=pathlib.Path(p);rel=pathlib.Path(relative) if relative else p.relative_to(ROOT)
        target=DEST/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
        exported[rel.as_posix()]=dict(sha256=sha(target),bytes=target.stat().st_size)
    for p in ROOT.glob('*'):
        if p.is_file() and p.suffix in ['.md','.json','.h'] and p.name not in ['CLOUD_HANDOFF.md','agent2_integration.md']:
            copy(p)
    for directory in ['bot_v34/src','bot_v36/src','tools','maps']:
        for p in (ROOT/directory).rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts and p.suffix in ['.py','.mjs','.cpp','.h','.md','.json','.map26a']:
                copy(p)
    official=WORK/'development/current/maps'
    for p in official.glob('*.map26a'):copy(p,pathlib.Path('maps/local-official')/p.name)
    for directory in ['agent2','agent3','evidence']:
        for p in (ROOT/directory).rglob('*'):
            if not p.is_file() or '__pycache__' in p.parts:continue
            rel=p.relative_to(ROOT)
            accepted=p.suffix=='.md' or (p.suffix=='.json' and p.stat().st_size<=1_000_000) or (directory in ['agent2','agent3'] and p.suffix in ['.py','.mjs','.cpp','.h'])
            if accepted:copy(p)
            elif p.suffix in ['.json','.md']:omitted.append(dict(path=rel.as_posix(),bytes=p.stat().st_size,sha256=sha(p),reason='Large raw/decoded diagnostic dataset; summary receipts exported'))
    for directory in ['results','schedules']:
        for p in (ROOT/directory).glob('*.json'):copy(p)
    for folder in (ROOT/'builds').iterdir():
        if not (folder/'manifest.json').exists():continue
        for filename in ['manifest.json','preprocessor.txt','build.log']:
            if (folder/filename).exists():copy(folder/filename)
        if (folder/'src').exists():
            target=DEST/'builds'/folder.name/'source.zip'
            with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
                for p in sorted((folder/'src').rglob('*')):
                    if p.is_file():
                        entry=zipfile.ZipInfo(p.relative_to(folder/'src').as_posix(),(1980,1,1,0,0,0));entry.compress_type=zipfile.ZIP_DEFLATED
                        z.writestr(entry,p.read_bytes())
            exported[target.relative_to(DEST).as_posix()]=dict(sha256=sha(target),bytes=target.stat().st_size)
    for name in ['spar_fish','spar_mite','spar_bush','live_platform_v35']:
        copy(pathlib.Path(manifest(name)['artifact']),pathlib.Path('opponents')/(name+'.wasm'))
    for p in (ROOT/'release').rglob('*'):
        if p.is_file() and not any(x in p.parts for x in ['archive_rebuild_source','verified_archive']):copy(p)
    index=dict(exported_files=exported,omitted_large_datasets=omitted,
        not_exported=['Raw full-match replay bundles/statistics/stdout under runs/; local archived historical databases; SDK/compiler/engine runtime binaries'],
        raw_run_root=str(ROOT/'runs'),purpose='Code, source snapshots, legal source/Wasm/rollback artifacts, complete result and schedule matrices, and compact independent evidence. Original local paths in immutable receipts preserve provenance.')
    write(DEST/'PUBLICATION_CONTENTS.json',index)
    print(json.dumps(dict(files=len(exported),bytes=sum(v['bytes'] for v in exported.values()),omitted_large_json=len(omitted),destination=str(DEST))),flush=True)

if __name__=='__main__':main()
