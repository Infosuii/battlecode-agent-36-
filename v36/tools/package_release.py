"""Freeze selected source defaults, build without experimental -D flags, bundle and verify."""
import argparse, json, pathlib, re, shutil, zipfile
from evaluate import ROOT, CLI, PROJECT, SWITCHES, build, manifest, run_cmd, sha, tree, write

def package(selected):
    if selected=='baseline':
        raise ValueError('Use cache-source-off for the parity-verified baseline-behavior candidate; original baseline is rollback only')
    original = manifest(selected)
    if tree(ROOT/'builds'/selected/'src') != original['source_tree'] or sha(original['artifact']) != original['artifact_sha256']:
        raise ValueError('Selected development source or artifact changed')
    flags = dict(SWITCHES)
    flags.update(original['switches'])
    if flags['NOVA_RUNTIME_TELEMETRY'] or flags['V36_FARM_DIAG'] or flags['V36_FARM_EXHAUSTIVE']:
        raise ValueError('Diagnostic build cannot be released')
    release = ROOT/'release'
    if release.exists():
        raise ValueError('Release is immutable; inspect existing package before any replacement')
    source = release/'bot_v36'
    shutil.copytree(ROOT/'builds'/selected/'src', source/'src')
    header = source/'src/common.h'
    text = header.read_text()
    for key, value in flags.items():
        text, count = re.subn(r'(?m)^(#define '+re.escape(key)+r') [^\r\n]+$',
                              lambda m:m.group(1)+' '+str(value), text)
        if count != 1:
            raise ValueError('Unambiguous packaged default required: '+key)
    header.write_text(text, encoding='utf-8')
    shutil.copyfile(PROJECT/'cambc.toml', release/'cambc.toml')
    exact = build('release-final', flags, 'release/bot_v36', materialized=True, normalize=True)
    shutil.copyfile(exact['artifact'], release/'bot_v36.wasm')
    archive = release/'bot_v36.zip'
    log = run_cmd([CLI, 'bundle', source, '--output', archive], cwd=PROJECT, timeout=120)
    (release/'bundle.log').write_text(log)
    extracted = release/'verified_archive'
    with zipfile.ZipFile(archive) as z:
        if any(pathlib.PurePosixPath(n).is_absolute() or '..' in pathlib.PurePosixPath(n).parts for n in z.namelist()):
            raise ValueError('Unsafe archive entry')
        z.extractall(extracted)
    roots = [p.parent for p in extracted.rglob('common.h')]
    if len(roots) != 1 or tree(roots[0]) != tree(source/'src'):
        raise ValueError('Bundled source does not match frozen release source')
    archive_source = release/'archive_rebuild_source'
    shutil.copytree(roots[0], archive_source/'src')
    rebuilt = build('release-final-archive', flags, str(archive_source.relative_to(ROOT)), materialized=True, normalize=True)
    if rebuilt['artifact_sha256'] != exact['artifact_sha256']:
        raise ValueError('Re-extracted archive did not reproduce exact release artifact')
    rollback = release/'rollback'
    rollback.mkdir()
    baseline = manifest('baseline')
    shutil.copyfile(baseline['artifact'], rollback/'baseline.wasm')
    shutil.copytree(ROOT/'bot_v34/src', rollback/'src')
    baseline_source=json.loads((ROOT/'BASELINE_MANIFEST.json').read_text())
    if sha(baseline_source['archive'])!=baseline_source['archive_sha256']:
        raise ValueError('Original rollback archive changed')
    shutil.copyfile(baseline_source['archive'],rollback/'bot_v34_original.zip')
    shutil.copyfile(ROOT/'BASELINE_MANIFEST.json',rollback/'BASELINE_MANIFEST.json')
    write(release/'CANDIDATE_MANIFEST.json', dict(selected_development_build=selected,
        selected_artifact_sha256=original['artifact_sha256'], exact_build=exact,
        archive_sha256=sha(archive), archive_source_tree=tree(roots[0]),
        archive_rebuild=rebuilt['artifact_sha256'], rollback_artifact_sha256=sha(rollback/'baseline.wasm'),
        platform_status='Local release preparation only; no live submission or publication.'))
    write(ROOT/'CANDIDATE_MANIFEST.json', json.loads((release/'CANDIDATE_MANIFEST.json').read_text()))
    print(json.dumps(dict(release=str(release), artifact=exact['artifact_sha256'], archive=sha(archive))), flush=True)

if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('selected'); args=p.parse_args(); package(args.selected)
