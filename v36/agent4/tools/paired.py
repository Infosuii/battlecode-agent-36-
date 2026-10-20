"""Paired full-match comparison: candidate vs baseline on a map set, both sides.

Usage:
  python paired.py --name EXP --cand NAME[:DEF=VAL,...] --base NAME[:DEF=VAL,...] --maps DIR[,DIR] [--limit N] [--jobs 8]

A bot spec is a source tree name under the repo (bot_v34, bot_v36) plus optional macro defines, which are prepended to
common.h of a generated copy in the cambc workspace (bots/<tree>__<hash>). Builds use --release (platform settings).
Matches are deterministic for a given (bots, map, side), so each map/side pair is one observation; the unit of
independence is the map (both sides together). Output: one JSON line per game in results/<EXP>.jsonl plus a summary
with a map-clustered bootstrap interval.
"""
import argparse
import hashlib
import json
import os
import random
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
WS = os.environ.get('CAMBC_WS', r'C:\Users\info\bcws')
CAMBC = os.environ.get('CAMBC', r'C:\Users\info\bcvenv\Scripts\cambc.exe')
OUT = os.path.join(REPO, 'v36', 'agent4', 'results')


def parse_spec(spec):
    tree, _, defs = spec.partition(':')
    # Defines are separated by ';' when any value contains commas (pick lists), else by ','.
    pairs = [d.split('=', 1) for d in defs.split(';' if ';' in defs else ',') if d]
    return tree, [(k, v) for k, v in pairs]


