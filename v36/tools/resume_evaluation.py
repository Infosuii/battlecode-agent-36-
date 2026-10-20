"""Registered isolated screens, same-source off controls, fixed chunk schedule."""
import json
from evaluate import ROOT, PROJECT, build, manifest, freeze, execute, write

def schedule(sid,candidate,opponents,maps,purpose):
    if not (ROOT/'schedules'/(sid+'.json')).exists():freeze(sid,candidate,opponents,maps,purpose)
    return execute(sid,3)

def main():
    # Group exact source snapshots, avoiding redundant off-control builds.
    for name,source in [('control-A','flee'),('control-B','governor')]:
        if not (ROOT/'builds'/name/'manifest.json').exists():build(name,source='builds/'+source)
    maps=sorted((ROOT/'maps/screen').glob('*.map26a'));official=sorted((PROJECT/'maps').glob('*.map26a'))
    registry=dict(protocol='Fixed 50-map chunks, 200-map maximum, mirrored sides; early rejection only after a whole chunk.',
        primary='match win points and submitted-gem margin; release faults/fuel supporting',
        early_stop='Reject at 100 games if upper descriptive map-cluster95% win interval <0.50 or any new runtime fault. Other results complete all400games.',
        selection='No gain claimed if interval includes0.50; fresh40map validation once then final frozen holdout. Correctness evidence alone does not establish performance.',
        variants=[dict(candidate=n,control=c) for n,c in [('flee','control-A'),('model-fix','control-A'),('relevance5','control-B'),('governor','control-B')]],
        strict_deferred='Native fixture rejected known official map; source repair and rerun identity gate required before performance screen.')
    write(ROOT/'EXPERIMENTS_RESUME.json',registry)
    for variant in registry['variants']:
        n,c=variant['candidate'],variant['control']
        if manifest(n)['source_tree']['sha256']!=manifest(c)['source_tree']['sha256']:raise ValueError('Not same-source control '+n)
        schedule(n+'-official',n,[c],official,'Registered isolated local17map34game release screen, unverified live pool.')
        completed=[];decision=None
        for i in range(4):
            sid=f'{n}-screen-{i+1}'
            completed+=schedule(sid,n,[c],maps[i*50:(i+1)*50],'Registered isolated new synthetic50map100game development chunk; not pristine final evidence.')
            from evaluate import summary
            report=summary(completed,n)
            if report['faults'] or report['cluster_bootstrap95'][1]<.5:
                decision='early_rejected';break
        write(ROOT/'results'/(n+'-screen-summary.json'),dict(report,decision=decision or 'fixed_screen_complete',chunks=(i+1)))
    schedule('bound-diagnostics','bound-diag',['baseline'],[PROJECT/'maps'/f'{n}.map26a' for n in ['rows','stampede','swirl']],
             'Correctness-only actual-state scorer bounds and coverage trace; logging affects fuel, no strength inference.')

if __name__=='__main__':main()
