# Decisions

- Preserve supplied archive and frozen source, use hashes for every build; local v36 is not platform v36.
- Use recovered current SDK/CLI 2.2.5, recording compatibility and exact engine hash. Do not rerun the old package as if it were current.
- Every new policy switch defaults off. Existing baseline macros remain frozen.
- Three shared full-match workers; no match-seed setting exists in inspected CLI. Geometry seeds do not create independent reruns of the same map.
- Synthetic screen uses 200 newly generated maps, paired sides (400 games); call them new synthetic maps, never historical maps_rdev. Freeze separate fresh validation and final holdout geometry before selection.
- Historical named calibrated opponent artifacts have not been recovered. Substitutes retain true artifact names and cannot satisfy that exact gate.
- CLI is signed out; do not infer platform submission identity from replay fingerprints or local names. No platform publication/challenges authorized by build request.

## Completed development decisions

All results use full matches, fixed artifact/map hashes, both sides and the official native engine. Screens are 200 generated maps/400 games, not 400 independent maps. Intervals below resample map clusters conditional on this generator and opponent.

| Change | Screen wins | Decision and reason |
|---|---:|---|
| Correct scorer bound | 208/400 (52%; 95% interval approximately48–56.25%) | OFF. Mathematical repair passed fixtures, but observed exhaustion increased1389 vs1225; no supported strength gain. |
| Flee-hit | 193/400 | OFF. No screen or official-suite improvement. |
| Simulator fixes | 191/400 | OFF. Some mechanics independently verified, but no screen gain; dash/spawn/order equivalence incomplete. |
| Threat relevance, five units | 210/400 | OFF. Interval includes no gain and official-suite result15/34. |
| Explicit camp awake state, revision2 | 203/400 | OFF. Lifecycle fixtures pass; no competitive improvement established. |
| Runtime governor | 220/400 (55%; interval50.75–59%) | Shortlist only. Exhaustion951 vs1202, but local proxy-pool82/136 vsbaseline88/136. Matched effect−4.41pp, interval−17.65 to+9.56pp. |
| Correct physical map identity, uncached | 200/400 | Reject runtime implementation for release. Native correctness passes, but repeated full comparisons add first-controller fuel; official exhaustion120 vs81 paired OFF opponents (baseline self-play separately100 across34 team-games). |
| Rows capture intent with uncached identity | 201/400 | Mechanism shortlist only. First collection32 vs40/42 in one paired development map; local pool86/136 vsbaseline88/136. Cached same-source foundation must be validated. |
| Optional draft/farming ML | Not enabled | OFF. Historical version attribution and independent policy-gain evidence are insufficient. |

## Final one-time selection

E04b retains full exact physical validation during draft and publishes its result through a checked own-team record. Capacity>=16 failures invalidate the entire extension; short writes must not retain an old valid tail. Missing/corrupt records fail closed. Cache codec and actual writer fixtures precede native probes, same-source screens and fresh validation.

`tools/fresh_protocol.py` supersedes the preliminary fresh script before any fresh games. It freezes artifact/map/schedule identities and numeric acceptance rules. Selection is once among baseline behavior, cached rows fix, and cached rows plus governor; the final holdout remains untouched until the exact release source/archive is frozen. Nonnegative point criteria are selection gates, not proof of significance.

The760-game fresh selection is complete. Rows-only loses one net pool win against baseline (118vs119/160) and−5.2 mean gem-margin effect; rows+governor loses five (114vs119/160) and its baseline head28–52. Neither satisfies the fixed rules. Select cache-source-off and materialize all new policy switches OFF. The normalized exact release2f73d773... reproduces from its source ZIP and matches baseline on34 official raw replays,120 final primary/shift head games and all320 final matched pool strata. Conditional equality is not a universal strength claim. No policy was retuned from final results.

The user explicitly authorized publication of code/results on20 October2026, resolving the earlier automatic-approval publication blocker. No cloud continuation, live submission or scheduled monitoring is active. GitHub publication identity is recorded separately.

