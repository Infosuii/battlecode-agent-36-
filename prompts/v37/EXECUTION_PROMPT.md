# Build v37 from the post-balance v36 release

You are the single implementation agent for the full Battlecode v37 engineering and ML research task. Execute the work through measured selection and release preparation, not just planning or compilation. The objective is better secured resources and full-match performance against the exact post-balance baseline. Fewer deaths, lower fuel or better prediction error alone do not establish an improvement. Research may legitimately reject a feature; do not force every experiment ON.

## 1. Starting context and authorization

Repository: https://github.com/Infosuii/battlecode-agent-36- (private).
Windows workspace, if available: `C:/projects/cambridge battlecode` (the root is not a Git repository).
Existing clone: `C:/projects/cambridge battlecode/work/agent3-review`.
Local handoff files: `prompts/v37/{HANDOFF,REQUIREMENTS,STATUS,INVENTORY}.md`.

This prompt supersedes their earlier statements that balance work is still ongoing. Read those concise files for scope and reusable resources; do not load the whole prior conversation or repeat completed v36 investigations. If you are on another host, retrieve the pinned repository sources and bring over these requirements; absence of the Windows paths is not itself a blocker.

Use one active implementation agent. Do not spawn subagents, message other chats, schedule monitoring or run unattended follow-ups. Local scripts may batch independent builds/games with sensible resource limits. Inspect ownership and uncommitted files before edits. Use a separate v37 tree/checkout and preserve v34, the post-balance v36 and the older combined integration. Task-related implementation, experiments, commits, branch publication and draft PR preparation are authorized. Do not merge to main or make a live platform submission without separate explicit authorization.

The earlier settings-only restriction applied to the completed balance pass. This v37 task explicitly includes source changes, replay/data tooling, simulator work and ML training within the requirements below.

The previous agent only located evidence and prepared this prompt. No v37 source, experiments, ML training or release selection has been performed in this handoff chat.

## 2. Verified balance result and baseline identity

The finished post-patch report is at source commit:
`a020248d8b13593bdf4c70460116f2bb061860b4` (Agent 4 branch).
Settings experiments were added in:
`64c5b73f5c67c0a2b5beb6f9370a70aa3f688f97`.
PR #11 was merged, with merge commit:
`b8dce2f4a3f6ed3c0cdc1e2d263833b9a31bcdca`.

Read at the exact source commit:
- `v36/agent4/EXPERIMENTS.md`, especially E8/E9/E10.
- `v36/agent4/reports/DRAFT_REPORT.md`.
- `v36/release/CANDIDATE_MANIFEST.json` and `v36/release/bot_v36.zip`.
- Relevant result summaries, `v36/agent4/tools/paired.py`, `tune.sh`, `e9.sh` and map manifests as needed.

The reported post-patch runtime is CAMBC CLI/SDK **3.0.2**, engine **1.0.1**, workspace `bcws3` on the balance agent's host. Its decision was to keep `bot_v36` unchanged: no tested setting or Heart of Ruin draft passed confirmation. The selected source ZIP is:
`v36/release/bot_v36.zip`
SHA-256: `5ee75d520caa0c921361ea6c83a6a3b991d05ea8a6a7707226dabc875822b320`.
The report says rebundling with CAMBC 3.0.2 produced that same ZIP.

Important provenance distinction: this tested bot is Agent 4's v34-derived release with the unknown-map draft change, not automatically the combined Agent 2/4 runtime/combat package. In its manifest, the source is `bot_v36/src`, `NOVA_V36_DRAFT=1`, `NOVA_MAPID_STRICT=0`; all listed translation units except draft match its v34 snapshot. Use the exact selected ZIP/source as the primary v37 baseline. Do not silently replace it with current main, the root workspace's partial `bot_v36/src`, or the combined integration. Preserve the combined package as a separate named reference; any useful imports from it are independent changes requiring new-version checks and evidence.

The selected manifest still records engine 0.0.3/SDK 2.2.5. It is historical source/package provenance, not a complete 1.0.1 build receipt. Recover or regenerate actual 3.0.2 compiler/SDK/native-engine identities, settings, build commands and WASM hashes. Do not invent a post-patch WASM hash or treat a ZIP hash as a WASM hash. Verify source/default/package parity and reproduce the baseline under the pinned post-patch runtime before experiments. If exact host-only dependencies are missing, ask once for the specific artifact and continue independent useful work.

