# Agent 2 interfaces: runtime budget, verdict schema, snapshot omissions, telemetry

Candidate source: `bots/a2_v36/src` (v34 + switches; all switches 0 builds the byte-identical v34 wasm `ECAFAE4C…`).
Switch defaults live in one block of `common.h` (a central-registry request to Agent 1).

## 1. Runtime budget API (`runtime_budget.h`, namespace `rb`)

Engine facts it relies on (measured, `agent2/FINDINGS.md` §1): a turn that exceeds its budget suspends at the exact
instruction and resumes next round with a fresh budget; budgets are 100k per normal turn, 200k for a champion's first
play turn. One `fuelExhausted` entry is logged per round boundary crossed.

| Call | Meaning |
|---|---|
| `rb::left()` | `bc::fuel_remaining()` |
| `rb::fits(cost, reserve)` | `left >= cost + reserve`, computed without unsigned wrap |
| `rb::admit(stage, cost, reserve)` | `fits` plus per-stage `ran`/`skipped` counters |
| `rb::search_cap(reserve)` | deepest distance-field search the fuel left pays for, from the engine cost model `(levels + 16) x chunks + 3000`; 0 when not even a short search fits |
| `rb::kTailReserve` (6,000) | fuel the rest of a turn needs after a movement stage (measured under 1k, plus margin) |

Rules for callers: compare a stage's **worst-case** cost (not its average) plus the reserve for the rest of the turn;
a refused or capped stage takes its documented fallback and never publishes half-built state.

Current uses (switch `A2_GOV` = `A2_GOV_WALK` + `A2_GOV_WARD`):
- `walk_toward`: both path searches capped by `search_cap(kTailReserve)`. A search the cap stopped (expansion count
  reached the cap) takes `greedy_step` (one step that strictly reduces Chebyshev distance to the nearest goal); a goal
  that is truly unreachable keeps v34's behaviour. With room to spare the result equals the uncapped one (follow_field
  only reads tiles nearer than ours, all inside the cap).
- `travel_around_threats`: capped; a cut-off detour reads as unreachable and the shortest route is used (v34's own
  fallback).
- `ward_ahead`: skipped this turn when a cluster it would label is still unlabelled and a worst-case labelling (70k)
  plus `kTailReserve` does not fit.

Exact-result fuel rewrites (no decision changes; real-game self-check `A2_CHECK`: 0 mismatches in ~150k comparisons):
`A2_BUSH_FAST` (world.cpp bush labelling with a fixed stack), `A2_FAST_AMBUSH` (counter_ambush one 11x11 scan),
`A2_CMICRO_FAST` (caster_micro per-tile danger cache and per-call camp filter).

## 2. Fight/run verdict schema (`tactical_decide`, recorded in `a2ts::snap.status`)

| Status | Meaning | Verdict |
|---|---|---|
| 0 no-enemy | no live enemy within 8, or the nearest beyond `APEX_TS_RANGE` | none |
| 1 safety | fuel below `kDetSafety + 10000` at entry | none |
| 2 budget | the deterministic plan cannot fit a pair even at one model and horizon 2 | none |
| 3 incomplete | a playout could not start above the reserve; an incomplete favourable half is never a verdict | none |
| 4 margin | completed pair inside `APEX_TS_DECIDE_MARGIN` | none |
| 5 fight | completed, fight better by the margin | +1 |
| 6 run | completed, run better by the margin | -1 |

Only completed pairs under the same enemy model and horizon are compared (v34 behaviour, unchanged).

## 3. Snapshot omission contract (`a2ts::snap`, filled by `ts_snapshot`)

`scene` (1 + allies within 7 + live enemies within 8, before selection), `n` (units modelled), `dropped_enemies`,
`dropped_allies`, `omit_reach` (a dropped enemy within `max(range + 2, 5)` of us, the formula's threat radius),
`link_partial` (a modelled Cassi/Issac whose visible partner is not modelled; our own partner counts).

`A2_SNAP = K` (relevance-ranked selection, K units including us): nearest first; at equal distance enemies before
allies, then larger threat (basic + spell), then lower handle (deterministic). The nearest enemy is always in; a
Cassi/Issac pair enters whole or not at all, our own partner first; at most three allies. Dropped units become static
blockers in the model's map (they are not erased as obstacles). Measured: K=4 lifts verdicts from 39% to 47%
(scene 6: 1% -> 36%); K=5 does not fit real entry fuel (~3% verdicts at scene >= 5).

`A2_TD_AUTH` (verdict authority, `handle_enemies`): a fight verdict clears the formula's danger only when the model
represents the risk: no `omit_reach`, no `link_partial`, and (with `APEX_TS_CAMPS` off) no awake camp reaching our
tile. Otherwise the formula keeps its flags and `escape_or_stand` decides (it stands and fights when escape is
blocked). Run verdicts are always taken.

## 4. Telemetry (`A2_TEL`, diagnostic builds only; release builds compile it out)

- Per turn: up to 64 named checkpoints `(name, fuel left, round)`; printed only for a turn that crossed a round boundary,
  as `A2SUS R<r> t<type> to R<r2>: name=fuel ...` (`*` marks the first checkpoint in the new round).
- Aggregates (sparse counters, dumped at most every 100 rounds at turn end with >= 40k left, one buffered write):
  search status by scene; playout cost by (n, horizon); preparation cost; helper costs (counter_ambush, ward_reveal,
  caster_micro by threat count, escape_or_stand, walk_toward); bush labelling cost by cluster size; omission flags by
  status; authority decisions; flee-hit follow-through; `A2_CHECK` mismatch counts.
- Readers: `agent2/tools/fuel_report.py` (engine exhaustion entries per team-game from stats + replays),
  `agent2/tools/tel_report.py` (everything above), `agent2/tools/pool_halves.py` (pool by seed halves).
- Overhead: `printf` and dense array scans are expensive under the ~1-fuel-per-instruction budget; the first two lab
  builds inflated exhaustion ~12x. This design matches v34's exhaustion rate within game-to-game variation.
