# Reconciled Agent 2 and Agent 4 candidate

This package integrates the already-published owner implementations into Agent 1's final source. Scope is integration, compilation and a small runtime regression. No new owner research, ML training, parameter search, balance pass or platform submission was performed.

## Pinned inputs

- Lead: `f2617f7d9e9d3e67a9ba9a52c2a944bc67c9616a`, final release source from PR #13.
- Agent 2: `f34268b6a3e563590781822400b4e3184881f86c`, `agent2/windows-runtime-combat/candidate/src`.
- Agent 4: `c392f0f9a424e8c88124ae059587c033870b614b`, draft changes from `bot_v36/src/draft.cpp`.

`RECONCILIATION.json` records input hashes, overlaps and adaptations. Both budget interfaces are retained. The imported Agent 2 flee-hit switch is renamed `A2_FLEE_HIT`, avoiding accidental double execution with the lead's independent `NOVA_FLEE_HIT`. Snapshot/authority selectors reject an ambiguous combination of the two implementations. The lead's physical identity/cache path is retained alongside Agent 4's draft policy.

The initial flat merge compiled but changed the OFF artifact; it was rejected as a fallback. `source/src/lead/` is an immutable copy of the lead translation units and `source/src/owners/` is the reconciled implementation. Top-level .cpp files choose the complete source family consistently. Internal C++ bodies use .inc so the SDK's recursive source discovery compiles each translation unit once. This duplicates the frozen fallback deliberately to preserve exact disabled code generation. Future edits belong in the reconciled owner path, with the frozen lead copy preserved for rollback.

## Ready-to-use files

- `bot_v36_owners_combined.zip`: official CLI source bundle, 55 files. Source defaults enable Agent 2's recommended fuel/combat switches and Agent 4's draft switch. No compiler-only setting is required to activate them.
- `bot_v36_owners_combined.wasm`: exact compiled combined artifact, SHA-256 `e82bb6648da312530e4927426b46c9373f3b37a600981779b7a74f728db8c37e`.
- `MANIFEST.json`: source/bundle identities, compiler command, switches and normalization evidence.
- `OWNER_BUILDS.json`: hashes for OFF, Agent 2, Agent 4 and the combination.
- `integration_checks.zip` and `RAW_MANIFEST.json`: exact inputs, statistics, replay bundles, schedules and logs with checksums.

## Integration checks and limits

All four profiles compiled. All owner switches OFF reproduces the published lead WASM `2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1` byte for byte. Materialized combined source defaults reproduce the experimental combined build exactly; every file in the official CLI ZIP matches that source.

28 full native release matches across four already-used development layouts completed with zero bot faults. All 56 team-roster assertions passed: official rows/lanes drafts remain baseline; the tested unknown 3-pick draft changes Varg to Ren, and the unknown 4-pick draft becomes C&I/Ren/Ryth/Varg on either side. The combined variant executes these picks with Agent 2's imported switches active. These are integration checks, not an independent strength screen. The combined variant scored 3/8 wins on this tiny development set; it is not promoted as a performance improvement. Preserve Agent 1's existing validated fallback while broader combined performance is assessed under the team's chosen protocol.

This run used CLI/SDK 2.2.5, WASI SDK 33 and native engine hash `d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5`. A new CLI was reported during integration, but none was found on this session's PATH; no upgrade or engine substitution was made. New-CLI compatibility/balance work remains separate at the user's instruction.

## Rebuild one profile

Set `V36_PROJECT` to a CAMBC SDK 2.2.5 workspace, `V36_CLI` to its CLI executable, and `V36_SDK` to WASI SDK 33. Then run `python tools/rebuild.py --profile combined --name independent-combined`. Other profiles are `off`, `a2`, and `a4`. Use a new build name; existing manifests are immutable. The script compiles source defaults with explicit profile overrides, strips non-executable metadata using the pinned SDK and requires the recorded exact artifact hash. Python 3.13 was used here. Build/manifests retain actual local paths as provenance, not portable path defaults.
