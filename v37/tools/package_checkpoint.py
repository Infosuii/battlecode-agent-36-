"""Freeze the unchanged fallback plus an honest research/evidence checkpoint."""
import json,pathlib,shutil,struct,collections
from lab import ROOT,WORK,WS,CLI,ENGINE,manifest,write,sha,tree,digest,run
def main():
    base=manifest('v37-baseline');off=manifest('v37-final-off2')
    assert base['wasm_sha256']==off['wasm_sha256'],'All-off parity failed'
    original=ROOT.parent/'v36/release/bot_v36.zip'
    assert sha(original)==sha(ROOT/'baseline-rebundled.zip')=='5ee75d520caa0c921361ea6c83a6a3b991d05ea8a6a7707226dabc875822b320'
    release=ROOT/'release';release.mkdir(exist_ok=True)
    shutil.copyfile(original,release/'bot_v37_fallback.zip')
    shutil.copyfile(base['artifact'],release/'bot_v37_fallback.wasm')
    shutil.copytree(ROOT/'baseline/src',release/'source/src',dirs_exist_ok=True)
    assert tree(release/'source/src')==base['source_files']
    summaries={p.stem:json.loads(p.read_text()) for p in (ROOT/'results').glob('*-summary.json')}
    for name in ('econ-link-dev-v1-summary','safe-state-dev-v1-summary'):
        s=summaries[name];assert s['win_points']<.53 or s['mean_secured_margin']<=0
    compiler=pathlib.Path(base['compile_commands'][0]['command'].split(' --target=')[0])
    macrocmd=[compiler,'-std=c++20','-msimd128','-fwasm-exceptions','-I'+str(WS/'sdk/cpp'),'-dM','-E','-x','c++',release/'source/src/common.h']
    macros=run(macrocmd);(release/'effective-preprocessor.txt').write_text(macros)
    import re
    settings={m[0]:m[1] for m in re.findall(r'^#define ((?:APEX_|NOVA_|V37_)\w+)\s+(.*)$',macros,re.M)}
    assert settings['NOVA_V36_DRAFT']=='1' if 'NOVA_V36_DRAFT' in settings else True
    # Draft-specific switch is declared in draft.cpp, not common.h.
    draft=(release/'source/src/draft.cpp').read_text();assert '#define NOVA_V36_DRAFT 1' in draft
    data=dict(decision='unchanged selected v36 fallback; no v37 policy promoted; full research task incomplete',
        source_commit='a020248d8b13593bdf4c70460116f2bb061860b4',zip_sha256=sha(release/'bot_v37_fallback.zip'),
        wasm_sha256=sha(release/'bot_v37_fallback.wasm'),source_files=base['source_files'],source_sha256=base['source_sha256'],
        cli_version=base['cli_version'],cli_sha256=base['cli_sha256'],sdk_config=base['sdk_config'],sdk_files=base['sdk_files'],
        compiler=base['compiler_version'],compiler_sha256=base['compiler_sha256'],compile_commands=base['compile_commands'],
        native_engine_version='1.0.1',native_engine_build_hash=ENGINE,visualiser_engine_wasm_sha256=sha(ROOT/'tools/engine-current.wasm'),
        source_default_production_parity=dict(all_off_wasm=off['wasm_sha256'],exact=True),
        draft_settings=dict(NOVA_V36_DRAFT=1,NOVA_MAPID_STRICT=0),source_defines=settings,
        effective_runtime_note='brain::adapt resets aggression, carry, retreat HP and shop gold each turn; source declarations alone are not runtime values. strategy.h preserved exactly in release.',
        full_match_rules='CLI release, default fuel and default match end; engine build checked in every accepted receipt',
        summary_files={n:sha(ROOT/'results'/(n+'.json')) for n in summaries},rollback='Use original selected v36 ZIP; byte-identical to this fallback.',
        no_live_submission=True,no_merge=True)
    write(release/'MANIFEST.json',data)
    raw=[];fuel_reports=[]
    from replay_bundle import read_bundle
    for path in sorted((ROOT/'runs').glob('*/receipt.json')):
        r=json.loads(path.read_text());assert r['match']['engineBuildHash']==ENGINE
        assert r['match']['mapHash']==r['key']['map_sha256']
        assert all(sha(path.parent/n)==h for n,h in r['raw_hashes'].items())
        raw.append(dict(job=path.parent.name,receipt_sha256=sha(path),folder=str(path.parent),key=r['key'],raw_hashes=r['raw_hashes']))
        bundle=path.parent/next(n for n in r['raw_hashes'] if n.endswith('.repl26a'));members=read_bundle(bundle)
        for side in ('a','b'):
            b=members[f'team-{side}.fuel']
            assert len(b)>=77 and int.from_bytes(b[:8],'little')==28839526091863649 and b[8:11]==bytes([0,0,1]) and (len(b)-77)%8==0
            values=[v[0] for v in struct.iter_unpack('<Q',b[77:])];ordered=sorted(values)
            fuel_reports.append(dict(job=path.parent.name,side=side,entries=len(values),limits=[list(struct.unpack_from('<4Q',b,o)) for o in (13,45)],
                mean=sum(values)/len(values) if values else None,p95=ordered[int(.95*(len(ordered)-1))] if ordered else None,maximum=max(values,default=None)))
    write(ROOT/'evidence/raw-index.json',dict(accepted_unique_games=len(raw),entries=raw))
    write(ROOT/'evidence/fuel-format-audit.json',dict(format='a62leuf; version 0.0.1, languages, 2x4 Q limits, Q values from offset 77',
        all_entries_validated=True,rows=fuel_reports,production_and_trace_separated_by_job_key=True))
    write(ROOT/'evidence/release-gates.json',dict(zip_byte_identity=True,source_zip_manifest_parity=True,all_off_wasm_identity=True,
        accepted_unique_games=len(raw),engine_identity_all_matches=True,map_integrity_all_matches=True,raw_hashes_all_verified=True,
        policies_rejected=['V37_LINK_EMERGENCY','V37_CAMP_STATE'],confirmation_games=0,final_games=0,
        clarification='No candidate passed screening, so frozen confirmation/final maps were not played. No strength claim for unchanged fallback.'))
    # Make durable build receipts available despite ignoring compiler intermediates.
    for name in ('v37-baseline','v37-off','v37-econ-link','v37-safe-state2','v37-final-off2','v37-trace','v37-trace2'):
        write(ROOT/'evidence/build-receipts'/(name+'.json'),manifest(name))
    print(json.dumps(dict(fallback_wasm=data['wasm_sha256'],zip=data['zip_sha256'],all_off_exact=True,accepted_games=len(raw))))
if __name__=='__main__':main()
