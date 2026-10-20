# Reproduction commands

Run in the isolated repository root. Use a real Python executable rather than the Windows Store alias. This host's bundled executable is `C:/Users/ishan/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`; substitute your Python in the commands below. Node paths used by decoder/export helpers are recorded in their scripts and may need substitution on another host.

```powershell
python -m pip install --target ../runtime-3.0.2 cambc==3.0.2
../runtime-3.0.2/bin/cambc.exe init --yes v37/workspace
python v37/tools/source_archives.py restore
python v37/tools/archive_evidence.py restore
python v37/tools/audit_baseline.py
python v37/tools/lab.py build NEW_BASE_ID --source v37/baseline
```

The runner takes `V37_CLI` and `V37_LOCALAPPDATA` overrides. Compiler/SDK/native engine identities must match the receipt before old evidence can be reused. Build IDs and schedules are immutable; use a new ID for a deliberate rebuild. Build outputs are in `v37/workspace/bots/NAME/.cambc/build-release/`. Do not rerun source-transform scripts against already transformed trees.

On this exact workspace, existing schedules resume/revalidate without counting cached matches again:

```powershell
python v37/tools/lab.py run base-official-35
python v37/tools/lab.py run econ-link-dev-v1
python v37/tools/lab.py run safe-state-dev-v1
python v37/tools/ledger.py base-official-35 --decode 00rows 00lanes 00corners
python v37/tools/check_oracle.py
python v37/tools/decision_data.py
python v37/tools/train_pilot.py
python v37/tools/check_model_export.py
python v37/tools/package_checkpoint.py
```

Raw schedules/manifests record exact maps, source, settings via source hashes, compiler commands, WASM and runner hash. `lab.freeze` creates a new immutable schedule and `lab.execute` uses two workers. Full matches have no round/fuel overrides. Runner stdout/stderr, native statistics and replay ZIPs are retained per artifact key; incomplete attempts are quarantined.

The official visualiser engine is copied from CAMBC 3.0.2's unpacked `0ba59fac.../visualiser/assets/v1_0_1-OYnUyDob.wasm` to `v37/tools/engine-current.wasm`. Recover it with the pinned CLI before replay/oracle/map tooling, and verify against `release/MANIFEST.json`. `prepare_oracle.py` and the trace/fixture preparation scripts document deliberate reuse of earlier local reference tools; the already prepared files are committed so those older local folders are not needed to inspect them.

`prepare_fresh.py` refuses an existing immutable map manifest. Its checked-in generator and seed registry describe the frozen maps. Do not regenerate over or tune on `maps/confirm` or `maps/final`. No games on either suite have been run. The old balance `paired.py` deletes run directories and lacks strict native build-key acceptance; use the v37 runner instead.
