# v36 changelog (candidate, from frozen v34)

Only `bot_v36/src/draft.cpp` differs from `bot_v34/src`.

- **New:** `NOVA_V36_DRAFT` (on): unknown, spread-out maps draft C&I+Stellar+Ren (3 picks), C&I+Ren+Ryth+Varg
  (4), C&I+Stellar+Ren+Ryth+Varg (5). vs v34 on 640 untouched generated maps: 55.3% (52.7–57.8), +51 gems.
- **Fix:** draft test overrides (`APEX_DRAFT_LIST`, `APEX_DRAFT_T3/T4/T5`) now take effect; no release effect.
- **New, off:** `NOVA_MAPID_STRICT` official-map check by gem/gold totals.
- **New, probe only:** `NOVA_MAPFACTS_PRINT`.

Artifact `v36/agent4/release/bot_v36.zip`, manifests in `v36/agent4/release/`. Evidence: `v36/agent4/reports/DRAFT_REPORT.md`,
`v36/agent4/EXPERIMENTS.md`. Rollback: `bot v34.zip` or `NOVA_V36_DRAFT=0`.
