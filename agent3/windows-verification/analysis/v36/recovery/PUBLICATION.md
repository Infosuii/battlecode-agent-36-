# Publication and newly available dependencies

## Later continuation: Agent 1 published and compiled independently

Agent 1's completed local release is now in PR #13, branch `v36/validated-results-20261020` at `f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a`. This supersedes the earlier missing-Agent-1 dependency below. Its final fallback was independently rebuilt on this host: WASM `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1` exactly matches the published artifact. All 22 source-bundle files match its compiled source; a full native match completed with zero bot faults. Rebuild inputs/output live under `work/recovery/compile-agent1`; easy-to-use output is `work/compiled-v36-20261020`. A review copy and actual build/run evidence are in `compiled-release-20261020/` beside this note.

The user deferred balance changes and asked to compile first. No balance change, live submission or silent merge of separate owner branches was performed. Compiling Agent 1's final fallback does not establish that its experiments evaluated the later independent Agent 2/4 branches. That reconciliation and external validation remain separate work.

## Earlier publication checkpoint (superseded where noted above)

Agent 3 checkpoint: draft PR https://github.com/Infosuii/battlecode-agent-36-/pull/12, initial checkpoint commit `536bf6da270d9aa5ae56358803c704492e2902ef`, branch `codex/v36-recovery-validation`. It extends fixture-only PR #9. The disabled baseline parity, 63-game evidence, native fuel and camp results are saved; the overall engineering task remains incomplete.

A final remote check after that push found new owner branches, fetched locally without merging:

- Agent 2: `agent2/windows-runtime-combat` at `ea57965d6858196a0da3769a55be54b1577f36ba`. Contains a full candidate, owner patches including `agent3_bot.patch`, runtime header, interfaces, findings, experiment notes and build table. README still depends on an external bench/pool harness and seeded opponents. The snapshot is now available for review; it has not been integrated or independently validated by this Agent 3 checkpoint.
- Agent 4: `agent4/draft-data` at `c392f0f9a424e8c88124ae059587c033870b614b`. Contains candidate/baseline trees, draft/data reports, experiment result JSONL, generator/tools and release manifests/ZIP. Report explicitly leaves replay intelligence and farming ML incomplete. Its draft performance claims have not been independently audited here, and consumed schedules must be respected during integration.
- Agent 1: no execution branch appeared in the final remote-head inventory. Its exact execution tree and remaining validation screen are still needed.

This supersedes earlier statements that Agent 2/4 sources were unavailable. Resume with the pinned owner snapshots before further parameter tuning; review overlapping bot/world/strategy changes and preserve disabled release parity. Do not merge or promote any feature solely from its owner's summary. No background job or balance-update tuning is active.
