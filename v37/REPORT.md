# v37 evidence checkpoint

No new policy earned promotion. The fallback remains the exact selected post-balance v36. The complete execution prompt is **not finished**; see the requirement-by-requirement status in `STATUS.md`.

## What was executed

GitHub pinned commit and report were inspected and fetched. An isolated checkout/branch was created without modifying previous v36 trees or `sources/`. CAMBC 3.0.2 was downloaded into the workspace; its official initialization recovered SDK 3.0.2, engine 1.0.1, and 35 current official maps. The actual compiler is Clang 22.1.0. `release/MANIFEST.json` contains full identities, source hashes and build commands; its native build hash and visualiser WASM hash are distinct fields.

The authoritative archive has 18 source files. Every archive member matches its pinned manifest, and the Git checkout is LF-equivalent. Builds use the original ZIP bytes. Official CLI rebundling reproduces the original 136,792-byte archive exactly. The all-features-off experimental source also reproduces the baseline WASM exactly; an intermediate short-circuit refactor did not, was corrected, and was not selected as the release.

The balance receipts establish 100 consumed `p-tune` and 200 consumed `p-confirm` maps. Missing receipts/raw files do not certify any other old map as untouched. Fresh seeds 37102061/62/63 generated 24 development, 128 confirmation and 128 final layouts with the current official builder and explicit Golem support. The independent identity tool includes terrain, camp members/rewards/respawns, both spawns and draft size, normalizing all eight dihedral orientations. Across 35 official and 280 generated files it found 315 distinct physical groups and zero cross-split duplicates. 80 files contain Golems. Production map/draft classification remains unchanged.

## Full-match results

All results below use release/default fuel/full match rules and the unchanged selected baseline as opponent. Each accepted receipt validates native engine version **and build hash**, map bytes, source/WASM/toolchain identity and raw output hashes. Deterministic reruns are cached, not new evidence. Both policy screens use 59 map groups (24 fresh development and 35 consumed official), both sides. The intervals bootstrap whole paired map groups; they are descriptive development intervals, not proof of ladder strength or a fresh final result.

| Experiment | Games | W/T/L | Win points | 95% paired-map interval | Secured gem margin/game | 95% margin interval | Decision |
|---|---:|---|---:|---|---:|---|---|
| Linked-partner emergency | 118 | 55/0/63 | 46.61% | 40.68–52.54% | −6.68 | −47.66 to +35.80 | rejected |
| Visible individual camp-state recovery geometry | 118 | 56/0/62 | 47.46% | 38.98–55.93% | −27.28 | −92.73 to +34.38 | rejected |

Neither reached the registered screen threshold (53% and positive secured margin). Neither proceeds to confirmation. Zero action faults occurred in these screens; candidate fuel-exhaustion counts were 1,110 and 1,031 respectively. These are counts of turn events, not unique match failures. Lower deaths/fuel would not override the failed primary outcomes. No combination is justified by these component results. No calibrated opponent/ladder/Elo attribution is claimed.

The 35 baseline self-play matches are diagnostics only; side asymmetry and self-play win rates are not strength estimates. They had zero action faults and 574 team-A exhaustion events (both teams are retained separately in raw statistics). The ledger counted 11,353 lost and 4,131 end-unsecured gems across both sides, and 248 camp-attributed physical deaths. Lost gems may transfer to killers; they are not all destroyed resources.

## Data and correctness

Native resource events reconcile collected = submitted + lost + carried for every body/team in baseline and both policy screens. Duplicate event identities and inconsistent roster/body/team joins are rejected. Clear, collection, submission, zero-gem captures and partial final rounds remain separate. Checkpoints refer to complete rounds; final scores include the partial final round. Original parser/farming-ledger code was reused, then extended; the replay decoder was updated from snapshot size 184696 to the officially supported 184760 extension. Rows, lanes and corners decode rounds/final scores agree with native statistics.

The current SDK identifies camp states as mutually exclusive dead=0, awake=1, asleep=2. Seven official-engine checks pass: successful cast consumes allowance; linked bodies have distinct bags and shared death; Ren dashes in open terrain and through an intermediate occupant; occupied landings fail; ranged movement excludes attack; actual champion attack wakes healthy group members followed by sleep when aggro is removed. Seventeen extracted home-policy fixtures pass, including healthy Tavi, emergency recall, valuable capture, interrupted recall fallback, healing departure and unequal partner health pools. These are controlled action fixtures, not comprehensive balance or match-strength evidence. No simulator authority was expanded.