Keep these earlier identities separate:
- Pre-balance main: `86a2d09335175c8903167f7de226d27f9559134f`, PR #14.
- Combined package: `integration/windows-20261020/`; WASM `e82bb6648da312530e4927426b46c9373f3b37a600981779b7a74f728db8c37e` on CLI/SDK 2.2.5.
- Its OFF fallback WASM: `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1`.
- Historical native engine: `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`.
Their measurements do not establish new-engine compatibility or strength for the selected baseline.

## 3. Reuse the balance evidence and protect fresh evaluation

The report records:
- E8: the selected v36 beat v34 68.5% on 640 historical generated maps under engine 1.0.1; the 35-map official comparison was 50%. Those are limited head-to-head results, not ladder strength or fresh v37 holdouts. The E8 generated maps used the old builder and lack Golems.
- E9: carry 48 reached 55% in screening but 52% on confirmation and was rejected; keep carry 72. End-bank 0/400/800 and unknown-map gold weights 0.05/0.20/0.35 did not pass the registered screen. Existing end-bank is 600; gold weight is 0.10.
- E10: Heart of Ruin draft screened at 57.5% but pooled confirmation was 48% over 400 games. Fewer deaths did not produce more wins. Keep the selected v36 draft. Existing Heart of Ruin handling reportedly uses dash for movement only; do not infer that a meta draft transfers to this bot.
- `APEX_AGGRESSION` changes were inert because the enabled brain resets aggression each turn. Verify effective settings and action changes before launching sweeps.
- The post-patch draft model used 6,375 observational matches from roughly one hour; it is not the requested decision-time policy dataset or a causal policy test.

Recover actual consumed map hashes, generated splits and schedules. Reported post-patch suites were `p-tune` (300 maps, seed 4001), `p-confirm` (1,000, seed 4002), `p-final` (400, seed 4003), generated with the 1.0.1 builder, plus 35 official maps. Screens used the first 100 tune maps; confirmation used at least maps 1–100, and the draft extension used 101–200. Audit receipts to establish precisely what was consumed; do not assume the rest is untouched merely from its name. All older v36 final/holdout suites already inspected are consumed development data. Register genuinely fresh confirmation groups before tuning.

## 4. BASE and DATA: runner, replay analysis and economic ledger

Establish a resumable, artifact-keyed runner recording source/settings/WASM, engine hash, map hash, opponent identity, side, full-match rules, faults and fuel. Check engine identity for every accepted result. Preserve raw replays/statistics/logs and immutable schedule IDs; do not count deterministic reruns as independent evidence. Use the correct current replay and fuel formats.

Extend the existing parsers and farming ledger rather than starting over. Distinguish camp clear, capture, resource collection and actual secured submission. Attribute travel, waiting/staging, combat, collection, recall, shopping, healing and idle time. Record first camp/capture/collection/banking; gold and collected/submitted gems by rounds 50/100/150/300 and match end; abandoned jobs, lost/destroyed bags, deaths and linked deaths; helper claims versus real positioning, arrival and damage/tanking contribution.

Validate match/team/champion/body joins, C&I body-versus-linked-entity semantics, duplicates, partial rounds, missingness, stale observations, event ordering and resource reconciliation. Do not double-count linked bodies or silently turn absent data into zero. Save a data-quality report and selected trace examples. Use the ledger to rank the largest secured-resource losses before choosing fixes.

## 5. ECON: shopping, recall, healing and banking

Implement scoped, independently switched policies justified by traces. Evaluate item benefit/payback against departure, recall, shopping, healing, return travel and missed camp/capture opportunities. Separate spendable gold from carried resources that can be secured or lost. Make return/heal decisions reflect health, exposure, recovery time, bag value and remaining match time rather than another blind global threshold sweep.

Explicitly cover:
- Healthy Tavi shopping in the field and emergency recall when unsafe.
- Low-health emergencies and vulnerable linked partners.
- A rich champion finishing a valuable, safely completable capture before leaving.
- Banking a valuable bag before a likely loss and making the final bank in time.
- Interrupted recalls, safe replacement positions and repeated failed attempts.
- Spawn arrival versus actual full-heal timing and correct departure after healing.
- Repeated shop/recall loops, stale purchase conditions, affordable/legal purchases and ineffective item trips.

Preserve existing successful semantics unless a measured change is intentional. Demonstrate each trigger with action/engine fixtures or reproducible traces, then measure submitted resources and full-match outcomes. Keep rejected settings separate from untested policies.

