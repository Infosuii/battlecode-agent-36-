# v37 checkpoint

- User now wants a detailed prompt for another agent, **not execution here**. See [EXECUTION_PROMPT.md](EXECUTION_PROMPT.md) for the complete transfer prompt, all workstreams and verified balance evidence.
- Finished balance evidence found at `a020248d8b13593bdf4c70460116f2bb061860b4`, experiments `64c5b73f5c67c0a2b5beb6f9370a70aa3f688f97`, merged PR #11 (`b8dce2f4a3f6ed3c0cdc1e2d263833b9a31bcdca`). Report: CAMBC 3.0.2 / engine 1.0.1; no tested setting/draft passed, so keep Agent 4's existing v36 ZIP (`5ee75d520caa0c921361ea6c83a6a3b991d05ea8a6a7707226dabc875822b320`).
- This tested source is v34 plus Agent 4 draft changes, distinct from the combined owner package. Historical release manifest still lists SDK 2.2.5 / engine 0.0.3; actual post-patch engine/compiler/WASM identities must be recovered/reproduced by the receiving agent. Source/report retrieved via GitHub; ZIP not downloaded or hashed locally here.
- No v37 source copy, feature implementation, ML training or v37 match selection has started.
- Pre-balance reference: main `86a2d09335175c8903167f7de226d27f9559134f`, combined integration in PR #14. Do not assume this is the final balanced baseline.
- Read HANDOFF.md/REQUIREMENTS.md and completed independent runner/replay/fixture inventory: see [INVENTORY.md](INVENTORY.md). Local review checkout is clean at `c700e6a` on `codex/v36-owner-integration`; its package explicitly excludes balance work.
- BASE has identified the selected source/report; toolchain reproduction is outstanding. DATA has inventory/planning only; ECON/ROUTE/SAFE/SIM/ML/STACK have no v37 implementation or validation here. No build or match was run.
- Next is transfer to the user's other agent using EXECUTION_PROMPT.md. No other agent's files were modified; no background monitoring or automatic wakeup is scheduled.
