# Recovery and validation — 20 October 2026

User scope: continue full-match/fuel/integration/economy/safety work; balance updates remain excluded. The warning about usage prompted immediate preservation/publication of this checkpoint, not a claim that all work is finished.

## Exact builds and parity

CAMBC/SDK 2.2.5; WASI SDK 33, Clang 22.1.0; release `-O3 -DNDEBUG`, wasm exceptions/SIMD/bulk-memory flags as recorded in builds-all.json. Native match engineBuildHash is `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`.

- Frozen baseline: `ecafae4c4571d3a5401d356d03f5c336e6c9d7c5d6dd494af463646622eaae55`, exactly matching Agent 2's reported Windows artifact.
- Disabled v3: same exact WASM hash, byte-identical baseline. Uses frozen scoring token stream in a compile-time legacy branch.
- Bound ON v3: `ec316b7a4f8ca4733b0d872e8da3557e04dc10e00e8c87cfe3621fe85214bec5`.
- Diagnostics v3: `ee8c580ac99d23e0d88442bccd271187a0997e75f288abf625badbe2269e15af`.

The original disabled implementation compiled differently and changed one heavy-map result. That failure is retained in recovered-development-01 evidence; it was not waived. The second fixed schedule achieved exact compact native-statistics parity in all 18 disabled-vs-baseline side checks. The original baseline source/archive remains unchanged. Agent 1's different `70139ee1…` artifact is still not supplied for direct comparison; the cross-host discrepancy is only partially reconciled.

All 18 scorer-fixture builds passed 24,014 cases each again after the parity repair, including exact exhaustive/pruned choice streams and zero-weight identity.

## Completed full matches

63 accepted v3 development games on nine reconstructed historical layouts: 9 baseline self-games, 18 disabled parity games, 18 bound ON/OFF games, and 18 diagnostic comparisons. Independent layout groups = 9. Repeated deterministic baselines and diagnostic comparisons are not additional independent maps. The native engine ran full matches to completion with default fuel; no round truncation or fuel override.

The earlier 63-game revision and an initial pipeline smoke game are retained for audit; they are not pooled as independent strength evidence. One original CLI preflight failed to persist a temporary file while using two workers. That failed attempt/log was preserved, then the missing match was completed serially. The accepted v3 schedule uses one worker. All accepted v3 matches have zero native bot faults.

Maps came from 52 deduplicated local historical replays. MapBuilder encode/decode checks preserved canonical camp records. Their current official names/identity are not asserted; maps are developmental and not unseen holdouts. Map-byte hashes and source replay provenance are in recovered_maps.json. The old analytics WASM was used to reconstruct map files only; native runner mechanics determine outcomes. The current official pool download requires platform authentication (HTTP303), and the historical calibrated opponents are on another host.

Bound ON versus OFF: **9 wins / 18 games**, mean submitted-gem margin **-11.67**, descriptive 95% bootstrap interval across nine map groups **[-169.33, 98.89]**. This is inconclusive and does not establish improvement. No statistical significance or generalization claim. Keep the bound OFF pending governor integration and broader evidence.

## Real release fuel

The replay's `team-*.fuel` files use the pinned visualiser's documented-in-code v0.0.1 format: 77-byte metadata header and little-endian u64 consumed-fuel records. Parser asserts magic/version/language/budgets, roster-derived record counts, and saturated first/normal-turn counts against native fuelExhausted entries separately for every team/game. All 126 team records in the accepted schedule cross-check. The doubled first play turn is handled separately.

ON had **36** native exhaustion events across 18 team-games; OFF had **30**. These are actual round-boundary events, not inferred empty records or bot faults. Do not add a separate inferred suspension count to these as extra incidents.

| ON/OFF cohort | Normal recorded champion turns | Mean fuel | p95 | p99 |
|---|---:|---:|---:|---:|
| OFF | 52,020 | 36,665.80 | 75,452.10 | 83,933.24 |
| ON | 52,022 | 37,351.71 | 78,874.85 | 87,142.90 |

These are descriptive distributions of all recorded post-first champion turns, including dead/shopping turns. Policies can enter different states, so their difference is not a pure same-state instruction-cost estimate. Instrumented diagnostic results are kept separate; logging/counters are charged and can change behavior. There were zero reported final-score bound violations and 723 cumulative diagnostic checkpoint lines, not 723 independent matches.

## Native camp and recall checks

Two controlled native-engine release fixtures (Ren, Varg, versus explicitly idle test actors) passed 18 assertions. Their artificial round-limit outcomes are mechanics fixtures, never competitive wins. Exact source/build/map hashes, observations and replay bundles are retained.

Verified: initial dormant state; attack wakes the group; healthy unhit group member can be awake; Varg splash wakes a separate diagonal group; individual group members can later sleep at different times; no further damage does not sleep an in-range camp; an idle spawn ally can keep a camp awake; safe recall outside every actual threat range remains uninterrupted; recall from round20 resolves at round25; arrival-turn healing becomes visible at round26. One initial assertion incorrectly assumed departure of the active champion meant every camp should sleep; the native trace showed the idle Tavi still in range. The assertion and explanation were corrected, retaining actual observations.

This contradicts using damaged health alone as exact awake state, and qualifies the saved narrative's "no damage for two turns" description. Use the actual SDK's visible camp.state() with explicit unknown visibility handling when integrating the world contract. No speculative camp-safety threshold was enabled.

## Still incomplete

- Agent 1 execution tree/remaining screen is still missing. Agent 2 and Agent 4 snapshots appeared during final publication; exact commits and remaining external harness dependencies are recorded in PUBLICATION.md. They are fetched but not integrated or independently validated here.
- Complete current official suite, 200-map unseen screen, calibrated varied opponents/fresh seeds, untouched final holdout and final stack interactions.
- Opening/shopping policy tuning, actual helper/tank departure policy, linked vulnerability, dense threat capacity, boxed respawns and all emergency/banking/Tavi shopping regression scenarios. Basic source semantics and limited mechanics checks are not complete policy validation.
- Detailed per-stage fuel governor/reserve calibration and integrated omission/action review.
- Final legal competition candidate and performance-supported promotion. The safe recommendation remains the frozen behavior with all new switches OFF.

No platform submission, challenge or balance-update tuning was performed.
