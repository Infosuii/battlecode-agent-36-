"""Freeze source identity and a reviewable patch after correctness checks."""
from pathlib import Path
import difflib, hashlib, json, zipfile

ROOT=Path(__file__).resolve().parents[4]
HERE=Path(__file__).resolve().parents[1]

def sha(data): return hashlib.sha256(data).hexdigest()

def main():
    baseline=ROOT/'bot_v34/src'
    candidate=ROOT/'bot_v36/src'
    results=HERE/'results'
    status=json.loads((results/'scorer_status.json').read_text())
    assert status['status']=='passed', 'Current checks must pass first'
    checked=json.loads((results/'scorer_results.json').read_text())
    for path,digest in checked['hashes'].items():
        assert sha((ROOT/path).read_bytes())==digest, f'Source changed after check: {path}'
    archive=ROOT/'analysis/2026-10-19/submission-v36.zip'
    baseline_files={p.name:p.read_bytes() for p in baseline.iterdir() if p.is_file()}
    verified=[]
    if archive.exists():
        with zipfile.ZipFile(archive) as z:
            entries={Path(n).name:z.read(n) for n in z.namelist() if not n.endswith('/')}
        for name,data in baseline_files.items():
            assert name in entries and data==entries[name], f'Frozen source mismatch: {name}'
            verified.append(name)
        assert len(verified)==18
    base_manifest={name:sha(data) for name,data in sorted(baseline_files.items())}
    effective=dict(base_manifest)
    overlay={p.name:sha(p.read_bytes()) for p in sorted(candidate.iterdir()) if p.is_file()}
    effective.update(overlay)
    bundle=lambda records: sha(''.join(f'{name} {digest}\n' for name,digest in sorted(records.items())).encode())
    manifest={
        'identity':'Agent 3 independent overlay, not the other host integrated candidate or a live release',
        'baseline':{'files':base_manifest,'source_manifest_sha256':bundle(base_manifest)},
        'candidate_overlay':{'files':overlay,'effective_source_manifest_sha256':bundle(effective),
                             'inherited_unchanged_files':[name for name in base_manifest if name not in overlay]},
        'switches':{'farm_score::kCorrectBound':False,'farm_score::kDiagnostics':False,
                    'farm_score::kSingleGroupDiagnostic':False,'farm_score::kExhaustive':False,
                    'central_switch_registration':'pending Agent 1; no common.h edits'},
        'fixtures':{'schedule':'A3-BOUND-UNIT-01','seed':3603,'variants':status['variants'],
                    'cases_per_variant':checked['variants'][0]['measurements'][0]['cases'],
                    'scorer_results_sha256':sha((results/'scorer_results.json').read_bytes()),
                    'compiler':checked['compiler']},
        'engine_matches':{'games':63,'independent_layout_groups':9,
                          'engine_build':'d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5',
                          'scope':'isolated repaired candidate, reconstructed historical development layouts, baseline opponent',
                          'release_fuel':'analysis/v36/recovery/recovered-development-v3-analysis.json',
                          'reason':'integrated other-host candidate, current official suite and varied pool still unavailable'},
        'archive':{'sha256':sha(archive.read_bytes()) if archive.exists() else None,
                   'entries_verified':len(verified),
                   'platform_team_submission_join':'unavailable on this host'},
    }
    (results/'SOURCE_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    patches=[]
    for path in sorted(candidate.iterdir()):
        if not path.is_file(): continue
        before=(baseline/path.name).read_bytes().splitlines(keepends=True) if (baseline/path.name).exists() else []
        after=path.read_bytes().splitlines(keepends=True)
        if before==after: continue
        patches.extend(difflib.diff_bytes(difflib.unified_diff,before,after,
            fromfile=f'a/bot_v36/src/{path.name}'.encode() if before else b'/dev/null',
            tofile=f'b/bot_v36/src/{path.name}'.encode()))
    # Preserve each source file's original line endings; text output on Windows
    # would convert the new LF header to CRLF and break byte-exact manifests.
    (results/'agent3_overlay.patch').write_bytes(b''.join(patches))
    print('Frozen baseline/archive entries verified:',len(verified))
    print('Baseline source manifest:',bundle(base_manifest))
    print('Effective independent candidate:',bundle(effective))
    print('Patch:',results/'agent3_overlay.patch')

if __name__=='__main__': main()
