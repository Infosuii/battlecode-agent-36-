"""Build a validated match-level dataset from collected match-group files.

Usage: python dataset.py [groups dirs...] -> ../data/matches.jsonl and ../reports/coverage.json

One row per finished match: group/match UUIDs (dedup key: match UUID), time, kind, map name/UUID, engine and schema
versions, rounds, outcome, and per side: team UUID/name, group-level ratingBefore (ranked groups only), ordered draft,
canonical lineup (sorted set; C&I is one pick), draft size, score, and end statistics. Rows with missing drafts,
unknown outcomes or incompatible schema are counted in the coverage report, not silently dropped.
Integer metrics arrive as strings in some schemas; all metrics are coerced to float here.
"""
import collections
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'data')
REP = os.path.join(HERE, '..', 'reports')

CANON = {'c&i': 'cassi_and_issac', 'cassi & issac': 'cassi_and_issac', 'cassi_and_issac': 'cassi_and_issac',
         'vesper': 'vesper_vale', 'vesper vale': 'vesper_vale'}


# Numeric drafts (engine 0.0.3 API) follow the SDK enum bc::champion_draft order (cambc.h, SDK 2.2.5).
ENUM = ['ryth', 'vesper_vale', 'vendetta', 'cassi_and_issac', 'stellar', 'prism', 'marcella', 'willow', 'ren', 'varg',
        'tavi', 'heart_of_ruin']  # heart_of_ruin added in SDK 3.0 (engine 1.0.0)


def canon(c):
    if isinstance(c, int) or str(c).isdigit():
        return ENUM[int(c)]
    c = str(c).strip().lower()
    return CANON.get(c, c.replace(' ', '_'))


def main():
    dirs = [a for a in sys.argv[1:]] or [os.path.join(DATA, 'groups')]
    files = [f for d in dirs for f in glob.glob(os.path.join(d, '*.json'))]
    cov = collections.Counter()
    seen = set()
    rows = []
    for f in files:
        try:
            g = json.load(open(f))
        except Exception:
            cov['bad_file'] += 1
            continue
        grp = g.get('group', g)
        gsides = {s['side']: s for s in grp.get('sides', [])}
        for m in g.get('matches', []):
            cov['matches'] += 1
            if m['id'] in seen:
                cov['duplicate'] += 1
                continue
            seen.add(m['id'])
            if m.get('status') != 'finished':
                cov['not_finished'] += 1
                continue
            st = m.get('statistics') or {}
            sides = m.get('sides') or []
            if len(sides) != 2 or not all(s.get('draft') for s in sides):
                cov['missing_draft'] += 1
                continue
            if st.get('outcome') not in ('teamA', 'teamB', 'draw', 'tie', None):
                cov['odd_outcome'] += 1
            res = {s['side']: s.get('result') for s in sides}
            if set(res.values()) - {'win', 'loss', 'draw'}:
                cov['missing_result'] += 1
                continue
            row = {'group': grp['id'], 'match': m['id'], 'kind': m.get('kind'), 'at': m.get('createdAt'),
                   'map': m.get('map'), 'map_id': m.get('mapId'), 'rounds': m.get('rounds'),
                   'engine': st.get('engineVersion') or (m.get('patchId') or '').split(':')[0] or None, 'schema': st.get('schemaVersion'), 'sides': []}
            stat_teams = st.get('teams') or []
            for i, s in enumerate(sorted(sides, key=lambda s: s['side'])):
                draft = [canon(c) for c in s['draft']]
                gs = gsides.get(s['side'], {})
                metrics = {}
                if i < len(stat_teams):
                    for k, v in stat_teams[i].items():
                        try:
                            metrics[k] = float(v)
                        except (TypeError, ValueError):
                            pass
                row['sides'].append({'side': s['side'], 'team': s['teamId'], 'name': s.get('teamName'),
                                     'rating_before': gs.get('ratingBefore'), 'draft': draft,
                                     'lineup': sorted(set(draft)), 'size': len(draft), 'score': s.get('score'),
                                     'result': s['result'], 'metrics': metrics})
            if row['sides'][0]['size'] != row['sides'][1]['size']:
                cov['size_mismatch'] += 1
            cov['kept'] += 1
            cov[f"engine_{row['engine']}"] += 1
            cov[f"kind_{row['kind']}"] += 1
            rows.append(row)
    os.makedirs(REP, exist_ok=True)
    with open(os.path.join(DATA, 'matches.jsonl'), 'w') as f:
        for r in sorted(rows, key=lambda r: r['at'] or ''):
            f.write(json.dumps(r) + '\n')
    cov['files'] = len(files)
    span = [rows and min(r['at'] for r in rows), rows and max(r['at'] for r in rows)]
    json.dump({'counts': dict(cov), 'span': span}, open(os.path.join(REP, 'coverage.json'), 'w'), indent=1)
    print(json.dumps({'counts': dict(cov), 'span': span}))


if __name__ == '__main__':
    main()
