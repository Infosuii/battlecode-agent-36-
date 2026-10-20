# v36 validation â€” release selection pending

Local release preparation is complete. Fresh selection rejected both shortlisted improvements, so all new policy switches remain OFF. The exact source-default package passed final parity and holdout checks and matches baseline results on the measured suite. This is a validated fallback, not a demonstrated upgrade. No live bot was submitted; GitHub code/results publication was explicitly authorized on20 October2026.

## Identities and reproducibility

The supplied `bot v34.zip` SHA-256 is `e6a95834df2cd14b0719d107309000d4f38724b436e1f617b7db5e85fddfa605`. All18 extracted baseline files remain frozen in `bot_v34/src`; source-tree identity is `406a2a51c2cc209f6111dc323d2f0679c7e76122e2296cdb20d656b7e97e2902`. The release-built baseline WASM is `70139ee190dc1e8a46338c78b54a1ee4b36d2459cb7944d6f2741d7d1432a757`.

Recovered SDK/CLI2.2.5 and WASI SDK33/clang22.1 produce real release Wasm matches at default native competition settings. Native engine build identity is `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`. The independently loaded visualizer engine binary has its own file hash; those two different identifiers must not be equated. Native turn fuel is100,000, with200,000 at initial invocation. Full-length games use the engine's actual2000-round/1000-gem rules, not a shortened simulated objective.

Every build has frozen source, complete macro dump, compiler command, SDK/CLI hashes and artifact hash. Every schedule pins both artifacts, map hash, side, engine and full-match settings. Results are resumable only after statistics checksum and replay-presence checks. Artifact-keyed deterministic duplicates are reused and are not new observations. Geometry seeds are map-generation inputs; this CLI exposes no match-seed control.

## Independent correctness and fuel evidence

- All-new-switches-OFF replay parity: the earlier34-game suite matched frozen baseline raw replay payloads exactly. The corrected cache freeze and exact final release also each pass34/34 raw replay checks.
- Actual official-engine scenarios verify one successful cast per turn despite attack/cooldown resets; Varg splash and reset attacks; ranged movement excluding basic attack; Ren lethal retaliation; linked C&I deaths and25-gem transfer; alternating first team; whole-group wake by real attack and sleep after two quiet rounds with damaged HP retained; and Ren dash passing an intermediate occupant while rejecting an occupied landing.
- Simulator self-tests and727 snapshot properties supplement, but do not replace, those engine transitions. Greedy dash steps still disagree with the actual jump; wall/hole paths, CC expiry, all spawn transformations and longer-horizon equivalence are incomplete. Simulator changes remain OFF.
- Farming extraction uses the actual final scorer:25,001 fixtures match exhaustive scoring, and the old bound has120 counterexamples. Native diagnostics check71,545 score arithmetic assertions with zero failures. Of these,474 sampled scores have stale round labels consistent with resumed execution;71,071 are current-round matched. Mathematical pruning and explicit work caps are separate approximations. The corrected bound remains OFF because measured strength/fuel did not justify promotion.
- Static camp-spawn census uses actual SDK groups: largest observed dangerous group6, with17 nearby lanes at radius6 and22 at radius8. This is not dynamic awake-threat coverage. A prior editor-based mixed census is withdrawn. Fifteen actual home-policy fixtures with a controlled SDK passed; these are not full banking or engine-legality tests. No broad shopping, recall or opening thresholds were changed.
- Baseline farm ledger joins17 current native replays and reconciles all34 team final submission totals and136 checkpoints. Clears, reward collections and secured submissions remain separate events, including zero-gem captures. Generic job/travel attribution remains incomplete.
- Native telemetry is parsed using official NUL frame delimiters and joined to awaiting-input controller state. Governor-OFF instrumented runs contain41 exhaustion incidents:19 interrupted play-turn plus22 after a completed probe. Governor-ON instrumented runs contain151:66 plus85. Both have zero unclassified incidents. Instrumentation changes workload, so these two numbers do not estimate governor causality. Distinct phases are a partition, not extra incidents.
- Governor-ON stage participation:5960 farming calls and5720 route calls in7725 controller turns;3604 refusal attempts across2812 unique stage/body/controller-round keys. Tactical69 admitted pairs/140 snapshots,71 refused pairs. Dense tactical coverage is not established. Observed post-probe tail maximum4907 and first SDK initialization around20,700 support bounded reserves, not a whole-game fuel guarantee.

See `agent2/RUNTIME_COMBAT_REPORT.md`, `agent3/FARMING_REPORT.md`, `evidence/engine-oracle.json`, `evidence/engine-boundaries.json`, and agent-specific raw receipts.

## Development performance

`DECISIONS.md` records all screened switches and rejection reasons. Each main screen uses200 generated maps and400 paired-side full games. All those screens have zero native bot faults. Governor wins220/400 (55%, map-cluster interval50.75â€“59%) with951 exhaustion entries versus1202 control, but its fixed four-opponent local pool wins82/136 versusbaseline88/136: matchedâˆ’4.41 percentage points, intervalâˆ’17.65 to+9.56. Head-to-head improvement alone does not establish general strength.

Exact physical map recognition verifies walls, holes, bushes, altars, orientation, actual pick count and physical camp-group definitions. The first generated identity header was rejected after native probes found editor/runtime disagreement. The corrected header is `3e84b617d5d38f92260ccbb6ee46e8c0937260abc75b461ad97cd3c87edeccf0`. Native20-scenario tests cover17 known and three altered maps, both teams,3/4/5 picks and C&I's extra physical body. The uncached strict build's120 exhaustion entries compare with81 for its paired OFF opponents; baseline self-play100 is a separate34-team-game diagnostic. Repeated full comparison at champion startup is not promoted.

