# Farming scorer and economy review

## Bound derivation

For one eligible group, let `R = gems + gold_weight() * gold`. Let `A` be the same nonnegative approach cost the scorer uses, `W` the same already weighted/truncated respawn wait, and `C = capture_time + 4`. The actual denominator is `P * (A + W + kill) + C + drain`, where `P >= 1`, `kill >= 0`, and `drain >= 0`. Therefore its denominator is at least `A + W + C`.

The upper reward applies the following factors in the actual scorer's order, using `max(1, factor)`:

- `1 + tune::kBootBonus` when the camp is harmless and the actual item count is below `APEX_BOOT_ITEMS`. This is a live runtime value, not the initial .3329 constant.
- `NOVA_BIGJOB_MULT` for a dominant group at or after `NOVA_BIGJOB_FROM`.
- `1 + tune::kAltarBonus` for the same altar readiness/range condition.
- `1 + tune::kNumbersBonus` for the same living-number and enemy-territory condition.
- `1 + abs(NOVA_SEED_EPS)` when jitter is enabled; this bounds the actual deterministic jitter in [-1,1].

Divide by the lower denominator, then apply `max(1, stickiness)` only for the current target and `max(1, learner::hi)` only when the residual is enabled with a nonzero used weight. Stickiness below 1 is therefore supported. The maximum learner multiplier is taken from its actual header, not duplicated as a second clamp constant. An all-zero used weight vector has an exact bound of 1: using the loose 2x clamp for an identity model admitted extra candidates, consumed the unchanged work cap and changed production choices in an initial failed fixture. This interaction was corrected before accepting results.

The other actual terms are discounts: incumbent/small-job .15, recent enemy .6, nonnegative territory <=1, cohesion and buddy denominators >=1, initiating .7, siege .8, and belief denominator >=1. Dropping these can only raise a nonnegative score. Negative territory or invalid/nonfinite factors invalidate the bound and cause that candidate to bypass pruning. The helper checks overflow and uses a small upward floating-point rounding guard. No infinity/NaN sentinel is compared to the threshold.

Assumptions are normal engine nonnegative rewards/health/camp damage, positive DPS for eligible combat jobs, nonnegative weighted wait, and the scorer's existing nonnegative recovery costs. Path inputs and the final score are exercised through the production method; the fixture's helper observations are explicit stubs, not engine predictions.

The threshold is unchanged: `best_score` for single-group choice, or `max(kth_score, best_score * route_slack)` for the existing route heuristic. The proof is for a single group's score only. In diagnostic single-group mode the threshold is just the incumbent best score. Route-global optimality does not follow.

## Correctness evidence

`fixtures/run_scorer_checks.py` extracts the actual candidate and frozen `score_groups` bodies and compiles them with deterministic supplied observation/eligibility inputs using GCC C++20. Each run freezes its sources, explicitly sequences random draws, writes exact `(group, need, float score)` decision streams, and compares their bytes. Disabled candidate decisions and eligibility side effects are also asserted against the original method inside the executable.

The schedule contains 24,014 fixtures per variant: 24,000 randomized multi-group scenes plus fixed boundary/threshold/order-cap checks. It crosses reward/alive/respawn states, runtime boots, item boundary, round 59/60/61, inclusive dominance, claim staffing and participants, recovery costs, optional altar/numbers, deterministic jitter, territory/cohesion/buddy discounts, belief discount and stickiness below/above 1. A dense 301-group fixture exercises the order-cap distinction.

The runner checks learner disabled, a nonzero 16-feature residual, and an enabled zero-weight residual. It compares pruned vs exhaustive single-group choices, diagnostics on/off, zero-weight identity, and disabled-switch parity. Bound violations are checked on each actual final score in exhaustive mode. Exact counts, compiler identity, input/executable hashes and three host timings per variant are in `results/scorer_results.json`.

The fixed dominant-group regression chooses group 0 under the old bound and group 1 under corrected/exhaustive scoring. This demonstrates a real choice error in a controlled state. It is not a competitive gain estimate.

The route fixture extracts the production route method. Of two supplied first legs, its restricted set chooses group 0 and its expanded set chooses group 1. The single scores are exact; the follow-up route makes the lower-ranked first leg preferable. This isolates route candidate loss from mathematical single-group pruning. It is a counterexample, not an estimate of how often this occurs in matches.

Host timings include fixture generation/copies as well as scoring; they are a coarse diagnostic-overhead screen. They are not Wasm fuel, release timing or a reliable performance comparison. Real release-fuel/disabled-layout parity remains an Agent 1/2 dependency.

## Camp and action fixtures

Actual production helpers verify that a closer tank suppresses current incoming damage, departure restores it, equal-distance aggro follows unit ID, and the configured camp range is honored. A healthy member of the selected awakened group remains in its safety calculation.

The unchanged soak model returns 7,800 for thirteen identical live camps (100 health, 10 damage, 10 team DPS). An uncapped sum is 9,100: the thirteenth camp is omitted. The existing experimental nearby-safety block reports 32 for 33 nearby awake camps whose actual damage sums to 132; the last camp with 100 damage is omitted. These are controlled dense scenes; occurrence and strategic cost on actual maps remain unmeasured. `NOVA_CAMP_SAFE` stays at its baseline default.

Sleep transitions, splash-awakened neighbors, actual helper arrival, linked-partner risk, boxed-in respawns and recall placement require exact awake-state and action semantics from the SDK/engine. A damaged flag alone does not prove that a group is still awake after the two-round sleep timeout. No unsupported replacement predictor was introduced.

The actual first-farm prefix fixture checks no target, retained target and avoided target when `field_calls == 0`. It supports the source trace: no target returns before score/route work. A real first-turn trace remains outstanding. Existing shopping/capture/banking code is unchanged and still needs the requested Tavi, emergency recall, interrupted recall, full heal and no-loop scenarios in the integrated runner.

## Economy ledger and policies

See `LOSS_LEDGER.md`. Input identities and parser semantics have not been joined on this host, so supplied sample incidents are not re-attributed and no farming-race policy is selected. Rows/lanes/corners names are hypotheses, not sufficient map-recognition or policy evidence. No map-specific policy, broad recall threshold or production learner is enabled.

## Integration and recommendation

Use canonical issue #6. The other execution reports an integrated candidate SHA-256 `578b4cae6f890073ae58e1e191750cf3446ae66c886a828a6f1d4ec98cd37924` on GitHub; that file has not been supplied for direct review. This local overlay must not replace it wholesale. Agent 2's governor/flee-hit changes are not claimed as locally integrated, and its runtime reserve is not duplicated privately.

Recommendation: review the bound repair, keep every local switch OFF in the retained baseline, and run the declared integrated parity/fuel and paired official/unseen gates before enabling it. No measured win-rate, submitted-gem improvement, final holdout result or ready platform release is claimed.
