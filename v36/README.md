# v36 code, experiments and validated fallback

No tested improvement survived the frozen fresh-validation rules. The finished source/Wasm package therefore retains baseline behavior with all new policy switches OFF. This is a documented fallback and research delivery, not a competitive upgrade.

- [Validation and limitations](VALIDATION_v36.md)
- [Feature decisions](DECISIONS.md), [changelog](CHANGELOG_v36.md), [status](STATUS.md)
- [Source ZIP](release/bot_v36.zip), [exact Wasm](release/bot_v36.wasm), [manifest](CANDIDATE_MANIFEST.json)
- [Original baseline rollback](release/rollback), [build instructions](BUILD_AND_ROLLBACK.md)
- [Final release checks](evidence/FINAL_RELEASE_REVIEW.json), [experiment index](EXPERIMENT_INDEX.json)
- [Frozen fresh selection](RELEASE_SELECTION.json), [full result matrices](results), [schedules](schedules)

The exact release matches all34 local official raw replays. Final heads finish40–40 on40 primary maps and20–20 on20 structural-shift maps. It wins233/320 against the fixed local proxy pool, identical to baseline in every paired point/gem/fuel stratum. The run contains6745 unique completed full-match jobs across327 distinct map files and zero native bot faults. Those6745 jobs are not6745 independent maps. Fuel suspensions remain:1097 native entries per320 final pool team-games.

`bot_v36/src` contains the switched research implementation; `release/bot_v36/src` is the exact packaged source. Each build manifest pins its source tree, artifact, switches and compiler environment. Published `builds/*/source.zip` snapshots preserve historical variant sources without duplicating large source diffs. Source archive members are relative to the original `src` directory. Source hashes can be checked against the corresponding manifest.

The four supplied opponent executables are uncalibrated recovered local proxies. They retain their actual identities; they are not substitutes relabeled as the requested historical calibrated opponents. All generated layouts are physically distinct, but shared generator templates prevent competition-family generalization claims. Live authentication, current submission windows and ladder A/B remain unavailable. Nothing here submits or replaces a live bot.

The publication contains code, legal release/rollback artifacts, maps/opponents, all result and schedule matrices, source/build manifests and compact evidence. `PUBLICATION_CONTENTS.json` inventories exactly what was exported. Large raw replay/stdout/decoded datasets and compiler/SDK/engine runtime installations remain on the original workstation. Immutable receipts retain original local paths and hashes; reproducing the evaluation needs the recorded runtime dependencies and path setup. The source-default release itself was rebuilt byte-for-byte from the re-extracted source bundle.

Agent reports reflect the time each review was performed. Root completed final-artifact checks after peer source/protocol review; earlier report statements about pending publication were superseded by the user's explicit20 October2026 authorization. GitHub publication is separate from live bot publication.
