# Agent 4 experiment registry

Runner: `v36/agent4/tools/paired.py` (cambc 2.2.5, engine 0.0.3, `--release`, default fuel). Matches are
deterministic per (bots, map, side), so the independent unit is the map; both sides are played. Intervals are 95%
bootstrap over maps. Baseline: `bot_v34` (archive SHA-256 E6A95834…FA605, matches the master prompt).

Map sets (from the team's 19 Oct snapshot, `ws/`): `maps` = 17 official; tuning = `maps-gen` (80) + `maps-big100`
(30); validation = `maps-fresh16` (160), used once per finalist; final holdout = `maps-holdout` (40), left to the
final check. Whether these generated sets resemble the competition's random maps is not verified.

## E0 parity0 — harness parity
bot_v36 (unchanged copy) vs bot_v34, maps-gen first 10, 20 games: 50.0%, gem diff 0, identical drafts. Pass.

## E1 unk-draft — forced lineup on unknown maps (registered before running)
- Hypothesis: on unknown maps a different fixed lineup than v34's rule (lineup 2, or 6 when dangerous camps are
  packed) wins more full matches.
- Switches: `NOVA_FALLBACK_LINEUP=k, NOVA_CLUSTER_DRAFT=0` (official maps unaffected), k in {2, 6, 19, 20, 8, 13}.
  k=2 alone isolates the cluster rule.
- Primary metric: paired win rate vs bot_v34 on the 110 tuning maps (220 games); secondary gem diff, deaths.
- Decision rule: a k goes to validation only if its tuning win rate is >= 53% and its lower bound is > 48%;
  validation on maps-fresh16 must then show >= 52% with gem diff > 0. Otherwise keep v34's rule.
Result (110 tuning maps, 220 games each, vs bot_v34; interval = 95% map bootstrap):

| k | win | 95% CI | gem diff | deaths cand/base |
|---|---|---|---|---|
| 2 (cluster rule off) | 48.2% | 46.4–49.6 | −12 | 1902/1921 |
| 6 | 51.8% | 45.5–58.2 | +7 | 1853/1689 |
| 8 | 51.4% | 44.6–57.7 | −6 | 1857/1619 |
| 19 | 50.9% | 44.6–56.8 | +29 | 1705/1924 |
| 20 | 42.7% | 36.4–49.1 | −44 | 1770/1873 |
| 13 | 31.4% | 25.5–37.7 | −106 | 1528/1545 |

Decision: no k meets the screen rule; keep v34's unknown-map rule (lineup 2, lineup 6 when dangerous camps are
packed). The cluster rule fires on 10/110 maps and is worth ~140 gems/game there (k=2 lost 25% of those games).
Per-size splits (~35 maps each) were looked at but not acted on: picking the best k per size from this screen would
be selection on noise. Validation set untouched.

## E2 mapid-strict — official-map identity requires gem/gold totals (registered before running)
- Finding (probe `NOVA_MAPFACTS_PRINT`, all 264 maps): v34's descriptor (w, h, size, groups) matched an official map
  on 4/247 generated maps (gen026_009~expanse, gen616_096~fragment, gen777_003~packs, gen777_031~fragment). Total camp
  gems and gold differ on all four; with `NOVA_MAPID_STRICT=1` the probe recognises 17/17 official maps and 0/247
  generated ones.
- Hypothesis: strict identity changes only those maps (correct unseen-map draft and play settings) at no fuel cost.
- Test: `NOVA_MAPID_STRICT=1` vs bot_v34 on all 264 maps. Pass rule: identical results on every other map, no new
  faults; the 4 changed maps are reported individually (8 games is too few for a strength claim).
Result (327 map entries incl. duplicates by set, 654 games, 0 faults): mirror on every map except
- the 4 collision maps: strict (unknown-map play) won 2/8 games; gem diffs −114, −182, −868, +772, +726, −24, −930,
  −810. v34's mistaken official treatment did better here (all 3-pick maps; v34 drafted C&I, Stellar, Ren).
- ambush (official): one game changed. The play-time totals read costs fuel and perturbed a fuel-sensitive game.
Decision: keep `NOVA_MAPID_STRICT` as a switch, default 0 (v34 behaviour). Correctness fix without strength
evidence; 8 games is anecdotal and points the other way. Handed to Agent 1/3 as an identity contract option.

## E3 unk3-l19 — lineup 19 picks on unknown 3-pick maps (registered before running; exploratory)
- Motivation (post hoc, so it needs fresh data): E1 k=19 on 3-pick tuning maps 54.4% (+31 gems, n=68 games) and
  E2's collision maps (3-pick) favoured C&I, Stellar, Ren over C&I, Stellar, Varg.
