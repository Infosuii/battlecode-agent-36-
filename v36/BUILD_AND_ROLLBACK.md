# Build and rollback

The finished package retains baseline behavior with every new policy switch OFF. No candidate demonstrated a supported upgrade through fresh validation. Final hashes and results are in `CANDIDATE_MANIFEST.json` and `VALIDATION_v36.md`.

## Reproduce a finished release

From this execution directory, use Python3.12 and the recovered SDK/CLI2.2.5 with WASI SDK33. Default runtime paths are recorded in `tools/evaluate.py`. Alternate installations can be specified through `V36_WORK_ROOT`, `V36_PROJECT`, `V36_CLI`, and `V36_SDK`. `V36_PROJECT` must contain the pinned C++ SDK and `cambc.toml`; `V36_SDK` is the WASI SDK directory. Windows is exercised; the Linux compiler-path adapter is untested.

After `CANDIDATE_MANIFEST.json` exists:

```powershell
python tools/rebuild_release.py independently-rebuilt-release
```

Use a new build ID; existing builds are immutable. The entry point copies `release/bot_v36/src`, compiles its materialized source defaults without experimental `-D` overrides, checks every selected macro, normalizes non-executable debug/name metadata with the pinned SDK llvm-strip, and requires the exact final WASM hash. Full compiler/normalization commands, SDK hash and tool versions are in `builds/release-final/manifest.json`. No CAMBC_DEBUG flag is set; SDK debug blocks are excluded. The package was additionally re-extracted and independently rebuilt byte-for-byte by `tools/package_release.py` before final evaluation. The initial unnormalized packaging attempts are retained as failed reproducibility experiments; their standard executable sections are identical.

`release/bot_v36.zip` is the official CLI source bundle. `release/bot_v36.wasm` is the exact locally evaluated compiled artifact. Selecting values only through local compiler overrides would leave a different source upload; selected defaults are therefore written into the packaged `common.h`. Original baseline tuning macros are preserved. Diagnostic and exhaustive-work flags remain OFF.

## Replay evaluation

Frozen schedule IDs are indexed in `EXPERIMENT_INDEX.json`. To resume a named schedule using three workers:

```powershell
python tools/evaluate.py run release-final-head --workers 3
```

The runner verifies artifact and map hashes. Existing completed jobs require their recorded statistics checksum and replay presence; partial output is quarantined. Repeated deterministic jobs reuse existing evidence and do not increase independent sample size. Do not rerun fresh selection or tune from final maps after a final failure.

## Restore the supplied baseline

The rollback directory contains the original unchanged source ZIP, all18 extracted source files, original source manifest and baseline WASM. The authoritative baseline artifact hash is `70139ee190dc1e8a46338c78b54a1ee4b36d2459cb7944d6f2741d7d1432a757`; supplied ZIP hash is `e6a95834df2cd14b0719d107309000d4f38724b436e1f617b7db5e85fddfa605`.

Use `release/rollback/bot_v34_original.zip` for the original source upload or `release/rollback/baseline.wasm` where compiled input is accepted. No platform submission is performed by these scripts. Local v36 names do not identify a live platform version or submission window.
