"""Static physical duplicates across reserved sets; no performance selection."""
import collections,json
from evaluate import ROOT,write,sha

def main():
    path=ROOT/'evidence/agent4/physical_map_layouts.json'
    data=json.loads(path.read_text());rows=data['rows'];groups=collections.defaultdict(list)
    for row in rows:groups[row['physical_sha256']].append(row)
    duplicates=[dict(signature=k,maps=[dict(stage=r['stage'],name=r['name']) for r in v]) for k,v in groups.items() if len(v)>1]
    official={r['physical_sha256']:r['name'] for r in rows if r['stage']=='official'}
    cross=[dict(stage=r['stage'],name=r['name'],official=official[r['physical_sha256']]) for r in rows if r['stage']!='official' and r['physical_sha256'] in official]
    compact=[{k:r[k] for k in ['stage','name','file_sha256','physical_sha256','seed','style']} for r in rows]
    result=dict(source_receipt_sha256=sha(path),engine_sha256=data['engine_sha256'],
        counts=dict(collections.Counter(r['stage'] for r in rows)),
        unique_physical_signatures=len(groups),exact_physical_duplicate_groups=duplicates,generated_matches_to_official=cross,
        maps=compact,scope='Static editor representations consistently compared across all sets; no bot matches or final outcomes used.',
        caveat='No exact physical duplicate does not imply unrelated families. Templates/generator remain shared; incidental code-layout/fuel effects can change inactive-policy builds.')
    write(ROOT/'evidence/agent4/physical_map_audit_summary.json',result)
    print(json.dumps({k:result[k] for k in ['counts','unique_physical_signatures','exact_physical_duplicate_groups','generated_matches_to_official']}))

if __name__=='__main__': main()
