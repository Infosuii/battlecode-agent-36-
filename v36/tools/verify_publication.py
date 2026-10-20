"""Finish fixture export, then verify every published file against its inventory."""
import json,pathlib,shutil,zipfile
from evaluate import ROOT,WORK,sha,write,tree

def main():
    dest=WORK/'v36-publish/v36';index=json.loads((dest/'PUBLICATION_CONTENTS.json').read_text())
    def sync(p):
        rel=p.relative_to(ROOT);target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
        index['exported_files'][rel.as_posix()]=dict(sha256=sha(target),bytes=target.stat().st_size)
    for p in (ROOT/'agent2').rglob('*'):
        if p.is_file() and '__pycache__' not in p.parts and p.suffix in ['.py','.mjs','.cpp','.h']:sync(p)
    for name in ['prepare_publication.py','verify_publication.py']:sync(ROOT/'tools'/name)
    write(dest/'PUBLICATION_CONTENTS.json',index)
    for rel,record in index['exported_files'].items():
        p=dest/rel
        if sha(p)!=record['sha256'] or p.stat().st_size!=record['bytes']:raise ValueError('Export changed: '+rel)
    candidate=json.loads((dest/'CANDIDATE_MANIFEST.json').read_text())
    if sha(dest/'release/bot_v36.wasm')!=candidate['exact_build']['artifact_sha256'] or sha(dest/'release/bot_v36.zip')!=candidate['archive_sha256']:
        raise ValueError('Published release differs from validated package')
    if tree(dest/'release/bot_v36/src')!=candidate['exact_build']['source_tree']:raise ValueError('Published release source changed')
    report=dict(files=len(index['exported_files']),bytes=sum(v['bytes'] for v in index['exported_files'].values()),
        all_file_hashes_match=True,source_tree_match=True,release_archive_and_wasm_match=True,
        largest_file=max((v['bytes'],k) for k,v in index['exported_files'].items()),
        raw_full_match_jobs=json.loads((dest/'EXPERIMENT_INDEX.json').read_text())['unique_completed_full_match_jobs'])
    write(ROOT/'evidence/PUBLICATION_EXPORT_REVIEW.json',report)
    print(json.dumps(report),flush=True)

if __name__=='__main__':main()
