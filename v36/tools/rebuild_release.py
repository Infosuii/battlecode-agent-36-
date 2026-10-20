"""Reproduce the selected source-default release under a new immutable build ID."""
import argparse, json
from evaluate import ROOT, build, sha

def main():
    p=argparse.ArgumentParser(); p.add_argument('build_id'); args=p.parse_args()
    frozen=json.loads((ROOT/'CANDIDATE_MANIFEST.json').read_text())
    result=build(args.build_id, frozen['exact_build']['switches'], 'release/bot_v36', materialized=True, normalize=True)
    if result['artifact_sha256']!=frozen['exact_build']['artifact_sha256']:
        raise ValueError('Rebuild differs from the validated release')
    print('Exact release reproduced: '+result['artifact_sha256'],flush=True)

if __name__=='__main__': main()
