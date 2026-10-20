# Runtime and combat investigation — Agent 2

Updated 20 October 2026, local continuation. This report distinguishes source repairs, independent engine facts, charged diagnostics and competitive evidence. New combat/runtime switches remain candidates until the lead freezes the exact release. No platform submission or strong-opponent gain is claimed.

## Decisions from completed screens

| Change | Same-source comparison | Screen result | Decision |
|---|---|---|---|
| Flee-hit | flee versus control-A, 400 games / 200 synthetic map groups | 193W–207L, 48.25%, clustered 95% interval 45–51.5%; mean submitted margin +3.85, interval −15.855 to +23.495 | Retain OFF. Legal implementation, no demonstrated policy gain. |
| One-cast/action allowances | model-fix versus control-A, 400 games / 200 synthetic map groups | 191W–209L, 47.75%, interval 44.25–51%; margin −11.07, interval −30.515 to +8.23 | Retain OFF for release policy. Keep verified repair as a research branch; a more accurate model alone did not improve policy. |
| Relevant five-unit snapshot | relevance5 versus control-B, 400 games / 200 synthetic map groups | 210W–190L, 52.5%, interval 48.75–56.5%; margin +8.755, interval −13.045 to +30.815 | Retain OFF: benefit inconclusive, dense-scene/strong-opponent authority unresolved. Existing estimate unchanged. |
| Runtime governor | governor versus control-B, 400 games / 200 synthetic map groups | 220W–180L, 55%, interval 50.75–59%; margin +18.945, interval −7.37 to +43.835 | Shortlist for fresh validation, not yet release promotion. Estimates remain provisional and exhaustion persists. |
| Observed group awake state | world-awake-v2 versus same-source OFF, 400 games / 200 groups | 203W–197L, 50.75%, interval 46.25–55.25%; margin +7.98, interval −17.225 to +33.255 | Retain OFF. Correctness-supported state candidate has no demonstrated policy gain. |
| Standalone positive-verdict safety | NOVA_TS_SAFETY | Source/fixture checks; no independent full protocol completed | Retain OFF unless separately validated. Relevance always includes mandatory omission authority restrictions. |
| Camp simulator | APEX_TS_CAMPS | Model lacks wake, lifetime, HP, blocking and camp deaths | Retain OFF. World awake-state repairs do not repair the simulator. |
| Action search / calibration / local network | APEX_TSEARCH / APEX_TS_CALIB / APEX_TS_LOCALNET | No new policy validation | Retain baseline OFF values. |

Flee and model-fix official smoke results (15/34 and 18/34 wins) have wide intervals and do not overrule their larger screens. All completed screen records report zero unhandled runtime faults; this does not prove absence of caught SDK refusals. Flee reported 1,230 and model-fix 1,182 candidate exhaustion entries over 400 games, respectively. These are counts, not gains over controls. Historical claimed flee-hit improvements were not reproduced here. New synthetic maps are not the historical maps_rdev/maps_real suites. Same deterministic match repetitions are not independent observations.

Receipts: `results/flee-screen-summary.json`, `results/model-fix-screen-summary.json`, relevant per-chunk schedules/results and immutable build manifests. The lead controls fresh validation and final holdout; no fresh holdout result is implied here.

Governor screen counted 951 candidate versus 1,202 opposing-control exhaustion entries, with at least one event in 282 versus 311 of the 400 games. This is a descriptive 20.9% count reduction against the control sharing those games, not a zero-exhaustion guarantee or separate baseline counterfactual. Relevance counted 1,234 versus 1,219 control entries, offering no runtime improvement. Official smoke was 16/34 governor wins and 15/34 relevance wins, both too small for a positive generalization claim. Exact existing-statistics/native-fuel supporting analysis is in `agent2/evidence/screen-diagnostics.json` when complete; fresh validation must address repeated candidate selection.

