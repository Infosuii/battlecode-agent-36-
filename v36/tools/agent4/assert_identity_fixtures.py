"""Finish identity fixtures using raw-engine actual picks; cutoff has no stats."""
import argparse,hashlib,json,pathlib,subprocess
from replay_bundle import read_bundle
V36=pathlib.Path(__file__).resolve().parents[2]
BASE={19:[3,4,8,0,9],2:[3,4,9,10,8],8:[2,9,3,4,8],20:[3,4,0,5,10],6:[2,9,3,0,8]}
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--fixture-name',default='identity_fixtures_corrected');a=parser.parse_args()
    root=V36/'evidence/agent4'/a.fixture_name;report=json.loads((root/'report.json').read_text());selection=[]
    for row in report['receipts']:
        bundle=pathlib.Path(row['bundle']);assert hashlib.sha256(bundle.read_bytes()).hexdigest()==row['bundle_sha256']
        raw=bundle.parent/'assertion.repl26a.raw';raw.write_bytes(read_bundle(bundle)['replay.repl26a']);selection.append(dict(name=row['name'],raw=str(raw)))
    spec=root/'roster_selection.json';spec.write_text(json.dumps(selection));rosterfile=root/'actual_rosters.json'
    p=subprocess.run(['node',str(V36/'tools/agent4/identity_replay_roster.mjs'),str(spec),str(rosterfile)],capture_output=True,text=True,timeout=120)
    if p.returncode:raise RuntimeError(p.stderr[-3000:])
    rosters={r['name']:r for r in json.loads(rosterfile.read_text())};modulechecks=[];pickchecks=[];knownpicks=unknownpicks=0;sizes=set()
    for row in report['receipts']:
        roster=rosters[row['name']]
        for side in range(2):
            picks=roster['drafts'][side];sizes.add(len(picks));assert len(picks) in (3,4,5) and len(set(picks))==len(picks)
            li=row['parsed']['ab'[side]]['lineup_trace'][0][5]
            assert li in BASE and picks==BASE[li][:len(picks)],f"{row['name']} declared lineup not executed"
            if row['expected_id']>=0:
                expected=2 if row['name']=='expanse' else 8 if row['name']=='rows' else 20 if row['name']=='stampede' else 19
                assert li==expected;knownpicks+=len(picks)
            else:assert li in (2,6);unknownpicks+=len(picks)
            bodies=[b for b in roster['bodies'] if b['team']==side];assert len(bodies)==len(picks)+int(3 in picks)
            if 3 in picks:assert sum(b['type'] in (3,4) for b in bodies)==2
            game=[o for o in row['parsed']['ab'[side]]['identity_observations'] if not o['draft_phase']]
            assert len(game)==len(bodies),f"{row['name']}: missing gameplay module result"
            expected_game=-1 if report.get('runtime_gameplay_expected_unknown') else row['expected_id']
            assert all(o['sdk_code']==0 and o['id']==expected_game for o in game)
            modulechecks.append(dict(map=row['name'],team=side,body_modules=len(bodies),first_gameplay_identity_observations=len(game),passed=True))
            pickchecks.append(dict(map=row['name'],team=side,picks=picks,lineup=li,physical_body_count=len(bodies),passed=True))
    final=dict(known_layout_scenarios=report['known_layout_scenarios'],unknown_descriptor_collision_scenarios=report['descriptor_collision_scenarios'],known_actual_picks=knownpicks,unknown_actual_picks=unknownpicks,
        actual_pick_sizes=sorted(sizes),physical_body_modules=sum(r['body_modules'] for r in modulechecks),all_passed=True,
        source_header_sha256='3e84b617d5d38f92260ccbb6ee46e8c0937260abc75b461ad97cd3c87edeccf0',
        artifact_sha256=report['receipts'][0]['artifact_sha256'],pick_checks=pickchecks,body_module_checks=modulechecks,
        scope=f"{len(report['receipts'])} two-round actual SDK scenarios, both sides; no strength inference. Draft/body assertions decoded independently from raw official-engine replays.")
    (root/'draft_module_assertions.json').write_text(json.dumps(final,indent=2));print(json.dumps({k:v for k,v in final.items() if k not in ['pick_checks','body_module_checks']}))
if __name__=='__main__':main()
