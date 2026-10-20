"""Regularized paired-comparison draft model with team effects.

Usage: python model.py [--min-team-games 20] [--C 1.0]

Each match is one row, side a vs side b (both sides drafted the same draft size). Features are differences a - b:
  team indicators (team strength; ridge-shrunk), champion indicators per draft size (3/4/5; C&I is one pick),
  champion x map indicators (heavily shrunk via a feature scale), and a side-a intercept.
Logistic regression with L2 (sklearn). Strength comes only from team effects fitted on the same window (no future
ratings). Cross-validation splits whole match groups (GroupKFold) and a later-time holdout (last 20% by time).
Outputs reports/draft_model.json: champion effects per size with bootstrap intervals over match groups, and the
expected log-odds of each v34 lineup and of top observed lineups per size, relative to v34's unknown-map choice.
"""
import collections
import json
import os
import sys

import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import GroupKFold

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'data')
REP = os.path.join(HERE, '..', 'reports')
CH = ['ryth', 'vesper_vale', 'vendetta', 'cassi_and_issac', 'stellar', 'prism', 'marcella', 'willow', 'ren', 'varg',
      'tavi', 'heart_of_ruin']
MAP_SCALE = 0.35  # champion x map features enter scaled down => stronger effective shrinkage

V34 = {  # lineups as pick-priority lists (first `size` distinct picks), from bot_v34 draft.cpp
    2: ['cassi_and_issac', 'stellar', 'varg', 'tavi', 'ren'],
    6: ['vendetta', 'varg', 'cassi_and_issac', 'ryth', 'ren'],
    8: ['vendetta', 'varg', 'cassi_and_issac', 'stellar', 'ren'],
    19: ['cassi_and_issac', 'stellar', 'ren', 'ryth', 'varg'],
    20: ['cassi_and_issac', 'stellar', 'ryth', 'prism', 'tavi'],
    13: ['stellar', 'vendetta', 'varg', 'tavi', 'ryth'],
}


def arg(name, default):
    return type(default)(sys.argv[sys.argv.index(name) + 1]) if name in sys.argv else default


def load():
    rows = [json.loads(l) for l in open(os.path.join(DATA, 'matches.jsonl'))]
    return [r for r in rows if r['sides'][0]['size'] == r['sides'][1]['size'] and r['sides'][0]['size'] in (3, 4, 5)
            and r['sides'][0]['result'] in ('win', 'loss')]


def design(rows, teams, maps):
    ti = {t: i for i, t in enumerate(teams)}
    mi = {m: i for i, m in enumerate(maps)}
    nt, nc, nm = len(teams), len(CH), len(maps)
    off_ch = nt
    off_map = nt + 3 * nc
    X = np.zeros((len(rows), off_map + nm * nc + 1))
    y = np.zeros(len(rows))
    for r_i, r in enumerate(rows):
        a, b = r['sides']
        k = a['size'] - 3
        for sgn, s in ((1, a), (-1, b)):
            if s['team'] in ti:
                X[r_i, ti[s['team']]] += sgn
            for c in s['lineup']:
                X[r_i, off_ch + k * nc + CH.index(c)] += sgn
                if r['map'] in mi:
                    X[r_i, off_map + mi[r['map']] * nc + CH.index(c)] += sgn * MAP_SCALE
        X[r_i, -1] = 1.0
        y[r_i] = 1.0 if a['result'] == 'win' else 0.0
    return X, y, off_ch


def fit(X, y, C):
    m = LogisticRegression(C=C, fit_intercept=False, max_iter=2000)
    m.fit(X, y)
    return m


def logloss(m, X, y):
    p = np.clip(m.predict_proba(X)[:, 1], 1e-6, 1 - 1e-6)
    return float(-np.mean(y * np.log(p) + (1 - y) * np.log(1 - p)))