The varied local proxy-pool comparison did not confirm the governor screen: candidate 82/136 wins versus baseline 88/136 on matched schedules, paired −4.41 percentage points with interval −17.65 to +9.56. These distinct recovered proxies are not the missing historical calibrated pool. Governor has not earned release promotion on this evidence; a lead-controlled fresh gate and broader non-regression assessment remain necessary if it is retained on the shortlist.

Native bundle denominators and death diagnostics were independently read for all 1,600 games across the four combat/runtime screens; all statistics reported complete detailed capture and matching engine identity:

| Candidate | Candidate controller turns | Fuel events / turn | Opposing control rate | Body-death difference per game, clustered 95% interval |
|---|---:|---:|---:|---|
| Flee | 848,561 | 0.14495% | 0.14507% | +0.0225 [−0.23,+0.275] |
| Model fix | 848,406 | 0.13932% | 0.13944% | +0.12 [−0.1475,+0.385] |
| Relevance5 | 848,323 | 0.14546% | 0.14369% | −0.0675 [−0.3375,+0.2] |
| Governor | 845,737 | 0.11245% | 0.14212% | +0.005 [−0.3025,+0.3125] |

Governor's mean fuel-event difference was −0.6275/game, interval −0.9275 to −0.3275, against the opposing control in the same games. Body deaths count linked bodies separately; none of these intervals demonstrates a death reduction, and no true sparexp comparison exists.

## Runtime API and admission limits

`runtime_budget.h` publishes:

- `fits(left, cost, reserve)`: tests the reserve first, then subtracts it. It avoids unsigned underflow and overflowing cost-plus-reserve arithmetic.
- `admit(Stage, cost, reserve=22000)`: always admits when NOVA_RUNTIME_GOVERNOR is off. When enabled, refuses before optional work and optionally logs the admission request.
- `Probe(Stage)`: when NOVA_RUNTIME_TELEMETRY is on, reads actual charged release fuel and round number; it reports cross-round scopes separately instead of treating their fuel as zero-cost work.

Agent 3 integrated the API into score entry, individual score candidates, route entry and fields/order preparation, preserving coherent current-target/no-target fallbacks and existing deterministic caps. The first farm with field_calls=0 already follows the low-fuel path; no unjustified blanket round-one route skip was added. Fields are built near the turn tail. Init, world observation, action execution and other mandatory work are not globally bounded by these admissions.

The 22,000 reserve and 18,000 score / 4,000 group / 20,000 route / 15,000 field-order estimates are engineering hypotheses, not worst-case proofs. Per-group cost can depend on helpers, groups and optional work. Governor admissions can count a farm refusal on a turn that the baseline would already have skipped because of field_calls or search participation; attempt-level refusals must not be presented as incremental lost body turns. Multiple same-stage admission calls can occur in one controller turn.

## Actual charged release diagnostics

Agent 4 verified the official visualiser's framing: every NUL increments the engine input position, including empty spans. Frames were joined to actual engine awaiting-controller snapshots, rather than inferred from a repeating champion order. All 4,002 plus 4,028 nonempty telemetry frames joined uniquely; no missing or ambiguous positions.

Two instrumented ambush matches produced:

| Quantity | Observed |
|---|---:|
| Actual candidate controller turns | 8,071 |
| Official fuelExhausted entries | 41 (0.508% of controller turns) |
| Detected suspended play_turn invocations | 19 |
| Official exhaustion entries after a completed play_turn probe | 22 |
| Unclassified official entries | 0 |
| Unhandled runtime faults | 0 |
| Deliberate governor admission refusals | 0 (governor OFF) |

The 19 scopes and 41 official events overlap and must not be added. A nested fields suspension belongs to its parent invocation. Printed start round is preserved separately from the controller round at which buffered output was flushed. The 22 events outside the completed play_turn scope show why a tail reserve matters; their exact printf/flush/end_turn substage remains uninstrumented.

