"""Interpretable observational predictors; no policy correction is activated."""
import json,math,collections,numpy as np
from lab import ROOT,write,sha,digest
FEATURES=['health_fraction','carried_gems','spendable_gold','distance','group_state','group_gems','group_gold','observation_age','observed_group_hp','base_score','value','hand_cost','drain','approach','wait','participants','need','lower_claims','higher_claims','siege','initiating','solo','visible_allies','visible_enemies']
def main():
    data=[json.loads(l) for l in (ROOT/'ml/decisions.jsonl').read_text().splitlines()]
    assert data
    train=[r for r in data if r['split']=='train'];valid=[r for r in data if r['split']=='validation'];test=[r for r in data if r['split']=='test']
    X=np.array([r['x'] for r in train],float);mean=X.mean(0);scale=X.std(0);scale[scale<1e-6]=1
    def design(rows):return np.c_[np.ones(len(rows)),np.clip((np.array([r['x'] for r in rows])-mean)/scale,-5,5)]
    def completed(rows):return [r for r in rows if r['label']['status']=='collected' and r['label']['elapsed']>0]
    tr=completed(train);xx=design(tr);y=np.log1p([r['label']['elapsed'] for r in tr])
    penalty=np.eye(xx.shape[1])*10;penalty[0,0]=0
    weights=np.linalg.solve(xx.T@xx+penalty,xx.T@y)
    baseline=float(np.median([r['label']['elapsed'] for r in tr]))
    cls=[r for r in train if r['label']['status']!='right_censored'];cx=design(cls)
    cy=np.array([r['label']['interruption'] for r in cls]);cw=np.zeros(cx.shape[1]);cw[0]=math.log((cy.sum()+1)/(len(cy)-cy.sum()+1))
    for _ in range(1500):
        p=1/(1+np.exp(-np.clip(cx@cw,-20,20)));grad=cx.T@(p-cy)/len(cy);grad[1:]+=.01*cw[1:];cw-=.03*grad
    rate=float(cy.mean())
    metrics={}
    for name,rows in [('train',train),('validation',valid),('test',test)]:
        comp=completed(rows);pred=np.clip(np.expm1(np.clip(design(comp)@weights,0,math.log1p(2000))),0,2000)
        actual=np.array([r['label']['elapsed'] for r in comp]);hand=np.array([r['x'][11]/max(r['x'][15],1) for r in comp])
        labeled=[r for r in rows if r['label']['status']!='right_censored'];prob=1/(1+np.exp(-np.clip(design(labeled)@cw,-20,20)));labels=np.array([r['label']['interruption'] for r in labeled])
        def loss(p):p=np.clip(p,1e-6,1-1e-6);return float(-np.mean(labels*np.log(p)+(1-labels)*np.log1p(-p)))
        bins=[]
        for lo,hi in [(0,.2),(.2,.4),(.4,.6),(.6,.8),(.8,1.01)]:
            take=(prob>=lo)&(prob<hi)
            bins.append(dict(range=[lo,hi],count=int(take.sum()),pred=float(prob[take].mean()) if take.any() else None,observed=float(labels[take].mean()) if take.any() else None))
        worst=sorted(zip(comp,pred,actual),key=lambda x:abs(x[1]-x[2]),reverse=True)[:5]
        metrics[name]=dict(rows=len(rows),physical_map_groups=len({r['map'] for r in rows}),completed=len(comp),censored=sum(r['label']['status']=='right_censored' for r in rows),
            elapsed_mae=float(np.abs(pred-actual).mean()),constant_median_mae=float(np.abs(actual-baseline).mean()),hand_cost_mae=float(np.abs(hand-actual).mean()),
            interruption_logloss=loss(prob),constant_interruption_logloss=loss(np.full(len(labels),rate)),brier=float(np.mean((prob-labels)**2)),calibration=bins,
            failure_examples=[dict(match=r['match'],body=r['body'],round=r['decision_round'],observed=float(y),predicted=float(p)) for r,p,y in worst])
    import sys
    model=dict(schema=1,features=FEATURES,python_version=sys.version,numpy_version=np.__version__,normalization=dict(mean=mean.tolist(),scale=scale.tolist(),clip=5),
        elapsed_log1p_weights=weights.tolist(),interruption_logit_weights=cw.tolist(),
        baseline=dict(completion_median=baseline,interruption_rate=rate),regularization=dict(ridge=10,logistic=.01),seed=None,
        dataset_sha256=sha(ROOT/'ml/decisions.jsonl'),splits_sha256=sha(ROOT/'ml/splits.json'),code_sha256=sha(__file__),
        activation_weight=0.0,conservative_correction_bound=[.98,1.02],heuristic_fallback=True,
        decision='trained exploratory pilot; not eligible for policy activation')
    write(ROOT/'ml/model.json',model)
    write(ROOT/'ml/evaluation.json',dict(metrics=metrics,model_sha256=sha(ROOT/'ml/model.json'),
        limitations=['Instrumented policy has measured fuel/decision drift.','Only six map groups; validation and test contain one each. Pilot test was inspected during label-quality repairs and is consumed development evidence.','Selected episodes are observational and overlapping, not independent samples.',
            'Camp-group enumeration join needs a separate SDK geometry fixture before relying on target-specific labels.','Damage/recovery/secure labels and full feature/exclusion coverage incomplete.',
            'Elapsed regression trains only observed collections; unfinished episodes retained separately, not treated as zero-duration successes.'],
        promotion=False,full_match_learned_policy_test='not run; prerequisite data-quality/instrumentation gates fail'))
    # Portable bounded inference preparation, kept out of the bot until gates pass.
    header=['#pragma once','#include <cmath>','namespace v37_pilot {','inline constexpr float activation_weight=0.0f;']
    for name,a in [('mean',mean),('scale',scale),('elapsed',weights),('risk',cw)]:header.append('inline constexpr double '+name+'[]={'+','.join(format(float(v),'.17g') for v in a)+'};')
    header.extend(['inline double predict(const double* x,const double* w){double y=w[0];for(int i=0;i<24;++i){if(!std::isfinite(x[i]))return NAN;double z=(x[i]-mean[i])/scale[i];z=z<-5?-5:z>5?5:z;y+=w[i+1]*z;}return y;}','}'])
    (ROOT/'ml/pilot_model.h').write_text('\n'.join(header)+'\n')
    print(json.dumps({k:{m:v[m] for m in ['rows','completed','elapsed_mae','constant_median_mae','hand_cost_mae','interruption_logloss','constant_interruption_logloss']} for k,v in metrics.items()}))
if __name__=='__main__':main()
