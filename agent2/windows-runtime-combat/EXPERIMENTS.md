# Agent 2 experiment registry (local; mirrors Infosuii/battlecode-agent-36- #5)

Engine: `cambc 2.2.5`, engineBuildHash `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`.
Toolchain: WASI SDK 33, Clang 22.1.0 (bundled with cambc).

## Builds

| Name | Source | Switches | Release wasm SHA-256 |
|---|---|---|---|
| `a2_v34base` | `bots/v34/src` copied verbatim (18/18 files hash-identical; archive `E6A958…FA605`) | as frozen | `ECAFAE4C4571D3A5401D356D03F5C336E6C9D7C5D6DD494AF463646622EAAE55` (identical to `bots/v34` build) |

## E1 — Baseline fuel exhaustion (registered 2026-10-19, before running)

- Hypothesis: v34 loses champion-turns to fuel exhaustion; reproduce the historical 2.5/game (local official) figure on
  the exact frozen build and characterise where.
- Schedule (fixed): `a2_v34base` vs itself on the 17 official maps, both sides (34 games); vs `sb_sparexp_b1` on the
  official maps, both sides (34 games); vs itself on the first 50 `maps_rdev` maps by name, both sides (100 games).
- Metrics: `fuelExhausted` entries per game and per 1,000 champion-turns (each team counted separately), games with at
  least one, rounds of the events, turns at exactly the budget in `team-*.fuel`, bot faults.
- No strength claim. Mirror games count both teams as v34 observations.
- Result: see FINDINGS.md section 2 (2.94 / 2.38 / 3.12 entries per v34 team-game).

## E2–E5 — Stage attribution and fixes (diagnostic builds; results in FINDINGS.md sections 3–5)

## E6 — Fuel package exhaustion (release builds `a2c_fix`, `a2c_gov`; same schedule as E1)

Entries per team-game, official mirror / vs sparexp / rdev mirror: v34 2.94 / 2.38 / 3.12; `a2c_fix` 0.29 / 0.47 / 0.55;
`a2c_gov` 0.41 / 0.41 / 0.06. `a2c_off` (all switches off) wasm = v34 wasm (`ecafae4c…`), byte-identical.

## E7 — Quick screen of the fuel package (registered before running)

- Candidate `a2c_gov` (A2_BUSH_FAST, A2_FAST_AMBUSH, A2_CMICRO_FAST, A2_GOV) vs `a2_v34base`.
- Schedule: all 200 `maps_rdev` maps, both sides (400 games); the 17 official maps, both sides (34 games).
- Primary: win rate and gem difference vs v34. Expected effect small (exhaustion is ~0.04% of champion-turns);
  the package is mainly an enabler for search coverage. Decision rule: keep the package if the screen is not clearly
  worse (win rate >= 47% on 400 unseen games) and the calibrated pool (E8) is not worse; it is not evidence of a gain.
- Result (vs v34; official 34 / unseen 400): fixes 19/34, 52.8% (+10,088 gems); fixes + strict ward gate (first
  `a2c_gov`) 12/34, 50.8%; after splitting: + walk cap 16/34, 53.0%; + lenient ward gate 19/34 (identical games to
  fixes), 53.0%; + both 16/34, 53.0% (+10,988). The strict ward gate (90k required) was replaced by a lenient one
  (worst-case labelling 70k + 6k tail) on mechanism grounds; prompted by this screen, so E8 decides.

## E8 — Calibrated pool for the fuel package (registered before running)

- Seeded builds (NOVA_SEED=k, k=1..8): `sb_a2base_c*` (all A2 switches off = v34 behaviour), `sb_a2fix_c*`
  (A2_BUSH_FAST, A2_FAST_AMBUSH, A2_CMICRO_FAST), `sb_a2gov_c*` (fixes + A2_GOV). Hashes in builds.tsv.
- Pool: farm, calm, v34c, sparexp, aggro, v30base, spartop (existing seeded builds), 17 official maps, both sides,
  seeds 1–8 (1,904 games per candidate). `tools/spar_pool.py`, scored with `tools/spar_compare.py a2base a2fix a2gov`.
- Seeds 1–4 and 5–8 reported separately (5–8 as the fresh check). Primary: weighted-pool paired change; secondary:
  deaths per game overall and vs sparexp; exhaustion entries. Keep rule: not worse than a2base on both halves.

## E9/E10 — Snapshot size, verdict authority, caster-micro tail, exact-result self-check (diagnostic builds)

- 4-unit relevance snapshot: verdict rate 39.3% -> 46.8% (official mirror); scene 6: 1% -> 36%, scene 7: 0 -> 21%.
  5-unit cap: verdicts at scene >= 5 collapse to ~3% (a 5-unit pair needs 78k at entry); rejected on measurement.
- The 4-unit snapshot raised post-search suspensions; cause: caster_micro scanned every camp per hot-zone tile
  (p99 ~33k even with one threat). A per-call camp filter (same camps, same order) cut it to p99 12–13k; suspensions
  with SNAP=4 back to 19 (mirror) / 25 (vs sparexp).
- A2_CHECK self-check (both paths computed in real games): 0 mismatches in ~132k caster-micro danger values,
  ~2.3k counter-ambush distances, ~15k bush-cluster labellings.

## E11 — Screens of the behaviour-changing switches (registered before running)

- All with the fuel package on (A2_BUSH_FAST, A2_FAST_AMBUSH, A2_CMICRO_FAST, A2_GOV), current source:
  `a2c_a2gov2` (package only), `a2c_a2s4` (+A2_SNAP=4), `a2c_a2s4a` (+A2_SNAP=4 +A2_TD_AUTH), `a2c_a2auth`
  (+A2_TD_AUTH), `a2c_a2fh` (+NOVA_FLEE_HIT). Each vs `a2_v34base`: 17 official maps x 2 sides, 200 maps_rdev x 2 sides.
- Screens only rank candidates for the pool (E12); no strength claim from them.

## E12 — Calibrated pool for snapshot, authority and flee-hit (registered before running)

- Seeded builds `sb_<name>_c1..8` (NOVA_SEED=k) of `a2gov2` (fuel package, current source; the ablation base),
  `a2s4`, `a2s4a`, `a2auth`, `a2fh` (each = a2gov2 + one change; a2s4a = + SNAP=4 + TD_AUTH).
- Stage 1: seeds 1–4 for all five (952 games each), scored against `a2gov2` with `pool_halves.py`, and against
  `a2base` for context. Stage 2: fresh seeds 5–8 only for candidates that are not worse than a2gov2 on seeds 1–4 by
  more than 1 SE; the decision uses seeds 5–8 (picks on 1–4 have shrunk 2–4 points on fresh seeds historically).
- Secondary: deaths per game overall and vs sparexp; for SNAP/AUTH the death change is the mechanism check.
