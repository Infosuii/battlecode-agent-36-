"""Prepare isolated, hash-verified baseline/off/on projects. Never edit frozen inputs."""
from pathlib import Path
import hashlib,json,shutil,argparse

ROOT=Path(__file__).resolve().parents[3]
EVAL=ROOT/'work/recovery/eval'
OUT=ROOT/'analysis/v36/recovery'
sha=lambda b:hashlib.sha256(b).hexdigest()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--variant-suffix',default='');args=parser.parse_args()
    records={}
    for profile in ['baseline','bound_off','bound_on','bound_diag']:
        name=profile+args.variant_suffix if profile!='baseline' else profile
        dest=EVAL/'bots'/name/'src'
        dest.mkdir(parents=True,exist_ok=True)
        sources={p.name:p.read_bytes() for p in (ROOT/'bot_v34/src').iterdir() if p.is_file()}
        if profile!='baseline':
            sources.update({p.name:p.read_bytes() for p in (ROOT/'bot_v36/src').iterdir() if p.is_file()})
            if profile in ['bound_on','bound_diag']:
                data=sources['farm_score.h'].decode().replace('kCorrectBound = false','kCorrectBound = true')
                if profile=='bound_diag': data=data.replace('kDiagnostics = false','kDiagnostics = true')
                sources['farm_score.h']=data.encode()
        for filename,data in sources.items():
            path=dest/filename
            if path.exists() and path.read_bytes()!=data:
                raise RuntimeError(f'Refusing to overwrite changed prepared source: {path}')
            path.write_bytes(data)
        hashes={name:sha(data) for name,data in sorted(sources.items())}
        records[name]={'files':hashes,'source_manifest_sha256':sha(''.join(f'{n} {h}\n' for n,h in hashes.items()).encode())}
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/('prepared_sources'+args.variant_suffix+'.json')).write_text(json.dumps(records,indent=2)+'\n')
    print('Prepared',', '.join(records))

if __name__=='__main__':main()
