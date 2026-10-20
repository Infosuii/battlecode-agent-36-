"""Additional actual-engine boundary scenarios; receipts, not win evidence."""
import json
from engine_oracle import Oracle,Engine
from evaluate import ROOT,WORK,sha,write

def main():
    e=Engine(WORK/'development/engine-current.wasm');rows=[]
    o=Oracle(e);o.draft();snap=o.snapshot();camp=next(c for c in snap['camps'] if c['type']==0)
    group=camp['group'];x,y=camp['pos'];o.teleport(0,x-1,y)
    before=[c for c in o.snapshot()['camps'] if c['group']==group]
    o.e.call('Game_Action_Attack',o.h,o.entity(camp['idx'],2));after=[c for c in o.snapshot()['camps'] if c['group']==group]
    assert all(c['awake']==1 for c in after)
    o.teleport(0,2,2);o.e.call('Game_API_GetSettings',o.args);sleep=e.i32(o.args+4)
    target_round=o.snapshot()['header']['round']+sleep+3;turns=[]
    for _ in range(2000):
        h=o.snapshot()['header']
        if h['round']>=target_round:break
        if h['phase']==10 and h['awaiting']:
            turns.append((h['round'],h['team'],h['cur_champ']));o.finish()
        else:o.advance()
    slept=[c for c in o.snapshot()['camps'] if c['group']==group]
    assert slept and all(c['awake']==0 for c in slept)
    leads={}
    for r,t,c in turns:leads.setdefault(r,t)
    rows.append(dict(id='camp_real_attack_wake_and_sleep',passed=True,camp_sleep_setting=sleep,before=before,after_attack=after,after_sleep=slept,
        observed_round_first_teams=leads,scope='Real champion attack then idle no nearby champions; engine phases advanced via runner.'))
    o.close()
    o=Oracle(e);o.draft([2,9,0,8,7]);x,y=o.arena();o.teleport(0,x,y);o.teleport(5,x+2,y);o.damage(0,5,970.0)
    first=o.cast(1,0xffffffff,x+1,y);o.e.call('Game_Action_Attack',o.h,o.entity(5));after=o.snapshot()['champions'][0];second=o.cast(0,0)
    assert first['success'] and after['cd']==(0,0) and second['code']==27
    rows.append(dict(id='vendetta_kill_resets_cooldowns_not_cast_allowance',passed=True,first_cast=first,after_kill=after,second_cast=second));o.close()
    for mode in ['empty','intermediate_occupied','landing_occupied']:
        o=Oracle(e);o.draft([8,9,0,4,7]);x,y=o.arena();o.teleport(0,x,y)
        if mode!='empty':o.teleport(5,x+(1 if mode=='intermediate_occupied' else 2),y)
        before=o.snapshot()['champions'][0];result=o.cast(1,0xffffffff,x+2,y);after=o.snapshot()['champions'][0]
        assert (result['code']==32 and after['pos']==before['pos']) if mode=='landing_occupied' else (result['success'] and after['pos']==(x+2,y))
        rows.append(dict(id='ren_dash_'+mode,passed=True,before=before['pos'],result=result,after=after['pos'],
            simulator_limitation='Model uses ordinary greedy substeps and cannot reproduce jumping over occupied intermediate tiles.'))
        o.close()
    report=dict(engine_wasm_sha256=sha(WORK/'development/engine-current.wasm'),fixtures=rows)
    write(ROOT/'evidence/engine-boundaries.json',report);print(json.dumps(report),flush=True)

if __name__=='__main__':main()