def materialize(spec):
    """Copy the source tree with the defines into the workspace; build once per unique content."""
    tree, defs = parse_spec(spec)
    src = os.path.join(REPO, tree, 'src')
    h = hashlib.sha256()
    for f in sorted(os.listdir(src)):
        h.update(f.encode())
        h.update(open(os.path.join(src, f), 'rb').read())
    h.update(repr(defs).encode())
    name = f'{tree}__{h.hexdigest()[:10]}'
    dst = os.path.join(WS, 'bots', name)
    wasm = os.path.join(dst, '.cambc', 'build-release', 'bot-injected.wasm')
    if not os.path.exists(wasm):
        shutil.rmtree(dst, ignore_errors=True)
        shutil.copytree(src, os.path.join(dst, 'src'))
        if defs:
            p = os.path.join(dst, 'src', 'common.h')
            body = open(p, encoding='utf-8').read()
            head = ''
            for k, v in defs:
                # Replace the first existing definition (the tuned block, or the guarded default) in place; a second
                # unguarded #define of the same macro would not compile cleanly.
                pat = re.compile(rf'^#define {re.escape(k)}\b.*$', re.M)
                if pat.search(body):
                    body = pat.sub(lambda _m: f'#define {k} {v}', body, count=1)
                else:
                    head += f'#define {k} {v}\n'
            open(p, 'w', encoding='utf-8').write(head + body)
        r = subprocess.run([CAMBC, 'build', '--release', name], cwd=WS, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(wasm):
            sys.exit(f'build failed for {spec}:\n{r.stdout[-2000:]}\n{r.stderr[-2000:]}')
    return name, h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--name', required=True)
    ap.add_argument('--cand', required=True)
    ap.add_argument('--base', required=True)
    ap.add_argument('--maps', required=True)
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--offset', type=int, default=0)
    ap.add_argument('--jobs', type=int, default=8)
    a = ap.parse_args()

    cand, cand_hash = materialize(a.cand)
    base, base_hash = materialize(a.base)
    maps = []
    for d in a.maps.split(','):
        maps += sorted(os.path.join(d, m) for m in os.listdir(os.path.join(WS, d)) if m.endswith('.map26a'))
    maps = maps[a.offset:]
    if a.limit:
        maps = maps[:a.limit]

    os.makedirs(OUT, exist_ok=True)
    run_dir = os.path.join(WS, 'runs', a.name)
    shutil.rmtree(run_dir, ignore_errors=True)
    os.makedirs(run_dir)
    rows = []
    with open(os.path.join(run_dir, 'manifest.txt'), 'w') as mf:
        for i, m in enumerate(maps):
            for side in 'AB':
                A, B = (cand, base) if side == 'A' else (base, cand)
                stem = os.path.join(run_dir, f'{i:04d}{side}')
                bots = os.path.join(WS, 'bots')
                line = (f'{os.path.join(bots, A)} {os.path.join(bots, B)} {os.path.join(WS, m)} {stem}.repl26a '
                        f'--statistics {stem}.json --json\n')
                mf.write(line.replace('\\', '/'))
                rows.append((m, side, stem))
    subprocess.run([CAMBC, 'run-batch', '--release', '--jobs', str(a.jobs), '--log-limit', '0', '--color', 'never',
                    os.path.join(run_dir, 'manifest.txt')], cwd=WS, capture_output=True, text=True)

    games = []
    for m, side, stem in rows:
        try:
            s = json.load(open(stem + '.json'))
        except Exception:
            games.append({'map': m, 'cand_side': side, 'missing': True})
            continue
        ct = 'teamA' if side == 'A' else 'teamB'
        ot = 'teamB' if side == 'A' else 'teamA'
        gems = {t['team']: int(t['submittedGems']) for t in s['teams']}
        draft = {}
        for d in s['roster']['draft']:
            draft.setdefault(d['team'], []).append(d['champion'])
        deaths = {ct: 0, ot: 0}
        for c in s['champions']:
            deaths[c['team']] += int(c['metrics']['deaths'])
        out = s['match']['outcome']
        games.append({'map': m, 'cand_side': side, 'result': 1.0 if out == ct else 0.0 if out == ot else 0.5,
                      'gem_diff': gems[ct] - gems[ot], 'cand_draft': draft.get(ct), 'base_draft': draft.get(ot),
                      'rounds': s['match']['roundsStarted'], 'cand_deaths': deaths[ct], 'base_deaths': deaths[ot],
                      'faults': s['diagnostics']['botFaults'], 'map_hash': s['match']['mapHash'],
                      'engine': s['match']['engineVersion']})

    meta = {'exp': a.name, 'cand': a.cand, 'cand_hash': cand_hash, 'base': a.base, 'base_hash': base_hash,
            'maps': a.maps, 'offset': a.offset, 'n_maps': len(maps)}
    with open(os.path.join(OUT, a.name + '.jsonl'), 'w') as f:
        f.write(json.dumps({'meta': meta}) + '\n')
        for g in games:
            f.write(json.dumps(g) + '\n')
    print(json.dumps(summarize(games, meta)))


def summarize(games, meta):
    ok = [g for g in games if not g.get('missing')]
    by_map = {}
    for g in ok:
        by_map.setdefault(g['map'], []).append(g)
    units = [(sum(g['result'] for g in v) / len(v), sum(g['gem_diff'] for g in v) / len(v)) for v in by_map.values()]
    n = len(units)
    if n == 0:
        return {**meta, 'games': 0}
    wr = sum(u[0] for u in units) / n
    gd = sum(u[1] for u in units) / n
    rng = random.Random(1)
    boots = sorted(sum(units[rng.randrange(n)][0] for _ in range(n)) / n for _ in range(2000))
    same = sum(1 for g in ok if g['cand_draft'] == g['base_draft'])
    return {**meta, 'games': len(ok), 'missing': len(games) - len(ok), 'win_rate': round(wr, 4),
            'ci95': [round(boots[49], 4), round(boots[1949], 4)], 'gem_diff': round(gd, 1),
            'cand_deaths': sum(g['cand_deaths'] for g in ok), 'base_deaths': sum(g['base_deaths'] for g in ok),
            'same_draft_games': same}


if __name__ == '__main__':
    main()