The narrow rows fix checks actual collection claims before treating an ally on a reward tile as group coverage. On the paired development map, first collection occurs at32 on both sides versus40/42 control. It does not universally improve first banking: one side banks later. Uncached rows wins201/400 synthetic games and86/136 local pool games versusbaseline88/136; this remains cost-confounded and does not prove general gain.

E04b checks the exact physical layout once during draft and publishes a versioned own-team record. Runtime missing/corrupt/mismatched records fail closed without an expensive fallback. The existing six-byte prefix is preserved. Capacity>=16 failures clear the full cache extension to avoid SDK short-write tail retention. Actual writer fixtures pass110 checks; compiled codec tests reject6912 single-bit corruptions,864 truncations and216 context mismatches. Actual SDK probes separately cover20 scenarios,170 draft picks and210 champion modules; four copy-only fault scenarios pass38 runtime UNKNOWN checks with30 draft picks preserved. Observed draft capacity is1024 bytes. In180 matched known-map idle probes, mean first-module fuel falls from83,861 to19,772 and maximum144,723 to20,834. This includes identical SDK initialization/printing and is not a full-bot performance gain. Cached development, one-time fresh selection and exact final evaluation are complete below.

## Data quality and unavailable evidence

Historical analysis contains5325 match UUIDs with unavailable version/rating/submission fields, and2125 decoded match contexts/1998 unique traces with127 duplicates. Stable-group splitting uses364 connected components (273 train/91 validation). A ridge team/period/map-lineup model is exploratory; it does not establish a causal lineup gain, and ML remains OFF. See `evidence/agent4/DRAFT_DATA_REPORT.md`.

The four recovered executable opponents retain their true names and hashes: `spar_fish`, `spar_mite`, `spar_bush`, `live_platform_v35`. They are uncalibrated proxies. Historical named `farm`, `calm`, `v34c`, `sparexp`, `aggro`, `v30base`, `spartop` artifacts were not recovered. No sparexp-specific claim is available.

Local official files number17, but current live pool/version/submission windows are unavailable because CLI authentication is absent. Historical `maps_rdev`/`maps_real` were not recovered. Screen, validation and primary final maps have disjoint hashes/seeds but share ten generator templates. Structural-shift maps add geometry styles, not independent competition provenance. Intervals condition on these maps and fixed opponents; no Elo estimate or unseen competition-family claim is made.

## Fresh selection and final exact artifact

The frozen fresh protocol completed760 full games across eight schedules before once-only selection. Rows-only won118/160 local proxy-pool games versusbaseline119/160: matched−0.625 percentage points, map-cluster95% interval[−1.875,0], and mean gem-margin effect−5.2[−17.0875,+1.5625]. The governor stack won114/160; versusbaseline its matched effect was−3.125pp[−11.25,+5.0] and−13.6375g[−69.8125,+39.4875]. It lost its fresh baseline head28–52 and its rows-only head27–53. Both candidates fail the predeclared promotion rules. No threshold was changed or fresh selection repeated.

The official rows mechanism remains an identified improvement on one known map, but is not promoted after the complete candidate fails fresh validation. Synthetic gains must not be attributed to rows-specific intent: all generated layouts are physically distinct from official rows, and added code/checks can affect fuel despite an inactive policy branch. A static engine-geometry audit found327 distinct physical signatures across200 screen,40 fresh,10 shifted fresh,40 final,20 shifted final and17 local official maps; none were exact physical duplicates. Shared templates still limit independence and generalization.

The selected development OFF artifact is `032c963b6311c8ee28a40df307448e481ee6df3ade892f084dc4a7aa9c3a4093`. Source-default packaging initially changed only custom Wasm metadata under different output filenames. All standard sections were identical. The final build uses the recorded SDK llvm-strip step to remove non-executable debug/name metadata; raw and normalized standard sections are checked equal. Source and re-extracted official CLI bundle now reproduce the exact normalized artifact byte-for-byte.

- Final Wasm: `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1`.
- Legal source ZIP: `6622fb3f57f26f56cec83e03f9e64a9d1638fe907574bef0da3602311c7427ce`.
- All new policy and diagnostic flags: OFF. The inactive unit-count parameter remains8; original baseline tuning macros are unchanged.
- Exact final local official parity:34/34 raw replays, scores, rounds, native fuel incidents and faults match baseline.
- Untouched primary final suite:40 maps/80 games,40W40L, zero mean gem margin and zero native faults.
- Untouched structural final suite:20 maps/40 games,20W20L, zero mean gem margin and zero native faults.
- Final fixed proxy pool:40 maps×4 opponents×2 sides=320 games per build. Both final and baseline win233/320, with identical point/gem/fuel values in every matched stratum. Paired effects and conditional intervals are exactly0. This degenerate measured equality is not proof of universal equivalence or general strength.
- Final pool native fuel exhaustion remains1097 entries in320 team-games for each build. Retaining baseline behavior retains its suspensions; no near-zero-fuel claim is made.

Final holdout is760 games including the matched baseline pool; the exact-artifact34-game parity prerequisite is separate. No final map was used to retune policies. The package includes original source ZIP, baseline source and original baseline Wasm for rollback. See `evidence/FINAL_RELEASE_REVIEW.json`, `CANDIDATE_MANIFEST.json`, `BUILD_AND_ROLLBACK.md`, and the immutable result/schedule matrices.

## Completion and external limitations

The source, research changes, resumable harness, experiment registry, legal source/Wasm package, rollback and local results are ready. Required original calibrated opponents, authenticated live version/windows and independent competition-family validation remain unavailable. These missing external results prevent claiming full ladder validation or a competitive upgrade. GitHub publication does not replace or submit a live bot. Parent issue3 and workstream issues4–7 are retained for the durable record; final publication identity is recorded separately.
