from pathlib import Path
import hashlib,json,re,zipfile
ROOT=Path(__file__).resolve().parents[3]
def main():
    results=[]
    for name in ['ren','varg']:
        folder=ROOT/'work/recovery/results'/('camp-'+name)
        stats=json.loads((folder/'stats.json').read_text())
        assert stats['match']['engineBuildHash']=='d7e29ae3546357b011419c764204b7b9e2232bdb1b07868250c6b35d4c18d2a5'
        assert sum(stats['diagnostics']['botFaults'].values())==0
        bundle=next(folder.glob('*.repl26a'))
        with zipfile.ZipFile(bundle) as z:text=z.read('team-a.stdout').decode('utf8',errors='replace')
        assert 'PROBE_ERROR' not in text
        lines=re.findall(r'(?:CAMP_PROBE|CAMP_STATE|HIT|RECALL_STARTED)[^\r\n]*',text)
        camp={};champ={}
        for line in lines:
            match=re.fullmatch(r'CAMP_STATE R(\d+) x=(\d+) y=(\d+) state=(\d+) hp=([\d.]+) max=([\d.]+)',line)
            if match:
                r,x,y,state,hp,mx=match.groups();camp[int(r),int(x),int(y)]={'state':int(state),'hp':float(hp),'max':float(mx)}
            match=re.fullmatch(r'CAMP_PROBE R(\d+) type=(\d+) pos=(\d+),(\d+) hp=([\d.]+) recalling=(\d+) sleep_time=(\d+)',line)
            if match:
                r,kind,x,y,hp,recalling,sleep=match.groups();champ[int(r)]={'pos':[int(x),int(y)],'hp':float(hp),'recalling':bool(int(recalling)),'sleep_time':int(sleep)}
        hit=int(re.search(r'HIT R(\d+)',text)[1])
        assert camp[1,6,2]['state']==2
        assert camp[hit+1,6,2]['state']==1
        assert all(camp[18,x,2]['state']==2 for x in [6,7,8])
        assert 'RECALL_STARTED R20' in text
        assert all(champ[r]['recalling'] for r in range(21,25))
        assert champ[25]['pos'][0]<3 and champ[25]['pos'][1]<3 and not champ[25]['recalling']
        assert champ[25]['hp']==champ[20]['hp'] and champ[26]['hp']>champ[25]['hp']
        assertions=['initial dormant camp','attack wakes group','selected group sleeps after departure',
                    'safe recall remains uninterrupted','recall completes at round25','arrival turn-end heal visible next turn']
        if name=='varg':
            healthy=camp[hit+1,8,2]
            assert healthy['state']==1 and healthy['hp']==healthy['max']
            assert camp[hit+1,5,3]['state']==1 and camp[hit+1,5,3]['hp']<camp[hit+1,5,3]['max']
            assert camp[hit+4,6,2]['state']==1 and camp[hit+4,6,2]['hp']==camp[hit+1,6,2]['hp']
            assert camp[11,8,2]['state']==2 and camp[11,6,2]['state']==1
            # The diagonal camp remains awake: an idle ally on spawn is still
            # in range. Departure of the active champion is not sufficient.
            assert camp[18,5,3]['state']==1
            tavi=next(c for c in stats['champions'] if c['team']=='teamA' and c['name']=='Tavi')
            assert float(tavi['metrics']['monsterDamageTaken'])>0
            assertions+=['healthy unhit member wakes','splash wakes separate diagonal group',
                         'no further damage does not sleep in-range camp','group members can later have different awake states',
                         'idle spawn ally keeps diagonal camp awake while recaller remains safe outside range']
        else:
            assert camp[hit+1,5,3]['state']==2
            assertions+=['single-target attack leaves separate diagonal group asleep']
        (folder/'observations.txt').write_text('\n'.join(lines)+'\n',encoding='utf8')
        results.append({'name':name,'status':'passed','assertions':assertions,'hit_round':hit,
                        'engine_build_hash':stats['match']['engineBuildHash'],
                        'bundle_sha256':hashlib.sha256(bundle.read_bytes()).hexdigest(),
                        'observations_sha256':hashlib.sha256((folder/'observations.txt').read_bytes()).hexdigest()})
    (ROOT/'analysis/v36/recovery/native-camp-validation.json').write_text(json.dumps(results,indent=2)+'\n')
    print('Native camp/recall assertions passed:',sum(len(r['assertions']) for r in results))
if __name__=='__main__':main()
