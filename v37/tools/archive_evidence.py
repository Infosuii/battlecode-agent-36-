"""Compress bulky machine evidence while retaining exact local files and hashes."""
import argparse,gzip,json,pathlib
from lab import ROOT,sha,write
def freeze():
    rows=[]
    for folder in ('evidence','results','ml','platform'):
        for p in sorted((ROOT/folder).glob('*')):
            if p.is_file() and p.suffix in ('.json','.jsonl') and p.stat().st_size>150000:
                dest=p.with_suffix(p.suffix+'.gz');dest.write_bytes(gzip.compress(p.read_bytes(),compresslevel=9,mtime=0))
                rows.append(dict(path=p.relative_to(ROOT).as_posix(),sha256=sha(p),compressed_sha256=sha(dest)))
    write(ROOT/'evidence/compressed-index.json',dict(schema=1,files=rows,raw_retained_locally=True))
    print(json.dumps(dict(compressed_files=len(rows))))
def restore():
    rows=json.loads((ROOT/'evidence/compressed-index.json').read_text())['files']
    for row in rows:
        p=ROOT/row['path'];z=p.with_suffix(p.suffix+'.gz');assert sha(z)==row['compressed_sha256']
        raw=gzip.decompress(z.read_bytes())
        import hashlib
        assert hashlib.sha256(raw).hexdigest()==row['sha256']
        if p.exists():assert p.read_bytes()==raw,'Do not overwrite modified evidence'
        else:p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('command',choices=['freeze','restore']);a=ap.parse_args()
    freeze() if a.command=='freeze' else restore()
