"""Freeze the independent rebuild and its native execution check for review."""
from pathlib import Path
import hashlib, json, shutil, zipfile

ROOT = Path(__file__).resolve().parents[3]
INPUT = ROOT / 'work/recovery/compile-agent1/v36'
BUILD = INPUT / 'builds/independent-windows-20261020'
OUT = ROOT / 'work/compiled-v36-20261020'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    published = json.loads((INPUT/'CANDIDATE_MANIFEST.json').read_text(encoding='utf8'))
    actual = json.loads((BUILD/'manifest.json').read_text(encoding='utf8'))
    artifact = BUILD/'independent-windows-20261020.wasm'
    assert sha(artifact) == published['exact_build']['artifact_sha256']
    assert actual['source_tree']['sha256'] == published['exact_build']['source_tree']['sha256']
    bundle = INPUT/'release/bot_v36.zip'
    with zipfile.ZipFile(bundle) as archive:
        assert len(archive.namelist()) == 22
        for name in archive.namelist():
            path = Path(name)
            assert path.parts[0] == 'src' and '..' not in path.parts
            assert archive.read(name) == (INPUT/'release/bot_v36'/path).read_bytes()
    smoke_dir = INPUT.parent/'smoke-rows'
    stats = json.loads((smoke_dir/'stats.json').read_text(encoding='utf8'))
    assert sum(stats['diagnostics']['botFaults'].values()) == 0
    assert stats['match']['engineBuildHash'] == 'd7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5'
    OUT.mkdir(exist_ok=False)
    for source, name in [(artifact,'bot_v36.wasm'),(bundle,'bot_v36.zip'),(BUILD/'manifest.json','BUILD_MANIFEST.json'),(BUILD/'build.log','build.log')]:
        shutil.copy2(source,OUT/name)
    shutil.copytree(smoke_dir, OUT/'smoke-rows')
    record = {
        'status':'passed', 'source_commit':'f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a',
        'source_pr':'https://github.com/Infosuii/battlecode-agent-36-/pull/13',
        'exact_published_wasm_reproduced':True, 'source_bundle_files_verified':22,
        'artifact_sha256':sha(OUT/'bot_v36.wasm'), 'source_zip_sha256':sha(OUT/'bot_v36.zip'),
        'source_tree_sha256':actual['source_tree']['sha256'],
        'sdk':'2.2.5', 'balance_changes_applied':False, 'all_new_policy_switches_off':True,
        'smoke':{'kind':'one full native release execution check, not independent strength evidence',
                 'map_sha256':sha(INPUT/'maps/local-official/rows.map26a'),
                 'statistics_sha256':sha(smoke_dir/'stats.json'),
                 'match':stats['match'], 'faults':stats['diagnostics']['botFaults']},
        'live_submission_performed':False,
        'scope':'Compile the published final fallback, preserving its source defaults; separate Agent 2/4 branches are not silently merged.'
    }
    (OUT/'VERIFICATION.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf8')
    (OUT/'README.md').write_text(
        '# Compiled v36 release\n\n'
        'Independent rebuild of Agent 1 PR #13 at f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a. '
        'The compiled WASM is byte-identical to the published final release. All 22 source ZIP files match the compiled source. '
        'One full native match completed with zero bot faults. This checks execution, not competitive improvement.\n\n'
        '- `bot_v36.wasm`: compiled local-runner artifact.\n'
        '- `bot_v36.zip`: unchanged legal source bundle for the platform.\n'
        '- `BUILD_MANIFEST.json`: source hashes, full build command, compiler identity and source defaults.\n'
        '- `VERIFICATION.json` and `smoke-rows/`: verification and raw execution evidence.\n\n'
        'CAMBC/SDK 2.2.5, WASI SDK 33, release -O3. Non-executable metadata normalized using the published llvm-strip step. '
        'All new policy switches remain OFF. No balance update or live submission was performed. '
        'This is a validated baseline fallback, not a demonstrated improvement or an integration of every separate agent branch.\n',encoding='utf8')
    evidence = ROOT/'work/agent3-review/agent3/windows-verification/analysis/v36/recovery/compiled-release-20261020'
    shutil.copytree(OUT,evidence)
    print(json.dumps(record,indent=2))

if __name__ == '__main__': main()
