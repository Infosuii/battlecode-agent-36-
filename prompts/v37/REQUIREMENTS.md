# v37 scope and efficient execution

Compare every proposed improvement against the **finished balanced v36**, rebuilt under the same pinned engine/settings. The objective is better secured resources and full-match performance. Fewer deaths, lower fuel, better predictions or compilation alone do not establish a competitive improvement.

## Remaining work

| ID | Work and completion evidence |
|---|---|
| BASE | Identify final balanced v36, preserve source/artifacts/settings, reproduce its build and establish the runner/schedules. Record actual version and available opponents. |
| DATA | Extend existing parsers/ledger: distinguish clear, capture, collection and secured submission; attribute travel, staging, fighting, collecting, recall, shopping, healing and idle time. Include lost bags and actual helper contribution. Validate joins, missingness, C&I body/link semantics and partial rounds. |
| ECON | Implement and measure justified buying/recall/healing/banking rules. Cover Tavi field shopping, low-health emergencies, valuable bags, rich champions finishing captures, interrupted recalls, full-heal timing and repeated-shop loops. |
| ROUTE | Diagnose lanes/corners/opening and farming-race losses from traces before changing policy. Use canonical map identity and robust unknown fallback. Preserve successful dominant-group commitments; measure both collection and first banking. Reuse the implemented rows mechanism rather than reimplementing it. |
| SAFE | Predict next camp-phase risk using visible awake state, threat reach, actual helper/tank position/arrival and linked-partner health. Cover tank departure, healthy awake camps, splash wake, sleep, boxed respawns, dense threats, stale/unknown observations and recall placement. A claim is not tanking; damaged health is not an exact awake-state proxy. |
| SIM | Complete remaining dash/terrain/occupant, status/shield boundary and longer-horizon simulator accuracy. Reuse existing engine fixtures and run relevant version-affected checks before expanding authority. Validate against the official engine, not the simulator itself. |
| ML | Build a versioned decision-time dataset; train interpretable completion-time, damage/recovery and interruption predictors before considering a small bounded score correction. Preserve eligible/unselected candidates and missing-feature fallbacks. Future outcomes are labels, not input features; unchosen outcomes are not observed counterfactuals. Split by whole matches/map groups and time/opponent where relevant. Record model/data/feature hashes, zero-weight identity, finite/clamped output, inference fuel and fresh full-match effects. Prediction accuracy alone is not promotion evidence. |
| STACK | Integrate justified features behind independent switches; assess interactions and key leave-one-out variants against balanced v36. Verify final source defaults, source ZIP and artifact agree. Preserve rollback and report every requirement's actual state. |

ML is an explicit v37 research workstream. Do not label it complete merely because old headers exist or it remains OFF. If data/access prevents training, record the precise blocker and completed preparation. A researched feature can be rejected or left OFF when evidence fails; distinguish that from work never implemented or tested.

## Order and validation

1. BASE and targeted DATA inventory; identify the largest measured losses.
2. Implement scoped ECON/ROUTE/SAFE/SIM changes in dependency order; collect ML-ready labels through the same tooling.
3. Train/evaluate ML once data is trustworthy; compare simple corrections first.
4. Run combined selection, fresh confirmation and exact release packaging. Register hypotheses and stop/promotion rules before expensive experiments. Keep final holdouts untouched until freeze; do not reuse v36's consumed final data as a pristine v37 holdout.

Use cheap substantive correctness/action checks before small match screens. Expand only promising candidates to registered larger screens, official maps, available varied opponents and fresh confirmation. Count independent map/opponent groups, not deterministic reruns. Keep instrumented fuel separate from production. Reuse verified unchanged results; changes/new failures justify reruns. Record unavailable external gates without substituting differently named artifacts or inventing wins/Elo.

## Usage discipline

- Keep STATUS.md short: pinned baseline, current work, evidence/commit, completed gates, next action and blockers. Update it at milestones, not every command.
- Read relevant source sections/diffs and one authoritative manifest first. Load old long prompts/reports only for an unresolved detail.
- Batch independent searches/reads; bound tool output. Parse JSON and print selected fields; never dump entire tool catalogs, metadata, datasets or logs.
- Let local scripts perform matches, replay processing, training and aggregation. Inspect summaries and selected failures; save raw artifacts to files.
- Reuse the runner, owner patches, map identity, fixtures and dataset tools already supplied. Check whether a result exists before rebuilding an entire workstream.
- One active implementation agent by default. No concurrent writers to shared bot/world/draft files. Do not send messages to other chats without human authorization.
- Keep meaningful progress updates and final reports concise. Do not repeatedly recap the entire plan. Do not trade away correctness or necessary validation to save tokens.
- Completion states: not started, in progress, blocked, implemented, validated/rejected, or promoted. Compilation is a build gate, not proof that all planned features are done.
