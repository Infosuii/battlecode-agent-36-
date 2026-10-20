"""Next registered development gates; holdouts remain untouched."""
from evaluate import ROOT,PROJECT,manifest,freeze,execute,write,summary

def schedule(sid,candidate,opponents,maps,purpose):
    if not (ROOT/'schedules'/(sid+'.json')).exists():freeze(sid,candidate,opponents,maps,purpose)
    return execute(sid,3)

def main():
    maps=sorted((ROOT/'maps/screen').glob('*.map26a'));official=sorted((PROJECT/'maps').glob('*.map26a'))
    pool=['spar_fish','spar_mite','spar_bush','live_platform_v35']
    write(ROOT/'EXPERIMENTS_POOL_WORLD.json',dict(
        pool_hypothesis='Governor reduces observed fuel exhaustion without losing fullmatch performance across diverse recovered artifacts',
        fixed_pool=pool,pool_scope='Uncalibrated local proxies and recovered historical source, not historical named farm/calm/sparexp pool or external strong-team executables.',
        primary='Matched candidate-minus-baseline win points and submitted gems, clustered by map across all opponents/sides',
        world_hypothesis='Visibilitychecked camp awake contract changes danger consistently; native mechanic/productionbody observation fixtures pass',
        world_rule='Evaluate fixed50map chunks; reject after first fullchunk if uppermapclusterwin95<.50 or runtimefault; otherwise400games. No holdout before candidatefreeze.',
        independence='200geometryseeds conditional common generator, not200competitionfamilies;17official mapclusters across fixed opponents'))
    schedule('governor-local-pool','governor',pool,official,'Registered fixed4opponent developmentpool on17local official maps, both sides136games')
    schedule('baseline-local-pool','baseline',pool,official,'Matched immutablebaseline reference for registered localpool136games')
    if manifest('world-awake-v2')['source_tree']['sha256']!=manifest('integration-off')['source_tree']['sha256']:
        raise ValueError('World sourcecontrol mismatch; rebuild actualOFF before test')
    schedule('world-v2-official','world-awake-v2',['integration-off'],official,'Isolated updatedworld awake state local34game correctness/performance gate')
    rows=[]
    for i in range(4):
        rows+=schedule(f'world-v2-screen-{i+1}','world-awake-v2',['integration-off'],maps[i*50:(i+1)*50],
            'Registered worldawake50map100game screen chunk; same-sourceOFF principal control')
        report=summary(rows,'world-awake-v2')
        if report['faults'] or report['cluster_bootstrap95'][1]<.50:break
    write(ROOT/'results/world-v2-screen-summary.json',dict(report,chunks=i+1,decision='screen_complete' if i==3 else 'early_rejected'))
    schedule('integration-off-parity','integration-off',['baseline'],official,'Latest complete source allswitchesOFF exactnative replay parity')

if __name__=='__main__':main()
