# Agent 3 handoff

## Latest continuation: 20 October 2026

Publication branch `codex/v36-recovery-validation`, draft PR #12, extends PR #9. See `analysis/v36/recovery/VALIDATION.md`, `NEXT.md` and `PUBLICATION.md` for current evidence, remaining work and newly published Agent 2/4 snapshots. Effective overlay source manifest is now `30a0c5db645a409ee0d12d633d326986d9bf38cae0c7380f286f5c377a68f93f`. Disabled release WASM exactly matches baseline `ecafae4c4571d3a5401d356d03f5c336e6c9d7c5d6dd494af463646622eaae55`.

63 accepted full games on nine developmental historical layouts completed; actual fuel streams cross-check against native exhaustion counters. Bound ON won 9/18 and exhausted 36 times versus OFF's 30, so keep OFF. Two native mechanics fixtures passed 18 camp/recall assertions. These results do not complete integrated safety or prove strength improvement. Missing exact other-agent code, broader independent games and economy/safety policy work remain. No background work or balance tuning is active.

## Earlier published fixture checkpoint (superseded where noted above)

- Draft PR: https://github.com/Infosuii/battlecode-agent-36-/pull/9
- Published commit: `4f4bbe25238630f23b1047db614fb206779def6c`
- Branch: `codex/agent3-farming-verification`
- Canonical coordination: issue #6, parent #3. Duplicate issue #8 was closed after discovering the existing execution.
- Frozen source-manifest SHA-256: `710456b69c06bb16639ac53708252502e5b7aeabd2f3cc8b043b0945a59458aa`
- Effective independent overlay source-manifest SHA-256: `26c1871194bbc84a188f681746ea6a5f740b4a4e19cb178987fb320329a87e98`
- Completed checks: 18 variants × 24,014 cases; exact pruned/exhaustive decisions; disabled-switch and diagnostics parity; enabled zero-weight identity; zero bound violations; baseline/archive 18/18; patch application and byte equality.
- No full engine games or release fuel measurements were run on this host. No policy switch is enabled by default. No platform submission was changed.

Integration must adapt the patch against the active execution's recorded bot.cpp hash rather than replacing its file. Await Agent 1 runner/schedules/central switches, Agent 2 actual governor/combat sources and review, and Agent 4 parser/platform identity/canonical map contract. These dependencies and the exact residual-zero/work-cap interaction were posted to issue #6; ledger dependencies were also posted to issue #7.

The local candidate in bot_v36/src is an owned-file overlay. The self-contained review package is under work/agent3-review/agent3/windows-verification. Source comparison and fixture evidence are under analysis/v36/agent3/results. This workstream remains partially complete until integrated engine and match gates pass.

## User-requested final balance pass

After the current engineering/integration work, use exactly one active agent for the balance-update tuning pass; no delegation or additional tuning agents. The user reports newly released balance changes; their contents have not yet been retrieved or verified here. At that stage, obtain the official changelog and matching SDK/engine, rebuild the unchanged baseline and integrated candidate under the new version, inspect affected draft/combat/economy assumptions, then tune only justified parameters and validate with fresh paired full matches. Keep old-version results separate from new-version results. Record exact engine, source and artifact identities. This is a workflow requirement, not a scheduled/background job or a claim that tuning has begun.
