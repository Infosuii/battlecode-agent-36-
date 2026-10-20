# v37 handoff

**Latest:** The balance report is now found, and the user wants execution transferred to another agent. Read [EXECUTION_PROMPT.md](EXECUTION_PROMPT.md) first; it supersedes the pending-balance statements below and records the tested-source/combined-package distinction and remaining toolchain provenance gap. No v37 execution was started here.

## User objective and timing

Another agent is tuning v36 for newly released balance changes. Start v37 from that agent's **finished, identified v36 release**, then address the unfinished economy, routing, safety, simulator, replay and ML work in REQUIREMENTS.md. The user authorized moving to a fresh chat to conserve usage. This is a fresh v37 task, not a request to repeat all completed v36 agent work.

Before implementation, find the balance agent's final source commit/path, materialized settings, source ZIP/WASM hashes, CLI/SDK/native-engine versions, build command, validation receipts and consumed map/split schedules. Do not infer completion from the newest commit alone. If the final handoff is missing, ask once for its location/commit and continue only independent planning/inventory. No polling automation or continued background execution is requested.

## Workspace and durable source

- Workspace: `C:/projects/cambridge battlecode` (not itself a Git repository).
- Repository: `https://github.com/Infosuii/battlecode-agent-36-` (private).
- Existing clone: `C:/projects/cambridge battlecode/work/agent3-review`.
- Prepare v37 in a separate source tree/checkout after the balanced v36 is identified; preserve that exact v36 and original v34 for comparison/rollback. Check existing work and ownership before changing branches or files.
- **Do not copy workspace-root bot_v36/src blindly:** it is an older partial Agent 3 overlay, not the final combined bot.

## Last verified state before the separate balance pass

Main commit `86a2d09335175c8903167f7de226d27f9559134f` merged PR #14. Its combined source and artifacts are under `integration/windows-20261020/`. This is the pre-balance integration checkpoint, not confirmation of the balance agent's finished release.

- Agent 1 final source: PR #13, commit `f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a`.
- Agent 2 imported snapshot: `f34268b6a3e563590781822400b4e3184881f86c`.
- Agent 4 imported snapshot: `c392f0f9a424e8c88124ae059587c033870b614b`.
- Combined WASM SHA-256: `e82bb6648da312530e4927426b46c9373f3b37a600981779b7a74f728db8c37e`.
- OFF fallback WASM SHA-256: `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1`.
- CLI/SDK 2.2.5, WASI SDK 33; native engine hash `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`.
- All four owner combinations compile; 28 full integration games on four previously used development maps had zero faults and 56 roster checks passed. Combined scored 3/8 in that tiny regression: this is not strength evidence.
- Agent 1 reported 6,745 full jobs and ultimately selected baseline behavior in its own experiments. Those results do not validate the separately integrated owner combination. Its final holdouts are already consumed.

The integrated package enables Agent 2's fuel rewrites/governor, 4-unit snapshot/authority and imported flee-hit, plus Agent 4's unknown-map lineups. Imported flee-hit is named A2_FLEE_HIT to distinguish the lead's NOVA_FLEE_HIT. Source includes an immutable lead fallback and a reconciled owner implementation selected consistently across translation units. Read the package README/RECONCILIATION only when modifying this mechanism; do not rerun its historical investigation from scratch.

## Available local tools and evidence (verify compatibility)

- Old isolated runtime: `work/recovery/.venv/Scripts/{python,cambc}.exe`.
- SDK workspace: `work/recovery/eval`; WASI SDK: `C:/Users/omdha/AppData/Local/cambc/wasi-sdk-33`.
- Combined ready files: `work/integrated-v36-20261020/`.
- Agent 3 historical verification: PR #12, branch `codex/v36-recovery-validation`; local evidence under `analysis/v36/recovery/` and `work/recovery/`.
- Official development maps, replay tools, manifests and consumed schedules are in the lead branch. Some historical calibrated opponents/harnesses remain on another host; distinguish unavailable artifacts from proxy opponents.
- A newer CLI was reported, but its installed location/version was not found in this chat. Check the balance agent's actual toolchain before installing/upgrading. Do not silently upgrade a pinned environment or mix old/new engine measurements.

## Operating rules

Use one implementation agent by default; no spawned agents without explicit authorization. Read REQUIREMENTS.md and STATUS.md next. Keep updates short and milestone-driven. Follow the efficiency rules there. Task-related repository work and draft PRs are authorized; no v37 live-platform submission is authorized by this handoff. Do not treat the earlier v36-main merge request as an instruction to merge every future v37 experiment.
