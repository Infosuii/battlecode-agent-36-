# v37 preparation inventory — 20 October 2026

**Superseded baseline status:** The finished balance report is now identified. See [EXECUTION_PROMPT.md](EXECUTION_PROMPT.md) and STATUS.md. Reusable historical resources below remain inventory only.

Planning/inventory only. The user confirmed the balance pass is **not done yet**. No final balanced source, settings, artifacts or toolchain has been identified. Nothing below establishes compatibility with that future release.

## Reusable locations inspected

- `work/agent3-review` is clean on `codex/v36-owner-integration`, local HEAD `c700e6a`. Its integration README explicitly excludes balance work. Do not treat HEAD or the pre-balance main checkpoint as the v37 baseline.
- `work/agent3-review/integration/windows-20261020/tools/evaluate.py` provides immutable build IDs, source/settings manifests, artifact-keyed resumable jobs, paired sides, deterministic-job deduplication and submitted-gem summaries. Its defaults and profile switches are historical; explicitly bind the final balance runner/settings before reuse. Check engine identity on every accepted result, not merely in the final summary.
- The same integration package's `README.md` documents frozen `source/src/lead/` and reconciled `source/src/owners/`. Inspect the final balance package's organization before choosing an edit path.
- `work/recovery/ops/analyze_suite.py` decodes historical replay fuel, checks body counts and partial-round bounds, and cross-checks native exhaustion counters. It is pinned to historical fuel limits/format and visualiser location; verify these against the balanced engine before reuse.
- `work/recovery/ops/native_camp_probe.cpp`, `prepare_native_probes.py` and `assert_native_probes.py` cover camp awake state, healthy awakened members, splash wake, departure/sleep and recall/heal timing. Historical assertions pin engine `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`; these are starting fixtures, not new-version passes.
- Local Git ref `origin/v36/validated-results-20261020` contains `v36/agent3/farming_ledger.py`, economy/capture/cache/scorer fixtures, Agent 2 model/snapshot/awake fixtures, experiment protocols and consumed schedules. These were inventoried using Git tree listings without checking out or modifying that branch. No training pipeline was established by this inventory.
- `work/integrated-v36-20261020/` holds the old combined ZIP, WASM and manifest. Preserve as reference only. `bot_v36/src` at workspace root is an older partial overlay and must not seed v37.

## First actions after final balance handoff

1. Record final commit/package, materialized switches, source/ZIP/WASM hashes, actual CLI/SDK/native engine/compiler identities, build command, validation receipts, available opponent provenance and consumed splits. Preserve v36/v34; create a separate v37 tree only after identity is resolved.
2. Reproduce the unchanged balanced release with that exact toolchain. Verify package/source/default parity and runner engine identity before comparing candidates.
3. Read the existing ledger and one representative replay/schema. Audit collection versus secured submissions, dropped/lost bags, body/link identity, missing observations and partial rounds. Rank economy/opening/safety losses from traces before choosing a policy hypothesis.
4. Register each hypothesis, correctness checks, limited development screen, expansion/stop rules and untouched confirmation groups before matches. Use independent switches and measure secured resources/full-match outcomes; keep diagnostic fuel separate.
5. Extend decision-time logging for ML with eligible and unselected candidates, missingness and observed outcome labels. Split whole matches/map groups; exclude future outcome features and unobserved counterfactual claims. Train interpretable predictors only after data joins and labels pass checks.

No builds, matches, feature changes, training, promotion or platform submission were performed in this preparation step. No other chat was messaged and no background monitoring was scheduled.
