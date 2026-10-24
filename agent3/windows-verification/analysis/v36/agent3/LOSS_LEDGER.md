# Farming time and resource loss ledger

Status: instrumentation counters implemented; historical match ledger awaits Agent 4's normalized parser and identity join. Missing entries are unavailable, not zero. No case-study incident has been attributed from its filename or directory/version name.

The supplied summaries have formatVersion 1.0, metricSemanticsVersion 1, engineVersion 0.0.3 and exact engine/map hashes. They have no platform submission/team-version join in their own top-level schema. Their `teamTimelines`/`championTimelines` use `sampleCount` plus value/duration runs; `events` holds owner/kind/entries, whose entries have round/input/substep/phase timestamps. The exact mapping of timeline index to completed/partial round and `campClear` semantics must come from Agent 4 and the compatible engine. A raw event name alone is not a capture definition.

| Ledger field | Available source | Required handling / outstanding dependency |
|---|---|---|
| Match/team/submission/source identity | Platform metadata + archived source manifest | Join exact match UUID and team; version names alone forbidden |
| First camp clear | campClear stream | Verify whether this is a monster or whole-group clear; never substitute for capture |
| First successful reward capture; captures at 50/100/150/300 | Normalized completed group captures | Agent 4 parser/engine semantics; deduplicate shared recipients by group/timestamp |
| Earned gold at 50/100/150/300 | Champion earnedGold timelines | Sum exact team roster; index boundary and partial round semantics required |
| Collected gems at 50/100/150/300 | Champion collectedGems timelines / gemCollection | Distinguish camp collection from stolen/transfer resources |
| Secured/submitted gems at checkpoints | Team submittedGems timeline / gemSubmission | Submission completion, not carried inventory or capture, is secured |
| Travel, waiting/staging, fighting, collection, recall, shop, healing, idle | Own policy/position/action traces + engine channel/spawn state | Exclusive activity taxonomy with explicit unknown gaps; policy modes alone do not prove successful action or true duration |
| Failed/abandoned jobs | Target/claim transitions + group/capture outcome | Do not classify an ally completion or low-fuel retained claim as an abandonment |
| Lost vs banked gem bags | gemLoss/gemSubmission + death/transfer context | Bags lost to enemies are transfers; camp losses are destroyed; exclude double counting |
| Helper claim vs contribution | Shared claim, observed position/HP, actual arrival/hits | A claim is neither current tanking nor guaranteed future DPS |
| Camp-caused body deaths | Death context + exact source | Count body deaths explicitly; retain causal attribution uncertainty |
| Linked deaths | Pair identity + synchronized causal incident | One linked incident can contain two body deaths and two lost bags |
| Bound/work/fuel/order/route and fallback cost | FARM_LEDGER aggregate counters | Implemented in owned source, independently disabled; instrumented release-fuel overhead still unmeasured |

`FARM_LEDGER` reports cumulative counts by champion/round, every 50 reached active turns when diagnostics are enabled. `considered` counts candidates visited after order/eligibility setup; `math` counts only bound-pruned candidates; `work_cap` and `fuel_cap` count the remaining ordered candidates at that exit, with work cap taking precedence if both apply. `order_cap` counts entries outside the production 256-order limit (including entries not examined for later eligibility); it is not a mathematical-pruning counter. `evaluated` counts candidates entering eligibility/need work and `scored` counts actual final scores. These intentionally differ.

`route_filtered` counts scored candidates not retained in top-K, distinct from mathematical pruning, order/work/fuel caps and unknown route loss among unscored groups. `route_expansions` counts actual eligible second/third-leg evaluations. Row construction, depth/follow-up limits and original route fuel checks remain heuristics. `stage_attempts/skips` refer specifically to the farm rescore gate; fallback kept/empty records its outcome. They are not a claim that every optional Bot stage is governed.

No use is made of an unverified private parser or a new private runtime-budget API. Requested owner contracts are documented on canonical issue #6.

Predeclared economy experiments once inputs exist:

- Rows: strict canonical identity; compare spawn-to-first completed capture and secured resources through a full match. A melee/tank opening is eligible only after reproducing the delay; stop for survival/resource regression.
- Lanes: measure each recall/shop/heal/return interval, missed respawns, item payback and bank safety; preserve emergency recall, completed reward captures and Tavi's field shopping. Change departure behavior only if these traces identify avoidable downtime.
- Corners: derive mechanism from capture/banking/route traces before choosing a policy.
- Dominant groups: compare early commitment, successful reward capture, survival/recovery and secured gems with corrected-bound on/off; do not tune the bonus upward from a scorer-only success.

None of these experiments has match outcomes on this host. The historical named opponent pool and fresh-validation/full unseen schedules remain with Agent 1.
