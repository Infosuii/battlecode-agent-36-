"""Exploratory team-controlled draft model; no policy is promoted by this fit.

Uses certified historical replay contexts, de-duplicates replay bytes, and keeps
whole match groups/duplicate-linked groups together in chronological splits.
Missing numeric versions and contemporaneous ratings remain missing.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','1')
os.environ.setdefault('OMP_NUM_THREADS','1')
import collections, csv, hashlib, json, math, pathlib
import numpy as np

V36=pathlib.Path(__file__).resolve().parents[2]
OUT=V36/'evidence/agent4'
CHAMP=['Ryth','Vesper','Vendetta','C&I','Stellar','Prism','Marcella','Willow','Ren','Varg','Tavi','HeartOfRuin']
BASE={19:[3,4,8,0,9],2:[3,4,9,10,8],8:[2,9,3,4,8],20:[3,4,0,5,10]}

def lineup(mapname,picks):
    return tuple(sorted(BASE[2 if mapname=='expanse' else 8 if mapname=='rows' else 20 if mapname=='stampede' else 19][:picks]))

def fit(X,y,penalty,maxiter=30):
    beta=np.zeros(X.shape[1]);iterations=0
    for iterations in range(maxiter):
        z=np.clip(X@beta,-30,30);p=1/(1+np.exp(-z));w=np.maximum(p*(1-p),1e-7)
        gradient=X.T@(p-y)+penalty*beta
        hessian=(X.T*w)@X+np.diag(penalty)
        delta=np.linalg.solve(hessian,gradient)
        beta-=delta
        if np.max(np.abs(delta))<1e-7:break
    return beta,iterations+1

def metrics(X,y,beta):
    p=np.clip(1/(1+np.exp(-np.clip(X@beta,-30,30))),1e-9,1-1e-9)
    return dict(games=len(y),log_loss=float(np.mean(-(y*np.log(p)+(1-y)*np.log(1-p)))) if len(y) else None,
                brier=float(np.mean((p-y)**2)) if len(y) else None)

def main():
    joins=json.loads((OUT/'identity_join_manifest.json').read_text())
    metadata=json.loads((V36.parent/'data/matches.json').read_text())
    certified=[r for r in joins if r['verified']]
    # Union match groups sharing any deterministic replay before choosing splits.
    parent={r['group_id']:r['group_id'] for r in certified}
    def find(a):
        while parent[a]!=a:parent[a]=parent[parent[a]];a=parent[a]
        return a
    def union(a,b):parent[find(a)]=find(b)
    tracegroups={}
    for r in certified:
        if r['trace_id'] in tracegroups:union(r['group_id'],tracegroups[r['trace_id']])
        else:tracegroups[r['trace_id']]=r['group_id']
    selected={}
    for r in sorted(certified,key=lambda r:(r['created_at'],r['match_id'])):selected.setdefault(r['trace_id'],r)
    rows=list(selected.values())
    component_time={}
    for r in certified:
        k=find(r['group_id']);component_time[k]=max(component_time.get(k,''),r['created_at'])
    components=sorted(component_time,key=lambda g:(component_time[g],g))
    cut=int(.75*len(components));train_groups=set(components[:cut])
    for r in rows:r['split']='train' if find(r['group_id']) in train_groups else 'later_validation'
    train=[r for r in rows if r['split']=='train'];valid=[r for r in rows if r['split']!='train']
    features=set()
    def terms(r):
        m=metadata[r['match_id']];s={x['side']:x for x in m['sides']};mapname=m['map'].replace('_','-')
        out=[('side|'+mapname,1.)]
        # Global team effects and shrunk team-by-six-hour effects account for
        # team strength and coarse temporal drift without inventing versions.
        period=m['createdAt'][:11]+str(int(m['createdAt'][11:13])//6)
        for side,sign in [('a',1.),('b',-1.)]:
            draft=tuple(sorted(int(c) for c in s[side]['draft']))
            out += [('team|'+s[side]['teamId'],sign),('team_period|'+s[side]['teamId']+'|'+period,sign),
                    ('lineup|'+mapname+'|'+str(len(draft))+'|'+','.join(map(str,draft)),sign)]
        return out
    for r in train:features.update(k for k,v in terms(r))
    names=sorted(features);index={k:i for i,k in enumerate(names)}
    def matrix(items):
        X=np.zeros((len(items),len(names)));y=np.zeros(len(items))
        for i,r in enumerate(items):
            for k,v in terms(r):
                if k in index:X[i,index[k]]+=v
            s={s['side']:s for s in metadata[r['match_id']]['sides']}
            y[i]=1. if s['a']['result']=='win' else 0. if s['a']['result']=='loss' else .5
        return X,y
    X,y=matrix(train);XV,yV=matrix(valid)
    penalty=np.array([8. if k.startswith('lineup|') else 12. if k.startswith('team_period|') else 2. for k in names])
    beta,iters=fit(X,y,penalty)
    p=1/(1+np.exp(-np.clip(X@beta,-30,30)));H=(X.T*(p*(1-p)))@X+np.diag(penalty)
    bread=np.linalg.inv(H);scores=collections.defaultdict(lambda:np.zeros(len(names)))
    for i,r in enumerate(train):scores[find(r['group_id'])]+=X[i]*(y[i]-p[i])
    meat=sum((np.outer(v,v) for v in scores.values()),start=np.zeros_like(H))
    covariance=bread@meat@bread
    support=collections.defaultdict(lambda:dict(contexts=0,teams=set(),groups=set(),wins=0.,splits=collections.Counter()))
    for r in rows:
        m=metadata[r['match_id']];mapname=m['map'].replace('_','-')
        for s in m['sides']:
            draft=tuple(sorted(map(int,s['draft'])));key=(mapname,len(draft),draft);v=support[key]
            v['contexts']+=1;v['teams'].add(s['teamId']);v['groups'].add(find(r['group_id']));v['wins']+=1. if s['result']=='win' else .5 if s['result'] not in ['win','loss'] else 0.
            v['splits'][r['split']]+=1
    table=[]
    for (mapname,picks,draft),s in sorted(support.items()):
        key='lineup|'+mapname+'|'+str(picks)+'|'+','.join(map(str,draft));base=lineup(mapname,picks)
        basekey='lineup|'+mapname+'|'+str(picks)+'|'+','.join(map(str,base));contrast=np.zeros(len(names))
        identified=key in index and basekey in index
        if key in index:contrast[index[key]]+=1
        if basekey in index:contrast[index[basekey]]-=1
        effect=float(contrast@beta) if identified else None
        se=math.sqrt(max(0,float(contrast@covariance@contrast))) if identified else None
        notes=[]
        if len(s['teams'])<2:notes.append('single-team lineup: cannot separate lineup from team effects')
        if s['contexts']<30 or len(s['groups'])<10:notes.append('sparse support')
        if not identified:notes.append('lineup or baseline missing from training')
        if not s['splits']['later_validation']:notes.append('no later-time validation support')
        notes.append('unknown submissions; observational confounding remains')
        table.append(dict(map=mapname,picks=picks,lineup=draft,lineup_names=[CHAMP[c] for c in draft],baseline_lineup=base,
          contexts=s['contexts'],distinct_teams=len(s['teams']),independent_group_components=len(s['groups']),
          train_contexts=s['splits']['train'],later_validation_contexts=s['splits']['later_validation'],raw_win_points=s['wins']/s['contexts'],
          adjusted_log_odds_vs_baseline=effect,cluster_sandwich95_model_conditional=[effect-1.96*se,effect+1.96*se] if identified else None,
          decision='retain baseline; no full-match policy validation',limitations=notes))
    (OUT/'lineup_candidates.json').write_text(json.dumps(table,indent=2))
    split=[dict(match_id=r['match_id'],trace_id=r['trace_id'],original_group_id=r['group_id'],component=find(r['group_id']),created_at=r['created_at'],split=r['split']) for r in rows]
    (OUT/'historical_split_manifest.json').write_text(json.dumps(split,indent=2))
    report=dict(specification='Penalized logistic Bradley-Terry: map-specific A-side intercept + A/B team effects + shrunk team/six-hour effects + A/B map/pick-count/canonical-lineup effects.',
        ordered_picks_preserved_in='identity_join_manifest.json',predictors='Actual canonical lineup, team ID, map and coarse time only; no future scores/ratings as features.',
        penalty=dict(lineup=8,team_period=12,team_and_side=2),hyperparameters='fixed before fitting; no model selection on validation',
        contexts_before_dedup=len(certified),unique_trace_contexts=len(rows),group_components=len(components),
        train_components=cut,validation_components=len(components)-cut,parameter_count=len(names),newton_iterations=iters,
        train=metrics(X,y,beta),later_validation=metrics(XV,yV,beta),
        validation_new_features=sum(k not in index for r in valid for k,v in terms(r)),
        uncertainty='Group-cluster sandwich around penalized fit, conditional on fixed specification; sparse/collinear effects and unknown changing source versions prevent causal interpretation.',
        decision='No production draft row change; insufficient version/strength provenance and no paired full-match validation of a robust supported candidate.',
        optional_ml='Off: no logged eligible farm candidate set, decision-time visibility, propensities or validated counterfactual job labels.',
        checks=dict(no_trace_cross_split=len({r['trace_id'] for r in train}&{r['trace_id'] for r in valid})==0,
                    no_original_group_cross_split=len({r['group_id'] for r in train}&{r['group_id'] for r in valid})==0))
    (OUT/'draft_model_report.json').write_text(json.dumps(report,indent=2))
    (OUT/'draft_model_coefficients.json').write_text(json.dumps(dict(zip(names,map(float,beta))),indent=2))
    print(json.dumps(report))

if __name__=='__main__':main()
