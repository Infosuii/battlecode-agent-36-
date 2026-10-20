"""Assert actual official-run ordered picks, body counts, and optional identity traces."""
import argparse, collections, json, pathlib
from replay_bundle import read_bundle, telemetry
V36=pathlib.Path(__file__).resolve().parents[2]
MAPS='ambush collision contest corners eight elkring expanse fragment interlock lanes monolith packs pilgrimage rows rushbait-v2 stampede swirl'.split()
BASE={19:['Cassi & Issac','Stellar','Ren','Ryth','Varg'],2:['Cassi & Issac','Stellar','Varg','Tavi','Ren'],8:['Vendetta','Varg','Cassi & Issac','Stellar','Ren'],20:['Cassi & Issac','Stellar','Ryth','Prism','Tavi'],6:['Vendetta','Varg','Cassi & Issac','Ryth','Ren']}
def validate(results,strict_trace=False,unknown=False):
    reports=[]
    for result in results:
        stat=json.loads(pathlib.Path(result['statistics']).read_text());name=pathlib.Path(result['map']).stem
        li=2 if name=='expanse' else 8 if name=='rows' else 20 if name=='stampede' else 19
        bundle=next(pathlib.Path(result['statistics']).parent.glob('*.repl26a'));members=read_bundle(bundle)
        traces=[]
        for member in ['team-a.stdout','team-b.stdout']:
            found=[]
            for line in members.get(member,b'').decode('utf-8',errors='replace').splitlines():
                if 'NOVA_MAPINFO ' in line:
                    vals=list(map(int,line.split('NOVA_MAPINFO ',1)[1].split()[:6]));found.append(vals)
            if strict_trace:
                assert found, f'{name}: missing identity trace in {member}'
                assert all(x[4]==(-1 if unknown else MAPS.index(name)) for x in found),f'{name}: incorrect strict identity {found}'
            traces+=found
        teams=[]
        for side in ['teamA','teamB']:
            picks=[r['champion'] for r in stat['roster']['draft'] if r['team']==side]
            bodies=[r for r in stat['roster']['champions'] if r['team']==side]
            assert len(picks) in (3,4,5) and len(set(picks))==len(picks),f'{name}: invalid/duplicate picks'
            assert len(bodies)==len(picks)+int('Cassi & Issac' in picks),f'{name}: body count mismatch'
            expected=BASE[li][:len(picks)]
            if not unknown:assert picks==expected,f'{name}: declared lineup not executed: {picks}!={expected}'
            elif strict_trace:
                assert picks in [BASE[2][:len(picks)],BASE[6][:len(picks)]],f'{name}: unknown fallback changed'
            teams.append(dict(side=side,picks=picks,body_count=len(bodies)))
        reports.append(dict(map=name,teams=teams,identity_trace=traces))
    return dict(games=len(results),draft_turns=sum(len(t['picks']) for r in reports for t in r['teams']),
                sizes=sorted({len(t['picks']) for r in reports for t in r['teams']}),passed=True,matches=reports)
def main():
    p=argparse.ArgumentParser();p.add_argument('results');p.add_argument('--strict-trace',action='store_true');p.add_argument('--unknown',action='store_true');p.add_argument('--out',required=True);a=p.parse_args()
    report=validate(json.loads(pathlib.Path(a.results).read_text()),a.strict_trace,a.unknown)
    pathlib.Path(a.out).write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['games','draft_turns','sizes','passed']}))
if __name__=='__main__':main()
