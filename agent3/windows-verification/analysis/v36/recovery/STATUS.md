# Current recovery checkpoint — 20 October 2026

**The earlier paused checkpoint below is superseded by [VALIDATION.md](VALIDATION.md) and [NEXT.md](NEXT.md).** Work subsequently continued at the user's request: release builds, 63 accepted full development matches on nine reconstructed historical layouts, verified native fuel streams, and 18 native camp/recall assertions are complete. This does not establish a strength improvement or complete the whole task.

Disabled release WASM is byte-identical to the frozen baseline. Bound ON won 9/18 paired games and incurred 36 native exhaustion events versus OFF's 30. All experimental switches remain OFF.

Publication branch: `codex/v36-recovery-validation`, extending draft PR #9. Raw evidence/artifacts are preserved in `match_evidence.zip` with a SHA-256 index. Exact owner-source integration, broader independent validation, opening/shopping tuning and full policy safety remain incomplete. Balance-update tuning was excluded. No live submission changed and no background job is running.

## Historical paused checkpoint (superseded)

User requested all remaining recovery/integration/validation except balance-update tuning. On discovering unpublished dependencies, user asked to check GitHub and leave the work until tomorrow if they remained missing. Work stopped at that point; no background jobs or automatic follow-up were scheduled.

Final GitHub check: only main, codex/agent3-farming-verification, codex/v36-master-prompts and copilot/ready-repo-for-agents were listed. PRs were #1, #2 and draft #9. Agent 1's work/v36-execution and Agent 2's latest a2_ sources/results were not published as an accessible branch/PR. Their milestone descriptions remain in issues #4/#5/#6. Requests to publish exact source/configs/results were posted in issues #3/#5. Agent 3's verification source/evidence remains safely pushed in PR #9.

Recovered locally:

- Project-local environment: work/recovery/.venv, Python 3.13.15, cambc==2.2.5 installed with uv. Use its Scripts/cambc.exe directly; venv ensurepip failed but uv installation succeeded.
- Isolated evaluation workspace: work/recovery/eval, cambc.toml pins SDK 2.2.5.
- Official init downloaded WASI SDK 33.0 (Clang 22.1.0), CMake 3.31.8, Ninja 1.12.1 and the matching SDK. Starter configurations were generated. No candidate/baseline full release builds or games were run in this recovery turn.
- Maps directory is empty. Platform map refresh returned HTTP 303; current maps page requires login. Do not claim recovery of the 17-map official suite. Local historical replays contain map data, but no extraction was performed.
- Old analytics resources exist under C:/projects/cambridge battlecode analytics; provenance/compatibility must be checked before reuse. Do not silently equate that engine with the pinned runner.
- Local review checkout branch: codex/v36-recovery-validation, created from 4f4bbe25238630f23b1047db614fb206779def6c. No recovery changes pushed yet.

Next dependency: publish or provide Agent 1's full execution folder and Agent 2's latest source/owner patches/build settings/pool outputs, including consumed holdout schedules. Resume by preserving/hashing those artifacts, reconciling baseline wasm hashes, and completing the existing validation protocol. Balance-update tuning remains excluded. Frozen baseline and live platform submission were not changed.
