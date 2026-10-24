# Compiled v36 release

Independent rebuild of Agent 1 PR #13 at f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a. The compiled WASM is byte-identical to the published final release. All 22 source ZIP files match the compiled source. One full native match completed with zero bot faults. This checks execution, not competitive improvement.

- `bot_v36.wasm`: compiled local-runner artifact.
- `bot_v36.zip`: unchanged legal source bundle for the platform.
- `BUILD_MANIFEST.json`: source hashes, full build command, compiler identity and source defaults.
- `VERIFICATION.json` and `smoke-rows/`: verification and raw execution evidence.

CAMBC/SDK 2.2.5, WASI SDK 33, release -O3. Non-executable metadata normalized using the published llvm-strip step. All new policy switches remain OFF. No balance update or live submission was performed. This is a validated baseline fallback, not a demonstrated improvement or an integration of every separate agent branch.
