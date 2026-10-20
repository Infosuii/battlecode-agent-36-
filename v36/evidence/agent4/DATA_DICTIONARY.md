# v36 Agent 4 data contract

Platform `match_id` is a UUID; `trace_id` is SHA-256 of raw replay bytes. They are separate identities. Several platform contexts can reference an identical deterministic replay. Source-tree, compiled-artifact, platform submission ID, and time/version window are four different identities. No version or contemporaneous rating is present in the recovered match-side metadata.

`identity_join_manifest.json` joins every recovered metadata UUID to its group, teams, map, patch and historical loader-certified trace when available. `ingestion_report.json` counts coverage/missingness. Its 17 independent payload checks, one per map, are explicitly separate from historical loader certification of all 2,125 decoded contexts. The read-only database is not copied or modified.

`engine_patch` records version, engine build hash and metric semantics together. The decoder's `engine="0.0.3"`, `schema=1`, and `abi=1` do not replace the engine-byte SHA-256. Historical analytics schema2 is a different dataset and must not be silently mixed with compact schema1. Native statistics integers are often JSON strings; convert explicitly before arithmetic.

`ordered_drafts` preserves actual order. `canonical_lineups` sorts those picks to represent composition. C&I is draft ID3: one pick, two bodies (runtime types3/4). Runtime types beyond C&I are shifted from draft IDs. Whole match groups and groups sharing a replay trace stay together in `historical_split_manifest.json`; each unique trace gets one fit weight.

The current CLI `.repl26a` is a ZIP bundle containing exact named members: `replay.repl26a`, `team-a.stdout`, `team-b.stdout`, `team-a.debug`, `team-b.debug`, `team-a.fuel`, `team-b.fuel`. The historical raw format begins `a62per`. `replay_bundle.py` reads fixed member names and bounds sizes without extracting arbitrary archive paths. Raw replay digest differs from bundle digest.

Compact replay arrays:

| Field | Ordered columns and limits |
| --- | --- |
| `round_states` | round, body ID, side, runtime type, x, y, hp, maxhp, alive, gold, gems, attack range, cd0, cd1, channel type, channel timer, strength, focus, priority, known flags |
| `events` | step, round, kind, actor kind, actor body ID, actor side, from x/y, to x/y, ability slot/type, priority, actor hp/gems, camp HP-loss list, champion HP-loss list |
| `deaths` | round, victim body ID, side, x/y, carried gems, actor kind/ID |
| `purchases` | round, body ID, side, gold before/after, 42 raw item counts |
| `gem_flows` | round, body ID, side, carried-gem delta, transition kind |
| `group_transitions` | round, group index, state before/after, raw capture-side field |

Group state1 means living camps, state2 means camps cleared/reward available, state3 means reward claimed/respawning. The raw capture-side field is neutral2 at both clear and collection in the checked current replay; it must not be used to assign the collector. Native `gemCollection` events with `context=campGroup` and target group provide collection ownership, including zero-gem gold-only captures. Distinct clear, reward collection and completed submission are separate quantities.

Event kind12 starts tribute; kind18 completes it. `first_bank_round` in the old compact metrics measures tribute start. Submitted score curves/native gemSubmission completion are secured-resource evidence. Positive carried-gem flow also includes noncamp sources. Actions/shopping counts are not downtime duration. Body deaths and logical linked incidents need separate denominators; do not double-count shared C&I reward transfers as new team resources.

`known` is an engine field-availability flag, not a controlled champion's vision mask. End-round states are omniscient descriptive samples and cannot establish visible snapshot coverage or decision-time model features. Partial final rounds are retained; censor them for full-round exposure rates. HP deltas omit overkill/absorbed damage and do not alone establish the cause of death.

Native `fuelExhausted` is a bucket per owner; count `len(entries)`, including zero for empty records. `botFault` is a separate diagnostic. Do not count buckets or infer every no-action turn was an exhaustion/suspension.
