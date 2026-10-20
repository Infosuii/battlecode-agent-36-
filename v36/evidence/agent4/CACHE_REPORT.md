# E04b exact identity cache: implementation and native gate

The expensive physical-layout equality check remains mandatory in the team draft module. With both `NOVA_STRICT_MAP_ID=1` and `NOVA_STRICT_MAP_CACHE=1`, runtime champion modules read a versioned record from their own immutable team draft-memory snapshot instead of repeating all camp/terrain queries. Missing, corrupt, unsupported or context-mismatched data returns unknown without an expensive runtime fallback. Existing strict-without-cache behavior is preserved; both new switches default off. `APEX_BRAIN=1` is required for the existing writer to run; it is enabled in the tested baseline configuration.

Agent 3 remains the sole draft-memory writer in strategy.h. Existing bytes0..5 and their meanings remain unchanged. For capacity>=16, the writer submits the full16-byte record on every draft update. If the helper fails, it preserves the prefix and explicitly zeroes extension6..15 before submitting all16 bytes. This is necessary because the SDK retains old tail bytes on short writes. For lower capacities, the original4/5/6-byte branches remain and runtime cannot accept a16-byte cache. The observed real SDK capacity and startup snapshot length are both1024 bytes.

Format v1:

| Bytes | Meaning |
| --- | --- |
| 0..5 | Existing strategy prefix: magic0xA5, actual picks, enemy casters, class, max group, flags |
| 6,7 | Cache magic0xD6, version1 |
| 8 | Canonical ID+1;0 means verified unknown,1..17 means known ID0..16 |
| 9..11 | Actual draft picks, width, height |
| 12..13 | Camp-group count, little endian |
| 14,15 | CRC8(poly0x07) over bytes0..13, terminator0x6B |

Widths/heights1..255, picks3..5 and group counts0..65535 are checked before encoding; values are never truncated into a valid record. Runtime verifies prefix magic/size, schema, checksum, complete context and known-ID agreement with the cheap descriptor. CRC detects accidental corruption; it is not authentication against a party able to forge an entire record. The trust boundary is the same team's own draft-only exact validator and immutable engine-delivered memory. A complete, deliberately forged record is outside that accidental-corruption claim.

Production source freeze: draft.cpp SHA73527394a0f8ea5081d7691e7d351307d4ace1ca0c9f3b4a658a1b3946f3903d; draft.h SHAac6b7cc136ceea0a6528e82a4f3a4bb6b85d5d23c56f811b5b7212f15cdeb455; codec SHA6211b9cdb09896f93b69c486381ebab379795e930dc258ed503fb717a7a38300; Agent3 writer strategy.h SHA6aa5144bd5f659932ca8f354ad1e52dc7fad74170db0bd922b14adf9d9ce55fc. Corrected physical-layout header remains SHA3e84b617d5d38f92260ccbb6ee46e8c0937260abc75b461ad97cd3c87edeccf0.

Native gate: `cache_identity_fixtures/draft_module_assertions.json` checks17 known layouts and3 same-descriptor altered layouts on both teams. All146 official picks and24 fallback picks are correct across actual draft sizes3/4/5; all210 physical bodies, including C&I's second body, receive correct runtime identity. Draft/body rosters are independently decoded from raw engine replays, not inferred from diagnostic labels. The probe artifact is SHAf4ddd86d78789d6f1a51db4c1072da6f62c0e1344532224187ef2198e52b46dd.

Fault gate: a separate immutable copy-only fixture injects CRC corruption(rows), absent extension(collision), unsupported version with a correct CRC(expanse), and a wrong known ID with a correct CRC(stampede). All38 runtime modules fail closed while all30 actual draft picks remain the exact draft policy. Injection code exists only in that fixture's copied strategy.h, never in the production source. Evidence: `cache_fault_fixtures/draft_module_assertions.json`; fixture artifact SHA84b3a1f2b34dc4542fcf7c1d3a77733eda404f9c684de787dc9dd1a91d704e35.

Compiled-codec format properties pass54 valid records,6,912 single-bit corruption rejections,864 truncated-record rejections,216 changed-live-context rejections, unsupported schemas with a recomputed CRC, prefix preservation and failed-encode nonmutation. See `cache_codec/report.json`. Agent3 independently exercises the actual writer across capacities and valid-to-helper-failure transitions; those110 checks are separate from these codec properties and the actual SDK scenarios.

Matched fuel probes use identical source files and printing, differing only in the CACHE switch. On180 known-map body modules, mean first-invocation fuel is83,861 uncached versus19,772 cached; maxima144,723 versus20,834, a mean reduction64,089(76.4%). `cache_first_module_fuel.json` contains every side/body value and artifact/source identities. These are total charged draft-only idle-gameplay invocations, including SDK initialization and diagnostics. They establish reduced initialization workload, not a complete-bot strength gain, a full-game reserve guarantee, or the final full-bot exhaustion rate.

All native scenarios explicitly stop after two rounds and are correctness fixtures, never full-match performance observations. Root-owned same-source OFF parity, complete-bot official/synthetic/pool tests, once-only fresh selection and exact final release evaluation remain separate gates. No final holdout data was read during cache development.
