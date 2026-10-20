"""Build a versioned observational pilot dataset from framed, decision-time telemetry.

Only selected decisions receive observed labels. Incomplete/failed episodes retain
their status. This pilot is not policy promotion evidence or a complete v37 dataset.
"""
import sys,json,pathlib,subprocess,collections,hashlib
from lab import ROOT,WORK,write,sha,digest
from replay_bundle import read_bundle
NODE=pathlib.Path('C:/Users/ishan/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe')
LENGTHS={'V37D':14,'V37A':10,'V37E':8,'V37C':12,'V37F':18,'V37S':9,'V37M':11}
def main():
    old=WORK/'v36-execution/tools/agent4/stdout_contexts.mjs'
    s=old.read_text().replace("path.join(root,'../development/engine-current.wasm')","new URL('engine-current.wasm',import.meta.url)").replace('184696','184760')
    (ROOT/'tools/contexts.mjs').write_text(s,newline='\n')
    results=json.loads((ROOT/'results/trace-6-v2.json').read_text())
    # Whole physical maps, including both sides and all repeated episodes, stay together.
    maps=sorted({r['assignment']['group'] for r in results})
    split={m:('train' if i<4 else 'validation' if i==4 else 'test') for i,m in enumerate(maps)}
    write(ROOT/'ml/splits.json',dict(schema=1,groups=split,seed=None,rule='sorted physical map SHA256; first four train, fifth validation, sixth pilot test; pilot test consumed during label-quality corrections, not a fresh final test',
        final_policy_holdout=False,opponent='selected v36 only',time_separation=False))
    decisions=[];candidate_rows=[];issues=collections.Counter();modes=collections.Counter();economy_examples=[]
    for r in results:
        j=r['assignment'];folder=ROOT/'runs'/j['id'];out=ROOT/'ml/raw'/j['id'];out.mkdir(parents=True,exist_ok=True)
        raw=read_bundle(next(folder.glob('*.repl26a')));side=j['side'];body_team=('teamA','teamB')[side]
        frames=[dict(position=i,text=t) for i,t in enumerate(raw[f'team-{"ab"[side]}.stdout'].decode().split('\0')) if t]
        rp=out/'raw.repl26a';rp.write_bytes(raw['replay.repl26a']);pos=out/'positions.json';write(pos,[f['position'] for f in frames]);ctx=out/'contexts.json'
        if not ctx.exists():
            p=subprocess.run([str(NODE),str(ROOT/'tools/contexts.mjs'),str(rp),str(pos),str(ctx)],capture_output=True,text=True,timeout=180)
            if p.returncode:raise RuntimeError(p.stderr)
        ctx_data=json.loads(ctx.read_text());contexts=ctx_data['contexts']
        controllers={(v['round'],v['champion'],v['team']) for v in ctx_data['controller_turns']}
        valid=[];body_modes=collections.defaultdict(list)
        for f in frames:
            context=contexts.get(str(f['position']))
            if not context or context.get('alternatives') or context['team']!=side:
                issues['unjoined_or_ambiguous_frame']+=1;continue
            parts=[l.split(',') for l in f['text'].splitlines() if l]
            if any(p[0] not in LENGTHS or len(p)!=LENGTHS[p[0]] for p in parts):
                issues['partial_or_malformed_frame']+=1;continue
            good=True
            for p in parts:
                declared=(int(p[1]),int(p[2]),side)
                if declared not in controllers or int(p[2])!=context['champion'] or not 0<=context['round']-int(p[1])<=1:good=False
                elif int(p[1])!=context['round']:issues['one_round_deferred_stdout_line']+=1
            if not good:issues['context_join_mismatch']+=1;continue
            valid.append((context,parts))
            for p in parts:
                if p[0]=='V37M':
                    body_modes[int(p[2])].append(dict(round=int(p[1]),mode=p[3],group=int(p[4]),hp=int(p[7])/100,bag=int(p[8]),gold=int(p[9]),heading_home=int(p[10])))
                    modes[p[3]]+=1
                    if p[3] in ('home','home-escape') and len(economy_examples)<20:economy_examples.append(dict(match=j['id'],frame=f['position'],row=p))
        stats=json.loads((folder/'statistics.json').read_text());events=[]
        for b in stats['events']:
            for e in b['entries']:events.append(dict(kind=b['kind'],owner=b['owner'],**e))
        for context,parts in valid:
            d=next((p for p in parts if p[0]=='V37D'),None);selected=next((p for p in parts if p[0]=='V37S'),None)
            if not d or not selected:continue
            round_=int(d[1]);body=int(d[2]);gid=int(selected[4]);call=int(d[3])
            scored={int(p[4]):p for p in parts if p[0]=='V37F' and int(p[3])==call}
            cs={int(p[4]):p for p in parts if p[0]=='V37C' and int(p[3])==call}
            for g,c in cs.items():candidate_rows.append(dict(match=j['id'],map=j['group'],round=round_,body=body,call=call,candidate=g,status=c[5],selected=g==gid,observed_outcome=None))
            if gid not in scored or gid not in cs:issues['selected_without_complete_feature_row']+=1;continue
            f=scored[gid];c=cs[gid]
            current=[m for m in body_modes[body] if m['round']==round_ and m['group']==gid]
            if not current:issues['not_final_executed_target']+=1;continue
            ambiguous_same_round=any(e['timestamp']['round']==round_ and
                ((e['kind']=='death' and e['owner'].get('id')==body and e['owner'].get('team')==body_team) or
                 (e['kind']=='gemCollection' and e['context']=='campGroup' and e['owner'].get('team')==body_team and e['target']['id']==gid)) for e in events)
            if ambiguous_same_round:issues['same_round_label_order_ambiguous']+=1;continue
            hp=int(d[7])/100;maxhp=int(d[8])/100
            x=[hp/max(maxhp,1),int(d[9]),int(d[10]),int(c[6]),int(c[7]),int(c[8]),int(c[9]),int(c[10]),int(c[11])/100,
               int(f[5])/10000,int(f[6])/100,int(f[7])/100,int(f[8])/100,*[int(v) for v in f[9:18]],int(d[12]),int(d[13])]
            # Candidate group index follows SDK enumeration; verify metadata in separate map audit before activation.
            subsequent=[m for m in body_modes[body] if m['round']>round_]
            farm_modes={'approach','fight','capture','to-capture','stage','wait-respawn','siege','bush-ward'}
            abandon=min((m['round'] for m in subsequent if m['group']!=gid or m['mode'] not in farm_modes),default=stats['match']['roundsStarted']+1)
            own=[e for e in events if e['owner'].get('id')==body and e['owner'].get('team')==body_team and e['timestamp']['round']>round_]
            death=min((e['timestamp']['round'] for e in own if e['kind']=='death'),default=stats['match']['roundsStarted']+1)
            captures=[e['timestamp']['round'] for e in events if e['kind']=='gemCollection' and e['context']=='campGroup' and e['owner'].get('team')==body_team and e['target']['id']==gid and e['timestamp']['round']>round_]
            capture=min(captures,default=stats['match']['roundsStarted']+1)
            stop=min(abandon,death,capture,stats['match']['roundsStarted'])
            status='collected' if capture==stop else 'death' if death==stop else 'abandoned' if abandon==stop else 'right_censored'
            decisions.append(dict(schema=1,match=j['id'],engine=r['match']['engineBuildHash'],map=j['group'],opponent=r['key']['wasm'][1-side],
                policy=r['key']['wasm'][side],source=r['key']['source'][side],split=split[j['group']],decision_round=round_,body=body,
                link_pick=next(c['draftPick'] for c in stats['roster']['champions'] if c['id']==body),candidate=gid,call=call,x=x,
                label=dict(status=status,elapsed=stop-round_,completion_round=capture if status=='collected' else None,
                    complete_within_50=int(status=='collected' and stop-round_<=50) if stop-round_>=50 or status!='right_censored' else None,
                    interruption=int(status in ('death','abandoned')) if status!='right_censored' else None,lost_bag=sum(int(e['value']) for e in own if e['kind']=='gemLoss' and e['timestamp']['round']<=stop)),
                missing=['shield','individual camp awake','status','preorder exclusions','full candidate geometric anchors'],
                synthetic=False,instrumented=True))
    path=ROOT/'ml/decisions.jsonl';path.write_text(''.join(json.dumps(d)+'\n' for d in decisions))
    cp=ROOT/'ml/candidates.jsonl';cp.write_text(''.join(json.dumps(c)+'\n' for c in candidate_rows))
    write(ROOT/'ml/data-quality.json',dict(rows=len(decisions),candidate_rows=len(candidate_rows),issues=dict(issues),
        dataset_sha256=sha(path),candidate_sha256=sha(cp),splits=collections.Counter(d['split'] for d in decisions),
        labels=collections.Counter(d['label']['status'] for d in decisions),mode_counts=dict(modes),economy_examples=economy_examples,
        not_activation_ready=['telemetry changes fuel/decisions','only six physical map groups','excluded reason coverage incomplete','camp-group index join needs geometry-level oracle','damage/recovery/secure episode labels not yet complete'],
        selection_bias='Only selected observed decisions have labels. All unselected outcomes null. Instrumentation policy differs from production.',
        inputs='Only bot-emitted values from the decision frame; replay/native future events supply labels only.'))
    print(json.dumps(dict(rows=len(decisions),issues=dict(issues),splits=collections.Counter(d['split'] for d in decisions),modes=dict(modes))))
if __name__=='__main__':main()
