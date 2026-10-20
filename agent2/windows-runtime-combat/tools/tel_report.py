"""Report for a2_lab_tel runs (Agent 2): where suspended turns ran out of fuel, and fight/run search coverage and cost.

usage: python -I agent2/tools/tel_report.py TAG [TAG ...] [--cand NAME]

Reads A2SUS and A2TD lines from team-*.stdout in each replay bundle (NUL padding stripped) for the teams played by
the candidate (default a2_lab_tel). A mirror run is read from side A only (side B repeats the same games).
  A2SUS R<r> t<type> to R<r2>: start=<fuel> init=<fuel> ... end=<fuel>   ('*' marks the first reading of a new budget)
  A2TD R<r> t<type> dc<cost of the previous dump> key=count ...            (counts since the previous dump)
Keys: 4096 + scene*8 + status; 8192 + (n*4 + horizon)*100 + fuel/1000 (one playout); 12288 + n*16 + fuel/1000 (prep).
"""
import re
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STATUS = ["no-enemy", "safety", "budget", "incomplete", "margin", "fight", "run"]
HELPERS = {5: "counter_ambush", 6: "ward_reveal", 7: "caster_micro", 8: "escape_or_stand", 9: "walk_toward",
           10: "ward_ahead"}

STAGE = {  # the stage that ran out, from the last mark before the new budget
    "start": "turn start (pre-init)", "init": "brain/shared/refresh", "boards": "init", "spawn_field": "init",
    "group_loop": "init", "tables": "init", "refresh": "world: vision", "w_vis": "world: camp groups",
    "w_groups": "world: hot zone", "w_hot": "world: champions", "w_champs": "world: enemy-ward clusters",
    "w_ward": "world: add sightings", "w_sight": "world: enemy-near-camp loop",
    "w_update": "world: beliefs/numbers", "world": "episode tick", "episode": "shared sightings read/share",
    "sight": "events publish", "events": "shop/emblems", "shop": "support casts", "support": "ward reveal",
    "wardrev": "counter-ambush", "ambush": "enemies: entry", "e_in": "enemies: altar lock",
    "e_altar": "enemies: burst kill", "e_lock": "enemies: bush ambush",
    "e_bush": "enemies: formula/target", "td0": "fight/run search", "e_td": "enemies: fight-sim/strength gate",
    "td1": "enemies: after search", "e_mid": "enemies: tsearch/cmicro/stand",
    "e_prestand": "enemies: escape_or_stand", "e_stand": "enemies: flee movement", "e_noflee": "enemies: kite",
    "e_prefight": "enemies: fight movement", "enemies": "post-fight / farm entry", "farm0": "farm: before scoring",
    "score0": "farm: scoring", "route0": "farm: route search", "route1": "farm: after route search",
    "farmsel": "farm: bank/claim/recall", "farmmove": "farm: movement", "decide": "ward/prewarm/altars/priority",
    "tail": "fields", "field": "fields", "fields": "turn tail",
}


def logs(tag, cand):
    for rp in sorted((ROOT / "bench_out" / tag).glob("*.repl26a")):
        base, rest = rp.stem.split("__", 1)
        side = rest.rsplit("_", 1)[1]
        mirror = base == cand
        if mirror and side == "B":
            continue
        z = zipfile.ZipFile(rp)
        for t in (["a", "b"] if mirror else ["a" if side == "A" else "b"]):
            yield [ln.strip("\x00 \t\r") for ln in z.read(f"team-{t}.stdout").decode("utf-8", "replace").split("\n")]


def quantile(hist, q):
    tot = sum(hist.values())
    acc = 0
    for k in sorted(hist):
        acc += hist[k]
        if acc >= q * tot:
            return k
    return None


