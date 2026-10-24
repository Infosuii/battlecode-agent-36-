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

No full-length official/unseen matches, release-fuel profile, integrated combat review or performance improvement is claimed here. This is partial completion of the assigned workstream until those dependencies arrive.
