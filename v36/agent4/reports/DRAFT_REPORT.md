# Agent 4 report: draft policy, map identity, data (v36)

Scope: Agent 4 of the four-agent plan, run alone (Agents 1–3 were not started). Everything below was measured in
this run unless marked historical. Full registry with pre-registered rules: `../EXPERIMENTS.md`.

## Result
`bot_v36` = v34 + one behaviour change, **`NOVA_V36_DRAFT=1`**: new picks for unknown, spread-out maps by draft size.
Official maps and clustered unknown maps draft exactly as v34.

| draft size | v34 picks (unknown map) | v36 picks | confirmation (1,000 fresh maps) |
|---|---|---|---|
| 3 | C&I, Stellar, Varg | C&I, Stellar, Ren | 53.4% (51.2–55.5), +48 gems |
| 4 | C&I, Stellar, Varg, Tavi | C&I, Ren, Ryth, Varg | 62.4% (60.4–64.3), +116 gems |
| 5 | C&I, Stellar, Varg, Tavi, Ren | C&I, Stellar, Ren, Ryth, Varg | 53.0% (51.1–55.1), +19 gems |

**Final check on untouched maps (E7):** 640 maps / 1,280 games vs v34: **55.3% (52.7–57.8), +51 gems, 14% fewer
deaths, 0 faults.** Official maps: exact mirror (34 games). Switch off: exact mirror (160 games).

**After patch 1.0.0/1.0.1 (engine 1.0.1, E8):** same 640 maps: **68.5% (66.2–70.9), +154 gems, 30% fewer deaths,
0 faults**; on the 17 official maps (of 35) where the draft differs: 50% (32–68), inconclusive. The new patch changes
champions, camps and maps; the draft model's ladder data is pre-patch and should be refreshed; Heart of Ruin is
not yet a candidate pick.

Release artifact: `v36/agent4/release/bot_v36.zip` (SHA-256 `5ee75d52…b320`, equal to the tested `bot_v36/src`), manifests
beside it. Rollback: submit `bot v34.zip`, or build with `#define NOVA_V36_DRAFT 0` (proven mirror of v34).

## Post-patch settings tuning (engine 1.0.1, settings only)
200 games per test vs v36 (see EXPERIMENTS E9/E10). Heart of Ruin meta draft: 48.0% on 400 confirmation games
(deaths −28%). Carry 48: 52.0% on confirmation; end bank 0/400/800: 47/53.5/47%; gold weight 0.05/0.20/0.35:
53/48/44%; aggression 1.0/1.5: no effect (the brain resets it each turn). **No change beat v36; `bot_v36.zip` is the
base bot for the ML approach** (byte-identical when bundled with cambc 3.0.2). At 200 games only effects of about
7 points are detectable; carry 48, end bank 400 and gold 0.05 are near misses worth larger tests.

## What the evidence does and does not show
- Opponent is **v34 only** (head-to-head). The plan warns head-to-head gains can mislead. The calibrated pool
  (`farm`, `calm`, `sparexp`, `aggro`, `v30base`, `spartop`) was **not** recovered: the snapshot zip has no bot
  sources, so those opponents could not be rebuilt. The change was not tested against other strong bots.
- Maps come from the team's generator (`tools/mapgen/gen.py`, engine 0.0.3 builder). If the competition's random
  maps differ from this generator, the gains may not transfer. The 4-pick gain is large and was consistent across
  every suite (59.0% screen, 62.4% confirm, 61.2% final); the 3- and 5-pick gains are small (+2–3 points).
- The ladder model independently agrees with all three changes (below), on other teams' bots and official maps.
- No platform A/B. Public match data has no submission IDs, so ladder results cannot be attributed to our
  versions without the team's login.

## Decisions per switch
| switch | default in bot_v36 | decision | evidence |
|---|---|---|---|
| `NOVA_V36_DRAFT` | 1 | ship | E4, E6b, E6c, E7 |
| `NOVA_MAPID_STRICT` | 0 | keep off | fixes 4/247 descriptor collisions, but those games went 2–6 and the play-time read changed one official game (E2) |
| draft test overrides moved first | n/a (test hooks) | keep | v34's `APEX_DRAFT_LIST/T3/T4/T5` could never take effect; release builds define none (parity exact) |
| `NOVA_MAPFACTS_PRINT` | undefined | probe builds only | prints draft-time map facts |
| Forced single lineup on unknown maps (E1: 6, 8, 19, 20, 13; cluster rule off) | — | reject | none >= 53%; 20 and 13 clearly worse; cluster rule is worth ~140 gems where it fires |
| Model-favoured Willow/Tavi 5-picks, Stellar-Ryth-Ren 3-pick, lineup-6 order 4-pick | — | reject | E5/E6a |

## Draft model (`tools/model.py`, `draft_model.json`)
98,110 ladder matches (19,622 groups, 24 h to 20 Oct, engine 0.0.3), 217 teams with >= 20 side-games. Logistic
paired comparison, L2: team effects + champion effects per draft size + shrunk champion x map terms. Validation:
grouped 5-fold CV log-loss 0.402 vs 0.424 for team strength alone; later-time holdout 0.405 vs 0.423, so picks carry
signal beyond who is playing. Lineup log-odds vs lineup 2 (95% bootstrap over groups): size 4 C&I+Ren+Ryth+Varg
+0.62 (0.47–0.71, 3,286 side-uses); size 3 lineup 19 +0.25 (0.19–0.32); size 5 lineup 19 +0.39 (0.33–0.46).
Limits: additive champions (no synergy terms), ratings are the window's own team effects (no contemporaneous
rating), observational. Its top size-5 pick (Willow) lost 37–63 in our bot: model rankings do not transfer reliably,
which is why every change went through full matches.

## Map identity
`nova_map_id` (w, h, size, groups) recognises all 17 official maps but also matched 4/247 (1.6%) generated maps, and
17/1,000 3-pick maps in E4. Orientation-invariant gem/gold totals separate all of them (`NOVA_MAPID_STRICT`).
`brain::map_id` (dimensions + altars) is a second classifier in `strategy.h` (Agent 3's file), untouched. Proposed
contract for Agents 1/3: one `map_identity()` in draft.cpp returning official index or -1, strict by totals, cached.

## Not done (Agent 4 scope)
- Task 4 replay intelligence (encounter sizes, farming timelines) for Agents 2/3: not started; no consumers ran.
- Task 5 farming ML: not started; decision is "insufficient evidence; leave off".
- Draft fixtures beyond full-match parity/probe checks (unit fixtures for SDK errors, transformed layouts).
- Counter-picking: not tested (historical attempts failed).
- GitHub issue structure from the master prompt: not created; this branch/PR is the record.