Native fuel records agree with actual controller order after the five draft invocations. Every official exhausted candidate invocation maps to its exact 100,000 fuel limit. Current runner limits are 100,000 normal turn, 200,000 first champion turn, 1,000,000 first draft turn and 100,000 subsequent draft turn. Native candidate gameplay fuel spent: p50 65,956; p90 84,307; p99 97,339; maximum 159,381 (first-turn allowance included).

| Charged scope | Non-suspended records | p50 | p99 | Maximum |
|---|---:|---:|---:|---:|
| Init | 12 | 65,895 | 65,903 | 65,903 |
| Farm scorer including nested route | 6,182 | 21,270 | 27,386 | 36,875 |
| Route | 5,911 | 5,919 | 8,628 | 13,840 |
| Snapshot | 180 | 2,619 | 6,079 | 7,307 |
| Tactical pair scope | 180 | 21,003 | 32,483 | 33,423 |
| Fields, including no-work calls | 7,687 | 131 | 9,673 | 61,332 |
| play_turn | 8,011 | 61,201 | 91,983 | 138,653 |

Nested scope costs overlap and are not additive. Exact same-frame route subtraction leaves scorer-only p50 about 14,803–15,138, p99 22,035 and maximum 28,239. Thus even the score estimate of 18,000 is not an observed maximum. Native fuel minus completed play_turn cost was typically 4,867, with observed maximum 4,907 on later turns; the first-turn difference was about 20,706–20,728 and includes initialization outside the probe.

Instrumentation uses charged counters and diagnostics rather than fuel-free debug_block. Its printing changes execution and can itself contribute to exhaustion. These two games cannot establish telemetry-free strength, broad fuel safety, or exhaustion improvements. All zero-cost cross-round scopes were excluded from distributions. Receipts: `agent2/evidence/telemetry/telemetry.json`, `framed-telemetry.json`, Agent 4 `evidence/agent4/stdout_framing`, immutable `builds/telemetry/manifest.json`.

### Governor-enabled participation diagnostic

A separately granted pair of ambush diagnostic games enabled governor plus telemetry, with 7,725 actual candidate controller turns. All 7,574 nonempty output frames joined uniquely; member sizes 734,595 and 822,696 were below the 1 MiB cap. Per-champion streams were reassembled across input frames to support interrupted printf calls; no RB/RBSKIP line crossed frames in these two bundles and no refusal prefix was unparsed.

| Stage | Logged refusal attempts | Distinct stage/body/controller-round refusals |
|---|---:|---:|
| Fields | 3,523 | 2,731 |
| Tactical pair | 71 | 71 |
| Farm score | 10 | 10 |
| Total | 3,604 | 2,812 |

Printed admission round and controller flush round were retained separately; their deduplicated totals agree here. There were 5,960 score scopes (77.15% of controller turns), 5,720 route scopes (74.05%), 140 snapshots/admission cases and 69 completed tactical pairs. The 71 pair refusals account for the remaining pair admissions. This demonstrates substantial farming/route participation in these instrumented games, not universal planner quality or performance. Scope calls include potential no-work or refused calls. Accepted admissions are not all logged, so general attempt-level refusal rates are unavailable. Farm refusals may overlap the baseline's own low-field/search fallback and are not all incremental lost decisions.

Native events were 151, reconciling with 66 suspended play_turn invocations and 85 events after a completed probe, zero unclassified and zero unhandled faults. They must not be added to each other or interpreted as a governor regression against the other telemetry variant: extra RBSKIP printing and divergent states materially change execution. Telemetry-free strength/fuel screens are the relevant evidence. Receipt: `agent2/evidence/governor-telemetry/framed-telemetry.json` and its immutable build/schedule.

## Search coverage, snapshot and measured costs

The original deterministic adapter already reduces two enemy models to one and shortens the horizon. Its minimum paired estimate for six units is 79,200, above the fixed 70,000 work budget. That estimate was not lowered. A comparison contributes only completed fight/run pairs at the same horizon and enemy model.

