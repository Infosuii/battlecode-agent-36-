"""Stage attribution and fight/run search coverage from a2_lab_* stdout (Agent 2, E2).

usage: python -I agent2/tools/prof_report.py TAG [TAG ...] [--cand NAME]

Reads team-*.stdout inside each replay bundle in bench_out/TAG for the teams played by the candidate NAME
(default a2_lab_prof; both teams in a mirror). Mirror games duplicate each other across sides, so a mirror run is
read from side A only.
Lines used:
  HEAVY <why> R<round> start <s> used <u>: sec=fuel ...    (profiler: suspended or >90k turns)
  SUSPAT R<round> before <checkpoint>                      (first checkpoint reached in a later round)
  TDS R<r> <type> e8 E a7 A n N m M h H st S prep P play p1 p2 p3 p4 total T fr F R
  TDX R<r> <type> e8 E a7 A why W                         (enemy within search range, search not called)
"""
import re
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path
from statistics import median

ROOT = Path(__file__).resolve().parents[2]
STATUS = {0: "no-enemy", 1: "safety", 2: "budget", 3: "incomplete", 4: "margin", 5: "fight", 6: "run"}


def lines_for(tag, cand):
    d = ROOT / "bench_out" / tag
    for rp in sorted(d.glob("*.repl26a")):
        base, rest = rp.stem.split("__", 1)
        side = rest.rsplit("_", 1)[1]
        mirror = base == cand
        if mirror and side == "B":
            continue
        teams = ["a", "b"] if mirror else ["a" if side == "A" else "b"]
        z = zipfile.ZipFile(rp)
        for t in teams:
            # The engine's capture pads lines with NUL bytes.
            yield rp.stem, [ln.strip("\x00 \t\r") for ln in z.read(f"team-{t}.stdout").decode("utf-8", "replace").split("\n")]


def main():
    args = sys.argv[1:]
    cand = args[args.index("--cand") + 1] if "--cand" in args else "a2_lab_prof"
    tags = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] != "--cand")]
    for tag in tags:
        heavy_why, heavy_top, suspat = Counter(), Counter(), Counter()
        heavy_first = 0
        tds_scene = defaultdict(Counter)
        tds_n = defaultdict(Counter)
        tdx = Counter()
        play_cost = defaultdict(list)
        prep_cost = defaultdict(list)
        total_cost = defaultdict(list)
        n_files = 0
        for stem, lines in lines_for(tag, cand):
            n_files += 1
            for ln in lines:
                if ln.startswith("HEAVY"):
                    m = re.match(r"HEAVY (\S+) R(\d+) start (\d+) used (\d+):(.*)", ln)
                    if not m:
                        continue
                    why, r, start = m.group(1), int(m.group(2)), int(m.group(3))
                    secs = dict((k, int(v)) for k, v in re.findall(r"(\w+)=(\d+)", m.group(5)))
                    heavy_why[why] += 1
                    heavy_first += start > 100000
                    # The deepest heavy section: the biggest one that is not an enclosing scope.
                    leaf = {k: v for k, v in secs.items() if k not in ("init",)}
                    top = max(leaf.items(), key=lambda kv: kv[1])[0] if leaf else "-"
                    heavy_top[(why, top)] += 1
                elif ln.startswith("SUSPAT"):
                    suspat[ln.split("before ", 1)[1].strip()] += 1
                elif ln.startswith("TDS"):
                    m = re.match(r"TDS R(\d+) (\S+) e8 (\d+) a7 (\d+) n (\d+) m (\d+) h (\d+) st (\d+) prep (\d+) "
                                 r"play (\d+) (\d+) (\d+) (\d+) total (\d+)", ln)
                    if not m:
                        continue
                    e8, a7, n, _, h, st, prep = (int(m.group(i)) for i in range(3, 10))
                    plays = [int(m.group(i)) for i in range(10, 14) if int(m.group(i)) > 0]
                    tds_scene[min(1 + a7 + e8, 11)][STATUS[st]] += 1
                    tds_n[n][STATUS[st]] += 1
                    for c in plays:
                        play_cost[(n, h)].append(c)
                    if plays:
                        prep_cost[n].append(prep)
                        total_cost[(n, h)].append(int(m.group(14)))
                elif ln.startswith("TDX"):
                    p = ln.split()
                    tdx[p[-1]] += 1
        print(f"== {tag}: {n_files} team logs")
        print("   HEAVY turns:", dict(heavy_why), f"(first-turn budget: {heavy_first})")
        print("   heaviest section on HEAVY turns:", ", ".join(f"{w}/{s} {c}" for (w, s), c in heavy_top.most_common(12)))
        print("   SUSPAT (checkpoint reached late):", ", ".join(f"{k} {v}" for k, v in suspat.most_common()))
        print("   search not called with an enemy within range:", dict(tdx))
        print("   fight/run status by original scene size (1 + allies<=7 + enemies<=8):")
        cols = list(STATUS.values())
        print("   scene  calls  " + "  ".join(f"{c:>10}" for c in cols) + "   verdict%")
        for sc in sorted(tds_scene):
            c = tds_scene[sc]
            tot = sum(c.values())
            ver = c["fight"] + c["run"]
            print(f"   {sc:>5}{'+' if sc == 11 else ' '} {tot:6d}  " + "  ".join(f"{c[k]:10d}" for k in cols) + f"   {100.0 * ver / tot:7.1f}")
        print("   by snapshot size n:")
        for n in sorted(tds_n):
            c = tds_n[n]
            tot = sum(c.values())
            print(f"   n={n}: {tot:6d}  " + ", ".join(f"{k} {c[k]}" for k in cols if c[k]))
        print("   measured fuel per playout by (n, horizon): median / max  [estimate 2200*n*(h+1)]")
        for (n, h) in sorted(play_cost):
            v = play_cost[(n, h)]
            print(f"   n={n} h={h}: {len(v):5d} playouts  median {median(v):7.0f}  max {max(v):7d}   est {2200 * n * (h + 1)}")
        print("   preparation (entry to first playout) by n: median / max")
        for n in sorted(prep_cost):
            v = prep_cost[n]
            print(f"   n={n}: median {median(v):6.0f}  max {max(v):6d}")


if __name__ == "__main__":
    main()