def main():
    args = sys.argv[1:]
    cand = args[args.index("--cand") + 1] if "--cand" in args else "a2_lab_tel"
    tags = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] != "--cand")]
    for tag in tags:
        stage, interval, left_at = Counter(), Counter(), defaultdict(list)
        chains = Counter()
        n_sus = 0
        status = defaultdict(Counter)
        cost = defaultdict(Counter)
        prep = defaultdict(Counter)
        bush = defaultdict(Counter)
        helper = defaultdict(Counter)
        omit = defaultdict(Counter)
        auth = Counter()
        fleehit = Counter()
        cm_nt = defaultdict(Counter)
        cm_eval = defaultdict(Counter)
        checks = defaultdict(Counter)
        dump_cost = []
        n_logs = 0
        for lines in logs(tag, cand):
            n_logs += 1
            for ln in lines:
                if ln.startswith("A2SUS"):
                    m = re.match(r"A2SUS R(\d+) t(\d+) to R(\d+):(.*)", ln)
                    if not m:
                        continue
                    n_sus += 1
                    chains[int(m.group(3)) - int(m.group(1))] += 1
                    marks = re.findall(r"(\w+)=(\d+)(\*?)", m.group(4))
                    for i, (w, f, star) in enumerate(marks):
                        if star and i > 0:
                            prev, pf = marks[i - 1][0], int(marks[i - 1][1])
                            stage[STAGE.get(prev, prev)] += 1
                            interval[(prev, w)] += 1
                            left_at[STAGE.get(prev, prev)].append(pf)
                            break
                    else:
                        stage["(no new-budget mark)"] += 1
                elif ln.startswith("A2TD"):
                    m = re.match(r"A2TD R\d+ t\d+ dc(\d+)(.*)", ln)
                    if not m:
                        continue
                    if int(m.group(1)):
                        dump_cost.append(int(m.group(1)))
                    for k, c in re.findall(r"(\d+)=(\d+)", m.group(2)):
                        k, c = int(k), int(c)
                        kind, idx = divmod(k, 4096)
                        if kind == 1:
                            status[min(idx // 8, 11)][STATUS[idx % 8]] += c
                        elif kind == 2:
                            nh, b = divmod(idx, 100)
                            cost[divmod(nh, 4)][b] += c
                        elif kind == 3:
                            n, b = divmod(idx, 16)
                            prep[n][b] += c
                        elif kind == 4:
                            sz, b = divmod(idx, 64)
                            bush[sz][b] += c
                        elif kind in HELPERS:
                            helper[HELPERS[kind]][idx] += c
                        elif kind == 11:
                            st, flags = divmod(idx, 8)
                            omit[STATUS[st]][flags] += c
                        elif kind == 12:
                            auth[idx] += c
                        elif kind == 13:
                            fleehit[idx] += c
                        elif kind == 14:
                            ntc, b = divmod(idx, 100)
                            cm_nt[ntc][b] += c
                        elif kind == 15:
                            ntc, b = divmod(idx, 100)
                            cm_eval[ntc][b] += c
                        elif kind in (16, 17, 18):
                            checks[{16: "caster_micro danger", 17: "counter_ambush distance", 18: "bush labels"}[kind]][idx] += c
        print(f"== {tag}: {n_logs} team logs, {n_suspended(n_sus)}")
        print("   rounds crossed per suspended turn:", dict(sorted(chains.items())))
        print("   stage running when the budget ran out (fuel left at its start: median):")
        for s, c in stage.most_common():
            v = sorted(left_at.get(s, [0]))
            print(f"     {s:36s} {c:4d}  ({100.0 * c / max(1, n_sus):4.1f}%)  left {v[len(v) // 2]:6d}")
        print("   fight/run search status by original scene size (1 + allies<=7 + enemies<=8):")
        print("   scene   calls " + "".join(f"{s:>11}" for s in STATUS) + "  verdict%")
        tot_all = Counter()
        for sc in sorted(status):
            c = status[sc]
            tot_all.update(c)
            tot = sum(c.values())
            print(f"   {sc:>4}{'+' if sc == 11 else ' '} {tot:7d} " + "".join(f"{c[s]:11d}" for s in STATUS) +
                  f"  {100.0 * (c['fight'] + c['run']) / tot:7.1f}")
        tot = sum(tot_all.values())
        print(f"   all   {tot:7d} " + "".join(f"{tot_all[s]:11d}" for s in STATUS) +
              f"  {100.0 * (tot_all['fight'] + tot_all['run']) / max(1, tot):7.1f}")
        print("   playout fuel by (n, horizon), thousands: median / p90 / p99 / max   [old estimate 2.2*n*(h+1)]")
        for (n, h) in sorted(cost):
            hst = cost[(n, h)]
            print(f"     n={n} h={h}: {sum(hst.values()):6d}  {quantile(hst, .5):3d} / {quantile(hst, .9):3d} / "
                  f"{quantile(hst, .99):3d} / {max(hst):3d}   [{2.2 * n * (h + 1):.1f}]")
        print("   preparation by n, thousands: median / max")
        for n in sorted(prep):
            print(f"     n={n}: {quantile(prep[n], .5)} / {max(prep[n])}")
        if bush:
            print("   bush-cluster labelling (first use per champion): cluster size bucket -> count, fuel (thousands) median / max")
            for sz in sorted(bush):
                h = bush[sz]
                print(f"     {sz * 50:4d}-{sz * 50 + 49:<4d} tiles: {sum(h.values()):5d} labels  "
                      f"{2 * quantile(h, .5):3d} / {2 * max(h):3d}")
        if omit:
            print("   omissions by status (calls; dropped enemy / dropped enemy that reaches us / split linked pair):")
            for st in STATUS:
                h = omit.get(st)
                if not h:
                    continue
                tot = sum(h.values())
                dropped = sum(v for k, v in h.items() if k & 4)
                reach = sum(v for k, v in h.items() if k & 1)
                link = sum(v for k, v in h.items() if k & 2)
                print(f"     {st:10s} {tot:6d}   {dropped:6d} ({100.0 * dropped / tot:4.1f}%)  {reach:6d} ({100.0 * reach / tot:4.1f}%)"
                      f"  {link:5d} ({100.0 * link / tot:4.1f}%)")
        if auth:
            fights = sum(auth.values())
            print(f"   fight verdicts: {fights}; in danger by formula {auth[2] + auth[3]}; of those with unmodelled risk "
                  f"(blocked from clearing danger) {auth[3]}; unmodelled risk while not in danger {auth[1]}")
        for name, h in sorted(checks.items()):
            print(f"   self-check {name}: {h[0] + h[1]} compared, {h[1]} different")
        if cm_nt:
            print("   caster_micro option cost by threats in view (thousands): calls  median / p90 / p99 / max")
            for k in sorted(cm_nt):
                h = cm_nt[k]
                print(f"     threats={k}: {sum(h.values()):6d}  {quantile(h, .5)} / {quantile(h, .9)} / {quantile(h, .99)} / {max(h)}")
                e = cm_eval.get(k)
                if e:
                    print(f"        of which option evaluation: {quantile(e, .5)} / {quantile(e, .9)} / {quantile(e, .99)} / {max(e)}")
        if fleehit:
            n_fh = sum(fleehit.values())
            print(f"   flee-hits: {n_fh}; then moved {fleehit[1] + fleehit[3]} ({100.0 * (fleehit[1] + fleehit[3]) / n_fh:.0f}%), "
                  f"then cast {fleehit[2] + fleehit[3]}")
        for name, h in sorted(helper.items()):
            print(f"   {name} past its early exits: {sum(h.values())} calls, fuel (thousands) median "
                  f"{2 * quantile(h, .5)} / p90 {2 * quantile(h, .9)} / max {2 * max(h)}")
        if dump_cost:
            dump_cost.sort()
            print(f"   telemetry dump cost: median {dump_cost[len(dump_cost) // 2]}, max {dump_cost[-1]}")


def n_suspended(n):
    return f"{n} suspended turns"


if __name__ == "__main__":
    main()