Diagnostic limitation: a predicted-budget refusal inside horizon selection can occur before model/horizon telemetry assignment; zero in those fields means unavailable, not a completed zero-horizon model. The small observed scenes all selected one enemy model under the current deterministic estimate.

The relevance candidate ranks observation-time candidates deterministically by reachable threat, proximity, attack opportunities and vulnerability, with ID ties. It groups observed Cassi/Issac dependencies, prioritizes self's observed linked body, never fabricates a hidden partner, and keeps omitted observed champions/camps/reservations as conservative stationary movement blockers. Original eligible population is reported separately from retained population. Missing partner, omitted enemy, nearby dangerous camp or preserved lethal-burst risk prevents a favorable search verdict from erasing the corresponding formula caution. It does not impose blanket retreat when fighting is the legal fallback.

The actual ambush diagnostic contained only original scene sizes two and three. Size two: 148 calls, 145 completed comparisons, three clear fight, twelve clear retreat, 130 inside margin, three safety refusals. Size three: 32 calls, thirteen completed comparisons, four clear fight, four clear retreat, five inside margin and nineteen safety refusals. It supplies no actual dense-scene coverage evidence. Original=0, when present in other telemetry, means admission refusal before observing a snapshot, not an empty scene.

Actual complete three-round pairs cost at most 24,497 with two retained units and 33,359 with three, against estimates 35,200 and 52,800. Full preparation was separately measured up to 11,167 and 12,395. These observations are too narrow to establish cheaper conservative bounds.

A separate single authorized 14-round release diagnostic constructed repeated synthetic fights with generic damage/reset/shield kits. Observed paired cost for horizon two and 2/3/4/5/6/7/8 units was 17,997 / 23,938 / 36,359 / 41,738 / 53,986 / 57,091 / 71,182. Horizon three for 2–7 units was 22,705 / 28,905 / 43,459 / 52,328 / 63,984 / 72,191. Eight units at horizon three produced no completed output before the cutoff; absence is not counted as an exhaustion event without its official receipt. Thirteen distinct scenario cells repeated across six bodies are not 78 independent observations. Snapshot preparation and real scene diversity were excluded. These diagnostics do not justify lowering estimates. Receipt: `agent2/evidence/runtime-probe/runtime-probe.json`.

## Simulator correctness and unresolved mechanics

Independent root engine scenarios confirmed:

- One successful cast per turn: Varg shield succeeds; following reset fails with code 27.
- A basic cannot repeat before reset; reset restores an attack; Varg splash deals the expected basic damage.
- A ranged ordinary move prevents its basic attack.
- Ren retaliates for 20 health at 100 focus even when the triggering basic hit is lethal.
- Linked death removes both bodies and awards their combined bags.
- Actual champion damage wakes a whole camp group. Later sleep can occur while camp health remains damaged. Independent boundary observation also confirms alternating starting team across rounds.

The action-allowance candidate tracks cast, ordinary move, attack and walked state, captures root's already-spent allowances, limits basic attacks until reset, and resets allowances on future simulated rounds. It changes the reproduced damage+basic+reset+basic counterexample from 200→150 to legal one-cast 200→170. No speculative Ren repair was added. Promotion remains OFF because policy screens did not demonstrate a gain.

Remaining material limitations:

1. Dash is represented by greedy substeps and coarse candidate landing checks, not the engine's complete path/terrain/occupancy rules. No broad dash differential proof exists.
2. `Map::blocked` is rooted in our team-relative static map. Simulated enemy movement/recovery at the two spawns is not fully team-relative.
3. The current-round team lead is still approximated outside separately calibrated modes; confirming alternating rounds alone does not establish exact snapshot action order or target ties.
4. Full stun/buff/cooldown boundary tests, Vendetta kill resets and multi-round 2v2/3v3/dense differential trajectories remain incomplete.
5. Camp simulator has no full camp HP/death/wake/sleep state and cannot predict camp gem destruction faithfully. It remains disabled.
6. Gold is not fully represented in the tactical state/objective. Simulator fixtures are not independent engine equivalence.
7. Live root cast allowance is derived from local successful-cast tracking; the newer SDK also exposes can_cast. Existing call sites must remain consistent with that tracking, especially after resumed stale execution.