## 6. ROUTE: openings, lanes, corners and farming races

Diagnose spawn-to-first-capture, farm order, contested travel, shopping departures, collection-to-banking delay and missed respawns on version-correct traces. Investigate lanes and corners from mechanisms rather than map names. Use canonical physical map identity that handles new/edited maps, names, symmetry and unknown layouts; preserve a robust fallback. Verify effective draft/brain settings, not just their declarations.

Reuse the existing rows/capture-intent mechanism when applicable; inspect and port it deliberately if absent from the selected source. Do not reimplement it or globally enable a formerly neutral opening. Preserve successful dominant-group commitments and avoid target oscillation. Distinguish mathematical score pruning from route approximations, candidate truncation and work/fuel caps. If an import or learned correction changes score bounds/order, revalidate exact scorer coverage and cap interactions.

Measure first collection and first secured bank, opening opportunity cost and full-match resource effects across relevant layouts, draft sizes and both sides. Test narrowly scoped variants before broader changes.

## 7. SAFE: camp-phase prediction and recovery safety

Predict damage before the next actual camp phase using visible awake state, attack reach, target/tie rules, current positions, observation age, health/shields/status, actual helper/tank presence and predicted arrival. Reassess when a tank leaves, becomes unsafe or cannot arrive before damage. A claimed job is not tanking; damaged camp health is not an exact awake-state proxy. Do not use future actual helper arrival as a decision-time feature.

Cover healthy members of awakened groups, splash waking nearby groups, individual sleep transitions, tank departure, late/absent helpers, boxed respawns, dense overlapping threats, stale/unseen camps, C&I partner vulnerability and linked death. Validate recall placement against all real threats, including idle allies maintaining aggro. Compare total completion/recovery cost, lost bags and secured resources; a policy that simply abandons profitable camps may reduce damage while weakening the bot.

## 8. SIM and runtime: official-engine oracle and bounded authority

Reuse existing simulator, snapshot, camp and action fixtures. Run checks affected by engine 1.0.1 before importing old runtime/combat changes or expanding simulator authority. Verify version-specific constants/mechanics from the pinned official SDK/engine. Add controlled official-engine comparisons for remaining gaps:
- Dash through terrain, holes/walls, intermediate occupants, omitted occupants and occupied landings, with exact champion rules.
- Movement/spell/reset/basic-attack allowances already consumed before search; successful casts/attacks and legal action ordering.
- Status application/expiry, shield boundaries, stun/sleep, recovery and camp phase timing.
- Team-relative spawn legality, linked bodies/partners and newly relevant threats such as Golems or Heart of Ruin where represented.
- One action, one round, then longer horizons; sparse/dense and open/blocked scenes.

The official engine is the oracle, not the simulator's own implementation. Record divergence categories, limits and safe fallbacks. Keep authority/relevance/model fixes independently switchable. Measure production inference/planning fuel and exhaustion, useful planning participation and actual action faults. A governor that eliminates nearly all planning is not a successful optimization. No expanded authority on compilation or fixture accuracy alone; require fresh match evidence.

## 9. ML: actual decision-time training and bounded policy experiments

ML is required research, not a label to attach to old headers or the draft model. Build and version a decision-time dataset through the validated ledger/logging pipeline. Start with interpretable predictors for completion time, damage/recovery cost and interruption/abandonment risk; compare simple baselines before a small bounded correction to the corrected hand score. Select model complexity by data quality, validation and fuel, not novelty.

Record match/engine/map/opponent/source/policy identities, decision round and body/link identity; candidate jobs/actions; legal/eligible/excluded/pruned/unselected candidates and exclusion reasons; base score and relevant score components; carried resources, gold/items, health/shields/status; current geometry, observed camp awake state/health/range; visible allies/enemies, actual current contribution, estimated arrival; observation age and explicit missingness. Use only information available to the bot at that decision. Full replays may supply labels but must not leak hidden or future facts into inputs.

Define observed labels for selected decisions: elapsed time to clear/capture/collect/secure as appropriate, realized damage, recovery/return cost, interruption, abandonment, death and lost bags. Treat unfinished episodes as censored/failed cases explicitly. Retain eligible unselected candidates but do not invent their outcomes or claim observed counterfactuals. Record selection bias and limits of observational fit. Any simulator-generated labels must be marked synthetic and validated separately.

