"""Read-only recovered-corpus audit; no credentials, network, or database writes.

Run with the bundled Python. All outputs live under evidence/agent4.
"""
from __future__ import annotations
import collections, csv, gzip, hashlib, json, pathlib, sqlite3

V36 = pathlib.Path(__file__).resolve().parents[2]
WORK = V36.parent
DATA = WORK / 'data'
OUT = V36 / 'evidence' / 'agent4'
OWN = 'fcf5f666-ea55-4da0-b8d4-97c64bb37236'
MAP_NAMES = 'ambush collision contest corners eight elkring expanse fragment interlock lanes monolith packs pilgrimage rows rushbait-v2 stampede swirl'.split()

def sha(p):
    h = hashlib.sha256()
    with pathlib.Path(p).open('rb') as f:
        for b in iter(lambda: f.read(1024*1024), b''): h.update(b)
    return h.hexdigest()

def write(name, obj):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/name).write_text(json.dumps(obj, indent=2), encoding='utf-8')

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    matches = json.loads((DATA/'matches.json').read_text())
    con = sqlite3.connect((DATA/'battlecode.sqlite').resolve().as_uri()+'?mode=ro', uri=True)
    con.execute('PRAGMA query_only=ON')
    db = {r[0]: r[1:] for r in con.execute('SELECT match_id,trace_id,decoded,engine_ok FROM games')}
    traces = {r[0]: r[1] for r in con.execute('SELECT trace_id,engine FROM traces')}
    lineage, quarantine, selections, deep_checked = [], [], {}, set()
    for mid, m in sorted(matches.items()):
        sides = {s['side']:s for s in m['sides']}
        if set(sides) != {'a','b'}:
            quarantine.append([mid,'metadata sides invalid']); continue
        row = dict(match_id=mid, group_id=m['group_id'], created_at=m['createdAt'],
                   map=m['map'], map_id=m.get('mapId'), engine_patch=m.get('patchId'),
                   kind=m['kind'], trace_id=db.get(mid,[None])[0],
                   submission_ids=[sides[s].get('submissionId') for s in 'ab'],
                   platform_versions=[sides[s].get('version') for s in 'ab'],
                   team_ids=[sides[s]['teamId'] for s in 'ab'],
                   ordered_drafts=[[int(p) for p in sides[s]['draft']] for s in 'ab'],
                   attribution='unknown submission; metadata team identity only')
        decoded = DATA/'decoded'/f'{mid}.json.gz'
        database_verified = mid in db and db[mid][1:] == (1,1) and traces.get(db[mid][0]) == '0.0.3'
        row['verified'] = database_verified
        row['verification_basis'] = 'historical loader marked decoded/engine_ok; team/draft/score/round joins independently spot-checked per map' if database_verified else 'metadata only'
        if database_verified:
            row.update(decoded_path=str(decoded), replay_path=str(DATA/'replays'/f'{mid}.repl26a'),
                       canonical_lineups=[sorted(d) for d in row['ordered_drafts']])
        # Bound memory and I/O: one deep payload check per map; the full-corpus
        # metadata/database join above is separate from these independent checks.
        if database_verified and m['map'] not in deep_checked:
            p = json.loads(gzip.decompress(decoded.read_bytes()))
            problems = []
            if (p.get('schema'),p.get('abi'),p.get('engine')) != (1,1,'0.0.3'): problems.append('decoder semantics unsupported')
            if p['drafts'] != row['ordered_drafts']: problems.append('draft mismatch')
            if p['scores'] != [int(sides[s]['score']) for s in 'ab']: problems.append('score mismatch')
            if p['rounds'] != m['rounds']: problems.append('round mismatch')
            if p['sha256'] != row['trace_id']: problems.append('database trace mismatch')
            # Logical C&I choice creates separate type3/type4 bodies.
            for side in range(2):
                kinds = [ch['type'] for ch in p['champions'] if ch['team']==side]
                if len(kinds) != len(p['drafts'][side]) + int(3 in p['drafts'][side]): problems.append('draft/body mismatch')
            replay = DATA/'replays'/f'{mid}.repl26a'
            row.update(decoded_path=str(decoded), replay_path=str(replay), schema=p['schema'], abi=p['abi'],
                       decoded_engine=p['engine'], verified=not problems,
                       canonical_lineups=[sorted(d) for d in p['drafts']])
            if problems: quarantine.append([mid,problems])
            deep_checked.add(m['map'])
            if not problems and replay.exists():
                mapname=m['map'].replace('_','-')
                if mapname in MAP_NAMES and mapname not in selections:
                    selections[mapname]=dict(map=mapname,map_id=MAP_NAMES.index(mapname),match_id=mid,
                        replay=str(replay.resolve()),replay_sha256=p['sha256'],engine_patch=m['patchId'],
                        map_file=str((WORK/'development/current/maps'/f'{mapname}.map26a').resolve()))
        lineage.append(row)
    verified=[x for x in lineage if x['verified']]
    unique={r['trace_id'] for r in verified}
    report=dict(metadata_contexts=len(matches), metadata_sides=sum(len(m['sides']) for m in matches.values()),
        decoded_contexts=len(verified),unique_replay_hashes=len(unique),duplicate_decoded_contexts=len(verified)-len(unique),
        missing_decoded_contexts=len(matches)-len(verified),
        side_versions_present=sum(s.get('version') is not None for m in matches.values() for s in m['sides']),
        side_submission_ids_present=sum(s.get('submissionId') is not None for m in matches.values() for s in m['sides']),
        contemporaneous_ratings_present=sum(s.get('ratingBefore') is not None for m in matches.values() for s in m['sides']),
        engine_strata=dict(collections.Counter(m.get('patchId') for m in matches.values())),
        kind_strata=dict(collections.Counter(m['kind'] for m in matches.values())),
        metadata_time_range=[min(m['createdAt'] for m in matches.values()),max(m['createdAt'] for m in matches.values())],
        decoded_by_map=dict(collections.Counter(r['map'] for r in verified)),
        independently_payload_checked_maps=sorted(deep_checked),
        independently_payload_checked_contexts=len(deep_checked),
        spot_checked_body_schema_passed=True if not quarantine else False,quarantine=quarantine,
        source_manifest=dict(matches_json_sha256=sha(DATA/'matches.json'),database_bytes=(DATA/'battlecode.sqlite').stat().st_size,
            database_access='SQLite URI mode=ro and PRAGMA query_only; no bulk DB hash, no DB mutation',
            decoder_sha256=sha(WORK/'development/fast_decode.mjs'),engine_sha256=sha(WORK/'development/engine-current.wasm')),
        limitations=['No true submission ID/version or contemporaneous rating in recovered metadata.',
          'Prior opening fingerprints identify behavioral strata, not source or submission versions.',
          'Offline corpus ends before this v36 execution; no live ladder result can be inferred.',
          'Recovered-loader certification is not a fresh decode of every payload; one decoded payload per map was independently checked.',
          'Replay trace hash duplicates must stay in one split and receive one observation weight.',
          'Summary capture/first_bank metrics are collected-reward/tribute-start proxies, not clear time or completed submission.',
          'Round snapshots are omniscient descriptive observations; not visibility-correct decision features.',
          'Decoder round-state sample includes partial final round; exposure rates require censoring it.'])
    write('identity_join_manifest.json',lineage)
    write('ingestion_report.json',report)
    write('map_replay_selection.json',[selections[n] for n in MAP_NAMES if n in selections])
    write('historical_map_manifest.json',[dict(map=n,path=str(WORK/'development/current/maps'/f'{n}.map26a'),
          sha256=sha(WORK/'development/current/maps'/f'{n}.map26a')) for n in MAP_NAMES])
    print(json.dumps({k:report[k] for k in ['metadata_contexts','decoded_contexts','unique_replay_hashes','duplicate_decoded_contexts','side_versions_present','quarantine']}))

if __name__=='__main__': main()
