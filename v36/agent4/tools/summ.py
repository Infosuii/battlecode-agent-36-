"""Summarize paired.py result files: python summ.py results/e1_*.jsonl [--by size|draft]"""
import collections
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from paired import summarize  # noqa: E402

by = sys.argv[sys.argv.index('--by') + 1] if '--by' in sys.argv else None
files = [f for a in sys.argv[1:] if not a.startswith('--') and a not in ('size', 'draft') for f in sorted(glob.glob(a))]
for f in files:
    lines = [json.loads(l) for l in open(f)]
    meta, games = lines[0]['meta'], lines[1:]
    s = summarize(games, meta)
    print(f"{meta['exp']:14s} n={s['games']:4d} win={s.get('win_rate')} ci={s.get('ci95')} gems={s.get('gem_diff')} "
          f"deaths {s.get('cand_deaths')}/{s.get('base_deaths')} same_draft={s.get('same_draft_games')}")
    if by:
        groups = collections.defaultdict(list)
        for g in games:
            if g.get('missing'):
                continue
            key = len(g['base_draft'] or []) if by == 'size' else ' / '.join(g['base_draft'] or [])
            groups[key].append(g)
        for k, v in sorted(groups.items(), key=lambda kv: -len(kv[1])):
            ss = summarize(v, {})
            print(f"    {str(k)[:60]:60s} n={ss['games']:4d} win={ss['win_rate']} gems={ss['gem_diff']}")
