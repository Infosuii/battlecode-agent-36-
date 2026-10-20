"""Compact exact variant snapshots; no duplicate source trees in the PR."""
import argparse,zipfile,pathlib
from lab import ROOT,tree,write,sha
FOLDERS=['baseline','econ-link','safe-state2','trace','trace2']
def freeze():
    out=ROOT/'source-archives';out.mkdir(exist_ok=True);rows=[]
    for name in FOLDERS:
        source=ROOT/name/'src';dest=out/(name+'.zip')
        with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for p in sorted(source.iterdir()):
                if p.is_file():
                    info=zipfile.ZipInfo('src/'+p.name,date_time=(2026,10,20,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
                    z.writestr(info,p.read_bytes())
        rows.append(dict(name=name,zip_sha256=sha(dest),source_files=tree(source)))
    write(out/'manifest.json',dict(variants=rows,compression_only=True))
def restore():
    data=__import__('json').loads((ROOT/'source-archives/manifest.json').read_text())
    for row in data['variants']:
        archive=ROOT/'source-archives'/(row['name']+'.zip');assert sha(archive)==row['zip_sha256']
        with zipfile.ZipFile(archive) as z:
            assert set(z.namelist())=={'src/'+n for n in row['source_files']}
            for n,h in row['source_files'].items():
                path=ROOT/row['name']/'src'/n;raw=z.read('src/'+n)
                import hashlib
                assert hashlib.sha256(raw).hexdigest()==h
                if path.exists():assert path.read_bytes()==raw,'Refusing to overwrite modified source'
                else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(raw)
        assert tree(ROOT/row['name']/'src')==row['source_files']
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('command',choices=['freeze','restore']);a=ap.parse_args()
    freeze() if a.command=='freeze' else restore()
