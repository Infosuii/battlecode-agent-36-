"""Verify pinned release bytes and audit actual consumed historical map receipts."""
import hashlib, json, pathlib, zipfile, collections
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT/'v37/evidence'
PIN = 'a020248d8b13593bdf4c70460116f2bb061860b4'
def sha(data): return hashlib.sha256(data).hexdigest()
def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')
def main():
    manifest = json.loads((ROOT/'v36/release/CANDIDATE_MANIFEST.json').read_text())
    archive = ROOT/manifest['zip']
    assert sha(archive.read_bytes()) == manifest['zip_sha256']
    checks = []
    with zipfile.ZipFile(archive) as z:
        assert len(set(z.namelist())) == len(z.namelist())
        assert set(z.namelist()) == {'src/'+n for n in manifest['files']}
        for name, expected in manifest['files'].items():
            data = z.read('src/'+name)
            assert sha(data) == expected, name
            checkout = (ROOT/'bot_v36/src'/name).read_bytes()
            assert checkout.replace(b'\r\n', b'\n') == data.replace(b'\r\n', b'\n'), name
            checks.append(dict(path=name, archive_sha256=sha(data), checkout_sha256=sha(checkout),
                               exact_bytes=checkout==data, lf_equivalent=True))
            # Build the authoritative package bytes, never silently prefer checkout line endings.
            for folder in ('baseline/src','candidate/src'):
                dest = ROOT/'v37'/folder/name
                if dest.exists():
                    if folder=='baseline/src':assert dest.read_bytes()==data, 'Baseline changed: '+str(dest)
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(data)
    write(OUT/'baseline-source-parity.json', dict(pin=PIN, zip_sha256=manifest['zip_sha256'],
        package_verified=True, files=checks, historical_manifest_toolchain_only=True,
        post_patch_wasm_sha256=None, candidate_modified=any((ROOT/'v37/candidate/src'/n).read_bytes()!=(ROOT/'v37/baseline/src'/n).read_bytes() for n in manifest['files']),
        note='Source/package audit only; actual post-patch WASM receipt is builds/v37-baseline/manifest.json and release/MANIFEST.json.'))
    suites=collections.defaultdict(lambda: dict(paths={}, receipts=[], engines=set(), games=0))
    by_hash=collections.defaultdict(set)
    for path in sorted((ROOT/'v36/agent4/results').glob('*.jsonl')):
        lines=[json.loads(line) for line in path.read_text().splitlines() if line.strip()]
        metas=[r['meta'] for r in lines if 'meta' in r]
        if not metas: continue
        meta=metas[0]
        for row in lines:
            if 'map' not in row: continue
            normalized=row['map'].replace('\\','/')
            suite=normalized.split('/')[0]
            record=suites[suite]
            record['games']+=1
            record['engines'].add(row.get('engine','unknown'))
            if path.name not in record['receipts']: record['receipts'].append(path.name)
            record['paths'].setdefault(normalized,set()).add(row.get('map_hash'))
            if row.get('map_hash'): by_hash[row['map_hash']].add(normalized)
    report={}
    for name,record in suites.items():
        record['engines']=sorted(record['engines'])
        record['paths']={k:sorted(v,key=lambda x:str(x)) for k,v in sorted(record['paths'].items())}
        record['consumed_maps']=len(record['paths'])
        report[name]=record
    write(OUT/'historical-consumption.json', dict(suites=report,
        hashes= {h:sorted(paths) for h,paths in sorted(by_hash.items())},
        caution='All listed maps consumed development data. Unlisted maps are not certified untouched. map_hash is the engine-reported identity, not verified file SHA256 without raw maps.',
        absent=['host bcws3 raw maps','full raw replays/statistics for balance receipts','compiler/native-engine build receipt','prompts/v37 handoff files (not on origin/main)']))
    print(json.dumps(dict(source_files=len(checks),zip_verified=True,
        consumed={k:v['consumed_maps'] for k,v in report.items() if k.startswith('p-')})))
if __name__=='__main__':main()