def main():
    C = arg('--C', 1.0)
    min_games = arg('--min-team-games', 20)
    rows = load()
    cnt = collections.Counter(s['team'] for r in rows for s in r['sides'])
    teams = sorted(t for t, n in cnt.items() if n >= min_games)
    maps = sorted({r['map'] for r in rows if r['map']})
    X, y, off = design(rows, teams, maps)
    groups = np.array([r['group'] for r in rows])

    # Validation: grouped CV and a later-time holdout, vs a team-only model.
    team_only = X.copy()
    team_only[:, off:-1] = 0
    cv = {'full': [], 'team_only': []}
    for tr, te in GroupKFold(5).split(X, y, groups):
        cv['full'].append(logloss(fit(X[tr], y[tr], C), X[te], y[te]))
        cv['team_only'].append(logloss(fit(team_only[tr], y[tr], C), team_only[te], y[te]))
    cut = int(len(rows) * 0.8)
    later = {'full': logloss(fit(X[:cut], y[:cut], C), X[cut:], y[cut:]),
             'team_only': logloss(fit(team_only[:cut], y[:cut], C), team_only[cut:], y[cut:])}

    model = fit(X, y, C)
    w = model.coef_[0]
    # Bootstrap over match groups for champion-effect intervals.
    ug = sorted(set(groups))
    gi = collections.defaultdict(list)
    for i, g in enumerate(groups):
        gi[g].append(i)
    rng = np.random.default_rng(1)
    boots = []
    for _ in range(arg('--boot', 60)):
        idx = np.concatenate([gi[ug[j]] for j in rng.integers(0, len(ug), len(ug))])
        boots.append(fit(X[idx], y[idx], C).coef_[0][off:off + 3 * len(CH)])
    boots = np.array(boots)

    def lineup_value(picks, size, coef):
        chosen = picks[:size]
        return float(sum(coef[off + (size - 3) * len(CH) + CH.index(c)] for c in chosen))

    out = {'n_matches': len(rows), 'n_groups': len(ug), 'n_teams_modelled': len(teams), 'maps': maps, 'C': C,
           'cv_logloss': {k: float(np.mean(v)) for k, v in cv.items()}, 'later_holdout_logloss': later,
           'champion_effects': {}, 'lineups': {}}
    support = collections.Counter((s['size'], c) for r in rows for s in r['sides'] for c in s['lineup'])
    for k, size in enumerate((3, 4, 5)):
        eff = {}
        for j, c in enumerate(CH):
            col = k * len(CH) + j
            lo, hi = np.percentile(boots[:, col], [2.5, 97.5])
            eff[c] = {'logodds': round(float(w[off + col]), 3), 'ci95': [round(float(lo), 3), round(float(hi), 3)],
                      'side_uses': support[(size, c)]}
        out['champion_effects'][size] = eff
        lu = collections.Counter(tuple(s['lineup']) for r in rows for s in r['sides'] if s['size'] == size)
        cands = {f'v34_lineup_{n}': p for n, p in V34.items()}
        for lin, n in lu.most_common(8):
            cands['observed:' + '+'.join(lin)] = list(lin)
        base = lineup_value(V34[2], size, w)
        tab = {}
        for name, p in cands.items():
            vals = [sum(b[k * len(CH) + CH.index(c)] for c in p[:size]) - sum(b[k * len(CH) + CH.index(c)]
                                                                            for c in V34[2][:size]) for b in boots]
            tab[name] = {'vs_lineup2_logodds': round(lineup_value(p, size, w) - base, 3),
                         'ci95': [round(float(np.percentile(vals, 2.5)), 3), round(float(np.percentile(vals, 97.5)), 3)],
                         'observed_side_uses': lu.get(tuple(sorted(set(p[:size]))), 0)}
        out['lineups'][size] = dict(sorted(tab.items(), key=lambda kv: -kv[1]['vs_lineup2_logodds']))
    os.makedirs(REP, exist_ok=True)
    json.dump(out, open(os.path.join(REP, 'draft_model.json'), 'w'), indent=1)
    print(json.dumps({k: out[k] for k in ('n_matches', 'n_groups', 'n_teams_modelled', 'cv_logloss',
                                          'later_holdout_logloss')}))
    for size, tab in out['lineups'].items():
        print(f'size {size}:')
        for name, v in list(tab.items())[:10]:
            print(f"   {name[:60]:60s} {v['vs_lineup2_logodds']:+.3f} {v['ci95']} uses={v['observed_side_uses']}")


if __name__ == '__main__':
    main()
