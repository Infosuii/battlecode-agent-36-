"""Predeclare geometry shifts before final selection; no bot games here."""
import importlib.util, json, sys
from evaluate import ROOT,WORK,write,sha
sys.path[:0]=[str(WORK/'runtime'),str(WORK/'battlecode-preparation/tools'),str(WORK/'battlecode-preparation/analytics')]
from battlecode_analytics.engine import Engine

def main():
    spec=importlib.util.spec_from_file_location('shift_generator',WORK/'development/mapgen.py');mg=importlib.util.module_from_spec(spec);spec.loader.exec_module(mg)
    engine=Engine(WORK/'development/engine-current.wasm');mg.Engine=lambda:engine
    sizes=[(43,51,3),(57,39,4),(69,63,5),(89,47,5),(77,81,3)]
    # Clustering is development-only. Bush-on-camp and monolith-first styles are
    # reserved for final structural robustness, never seen in policy screens.
    for stage,styles,n,start in [('shift_validation',['clustered'],10,362010000),('shift_final',['bush_camps','monolith'],20,362020000)]:
        folder=ROOT/'maps'/stage;folder.mkdir(parents=True,exist_ok=True)
        if (folder/'manifest.json').exists():continue
        rows=[]
        for i in range(n):
            style=styles[i%len(styles)];rows.append(mg.generate(start+i,*sizes[(i//len(styles))%len(sizes)],folder/f'{style}_{i:03d}.map26a',style))
        write(folder/'manifest.json',dict(stage=stage,maps=rows,match_performance_untouched=True,generator_sha256=sha(WORK/'development/mapgen.py'),
            caveat='Reserved structural shifts, but shared generator/mirrored corner-spawn and wall mechanisms; not proof of generalization to all competition families.',
            freeze_policy='No final bot games before selected combined source and switches are frozen.'))
        print(json.dumps(dict(stage=stage,maps=len(rows))),flush=True)

if __name__=='__main__':main()
