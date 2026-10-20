"""Controlled 1.0.1 action/phase checks using the inherited official-engine harness."""
import sys,struct,json
from lab import ROOT,WORK,write,sha
sys.path.insert(0,str(WORK/'runtime'))
from oracle import Oracle,Engine
def main():
    engine=Engine(ROOT/'tools/engine-current.wasm');rows=[]
    def test(name,fn):
        try:rows.append(dict(id=name,passed=True,observed=fn()))
        except Exception as e:rows.append(dict(id=name,passed=False,error=repr(e)))
    def casts():
        o=Oracle(engine)
        try:
            o.draft();first=o.cast(1,0);second=o.cast(0,0)
            assert first['success'] and not second['success']
            return dict(first=first,second=second,state=o.snapshot()['champions'][0])
        finally:o.close()
    test('successful_cast_consumes_shared_cast_allowance',casts)
    def linked():
        o=Oracle(engine)
        try:
            o.draft([3,9,0,8,7]);s=o.snapshot()['champions'];att=next(c for c in s if c['team']==0 and c['type']=='Cassi')
            targets=[c for c in s if c['team']==1 and c['type'] in ['Cassi','Issac']]
            x,y=o.arena();o.teleport(att['idx'],x,y)
            for k,c in enumerate(targets):
                o.teleport(c['idx'],x+1,y+k);o.e.call('Game_Sandbox_GiveGems',o.h,c['idx'],12+k)
            before=o.snapshot()['champions'];assert sorted(c['gems'] for c in before if c['idx'] in [t['idx'] for t in targets])==[12,13]
            target=targets[0];o.damage(att['idx'],target['idx'],target['hp']-10)
            o.e.call('Game_Action_Attack',o.h,o.entity(target['idx']));after=o.snapshot()['champions']
            assert all(not after[c['idx']]['alive'] for c in targets) and after[att['idx']]['gems']==25
            return dict(wallets='distinct physical bodies',linked_death=True,bag_transfer=25,before=before,after=after)
        finally:o.close()
    test('linked_bodies_distinct_bags_shared_death',linked)
    def dash(mode):
        o=Oracle(engine)
        try:
            o.draft([8,9,0,4,7]);x,y=o.arena();o.teleport(0,x,y)
            if mode!='open':o.teleport(5,x+(1 if mode=='intermediate_occupied' else 2),y)
            before=o.snapshot()['champions'][0];r=o.cast(1,0xffffffff,x+2,y);after=o.snapshot()['champions'][0]
            if mode=='landing_occupied':assert not r['success'] and after['pos']==before['pos']
            else:assert r['success'] and after['pos']==(x+2,y)
            return dict(result=r,before=before['pos'],after=after['pos'],simulator_authority='unchanged; dash path mismatch requires bounded fallback')
        finally:o.close()
    for mode in ('open','intermediate_occupied','landing_occupied'):test('ren_dash_'+mode,lambda m=mode:dash(m))
    def ranged():
        o=Oracle(engine)
        try:
            o.draft([4,9,0,8,7]);x,y=o.arena();o.teleport(0,x,y);o.teleport(4,x+2,y)
            o.e.call('Game_Action_Move',o.h,x,y+1,o.e.out);m=bool(o.e.i32(o.e.out));a=o.e.f('Game_Action_Attack',o.h,o.entity(4))
            assert m and a!=0
            return dict(move=m,attack_error=a)
        finally:o.close()
    test('ranged_move_excludes_basic_attack',ranged)
    def sleep():
        o=Oracle(engine)
        try:
            o.draft();s=o.snapshot();c=next(c for c in s['camps'] if c['type']==0);x,y=c['pos'];group=c['group']
            o.teleport(0,x-1,y);o.e.call('Game_Action_Attack',o.h,o.entity(c['idx'],2))
            after=[c for c in o.snapshot()['camps'] if c['group']==group];assert all(c['awake']==1 for c in after)
            o.teleport(0,2,2);o.e.call('Game_API_GetSettings',o.args);duration=o.e.i32(o.args+4)
            end=o.snapshot()['header']['round']+duration+3
            for _ in range(3000):
                h=o.snapshot()['header']
                if h['round']>=end:break
                if h['phase']==10 and h['awaiting']:o.finish()
                else:o.advance()
            slept=[c for c in o.snapshot()['camps'] if c['group']==group];assert all(c['awake']==0 for c in slept)
            return dict(real_attack_wakes_healthy_members=True,sleep_rounds=duration,after_attack=after,after_sleep=slept)
        finally:o.close()
    test('actual_attack_wakes_group_then_sleep_transition',sleep)
    write(ROOT/'evidence/oracle-1.0.1.json',dict(engine_wasm_sha256=sha(ROOT/'tools/engine-current.wasm'),fixtures=rows,
        authority_expanded=False,remaining=['dash through holes/walls','Heart of Ruin dash','shield/status expiry boundaries','tank leaving during next camp phase','boxed respawns','dense stale threats','longer simulator-vs-oracle horizons']))
    print(json.dumps([dict(id=r['id'],passed=r['passed'],error=r.get('error')) for r in rows]))
if __name__=='__main__':main()
