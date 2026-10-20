"""Freeze identities, resources, and new geometry schedules before selection."""
import ctypes, datetime, importlib.util, json, os, pathlib, shutil, sys
from evaluate import ROOT, WORK, PROJECT, CLI, SDK, sha, tree, write, run_cmd

def prepare():
    archive=pathlib.Path('C:/Users/ishan/Downloads/bot v34.zip')
    base=dict(archive=str(archive),archive_sha256=sha(archive),source=tree(ROOT/'bot_v34/src'),
        file_count=len(list((ROOT/'bot_v34/src').iterdir())),note='Supplied archive is authoritative; earlier archive identity is historical only.')
    write(ROOT/'BASELINE_MANIFEST.json',base)
    root_engine=WORK/'development/engine-current.wasm'
    if not root_engine.exists(): raise RuntimeError('Missing recovered current engine')
    env=dict(cli_version=run_cmd([CLI,'--version']).strip(),cli_sha256=sha(CLI),sdk=tree(PROJECT/'sdk/cpp'),
        compiler=run_cmd([SDK/'bin/clang++.exe','--version']),engine_wasm_sha256=sha(root_engine),
        logical_cpus=os.cpu_count(),selected_match_workers=3,
        sdk_config=(PROJECT/'cambc.toml').read_text(),live_auth='CLI maps list: Sign in first',
        discovered_opponents='Historical local artifacts available under original names, not recovered farm/calm/v34c/sparexp/aggro/v30base/spartop.',
        missing=['authenticated platform submission windows','historical named calibrated pool','historical maps_rdev/maps_real'])
    class Memory(ctypes.Structure):
        _fields_=[('length',ctypes.c_ulong),('load',ctypes.c_ulong)]+[(x,ctypes.c_ulonglong) for x in ['total_phys','avail_phys','total_page','avail_page','total_virtual','avail_virtual','avail_ext']]
    mem=Memory(); mem.length=ctypes.sizeof(mem)
    if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(mem)): env['memory']={k:getattr(mem,k) for k in ['total_phys','avail_phys','load']}
    env['disk_free_bytes']=shutil.disk_usage(ROOT).free
    write(ROOT/'ENVIRONMENT.json',env)
    official=[dict(path=str(p),sha256=sha(p)) for p in sorted((PROJECT/'maps').glob('*.map26a'))]
    write(ROOT/'schedules/local-official-maps.json',dict(count=len(official),live_pool_verified=False,maps=official))
    sys.path[:0]=[str(WORK/'runtime'),str(WORK/'battlecode-preparation/tools'),str(WORK/'battlecode-preparation/analytics')]
    spec=importlib.util.spec_from_file_location('recovered_mapgen',WORK/'development/mapgen.py'); mg=importlib.util.module_from_spec(spec);spec.loader.exec_module(mg)
    from battlecode_analytics.engine import Engine
    mg.Engine=lambda:Engine(root_engine)
    # These are new, declared synthetic geometry families, not the historical maps_rdev suite.
    sizes=[(37,43,3),(53,47,4),(61,39,5),(83,33,5),(67,71,3),(91,79,4),(29,35,3),(49,57,5),(73,41,4),(97,93,5)]
    stages=[('screen',200,361019000),('validation',40,361119000),('final_holdout',40,361219000)]
    for stage,n,start in stages:
        dest=ROOT/'maps'/stage; dest.mkdir(parents=True,exist_ok=True)
        if (dest/'manifest.json').exists(): continue
        rows=[]
        for i in range(n):
            p=dest/f'{stage}_{i:03d}.map26a'
            rows.append(mg.generate(start+i,*sizes[i%len(sizes)],p))
            if (i+1)%25==0: print(json.dumps(dict(map_stage=stage,completed=i+1,total=n)),flush=True)
        write(dest/'manifest.json',dict(stage=stage,created_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),maps=rows,
            engine_sha256=sha(root_engine),generator_sha256=sha(WORK/'development/mapgen.py'),
            purpose='untouched final evaluation after exact candidate freeze' if stage=='final_holdout' else stage,
            limitations='Synthetic separated-camp families; engine full-match length is unchanged. Not competition-map distribution proof.',
            match_seeds=False))
    print(json.dumps(dict(prepared=True,baseline_archive_sha256=base['archive_sha256'],official_count=len(official),environment=env)),flush=True)

if __name__=='__main__': prepare()
