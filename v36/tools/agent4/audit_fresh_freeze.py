"""Independent read-only frozen-grid audit; never reads final holdout maps."""
import hashlib, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
def read(p): return json.loads((ROOT / p).read_text())
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def main():
    protocol = read('FRESH_VALIDATION_PROTOCOL.json')
    receipt = read('evidence/fresh-freeze-receipt.json')
    assert protocol['version'] == 2
    assert sha(ROOT/'FRESH_VALIDATION_PROTOCOL.json') == receipt['protocol_sha256']
    specs = protocol['schedules']
    assert len(specs) == 8 and len({s['id'] for s in specs}) == 8
    assert set(receipt['schedule_sha256']) == {s['id'] for s in specs}
    identities = protocol['identities']
    artifacts = {}
    for name, expected in identities.items():
        m = read('builds/'+name+'/manifest.json')
        actual = sha(m['artifact'])
        assert actual == expected == m['artifact_sha256'], name
        artifacts[name] = actual
    rows = []; all_hashes = set(); paths = {}; total = 0
    for spec in specs:
        sid = spec['id']; path = ROOT/'schedules'/(sid+'.json')
        assert sha(path) == receipt['schedule_sha256'][sid], sid
        saved = json.loads(path.read_text())
        c, opponents, maps = spec['candidate'], spec['opponents'], spec['map_hashes']
        assert len(maps) == len(set(maps))
        assert saved['candidate'] == c and saved['opponents'] == opponents
        expected = {(h,o,s) for h in maps for o in opponents for s in (0,1)}
        actual = set(); ids = set()
        for job in saved['jobs']:
            assert c in (job['a'],job['b'])
            side = 0 if job['a'] == c else 1
            other = job['b'] if side == 0 else job['a']
            actual.add((job['map_sha256'],other,side)); ids.add(job['id'])
            assert job['side'] == side and job['group_id'] == job['map_sha256']
            assert job['sha_a'] == identities[job['a']] and job['sha_b'] == identities[job['b']]
            assert sha(job['map']) == job['map_sha256']
            paths[job['map_sha256']] = job['map']
        assert actual == expected and len(ids) == len(saved['jobs']) == len(expected) == saved['games']
        assert saved['distinct_maps'] == len(maps)
        total += len(expected); all_hashes.update(maps)
        rows.append(dict(schedule=sid,maps=len(maps),games=len(expected),all_checks_passed=True))
    assert total == 760 and len(all_hashes) == 50
    development = {sha(p) for p in (ROOT/'maps/screen').glob('*.map26a')}
    # Some historical map generators call the directory screening.
    development |= {sha(p) for p in (ROOT/'maps/screening').glob('*.map26a')}
    assert not (development & all_hashes)
    report = dict(protocol_sha256=receipt['protocol_sha256'],games=total,distinct_fresh_maps=len(all_hashes),
        artifact_hashes_verified_from_bytes=artifacts,schedules=rows,development_map_hash_overlap=sorted(development & all_hashes),
        development_maps_compared=len(development),all_passed=True,
        scope='Actual artifact/map bytes, exact frozen schedule grids and receipt hashes. No final holdout file, manifest or performance inspected.',
        caveat='Distinct hashes do not remove shared-generator family dependence. Freeze-before-run chronology was observed separately; a JSON boolean alone cannot prove chronology.')
    dest=ROOT/'evidence/agent4/fresh_freeze_audit.json'
    dest.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__': main()
