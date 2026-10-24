# Agent 3: farming release and native-engine verification

Review supplement for [workstream #6](https://github.com/Infosuii/battlecode-agent-36-/issues/6), produced by the user's separate Windows Agent 3 session. This namespaced package does not replace the active execution's integrated bot tree.

See [farming report](analysis/v36/agent3/FARMING_REPORT.md), [status](analysis/v36/agent3/STATUS.md), [reproduction instructions](analysis/v36/agent3/README.md), and [exact source manifest](analysis/v36/agent3/results/SOURCE_MANIFEST.json).

- All 18 archived baseline entries are preserved by SHA-256.
- 18 fixture variants passed 24,014 cases per variant; exact pruned/exhaustive choices, disabled-switch parity and zero-weight identity pass; no bound violations.
- Actual camp/route methods reproduce separate dense-camp truncation and route-candidate loss.
- Disabled release WASM is byte-identical to the frozen baseline, with all experimental switches OFF.
- 63 accepted full development games on nine reconstructed historical layouts completed, with verified native fuel streams and zero native bot faults.
- Bound ON won 9/18 paired games and had 36 exhaustion events versus OFF's 30. Improvement is unproven; keep it OFF.
- Two native camp/recall fixtures passed 18 assertions. Complete policy safety and exact other-agent integration remain incomplete.

See [current validation](analysis/v36/recovery/VALIDATION.md), [resume instructions](analysis/v36/recovery/NEXT.md), and the raw evidence archive and SHA-256 index in that directory. This extends the fixture-only PR #9 checkpoint. Balance-update tuning and live platform changes were excluded.

The evidence ZIP extracts paths relative to this package root. Run `work/recovery/ops/analyze_suite.py` and `assert_native_probes.py` against the extracted evidence to audit it. Scripts use Python 3.13; native rebuilds require CAMBC 2.2.5 and its pinned toolchain, initialized in `work/recovery/eval` with `cambc init --yes`. Create `work/recovery/.venv` and install `cambc==2.2.5`; the runtime/cache is intentionally excluded. `prepare.py --variant-suffix _v3` reconstructs baseline/off/on/diagnostic source projects. Map files are included, so map generation via the historical external analytics installation is optional and requires that separately recorded dependency. The generated probe C++ sources can be reconstructed directly from the included `native_camp_probe.cpp` and the three mode values recorded by `prepare_native_probes.py`.

`bot_v36/src` contains only the owned overlay files; other files inherit the frozen baseline for source-manifest purposes. `results/agent3_overlay.patch` is an integration review artifact, not permission to overwrite the other execution's updated bot.cpp. Preserve line endings when applying it (`git -c core.autocrlf=false apply`) to reproduce source hashes.
