"""Static-spawn camp bound from corrected, actual-SDK runtime groups only."""
import json,pathlib
V36=pathlib.Path(__file__).resolve().parents[2]
def main():
    root=V36/'evidence/agent4';maps=json.loads((root/'official_layouts.json').read_text());rows=[]
    for m in maps:
        assert m.get('group_provenance',{}).get('kind')=='actual runtime SDK serialized static definitions'
        dangerous=[tuple(g[4+i:7+i]) for g in m['groups'] for i in range(0,3*g[3],3) if g[4+i] in (0,1,4)]
        maximum=max(sum(g[4+i] in (0,1,4) for i in range(0,3*g[3],3)) for g in m['groups'])
        prox={str(radius):max(sum(max(abs(x-c[1]),abs(y-c[2]))<=radius for c in dangerous) for x in range(m['width']) for y in range(m['height'])) for radius in (3,6,8)}
        rows.append(dict(map=m['map'],dangerous_total=len(dangerous),max_dangerous_per_group=maximum,max_dangerous_within_cheb=prox,
          scope='Corrected actual-SDK immutable spawn positions; not awake/current moved camp positions or path reachability.'))
    (root/'dangerous_camp_limits.json').write_text(json.dumps(rows,indent=2));print(json.dumps(rows))
if __name__=='__main__':main()
