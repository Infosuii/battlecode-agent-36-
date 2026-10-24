"""Preserve this local checkpoint and copy its reviewable inputs into the Git clone."""
from pathlib import Path
import hashlib, json, shutil, subprocess, zipfile

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'analysis/v36/recovery'
PACKAGE = ROOT / 'work/agent3-review/agent3/windows-verification'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    patch = ROOT / 'analysis/v36/agent3/results/agent3_overlay.patch'
    check = OUT / 'patch_check_v3'
    dest = check / 'bot_v36/src'
    dest.mkdir(parents=True, exist_ok=False)
    for p in (ROOT / 'bot_v34/src').iterdir():
        if p.is_file(): shutil.copy2(p, dest / p.name)
    for extra in [['--check'], ['--whitespace=nowarn']]:
        subprocess.run(['git', '-c', 'core.autocrlf=false', 'apply', *extra, str(patch)], cwd=check, check=True)
    for p in (ROOT / 'bot_v36/src').iterdir():
        if p.is_file(): assert p.read_bytes() == (dest / p.name).read_bytes(), p.name
    record = {'status':'passed', 'method':'git -c core.autocrlf=false apply --check, apply to frozen copy, byte equality with all five owned candidate files', 'overlay_sha256':sha(patch)}
    (ROOT / 'analysis/v36/agent3/results/patch_check.json').write_text(json.dumps(record, indent=2)+'\n', encoding='utf8')

    files = sorted(p for folder in ['results', 'artifacts'] for p in (ROOT / 'work/recovery' / folder).rglob('*') if p.is_file())
    archive = OUT / 'match_evidence.zip'
    index = []
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
        for p in files:
            name = p.relative_to(ROOT).as_posix()
            bundle.write(p, name)
            index.append({'path':name, 'bytes':p.stat().st_size, 'sha256':sha(p)})
    with zipfile.ZipFile(archive) as bundle: assert bundle.testzip() is None
    (OUT / 'match_evidence_manifest.json').write_text(json.dumps({'archive_sha256':sha(archive), 'archive_bytes':archive.stat().st_size, 'files':index}, indent=2)+'\n', encoding='utf8')

    selected = []
    for folder in ['bot_v36/src', 'analysis/v36/agent3', 'work/recovery/ops', 'work/recovery/eval/maps']:
        for p in (ROOT / folder).rglob('*'):
            if p.is_file() and not any(part in ['scorer_builds', 'patch_check', '__pycache__'] for part in p.relative_to(ROOT / folder).parts): selected.append(p)
    selected += [p for p in OUT.iterdir() if p.is_file()]
    selected += [ROOT / 'work/recovery/eval/cambc.toml']
    for p in selected:
        target = PACKAGE / p.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, target)
        assert sha(p) == sha(target)
    # The old SDK is reinstalled through the pinned CLI; do not publish its cache/runtime.
    print(json.dumps({'archive_bytes':archive.stat().st_size, 'raw_files':len(files), 'published_files':len(selected), 'patch_check':record}, indent=2))

if __name__ == '__main__': main()