Split by whole matches and map groups; group mirrored sides, related generated layouts and repeated episodes to prevent leakage. Add time/opponent separation where relevant. Fit preprocessing only on training data. Keep tuning/validation separate from a frozen final test. Version feature definitions, normalization, missingness/fallbacks, labels, split manifests, dataset hash, code, random seeds and model/export hashes.

Train and evaluate the interpretable models, report baseline comparisons, errors/calibration and subgroup failure cases, then inspect actual decisions. Check exported C++/WASM inference against offline outputs, finite/clamped values, missing/out-of-distribution fallback and inference fuel. Keep correction weight zero initially. Existing learner support (including a wide historical 0.5–2 multiplier) is not an activation recommendation: pre-register a conservative correction bound and retain heuristic fallback.

Require disabled-feature parity, enabled-zero-weight decision identity, preserved scorer bounds/work-cap behavior, and unchanged production behavior where intended. Verify instrumentation effects separately. Evaluate simple deterministic corrections against learned ones, independent model components and the integrated policy. Promote only with fresh held-out full-match secured-resource/performance evidence. Prediction accuracy alone is not promotion evidence.

If required raw data or runtime access is genuinely unavailable, document exactly what is missing, what preparation/training was completed and what remains. Do not call ML complete because it stays OFF; distinguish trained-and-rejected from never trained. Continue every unaffected workstream.

## 10. Experimental order, integration and promotion

Order: pin/reproduce BASE; audit DATA and rank losses; implement targeted ECON/ROUTE/SAFE/SIM changes in dependency order while collecting ML-ready data; train/evaluate ML; assess justified combinations and key leave-one-out variants; freeze and run fresh confirmation; package the exact release.

Before expensive runs, register the hypothesis, expected observable behavior, correctness/action gates, independent switches, maps/opponents/sides, primary outcome, sample/group count, stop/promotion rule and treatment of multiple comparisons or extensions. Use cheap substantive checks, then small full-match development screens. Expand only promising changes. Do not repeat all historic suites, blindly rerun the rejected settings sweep, or arbitrarily treat 200 games as sufficient for a small effect. Report uncertainty by independent map/opponent groups; audit inherited confidence calculations rather than copying them unquestioned.

Compare against the unchanged selected v36 rebuilt with the same pinned toolchain. Include relevant current official and fresh unknown maps, draft sizes and varied available opponents, then independent final confirmation. Distinguish actual calibrated opponents from proxies and missing host-only artifacts. Report wins/ties/losses, secured-resource margins, faults/exhaustion, key economic/safety effects and critical regressions. Do not invent Elo or ladder attribution without submission identity.

Keep instrumented fuel separate from production. Reuse verified unchanged results keyed to exact artifacts/settings; code/engine changes or new failures justify reruns. Evaluate interactions, especially shopping versus camp completion, safety versus helpers/routes, model corrections versus pruning/caps and simulator authority versus runtime budget. Ship only supported switches/defaults, preserving a clean rollback. If no candidate passes, release the unchanged baseline and honestly report rejected research and unresolved gates.

## 11. Deliverables and working discipline

Maintain concise durable status: pinned baseline/toolchain, current work, evidence paths/commit, completed gates, next action and exact blockers. Use explicit states: not started, in progress, blocked, implemented, validated, rejected, promoted. Compilation is only a build gate. Update at milestones, not every command.

Deliver an isolated v37 source tree/branch; baseline and release manifests; effective defaults; source ZIP and release WASM with hashes; reproducible build/run/analyze/train commands; experiment/split registry; raw-evidence index; replay/ledger quality report; model/data/feature/export hashes and evaluations; final combined/ablation/confirmation results; rollback and a requirement-by-requirement BASE/DATA/ECON/ROUTE/SAFE/SIM/ML/STACK status. Verify the exact shipped source defaults, ZIP and tested WASM agree. Attach any created draft PR, but do not merge or submit live.

Conserve usage through targeted source reads, one authoritative manifest first, bounded searches and compact summaries. Let scripts process matches/replays/training locally and inspect selected failures. Preserve detailed raw evidence in files rather than dumping datasets, huge PR diffs or logs into chat. Keep progress updates concise and milestone-driven; avoid repeated recaps. Do not sacrifice correctness or required validation to save usage.

Begin by verifying the pinned post-balance ZIP/source and recovering the actual CAMBC 3.0.2 / engine 1.0.1 build identity. Then execute the remaining work autonomously within this scope.