## ML research actually performed

The first float-formatted trace variant caused 9,559 candidate exhaustion events across 12 matches and was rejected as production-neutral instrumentation. Integer formatting and sampling reduced this to 4,198 but still changes behavior. Both variants and their raw evidence are retained separately. Instrumented performance is never pooled with production performance.

The second variant yielded **1,747** selected decision records and **6,181** candidate records after strict frame/shape/controller checks. Native output is NUL-position-framed, not plain lines. Actual replay controller identities validate body/team/round; at most one-round-deferred stdout is recorded explicitly, and inconsistent/ambiguous or partial frames are excluded. Same-round collection/death labels with unresolved ordering are excluded. Future events provide labels only; inputs come from the bot's decision frame. Eligible unselected candidates have null outcomes. Selected collection/abandonment/death/censoring episodes are retained; observed mode changes interrupt a job. Incomplete feature/exclusion coverage and the unverified SDK group-index geometry join are explicit gating limitations.

Four whole map groups are training data, one validation and one pilot test, with mirrored sides and all episodes grouped. The pilot test was inspected during quality corrections and is consumed development evidence. No final-policy holdout was used. Preprocessing is fit only on training rows. A fixed ridge predictor models log completion time on observed completed episodes; a regularized logistic predictor models interruption. Censored cases are retained and excluded from inappropriate complete-outcome fits. No unobserved counterfactual outcome is invented.

| Diagnostic split | Completion MAE | Constant median MAE | Hand-cost MAE | Interruption log loss | Constant-risk log loss |
|---|---:|---:|---:|---:|---:|
| Train | 5.21 rounds | 8.67 | 5.99 | 0.471 | 0.637 |
| Validation (one map) | 3.94 | 6.86 | 3.67 | 0.479 | 0.680 |
| Pilot test (one map) | 6.79 | 10.03 | 6.53 | 0.531 | 0.751 |

Completion prediction does not beat the hand-cost estimate on validation/test. Risk prediction improves observational fit, but the six-map, instrumented, selected-action data cannot justify activation. Calibration and failure examples are saved. Dataset/split/code/model/header/export hashes are versioned. Twenty-three C++/WASM inference comparisons pass, including clamped extreme inputs and explicit missing-input failure; offline/export raw outputs agree within 1e-9. A future correction is conservatively bounded to 0.98–1.02 with initial activation weight zero, but is **not linked into the bot**. Required full damage/recovery/secure labels, complete features/exclusions, production inference/fuel/bounds/zero-weight behavior and fresh learned-policy match evaluation remain incomplete. This is trained pilot research, not completed ML policy research.

## Release and continuation

`release/bot_v37_fallback.zip` is byte-identical to selected v36, SHA-256 `5ee75d520caa0c921361ea6c83a6a3b991d05ea8a6a7707226dabc875822b320`.

`release/bot_v37_fallback.wasm` is the locally rebuilt/tested production artifact, SHA-256 `5b9243b1d9513a450a2db00b89c5288a14a4e7a7f2f3fdff984be4fc526d1832`. This is not an invented original balance-host WASM hash. The source defaults, package and tested module agree. Both experimental policies default OFF. Rollback is the original selected ZIP itself.

There are 295 unique accepted full matches: 35 baseline diagnostics, 118 per policy, 12 per trace version. All raw checksums were reverified at packaging. `evidence/raw-index.json` points to retained local bundles/statistics/logs; those bulky raw files are deliberately outside Git but are not deleted. `evidence/build-receipts/` preserves complete build identities despite ignored compiler directories. `COMMANDS.md` records reproduction steps.

No live submission or merge occurred. All four handoff files were subsequently supplied and read; they confirm the selected baseline and scope. Raw balance-host artifacts and calibrated opponents remain unavailable. The user's authenticated website can download platform replays; a five-game version-38 bundle was retrieved separately without attributing it to v36. Remaining local implementation/validation is listed honestly in `STATUS.md`; missing external files are not used as a blanket explanation for unfinished work.
