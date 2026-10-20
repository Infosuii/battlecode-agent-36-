# Agent 4 data report: provenance, dictionary, coverage

## Sources (all public, no login)
| Source | Access | What it holds | Limits |
|---|---|---|---|
| `GET /api/v1/ladder?page=N` (25/page) | `tools/collect.py` | rank, rating, rating history per team | snapshot at collection time only |
| `GET /api/v1/match-groups?before=<cursor>` | `tools/collect.py` | global feed of groups, newest first | list has no drafts |
| `GET /api/v1/match-groups/<id>` | `tools/collect.py` (4 threads, polite delay) | 5 matches: map, rounds, ordered drafts, per-side result/score, `patchId` | **no submission IDs or versions (null)**, no end statistics in the current schema |
| Team snapshot 14–15 Oct (`research/groups`, 819 groups) | old workspace zip | same plus end statistics | engine 0.0.1, not mixed with 0.0.3 data |
| Local matches (`cambc run-batch --statistics --json`) | `tools/paired.py` | full statistics: events, timelines, roster, map hash, engine build hash, seed | own bots only |

Base URL `https://2026a.battlecode.co.uk/api/v1` (the one the team's earlier `collect.py` used).

## Identity
- Source tree: `bot_v34/src` = 18 files of `bot v34.zip`; archive SHA-256 `E6A95834DF2CD14B0719D107309000D4F38724B436E1F617B7DB5E85FDDFA605`
  (matches the master prompt). Candidate `bot_v36/src`.
- Compiled artifact: each runner build is named `<tree>__<sha256(src files + defines)[:10]>`; full hashes are in every
  result file's `meta` line. `bot_v34` content hash `5fa205f2…35ef`.
- Platform submission / version window: **unavailable**. Public match data carries `submissionId: null` and
  `version: null`, so no ladder result here is attributed to v34, v32b or platform-v36. Doing so needs the team's
  platform login (`cambc login`, then `cambc matches list`), which only a team member can do.

## Dictionary (`data/matches.jsonl`, one row per finished match; built by `tools/dataset.py`)
`group`, `match` (UUID, dedup key), `kind` (ranked/unranked), `at` (createdAt, UTC), `map` (name), `map_id`,
`rounds`, `engine` (from `statistics.engineVersion`, else `patchId` prefix), `schema`, `sides[2]`: `side`, `team`,
`name`, `rating_before` (ranked groups only), `draft` (ordered picks), `lineup` (sorted set; C&I is one pick, two
bodies), `size`, `score`, `result` (win/loss/draw), `metrics` (end statistics when present; integer strings coerced).
Numeric drafts (engine 0.0.3 API) decode by the SDK enum `bc::champion_draft` order (`cambc.h`, SDK 2.2.5):
0 ryth, 1 vesper_vale, 2 vendetta, 3 cassi_and_issac, 4 stellar, 5 prism, 6 marcella, 7 willow, 8 ren, 9 varg,
10 tavi. Check: the modal 5-pick draft decodes to exactly v34's lineup 19.

## Event counting (local statistics)
`fuelExhausted` events are per-champion buckets; count `len(entries)`, not buckets. Empty buckets are not
exhausted turns. On E4 (2,000 games, v36 cand vs v34): 731 entries per side (cand 370+361, base 345+386), ~0.37
per team-game; at least one in 18–19% of games.

## Coverage
See `coverage.json` (rewritten by every `dataset.py` run): files, matches, duplicates, unfinished, missing drafts,
missing results, engine and kind counts, time span.

## Splits
Model validation splits whole match groups (GroupKFold, 5 folds) and holds out the last 20% of matches by time.
Full-match tests use disjoint generated map suites per role (tuning, validation, confirmation), listed in
`EXPERIMENTS.md`; each map plays both sides; the independent unit is the map.
