"""Collect the facts the draft reads (MAPFACTS line of a NOVA_MAPFACTS_PRINT probe build) for every map in the sets.

Usage: python mapfacts.py [sets...]  -> ../data/mapfacts.json ; requires bots/probe built in the cambc workspace.
Then reports descriptor collisions: unseen maps whose (w, h, size, groups) match an official descriptor.
"""
import json
import os
import re
import subprocess
import sys
import zipfile

WS = r'C:\Users\info\bcws'
CAMBC = r'C:\Users\info\bcvenv\Scripts\cambc.exe'
HERE = os.path.dirname(os.path.abspath(__file__))
sets = sys.argv[1:] or ['maps', 'maps-gen', 'maps-big100', 'maps-fresh16', 'maps-holdout']
run = os.path.join(WS, 'runs', 'mapfacts')
import shutil
shutil.rmtree(run, ignore_errors=True)
os.makedirs(run, exist_ok=True)
rows = []
with open(os.path.join(run, 'manifest.txt'), 'w') as mf:
    for s in sets:
        for m in sorted(os.listdir(os.path.join(WS, s))):
            if m.endswith('.map26a'):
                key = f'{s}/{m}'
                stem = os.path.join(run, key.replace('/', '__'))
                mf.write(f"{WS}\\bots\\probe {WS}\\bots\\starter-cpp {WS}\\{s}\\{m} {stem}.repl26a\n")
                rows.append((key, stem + '.repl26a'))
subprocess.run([CAMBC, 'run-batch', '--release', '--jobs', '8', '--rounds', '2', '--color', 'never',
                os.path.join(run, 'manifest.txt')], cwd=WS, capture_output=True)
facts = {}
for key, f in rows:
    try:
        t = zipfile.ZipFile(f).read('team-a.stdout').decode('utf-8', 'replace')
        line = re.search(r'MAPFACTS ([^\n]*)', t).group(1)
        facts[key] = {k: float(v) if '.' in v else int(v) for k, v in (kv.split('=') for kv in line.split())}
    except Exception as e:
        facts[key] = {'error': str(e)}
json.dump(facts, open(os.path.join(HERE, '..', 'data', 'mapfacts.json'), 'w'), indent=0)

desc = lambda f: (f['size'], f['groups'], tuple(sorted((f['w'], f['h']))))
official = {desc(f): k for k, f in facts.items() if k.startswith('maps/') and 'error' not in f}
print('errors:', [k for k, f in facts.items() if 'error' in f])
print('official maps recognized (mid>=0):', sum(1 for k, f in facts.items() if k.startswith('maps/') and f.get('mid', -1) >= 0), '/',
      sum(1 for k in facts if k.startswith('maps/')))
for k, f in facts.items():
    if not k.startswith('maps/') and f.get('mid', -1) >= 0:
        o = official.get(desc(f))
        fo = facts.get(o, {})
        print(f"COLLISION {k} mid={f['mid']} ~ {o}: gems {f['gems']} vs {fo.get('gems')}, gold {f['gold']} vs "
              f"{fo.get('gold')}, altars {f['altars']} vs {fo.get('altars')}, maxgems {f['maxgems']} vs {fo.get('maxgems')}")
