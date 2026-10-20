"""Compact release evidence index; do not count incomplete or duplicate games."""
import json
from evaluate import ROOT, tree, sha, write

def main():
    baseline=json.loads((ROOT/'BASELINE_MANIFEST.json').read_text())
    if tree(ROOT/'bot_v34/src')!=baseline['source']: raise ValueError('Frozen baseline source changed')
    if sha(baseline['archive'])!=baseline['archive_sha256']: raise ValueError('Frozen supplied archive changed')
    builds={}
    for p in sorted((ROOT/'builds').glob('*/manifest.json')):
        m=json.loads(p.read_text())
        if sha(m['artifact'])!=m['artifact_sha256']: raise ValueError('Artifact changed: '+p.parent.name)
        if 'source_tree' in m and (p.parent/'src').exists() and tree(p.parent/'src')!=m['source_tree']: raise ValueError('Build source changed: '+p.parent.name)
        builds[p.parent.name]=dict(artifact_sha256=m['artifact_sha256'],source_tree=m.get('source_tree',{}).get('sha256'),
            switches=m.get('switches'),manifest=str(p),source_snapshot_present=(p.parent/'src').exists(),
            source_provenance=m.get('source_tree_provenance','Frozen copied source snapshot for this build'))
    completed={};pending={};unique={};excluded={};metadata={}
    for p in sorted((ROOT/'schedules').glob('*.json')):
        s=json.loads(p.read_text());result=ROOT/'results'/p.name
        if 'jobs' not in s:
            metadata[p.stem]='Map inventory, not an executable match schedule';continue
        if p.stem=='baseline-smoke':
            excluded[p.stem]='Superseded partial initialization smoke. Completed baseline-smoke-v2 and baseline-official replace it; incomplete schedule not counted.';continue
        if not result.exists(): pending[p.stem]='not run';continue
        rows=json.loads(result.read_text());jobs={j['id']:j for j in s['jobs']}
        if len(rows)!=s['games'] or {r['id'] for r in rows}!=set(jobs):
            pending[p.stem]=dict(completed=len(rows),planned=s['games']);continue
        for row in rows:
            job=jobs[row['id']]
            if any(row[k]!=job[k] for k in ['a','b','sha_a','sha_b','map_sha256','group_id','side']):
                raise ValueError('Frozen schedule/result identity mismatch: '+p.stem)
            unique[row['id']]=row
        completed[p.stem]=dict(games=s['games'],maps=s['distinct_maps'],candidate=s['candidate'],
            schedule_sha256=sha(p),results_sha256=sha(result),purpose=s['purpose'])
    output=dict(baseline_source_unchanged=True,baseline_archive_unchanged=True,builds=builds,
        completed_schedules=completed,incomplete_schedules=pending,excluded_initialization_schedules=excluded,metadata_inventories=metadata,
        unique_completed_full_match_jobs=len(unique),unique_completed_map_hashes=len({r['map_sha256'] for r in unique.values()}),
        engine_identities=sorted({r['engine'] for r in unique.values()}),
        native_fault_entries={side:sum(r['faults'][side] for r in unique.values()) for side in ['teamA','teamB']},
        caveat='Unique full-match jobs are artifact/opponent/map/side/settings units, not independent maps. Native short fixtures and deterministic duplicate schedule assignments are excluded from this total.')
    write(ROOT/'EXPERIMENT_INDEX.json',output)
    print(json.dumps({k:output[k] for k in ['unique_completed_full_match_jobs','unique_completed_map_hashes','native_fault_entries','incomplete_schedules']}),flush=True)

if __name__=='__main__': main()
