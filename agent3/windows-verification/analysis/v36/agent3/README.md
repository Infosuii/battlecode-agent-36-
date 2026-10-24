# Agent 3 supplementary farming verification

This package belongs to canonical issue #6 in Infosuii/battlecode-agent-36-. It was produced in the user's separately started Windows Agent 3 session. It is an isolated review overlay and correctness evidence, not a replacement for the other execution's integrated candidate, runtime governor, event parser or release evidence.

Read FARMING_REPORT.md for the derivation/results/limits; STATUS.md for dependencies; LOSS_LEDGER.md for unavailable resource/time evidence; DECISIONS.md and EXPERIMENTS.json for policy choices and gates.

Reproduce from a package root containing `bot_v34/src`, the owned overlay files in `bot_v36/src`, and this `analysis/v36/agent3` directory:

```powershell
python analysis/v36/agent3/fixtures/run_scorer_checks.py --compiler C:/msys64/ucrt64/bin/g++.exe
python analysis/v36/agent3/fixtures/record_evidence.py
```

A working Python 3 runtime and C++20 compiler are sufficient for these unit fixtures. The bot itself still needs its actual competition SDK/toolchain; the fixture stubs do not replace those dependencies. The frozen baseline and overlay are included in a namespaced review package so tests can be reproduced without copying files into the active bot tree.

All four local source switches default OFF. The script compiles its explicitly recorded fixture variants without changing the candidate files. Generated binaries/build directories are excluded from Git; exact hashes, compact output logs, and the final JSON report remain review evidence. `SOURCE_MANIFEST.json` defines its canonical source-manifest digest as SHA-256 of sorted UTF-8 `filename sha256\n` records.

For integration, inspect `results/agent3_overlay.patch` against the integrated source's recorded SHA-256. Agent 3 remains the sole writer of bot.cpp/.h. Do not apply the entire overlay over an updated file, enable the diagnostic uncapped mode in competition, or promote a policy on unit-fixture evidence alone.