- Switch: `NOVA_UNSEEN3=C&I,Stellar,Ren,...` (lineup 19 order), `NOVA_UNSEEN4/5` = lineup 2 order (unchanged);
  applies only to unknown, non-clustered maps of draft size 3.
- Test once on the validation set `maps-fresh16` (160 maps; ~54 are size 3). Pass: size-3 win rate >= 55% with
  lower 95% bound > 50% and gem diff > 0; other sizes must be mirror.
Result (maps-fresh16, 160 maps, 320 games): size-3 maps 52.7% (95% CI 42.7–61.8, 55 maps), gem diff +78, deaths
879 vs 1224; sizes 4/5 mirror as designed. Pooled with E1's size-3 tuning games: 53.4% (46.1–60.7, 89 maps), +60 gems.
Decision: fails the registered pass rule (>= 55%, lower bound > 50%). Retain v34. Promising but unresolved: deaths
fall by ~28% and gems rise consistently; resolving +3 points needs ~1,000 independent 3-pick maps. Candidate for a
larger generated suite or a ladder A/B, not for release on this evidence.

## E4 unk3-l19-confirm — E3 on 1,000 fresh 3-pick maps (registered before generating the maps)
- Maps: `tools/mapgen/gen.py` (team generator, engine 0.0.3 builder), `GEN_TEAM=3`, seed 3601, 1,000 maps
  (`maps-s3`), never used for any selection. Same generator family as the tuning/validation sets.
