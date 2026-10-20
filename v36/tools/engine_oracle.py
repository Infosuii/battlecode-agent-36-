"""Independent scenarios through the recovered official engine's visualiser API.

Function signatures/struct encodings are taken from the bundled visualiser JS,
not invented. Sandbox mutations create fixtures and are never match evidence.
"""
import json, pathlib, struct, sys
from evaluate import ROOT, WORK, PROJECT, sha, write
sys.path[:0]=[str(WORK/'runtime'),str(WORK/'battlecode-preparation/analytics')]
from battlecode_analytics.engine import Engine, header, champions_full, camps_full

class Oracle:
    def __init__(self, engine=None):
        self.e=engine or Engine(WORK/'development/engine-current.wasm');e=self.e
        self.snap=e.malloc(184696);self.persp=e.malloc(16);self.args=e.malloc(256)
        e.write(self.persp,struct.pack('<II',2,0xffffffff))
        raw=(PROJECT/'maps/rows.map26a').read_bytes();p=e.malloc(len(raw));e.write(p,raw)
        e.call('Game_FromMapFileBytes',p,len(raw),e.out,0);self.h=e.u64(e.out);e.f('Engine_Free',p)
        e.call('Game_Runner_Init',self.h)
    def state(self):
        self.e.call('Game_Runner_GetState',self.h,self.args)
        return dict(zip(['phase','team','round','champion'],struct.unpack('<4I',self.e.read(self.args,16))))
    def snapshot(self):
        self.e.call('Game_Visualiser_ReadState',self.h,self.persp,self.snap)
        b=self.e.read(self.snap,184696)
        return dict(header=header(b),champions=champions_full(b),camps=camps_full(b))
    def entity(self,index,kind=1,offset=0):
        self.e.write(self.args+offset,struct.pack('<II',kind,index));return self.args+offset
    def position(self,x,y):
        self.e.write(self.args+64,struct.pack('<ii',x,y));return self.args+64
    def advance(self):self.e.call('Game_Runner_Advance',self.h)
    def finish(self):self.e.call('Game_Runner_EndTurn',self.h,0)
    def draft(self,picks=None):
        picks=picks or [9,0,8,4,7];counts=[0,0]
        history=[]
        for _ in range(100):
            state=self.snapshot()['header'];history.append(state)
            if state['phase']==12:
                t=state['team'];self.e.call('Game_Action_Draft',self.h,picks[counts[t]]);counts[t]+=1;self.finish()
            elif state['phase']==10:return history
            else:self.advance()
        raise RuntimeError('Draft/input loop did not converge '+str(history[-5:]))
    def cast(self,slot,idx=0xffffffff,x=-1,y=-1,kind=1):
        rc=self.e.f('Game_Action_Cast',self.h,slot,self.entity(idx,kind),self.position(x,y),self.e.out)
        return dict(code=rc,message=self.e.cstr(self.e.f('GetLastErrorMsg')) if rc else '',success=bool(self.e.i32(self.e.out)) if not rc else False)
    def teleport(self,idx,x,y):self.e.call('Game_Sandbox_Teleport',self.h,self.entity(idx),self.position(x,y))
    def damage(self,src,dst,amount):self.e.call('Game_Sandbox_Damage',self.h,self.entity(src),self.entity(dst,offset=16),amount)
    def arena(self):
        e=self.e; boards=[]
        for fn in ['Game_GetWallBuffer','Game_GetHoleBuffer','Game_GetAltarBuffer']:
            e.call(fn,self.h,e.out);ptr=e.u32(e.out);boards.append(ptr)
        e.call('Game_GetBitboardMetadata',self.h,e.out);w,h,cw,ch=struct.unpack('<4i',e.read(e.out,16))
        def clear(x,y):
            chunk=(y//8)*cw+x//8;bit=(y%8)*8+x%8
            return not any(e.u64(p+chunk*8)>>bit&1 for p in boards)
        for y in range(8,h-8):
            for x in range(8,w-8):
                if all(clear(x+dx,y+dy) for dx in range(3) for dy in range(3)):return x,y
        raise RuntimeError('No open arena')
    def close(self):
        self.e.call('Game_Destroy',self.h)
        for p in [self.snap,self.persp,self.args]:self.e.f('Engine_Free',p)

def fixtures():
    results=[];engine=Engine(WORK/'development/engine-current.wasm')
    o=Oracle(engine);o.draft();before=o.snapshot()['champions'][0]
    first=o.cast(1,0);second=o.cast(0,0);after=o.snapshot()['champions'][0]
    assert first['success'] and second['code']==27 and after['cd']==(0,8)
    results.append(dict(id='one_cast',before=before,first=first,second=second,after=after,passed=True));o.close()
    o=Oracle(engine);o.draft();x,y=o.arena();o.teleport(0,x,y);o.teleport(6,x+1,y);o.damage(0,6,860.0)
    before=o.snapshot()['champions'];o.e.call('Game_Action_Attack',o.h,o.entity(6));after=o.snapshot()['champions']
    assert before[0]['hp']-after[0]['hp']==20.0 and not after[6]['alive']
    results.append(dict(id='ren_lethal_retaliation',passed=True,before=before,after=after,attacker_hp_loss=before[0]['hp']-after[0]['hp'],
        target_died=not after[6]['alive'],scope='observed engine transition; simulator comparison performed separately'));o.close()
    o=Oracle(engine);o.draft();x,y=o.arena();o.teleport(0,x,y);o.teleport(4,x+1,y);o.teleport(5,x+1,y+1)
    before=o.snapshot()['champions'];o.e.call('Game_Action_Attack',o.h,o.entity(4));after1=o.snapshot()['champions']
    second=o.e.f('Game_Action_Attack',o.h,o.entity(4));reset=o.cast(0,0);o.e.call('Game_Action_Attack',o.h,o.entity(4));after2=o.snapshot()['champions']
    assert before[4]['hp']-after1[4]['hp']==50 and before[5]['hp']-after1[5]['hp']==50
    assert second!=0 and reset['success'] and before[4]['hp']-after2[4]['hp']==100
    results.append(dict(id='varg_splash_and_reset_allowance',passed=True,before=before,after_first=after1,second_attack_code=second,
        reset=reset,after_reset_attack=after2));o.close()
    o=Oracle(engine);o.draft([4,9,0,8,7]);x,y=o.arena();o.teleport(0,x,y);o.teleport(4,x+2,y)
    o.e.call('Game_Action_Move',o.h,x,y+1,o.e.out);move=bool(o.e.i32(o.e.out));attack=o.e.f('Game_Action_Attack',o.h,o.entity(4))
    assert move and attack!=0
    results.append(dict(id='ranged_move_excludes_attack',passed=True,move_success=move,attack_error_code=attack,state=o.snapshot()['champions'][0]));o.close()
    o=Oracle(engine);o.draft([3,9,0,8,7]);s=o.snapshot()['champions'];me=next(c for c in s if c['team']==0 and c['type']=='Cassi')
    enemies=[c for c in s if c['team']==1 and c['type'] in ['Cassi','Issac']];assert len(enemies)==2
    x,y=o.arena();o.teleport(me['idx'],x,y)
    for i,c in enumerate(enemies):
        o.teleport(c['idx'],x+1,y+i);o.e.call('Game_Sandbox_GiveGems',o.h,c['idx'],12+i)
    target=enemies[0];o.damage(me['idx'],target['idx'],target['hp']-10)
    before=o.snapshot()['champions'];o.e.call('Game_Action_Attack',o.h,o.entity(target['idx']));after=o.snapshot()['champions']
    assert all(not after[c['idx']]['alive'] for c in enemies) and after[me['idx']]['gems']==25
    results.append(dict(id='linked_death_and_bags',passed=True,before=before,after=after));o.close()
    o=Oracle(engine);o.draft();s=o.snapshot();c=next(c for c in s['camps'] if c['type']==0)
    group=[q for q in s['camps'] if q['group']==c['group']];o.e.call('Game_Sandbox_Damage',o.h,o.entity(0),o.entity(c['idx'],2,16),1.0)
    after=[q for q in o.snapshot()['camps'] if q['group']==c['group']]
    results.append(dict(id='camp_group_awake',before=group,after=after,
        observation='Sandbox damage behavior; real champion attack and sleep-duration validation still required'));o.close()
    report=dict(engine_wasm_sha256=sha(WORK/'development/engine-current.wasm'),fixtures=results,
        scope='Official engine visualiser sandbox scenarios, no policy/full-match performance claim',
        remaining=['alternating round order','reset cooldown on kills','camp sleep/targeting','dash paths','long horizons'])
    write(ROOT/'evidence/engine-oracle.json',report)
    print(json.dumps(dict(fixtures=[{k:r.get(k) for k in ['id','passed','attacker_hp_loss','target_died']} for r in results])),flush=True)

if __name__=='__main__':
    fixtures()