Production model fixtures were rerun both OFF and ON, passing allowance, shield, link, stun, occupancy and admission arithmetic cases. A verbatim production snapshot-body fixture passed 727 properties, including all 720 permutations of six enemies, observed/hidden linked partners and omitted risk. These use controlled observation helpers. Receipts: `agent2/evidence/model-off/model-fixture.json`, `model-fixes/model-fixture.json`, `snapshot-fixture/snapshot-fixture.json`; independent engine receipt `evidence/engine-oracle.json` and `engine-boundaries.json`.

## Independent shared-code reviews

Agent 3 alone applied bot.cpp changes from Agent 2's recorded-hash snippets. Flee-hit is melee-only, checks the real can_attack allowance, chooses weakest shield-adjusted visible adjacent health, uses attack_unit, casts no reset and continues escaping. The guard preserves other movement/spell allowances. No successful double attack is introduced.

Farming bound review found all active positive score modifiers bounded conservatively, positive participants/kill/drain omitted only from the optimistic denominator, unsupported values unprunable, overflow guarded and rounding padded. Agent 3's actual scorer-body fixtures passed 25,001 pruned/exhaustive choices and exposed 120 old-bound counterexamples under controlled optional modifiers. This is separate from the 16-group approximation and route objective. No mathematical-scoring review blocker remains.

The world awake-state review found a real candidate issue: old observed asleep state could hide newly awakened unseen survivors when the only newly visible member was dead. Root repaired this using fresh observation precedence and explicit inferred provenance; predicted respawn no longer forges a direct-observation timestamp. Verbatim production World methods passed ten state properties, including that regression, damaged-but-asleep, hidden memory, inferred/observed replacement and respawn/event provenance. Receipt: `agent2/evidence/world-awake-fixture/world-awake-fixture.json`. This supports correctness of the lifecycle candidate, not performance promotion or camp simulation.

The strict rows capture-intent candidate was reviewed for action/fuel legality: it adds no action, bounds ally-slot scans by six, reads cached claim memory, computes exact map identity lazily once and falls back on missing facts. Its group-reference subtraction is used only with World group entries in reviewed call sites. First-call identity fuel and farming-race behavior still require the lead's matched tests; no outcome is inferred from the helper fixture.

## Cached identity and release review

Independent cache/action/fuel review found no action-legality blocker in the final cache design. The writer respects immutable startup snapshots and SDK retained-tail semantics, preserves the six original prefix bytes, and fails closed on extension/context corruption. Agent 3's actual writer checks and Agent 4's 210 native normal-context plus 38 fault-context body modules match the inspected draft/codec/strategy sources. Same-source first-invocation probe mean drops from 83861.07 to 19772.34 across 180 known-layout modules, including SDK initialization and identical printing; this is not a full-bot or whole-game fuel claim.

The reviewed fresh protocol now fixes numeric acceptance, exact schedule grids, map counts, artifact hashes and immutable freeze receipts before fresh games. Exact source-default packaging verifies selected source/artifact identity, bundle tree equality and extracted-archive binary equality. Actual final release approval remains pending its selected files and exact-artifact holdout. Detailed checklist and remaining receipt-integrity recommendations are in agent2/CACHE_FRESH_RELEASE_REVIEW.md. No policies or production sources were changed during this review.

## Unavailable evidence and release limits

No authenticated current platform window, true sparexp artifact, calibrated historical opponent pool or historical unseen suites were available to this agent. Proxy artifacts must keep their own identities. Historical omniscient replay states lack controlled vision masks and were not used as observed snapshot coverage. Strong-opponent deaths, current ladder A/B, final mixed-stack interactions and exact release proof remain the lead's release gates. No gain from separate candidates is added arithmetically, and no final holdout is tuned here.
