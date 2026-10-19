"""Fuel-exhaustion report for bench runs (Agent 2, E1).

usage: python -I agent2/tools/fuel_report.py TAG [TAG ...] [--cand NAME]

Counts, for the teams played by the bench candidate NAME (default a2_v34base; both teams in a mirror), the
`fuelExhausted` entries (round, champion, phase), bot faults, and from the replay's team-*.fuel file the play turns
that spent exactly their budget. Denominators: games, team-games and play turns (fuel-file turns after the draft).
Bench file names are BASE__MAP_SIDE, where SIDE is the candidate's team.
"""
import json
import struct
import sys
import zipfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fuel_turns(blob):
    vals = [struct.unpack_from("<Q", blob, off)[0] for off in range(13, len(blob) - 7, 8)]
    head, turns = vals[:8], vals[8:]
    # Draft turns come first; a team's first play turns (budget head[2]) normally spend more than a normal turn's
    # budget head[3], so the draft ends before the first value above head[1].
    k = 1
    while k < len(turns) and turns[k] <= head[1]:
        k += 1
    return head, turns[:k], turns[k:]


def main():
    args = sys.argv[1:]
    cand = args[args.index("--cand") + 1] if "--cand" in args else "a2_v34base"
    tags = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] != "--cand")]
    for tag in tags:
        d = ROOT / "bench_out" / tag
        games = team_games = entries = with_any = faults = 0
        play_turns = full_budget = full_first = 0
        rounds, champs, phases = Counter(), Counter(), Counter()
        per_game = []
        for st in sorted(d.glob("*.json")):
            base, rest = st.stem.split("__", 1)
            cand_team = "teamA" if rest.rsplit("_", 1)[1] == "A" else "teamB"
            teams = ["teamA", "teamB"] if base == cand else [cand_team]
            j = json.loads(st.read_text())
            games += 1
            g_entries = 0
            for e in j["events"]:
                if e["kind"] != "fuelExhausted" or e["owner"]["team"] not in teams:
                    continue
                for en in e["entries"]:
                    g_entries += 1
                    rounds[min(en["timestamp"]["round"] // 100 * 100, 1900)] += 1
                    champs[e["owner"]["name"]] += 1
                    phases[en["timestamp"].get("phase", "?")] += 1
            faults += sum(int(j.get("diagnostics", {}).get("botFaults", {}).get(t, 0)) for t in teams)
            entries += g_entries
            team_games += len(teams)
            with_any += g_entries > 0
            per_game.append((st.stem, g_entries))
            rp = st.with_suffix(".repl26a")
            if rp.exists():
                z = zipfile.ZipFile(rp)
                for t in teams:
                    head, _, turns = fuel_turns(z.read(f"team-{'a' if t == 'teamA' else 'b'}.fuel"))
                    play_turns += len(turns)
                    full_budget += sum(v == head[3] for v in turns)
                    full_first += sum(v == head[2] for v in turns)
        print(f"== {tag}: {games} games, {team_games} counted team-games, {play_turns} play turns")
        print(f"   fuelExhausted entries {entries}: {entries / max(1, team_games):.2f} per team-game, "
              f"{1000.0 * entries / max(1, play_turns):.3f} per 1000 play turns; games with any {with_any}/{games}")
        print(f"   fuel file: turns at exactly the normal budget {full_budget}, at the first-turn budget {full_first}; "
              f"bot faults {faults}")
        print("   by round:", ", ".join(f"r{k}+:{v}" for k, v in sorted(rounds.items())))
        print("   by champion:", ", ".join(f"{k} {v}" for k, v in champs.most_common()))
        print("   by phase:", dict(phases))
        worst = [x for x in sorted(per_game, key=lambda x: -x[1])[:5] if x[1]]
        print("   worst games:", ", ".join(f"{s} {n}" for s, n in worst))


if __name__ == "__main__":
    main()
