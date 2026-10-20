"""E04b same-source development gates; run only after native cache review."""
import json
from evaluate import ROOT, PROJECT, build, manifest, freeze, execute, write, sha

VARIANTS = {
    'cache-source-off': {},
    'strict-cached': dict(NOVA_STRICT_MAP_ID=1, NOVA_STRICT_MAP_CACHE=1),
    'rows-cached': dict(NOVA_STRICT_MAP_ID=1, NOVA_STRICT_MAP_CACHE=1, V36_ROWS_CAPTURE_INTENT=1),
    'stack-cached': dict(NOVA_STRICT_MAP_ID=1, NOVA_STRICT_MAP_CACHE=1, V36_ROWS_CAPTURE_INTENT=1, NOVA_RUNTIME_GOVERNOR=1),
}

def run(sid, candidate, opponents, maps, purpose):
    if not (ROOT/'schedules'/(sid+'.json')).exists():
        freeze(sid, candidate, opponents, maps, purpose)
    schedule=json.loads((ROOT/'schedules'/(sid+'.json')).read_text())
    if schedule['candidate']!=candidate or schedule['opponents']!=opponents:
        raise ValueError('Cached development schedule has different participants')
    expected={(sha(m), opponent, side) for m in maps for opponent in opponents for side in [0,1]}
    actual={(j['map_sha256'],j['b'] if j['a']==candidate else j['a'],0 if j['a']==candidate else 1) for j in schedule['jobs']}
    if actual!=expected or len(schedule['jobs'])!=len(expected): raise ValueError('Cached development paired grid differs')
    for job in schedule['jobs']:
        if job['sha_a']!=manifest(job['a'])['artifact_sha256'] or job['sha_b']!=manifest(job['b'])['artifact_sha256']:
            raise ValueError('Cached development artifact differs')
    return execute(sid, 3)

def main():
    for name, flags in VARIANTS.items():
        if not (ROOT/'builds'/name/'manifest.json').exists():
            build(name, flags)
    if len({manifest(n)['source_tree']['sha256'] for n in VARIANTS}) != 1:
        raise ValueError('Source changed during frozen ablation builds')
    official = sorted((PROJECT/'maps').glob('*.map26a'))
    screen = sorted((ROOT/'maps/screen').glob('*.map26a'))
    run('cache-off-parity', 'cache-source-off', ['baseline'], official, 'E04b all-new-switches-OFF exact parity prerequisite')
    for name, control in [('strict-cached', 'cache-source-off'), ('rows-cached', 'strict-cached')]:
        run(name+'-official', name, [control], official, 'E04b corrected cached identity and E06 same-source official suite')
        run(name+'-screen', name, [control], screen, 'E04b frozen development 200 maps, paired sides, 400 full games')
    pool = ['spar_fish', 'spar_mite', 'spar_bush', 'live_platform_v35']
    for name in ['rows-cached', 'stack-cached']:
        run(name+'-local-pool', name, pool, official, 'E04b complete cached stack local development pool, 17 maps four fixed uncalibrated opponents, both sides')
    write(ROOT/'evidence/cache-development-complete.json', dict(source_tree=manifest('rows-cached')['source_tree']['sha256'],
        variants={n:manifest(n)['artifact_sha256'] for n in VARIANTS},
        next='Review development evidence; fresh validation is a separate explicit invocation.'))

if __name__ == '__main__':
    main()
