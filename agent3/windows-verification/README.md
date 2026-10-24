# Agent 3: independent farming verification

Review supplement for [workstream #6](https://github.com/Infosuii/battlecode-agent-36-/issues/6), produced by the user's separate Windows Agent 3 session. This namespaced package does not replace the active execution's integrated bot tree.

See [farming report](analysis/v36/agent3/FARMING_REPORT.md), [status](analysis/v36/agent3/STATUS.md), [reproduction instructions](analysis/v36/agent3/README.md), and [exact source manifest](analysis/v36/agent3/results/SOURCE_MANIFEST.json).

- All 18 archived baseline entries are preserved by SHA-256.
- 18 fixture variants passed 24,014 cases per variant; exact pruned/exhaustive choices, disabled-switch parity and zero-weight identity pass; no bound violations.
- Actual camp/route methods reproduce separate dense-camp truncation and route-candidate loss.
- All experiment switches remain OFF. No engine match, release fuel profile or strength gain is claimed.

`bot_v36/src` contains only the owned overlay files; other files inherit the frozen baseline for source-manifest purposes. `results/agent3_overlay.patch` is an integration review artifact, not permission to overwrite the other execution's updated bot.cpp. Preserve line endings when applying it (`git -c core.autocrlf=false apply`) to reproduce source hashes.
