# Agent 2 (Windows host): runtime governor, combat snapshot, verdict authority, flee-hit

Workstream issue: #5. Base: frozen v34 (`bots/v34` = archive `E6A958…FA605`; per-file hashes in
`patches/BASE_SHA256.txt`). Engine `cambc 2.2.5`, engineBuildHash `d7e29ae3…`; WASI SDK 33 / Clang 22.1.

## Recommended stack

Fuel package (`A2_BUSH_FAST`, `A2_FAST_AMBUSH`, `A2_CMICRO_FAST`, `A2_GOV`) + `A2_SNAP=4` + `A2_TD_AUTH` +
`NOVA_FLEE_HIT` (E13 in EXPERIMENTS.md). Against v34 on the calibrated pool (1,904 paired games, seeds 1–8):
**+1.7 ± 1.6 points (not significant); deaths −0.88 ± 0.19 per game**, consistent on seeds 1–4 and fresh seeds 5–8.
Fuel exhaustion 0.08–0.53 entries per team-game against v34's 2.4–3.1. Not yet tested: `maps_real` pool, final
holdout, ladder.

## Contents

| Path | What |
|---|---|
| `FINDINGS.md` | Engine fuel semantics, v34 exhaustion rates and causes, search coverage and real playout cost |
| `INTERFACES.md` | Runtime-budget API, verdict/status schema, snapshot omission contract, telemetry fields |
| `EXPERIMENTS.md` | Registered experiments E1–E13 with schedules, decisions and results |
| `builds.tsv` | Every variant built: defines, source SHA-256, release wasm SHA-256 |
| `candidate/src/` | Full candidate source (v34 + switches). **All new switches 0 builds the byte-identical v34 wasm `ECAFAE4C…`** |
| `patches/` | Owner patches against v34, verified to reproduce `candidate/src` exactly with `git apply` |
| `tools/` | `fuel_report.py`, `tel_report.py`, `pool_halves.py`, `prof_report.py`, `mkvar.py` |
| `probe/` | The deliberate-exhaustion probe used to establish engine suspend/resume semantics |

## Patches by owner

- `agent3_bot.patch` (Agent 3: `bot.cpp`, `bot.h`): `A2_FAST_AMBUSH`, `A2_GOV_WALK` (`walk_toward` cap +
  `greedy_step`), `A2_GOV_WARD`, `A2_TD_AUTH`, `NOVA_FLEE_HIT` port, telemetry checkpoints (compiled out unless `A2_TEL`).
- `agent1_world_common.patch` (Agent 1: `world.cpp`, `common.h`): `A2_BUSH_FAST`, the switch-default block
  (central registry request), telemetry namespace.
- `agent2_owned_tsearch.patch` + `runtime_budget.h` (Agent 2 files): `A2_SNAP`, `A2_CMICRO_FAST`, status recording.

Apply from a copy of `bots/v34`: `git apply agent3_bot.patch agent1_world_common.patch agent2_owned_tsearch.patch`
then copy `runtime_budget.h` into `src/`. Every hunk is inside a named switch.

## Switch status (evidence in EXPERIMENTS.md)

| Switch | Kind | Status |
|---|---|---|
| `A2_BUSH_FAST`, `A2_FAST_AMBUSH`, `A2_CMICRO_FAST` | exact-result fuel rewrites (0 mismatches in ~150k real-game checks) | pool neutral (−0.9 ± 1.1 pts); keep |
| `A2_GOV` (`_WALK`, `_WARD`) | fuel governor | with the rewrites: exhaustion −80–98%, pool neutral (−0.1 ± 1.2 pts); keep |
| `A2_SNAP=4` | relevance-ranked 4-unit snapshot | verdicts 39% → 47%; alone neutral on fresh seeds; keep with `A2_TD_AUTH` |
| `A2_SNAP=5` | 5-unit cap | rejected: does not fit real entry fuel (~3% verdicts) |
| `A2_TD_AUTH` | verdict authority for unmodelled risk | alone neutral; with SNAP=4 deaths −0.73 ± 0.19 per game |
| `NOVA_FLEE_HIT` | flee-hit (v36fight port) | +1.1 ± 1.1 pts, deaths −0.31 ± 0.17 |

## Reproduce

Bench and pool runs use this workspace's existing harness (`bench.py`, `tools/spar_pool.py`) and the seeded pool
builds, which are not in this repo. `tools/mkvar.py NAME DEF=VAL …` builds a variant from `candidate/src` and logs
its hashes.
