# Agent 2 findings: fuel, exhaustion and fight/run search coverage (v34 baseline)

All numbers are local, engine `cambc 2.2.5` (engineBuildHash `d7e29ae3…`), release builds, default competition fuel.
Baseline build `a2_v34base` = `bots/v34` source, 18/18 files hash-identical to `submissions/bot v34.zip`
(archive SHA-256 `E6A95834…FA605`); release wasm `ECAFAE4C…AE55` (byte-identical to the existing `bots/v34` build,
so builds here are deterministic; the other run reported `70139ee1…` for its baseline wasm, worth reconciling).
These are diagnostics, not strength evidence.

## 1. Engine semantics (probe `bots/a2_probe_exh`, `bench_out/a2_probe_exh`)

- Budgets from the engine's own fuel header: first draft turn 1,000,000; later draft turns 100,000; each champion's
  first play turn 200,000; normal turn 100,000.
- When a turn runs out, execution **suspends at the exact instruction and resumes next round** with a fresh budget
  (loop counter preserved; `fuel_remaining()` about 99k on resume).
- **One `fuelExhausted` entry per round boundary crossed.** 150k burn: entry at R40. 260k burn: entries R80, R81.
  A resumed v34 turn that ran out again added a cascade entry (R82).
- **Actions before the suspension land in the original round** (the R120 move applied immediately; `can_move` false).
- **After resuming, the champion has the new round's allowances and the stale code spends them**: the R120 code
  moved again in R121. So an exhausted turn costs the champion its fresh decision for the next round, and the old
  turn's remaining actions run on stale observations.

## 2. Baseline exhaustion (`agent2/tools/fuel_report.py`; E1)

| Schedule | Independent games | Entries per v34 team-game | Games with any | Per 1,000 play turns |
|---|---|---|---|---|
| v34 mirror, 17 official maps | 17 | 2.94 | 26/34 | 1.54 |
| v34 vs `sb_sparexp_b1`, official | 34 | 2.38 | 17/34 | 1.16 |
| v34 mirror, 50 `maps_rdev` | 50 | 3.12 | 88/100 | 1.72 |

Mirror runs repeat each game on both sides (identical results), so they count 17 / 50 independent games.
Bot faults: 0 everywhere. Round-1 exhaustion appears only on `lanes` (8 entries per official run, about 0.24 per game,
matching the plan's figure). Its cause is initialisation (about 73k on the 90x30, 42-group map: camp-group loading 36k)
plus the unguarded world update, not route search (farm costs <0.5k on that turn).

## 3. Where turns run out (`bots/a2_lab_tel`, `agent2/tools/tel_report.py`)

The telemetry build records the remaining fuel and round at ~40 checkpoints per turn and prints the trail only for
suspended turns; search counters are aggregated in memory and dumped sparsely. Its exhaustion rate matches v34 within
game-to-game variation (2.4–2.9 per team-game official, 3.6–4.2 unseen), so its attribution stands in for v34's.
(The first two lab builds, with per-call `printf` and dense dumps, inflated exhaustion to ~35 per team-game: printing
and even plain array scans are expensive under a ~1-fuel-per-instruction budget.)

Stage running when the budget ran out (share of suspended turns; median fuel left at the stage's start):

| Stage | Official mirror | vs sparexp | Unseen (rdev) |
|---|---|---|---|
| `counter_ambush()` | 41% (81k) | 35% (82k) | 61% (76k) |
| `ward_reveal()` | 22% (88k) | 25% (88k) | — |
| after the fight/run search: cmicro / stand | 20% (46k) | 19% (36k) | 1% |
| route search | 7% (46k) | 1% | 20% (54k) |

- `counter_ambush` past its early exits costs **median 72–80k, p90 90k, max 108–130k** per call (up to 729
  `bush_cluster` lookups at ~100 fuel each). Calls: ~1 per team-game official, ~6 per team-game unseen.
- `ward_reveal` is median 4k, but first-use labelling of a bush cluster costs ~800 fuel per tile:
  50–99 tiles ~50k, 100–149 tiles **112k**. Those spikes are its suspensions.
- The fight/run search itself rarely runs out; it drains the budget and the action after it (caster micro,
  `escape_or_stand`) runs out.

## 4. Fight/run search (`tactical_decide`) coverage and cost

Status by original scene size (1 + allies within 7 + enemies within 8), official mirror, 17,419 calls:

| Scene | Share of calls | Verdict | Main no-verdict reason |
|---|---|---|---|
| 2–4 | 63% | 45–54% | completed comparison inside the 2.0 margin |
| 5 | 17% | 18% | **incomplete**: a 5-unit pair needs 78,000 at entry; after world + helpers it rarely has it |
| 6+ | 17% | 0–3% | predicted budget (2,200 × n × (h+1) per playout > 70,000 for a pair) |

- **The two-enemy-model comparison never runs**: even n=2, h=3 needs 4 × 17,600 = 70,400 > 70,000. Every verdict
  uses one model.
- Real playout cost vs the 2,200 estimate (thousands; median / p90 / p99 / max): n=3,h=3: 14/22/27/34 [26.4];
  n=4,h=2: 15/22/29/37 [26.4]; n=5,h=2: 19/27/35/42 [33.0]. The estimate is ~1.7x the median but **below the tail**
  (max 127–136% of it), so it is neither tight nor a bound. Preparation (snapshot to first playout): 1–2k, max 7k.
- Local games reproduce the plan's 17–21% share of 6+ scenes (16.9% official mirror, 17.6% vs sparexp; unseen 0.8%).
- The search is not called in ~13% of turns with an enemy within search range because no target exists.
