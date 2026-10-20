# v36 execution status — local validation complete

20 October 2026. Intended repository: https://github.com/Infosuii/battlecode-agent-36- ; parent issue3, workstreams4–7.

The finished release retains baseline behavior. Both shortlisted candidates failed the frozen fresh-validation promotion rules: rows-only won118/160 proxy-pool games versusbaseline119/160, and rows+governor114/160. All new policy and diagnostic switches remain OFF. No competitive upgrade or ladder gain is claimed.

Exact source-default release artifact2f73d77325ba45398672f329226eeba9a6033f2ced310d4e5a21c78ce748a4e1 reproduces byte-for-byte from the official CLI source ZIP after metadata normalization. It passes34/34 official raw replay checks,40–40 on40 primary final maps,20–20 on20 structural-shift final maps, and233/320 against four local proxy opponents, exactly matching baseline in every matched pool point/gem/fuel stratum. Native exhaustion remains1097 entries per320 pool team-games; zero native bot faults were observed in all6745 unique completed full-match jobs.

Source/archive identities, source snapshots, all experiment decisions and result matrices are retained. Baseline original source ZIP and baseline WASM70139ee190dc1e8a46338c78b54a1ee4b36d2459cb7944d6f2741d7d1432a757 remain unchanged for rollback. See VALIDATION_v36.md, CANDIDATE_MANIFEST.json, BUILD_AND_ROLLBACK.md and evidence/FINAL_RELEASE_REVIEW.json.

The user explicitly authorized GitHub code/results publication on20 October2026. This resolves the earlier publication-approval blocker. Publication receipt/PR identity is recorded separately. No cloud task, live submission, challenge or scheduled monitoring was created.

External limitations remain: original calibrated historical opponents/maps were unavailable; CLI is signed out, so current live pool/submission windows and ladder A/B are unverified. All generated geometry is physically distinct, but shared templates/generator prevent claims of competition-family independent generalization. Local preparation is complete; those external master-plan gates are not represented as completed.
