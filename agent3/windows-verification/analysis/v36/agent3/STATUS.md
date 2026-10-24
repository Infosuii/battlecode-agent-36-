# Agent 3 — Windows verification session

Repository: https://github.com/Infosuii/battlecode-agent-36-
Canonical workstream: https://github.com/Infosuii/battlecode-agent-36-/issues/6
Parent: https://github.com/Infosuii/battlecode-agent-36-/issues/3

The user started this session as Agent 3. No child agents were spawned. A separate execution already has an integrated Agent 3 candidate; its source is not present here. This session's files are an isolated, independently tested farming overlay and supplementary evidence. They must be reviewed/adapted against that execution's recorded source hash, never copied wholesale over its bot.cpp.

Completed locally:

- Frozen-source audit and an independently switchable admissible single-group bound.
- Separate mathematical pruning, work-cap, fuel-cap, order-cap, route and fallback counters, disabled by default.
- Actual scorer, camp-helper, first-farm-prefix and route-method unit fixtures: PASS, 18 builds, 24,014 cases each. Pruned/exhaustive byte streams agree, zero-weight identity and disabled-switch parity pass, and exhaustive builds report zero bound violations. See results/scorer_results.json and matching source hashes.
- Frozen archive parity: all 18 source entries match. Review patch applies to a frozen copy and reproduces every owned candidate file byte for byte (results/patch_check.json).
- Documented resource/time-ledger requirements and blocked evidence joins.
- GitHub coordination and a review package; no live platform changes.

Dependencies requested on issue #6:

| Owner | Needed artifact | Status |
|---|---|---|
| Agent 1 | Executable SDK/runner, integrated source ref, central switch registration, named paired schedule | Requested; execution exists on another host, inaccessible locally |
| Agent 2 | runtime_budget.h and hash-based combat/governor patch from that execution | GitHub milestone describes them; source/API not yet supplied |
| Agent 4 | Actual parser semantics, platform identity join, canonical map recognition | Requested; not yet supplied |

Continuation on 20 October: the pinned SDK/toolchain is now recovered. The local release baseline reproduces Agent 2's ECAFAE4C…AE55 hash. The repaired all-switches-off binary is byte-identical to that baseline. A fixed 63-game schedule on nine reconstructed historical development layouts passed (18/18 matched disabled parity; zero faults). Bound ON won 9/18 with 36 exhaustion events versus OFF's 30, so no promotion. Native replay fuel records were decoded against the bundled visualiser format and independently cross-checked against all native exhaustion events. Two controlled native-engine camp/recall fixtures passed 18 assertions. See ../recovery/VALIDATION.md and its exact evidence.

This is still partial completion: no current full official/unseen/varied-opponent validation, combined other-host code review or demonstrated performance improvement. During final publication Agent 2/4 source branches appeared and were fetched; see ../recovery/PUBLICATION.md for exact commits. Agent 1's execution tree, calibrated pool and combined contract review remain outstanding. Balance updates remain excluded.
