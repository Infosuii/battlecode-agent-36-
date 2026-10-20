"""Calibrated-pool comparison split by seed halves (Agent 2).

usage: python -I agent2/tools/pool_halves.py BASE_TAG CAND_TAG [CAND_TAG ...]   (tags = bench_out/pb_<tag>)

Same pairing and weights as tools/spar_compare.py (same opponent, seed, map and side; RECOMMENDED pool weights), reported
for seeds 1-4, the fresh seeds 5-8, and all; plus our deaths per game and the change against sparexp.
"""
import math
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from spar_compare import POOL, games, mse  # noqa: E402

HALVES = {"seeds 1-4": {1, 2, 3, 4}, "seeds 5-8": {5, 6, 7, 8}, "all": set(range(1, 17))}


def seed_of(stem):
    return int(stem.split("__")[2][1:])


def main():
    base_tag, cands = sys.argv[1], sys.argv[2:]
    base = games(base_tag)
    print(f"{'candidate':10} {'half':10} {'paired':>6} {'pool change (pts)':>18} {'vs sparexp (pts)':>17} "
          f"{'deaths/game':>12} {'deaths vs sparexp':>18}")
    for c in cands:
        g = games(c)
        for name, seeds in HALVES.items():
            common = [s for s in g if s in base and seed_of(s) in seeds]
            if not common:
                continue
            per = defaultdict(list)
            for s in common:
                per[s.split("__")[1]].append((g[s][0] - base[s][0], g[s][1] - base[s][1]))
            tot = sum(POOL[o] for o in per if o in POOL)
            d = var = 0.0
            for o, v in per.items():
                if o not in POOL:
                    continue
                m, se = mse([x for x, _ in v])
                d += POOL[o] / tot * m
                var += (POOL[o] / tot * se) ** 2
            sx = per.get("sparexp", [])
            sxm, sxse = mse([x for x, _ in sx]) if sx else (0.0, 0.0)
            dm, dse = mse([y for v in per.values() for _, y in v])
            sdm, sdse = mse([y for _, y in sx]) if sx else (0.0, 0.0)
            print(f"{c:10} {name:10} {len(common):6} {100 * d:+9.1f} +- {100 * math.sqrt(var):4.1f} "
                  f"{100 * sxm:+9.1f} +- {100 * sxse:4.1f} {dm:+6.2f}+-{dse:.2f} {sdm:+10.2f} +- {sdse:.2f}")


if __name__ == "__main__":
    main()