- Candidate/baseline as E3. Pass: win rate >= 52% with lower 95% bound > 50% and gem diff > 0; then it becomes the
  v36 draft candidate for unknown 3-pick maps (switch default stays off until Agent 1's combined-stack check).
Result (1,000 maps, 2,000 games, 0 faults): **53.4% (95% CI 51.2–55.5), gem diff +48, deaths 12,838 vs 16,765
(−23%)**. On the 877 maps where the draft changed: 53.8%, +55 gems. 106 maps were clustered (lineup 6, unchanged)
and 17 maps (1.7%) were again misread as official 3-pick maps (unchanged). **Pass.** Candidate for v36: lineup 19 order
on unknown, non-clustered 3-pick maps. Default left off in source until the combined-stack check (no Agent 1 is
running; the user decides).

## E5 model-cands — draft-model suggestions per size (registered before running)
- Source: `tools/model.py` on 29,785 ladder matches (19 Oct 15:07–21:46 UTC, engine 0.0.3, 135 teams with >= 20
  side-games, team effects + per-size champion effects, L2). Observational, official maps only, other teams' bots:
  it nominates; full matches decide.
- Candidates (others sizes keep lineup 2 order): size 3: Stellar, Ryth, Ren (+0.82 model); size 4: C&I, Ren, Ryth,
  Varg (+0.45); size 4: lineup 6 order (+0.12); size 5: C&I, Ren, Stellar, Tavi, Willow (+0.70); size 5: C&I, Ren,
  Ryth, Stellar, Tavi (+0.63); size 5: lineup 19 order (+0.44).
- Screen: fresh generated suites per size, 300 maps each (seeds 3612/3613/3614 for sizes 3/4/5), vs bot_v34
  (size-3 candidate also vs the E4 candidate). Pass to confirmation: >= 53% with lower bound > 49%. Confirmation on
  1,000 fresh maps (new seeds), same rule as E4.
Result (300 maps / 600 games each, vs bot_v34):

| size | candidate | win | 95% CI | gems | deaths cand/base | verdict |
|---|---|---|---|---|---|---|
| 3 | Stellar, Ryth, Ren | 47.0% | 42.5–51.0 | +13 | 2689/5990 | fail |
| 4 | C&I, Ren, Ryth, Varg | — | — | — | — | **invalid run**: script put the list in the size-3 slot (exact mirror); rerun as E5b |
| 4 | lineup 6 order | 55.0% | 51.5–58.7 | +26 | 6985/6656 | pass → confirm |
| 5 | C&I, Ren, Stellar, Tavi, Willow | 36.7% | 33.3–40.2 | −85 | 9514/9112 | fail |
| 5 | C&I, Ren, Ryth, Stellar, Tavi | 52.7% | 49.0–56.3 | +34 | 8633/9896 | fail (bar 53%) |
| 5 | lineup 19 order | 53.5% | 50.0–57.0 | +31 | 9094/8975 | pass → confirm |

Model agreement: the model's top size-5 pick (Willow) was the worst in our bot; its size-3 top pick also failed.
Our bot's play is tuned to its own lineups, so other teams' observational lineup value transfers poorly.

## E5b / E6 (registered before running)
- E5b: size 4 C&I, Ren, Ryth, Varg on maps-t4 (the corrected E5 run), same pass rule.
- E6 confirmations, 1,000 fresh maps each, rule as E4 (>= 52%, lower bound > 50%, gems > 0):
  E6a size 4 lineup 6 order (seed 3641, `maps-c4`); E6b size 5 lineup 19 order (seed 3651, `maps-c5`); plus any E5b pass.
E5b result: size 4 C&I, Ren, Ryth, Varg 59.0% (95% CI 55.3–62.7), gems +104, deaths 5608/7370. Pass → confirm (E6c),
on the same `maps-c4` suite as E6a so the two size-4 candidates are compared on identical maps.
E6 results (1,000 fresh maps / 2,000 games each, vs bot_v34, 0 faults expected; checked in E7):

| run | size | lineup | win | 95% CI | gems | deaths cand/base | verdict |
|---|---|---|---|---|---|---|---|
| E6a | 4 | lineup 6 order | 51.0% | 48.9–53.1 | +7 | 24418/22712 | fail (E5's 55% was selection luck) |
| E6b | 5 | lineup 19 order | 53.0% | 51.1–55.1 | +19 | 25986/25940 | pass |
| E6c | 4 | C&I, Ren, Ryth, Varg | 62.4% | 60.4–64.3 | +116 | 19239/25478 | pass |

Final draft model (98,110 matches, 19,622 groups, 217 teams, 24 h to 20 Oct; CV log-loss 0.402 vs 0.424 team-only,
later-time holdout 0.405 vs 0.423) ranks C&I+Ren+Ryth+Varg first at size 4 (+0.62 vs lineup 2, CI 0.47–0.71) and
lineup 19 above lineup 2 at sizes 3 (+0.25) and 5 (+0.39): consistent with E4/E6. It also ranked Willow/Tavi
5-pick lineups high, which lost in our bot (E5): the model nominates, matches decide.

## E7 combined — `NOVA_V36_DRAFT=1` (sizes 3/4/5 together) on untouched maps (registered before running)
- Switch in `bot_v36/src/draft.cpp` (default 1 in the candidate). Maps: `maps-holdout` (40, never used for
  selection) + `maps-final` (600 fresh mixed-size maps, seed 3701). Also all 17 official maps (must be mirror) and
  parity of `NOVA_V36_DRAFT=0` vs v34 on maps-gen (must be mirror).
- Pass: holdout+final win >= 53% with lower bound > 50%, gems > 0, 0 faults; official mirror; parity mirror.
E7 result: **pass.** maps-holdout + maps-final (640 maps, 1,280 games): **55.3% (95% CI 52.7–57.8), gems +51,
deaths 12,839 vs 14,894, 0 faults**. By size: 3 picks 52.8% (+35), 4 picks 61.2% (+113), 5 picks 52.1% (+8).
Official maps (17, 34 games): exact mirror, 0 faults. `NOVA_V36_DRAFT=0` vs v34 on maps-gen (80 maps): exact mirror.

## E8 engine 1.0.1 recheck (after patch 1.0.0/1.0.1; cambc 3.0.2, workspace `bcws3`)
Patch 1.0.0 nerfed C&I, Stellar, Varg, Ren, Ryth, cut camp gems (wolf 22→4, elk 36→15, gem cluster 24→7), added
Heart of Ruin and Golem, raised kill gold to 100, ward cooldown to 10. All earlier results are engine 0.0.3.
Same bots (bot_v36 vs bot_v34, both compile unchanged on SDK 3.0.2), same maps as E7:
- maps-holdout + maps-final (640 maps, 1,280 games): **68.5% (95% CI 66.2–70.9), gems +154, deaths 28,405 vs
  40,545, 0 faults**. 3 picks 65.7%, 4 picks 78.8%, 5 picks 61.2%. Games last ~1,590 rounds (was ~690).
- Official pool, now 35 maps (70 games): 50.0% overall; the 17 maps where the draft differs (v34 no longer
  recognises 15 new maps plus edited expanse and monolith): 50.0% (32–68), gems +46. Inconclusive (17 maps).
Caveats: generated maps come from the 0.0.3 builder (no Golems); opponent is still v34 only; neither bot drafts
Heart of Ruin, which dominated the top ladder before its 1.0.1 nerf.

## v37 tuning on engine 1.0.1 (patch 1.0.0/1.0.1). Settings only: no code changes (user instruction).
Workspace `bcws3` (cambc 3.0.2). Base: bot_v36. Candidate: bot_v37 (= bot_v36 source) with changed `#define`s,
substituted in place by `tools/paired.py`. Maps from the team generator with the engine 1.0.1 builder
(`v1_0_1` wasm, generator unchanged): `p-tune` 300 maps (seed 4001), `p-confirm` 1,000 (seed 4002),
`p-final` 400 (seed 4003); plus the 35-map official pool (`maps`).

### E9 economy screen (registered before running)
One setting at a time vs bot_v36 on `p-tune` (600 games): `APEX_TRIBUTE_CARRY` 48 / 96 (now 72); `APEX_END_BANK`
0 / 400 / 800 (now 600); `NOVA_GOLD_UNSEEN` 0.05 / 0.20 / 0.35 (now 0.10; it overrides `APEX_GOLD_WEIGHT` on every
map v36 does not recognise, which is most maps now); `APEX_AGGRESSION` 1.0 / 1.5 (now 1.2).
Pass to confirmation: >= 53% with lower bound > 49%. Confirmation on `p-confirm`: >= 52%, lower bound > 50%,
gems > 0. Winners are then combined and checked on `p-final` + official.
Change (user instruction, before any E9 result was read): every test is 200 games (first 100 maps of the suite,
both sides). At this size the 95% interval is about ±7 points, so the screen catches only large effects; pass rule
for the screen becomes >= 55%, and confirmation runs on 100 different maps (`p-confirm`) with >= 53% and gems > 0.

### E10 post-patch draft (registered before running)
Model on 6,375 post-patch matches (engine 1.0.1, 20 Oct 20:16–21:22 UTC; one hour, so noisy). Modal top lineups,
all with Heart of Ruin: 3 HoR+Ren+Stellar (826 side-uses, +1.18 vs v34 lineup 2), 4 HoR+Ren+Ryth+Stellar (878,
+1.45), 5 HoR+Ren+Ryth+Stellar+Varg (1,687, +1.19). Candidate D1 = these as `NOVA_UNSEEN3/4/5` (settings only; the
bot uses HoR's dash only for movement, so its value may be lower in our bot). 200 games on `p-tune` vs bot_v36;
pass >= 55%.
E10 results: screen (p-tune, 200 games) 57.5% (51.0–64.0), gems +46, deaths 3532/5600 → pass. Confirmation
(p-confirm maps 1–100) 51.0% (44.0–57.5), gems +22, deaths 4473/5981 → below 53%. Extension registered before
running: p-confirm maps 101–200 (200 games); decide on the pooled 200 confirmation maps: >= 53% and gems > 0.
E10 extension: p-confirm maps 101–200: 45.0% (38.5–51.5), gems −0. Pooled 200 confirmation maps: **48.0%
(43.3–52.5), gems +11, deaths 7,921 vs 10,929. Fail; keep v36's draft.** The HoR meta lineup dies less in our bot
but does not win more (our bot only uses HoR's dash to move; changing that is code, out of scope).
E9 so far (p-tune, 200 games each, vs bot_v36): carry 48: 55.0% (51.5–58.5) +22 → confirm; carry 96: 48.5% −10;
end bank 0: 47.0% −20; end bank 400: 53.5% (49.5–57.5) +11 (below the 55% screen bar); end bank 800: 47.0% −11.
Carry-48 confirmation (p-confirm 1–100): 52.0% (48.5–55.0), +8 → fail (bar 53%). Keep 72.
E9 rest (p-tune, 200 games each): gold weight on unknown maps 0.05: 53.0% (46.5–59.5) +5; 0.20: 48.0% −7;
0.35: 44.0% −29. Aggression 1.0 and 1.5: exact mirror (50.0%, identical deaths): `APEX_AGGRESSION` has no effect in
this build because the brain (`APEX_BRAIN=1`) resets aggression every turn; tune the brain's values instead.

**v37 decision:** no setting or pick list beat v36 on engine 1.0.1, so the v37 base bot = bot_v36 unchanged
(it beats v34 68.5% on engine 1.0.1, E8). Bundling it with cambc 3.0.2 gives a zip byte-identical to `v36/agent4/release/bot_v36.zip` (SHA-256 5ee75d52…b320), so that zip is the base bot for both CLIs.
