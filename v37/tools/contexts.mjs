// Join official NUL-log positions to actual awaiting-input controller snapshots.
// No actor is guessed from champion order, output round text, or file byte count.
import fs from 'node:fs';import path from 'node:path';import {WASI} from 'node:wasi';
const [rawReplay,positionsFile,output]=process.argv.slice(2);if(!output)throw Error('rawReplay positions.json output.json');
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(?:([A-Z]:))/,'$1')),'../..');
const wanted=new Set(JSON.parse(fs.readFileSync(positionsFile))),w=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
const m=await WebAssembly.compile(fs.readFileSync(new URL('engine-current.wasm',import.meta.url))),instance=await WebAssembly.instantiate(m,w.getImportObject());w.initialize(instance);const e=instance.exports;
const bytes=fs.readFileSync(rawReplay),p=e.Engine_Malloc(bytes.length),out=e.Engine_Malloc(8192),snap=e.Engine_Malloc(184760),pers=e.Engine_Malloc(16);new Uint8Array(e.memory.buffer).set(bytes,p);
const v=()=>new DataView(e.memory.buffer),u=off=>v().getUint32(snap+off,true);v().setUint32(pers,2,true);v().setUint32(pers+4,0xffffffff,true);
const call=(name,...args)=>{let rc=e[name](...args);if(rc)throw Error(`${name} ${rc}`);};call('Game_FromReplayFileBytes',p,bytes.length,out);const game=v().getBigUint64(out,true);
let steps=0,allControllers=0;const contexts={},turns=new Map(),rollovers=[];
while(true){
 call('Game_Visualiser_ReadState',game,pers,snap);if(u(0)!==1||u(4)!==184760)throw Error('Unsupported snapshot ABI');
 const round=u(8),phase=u(24),position=u(100),awaiting=u(20),champ=u(60),kind=u(68),actorKind=u(72),actor=u(76),substep=u(104);
 if(kind===26&&actorKind===1)rollovers.push({round,position,actor,substep,step:steps});
 if(awaiting&&phase===10){
  const count=u(120);let team=-1,type=-1;
  for(let k=0;k<count;k++){const base=140+456*k;if(u(base+4)===champ){team=u(base+8);type=u(base+12);break;}}
  if(team<0||team>1)throw Error('Awaiting input without valid champion');
  turns.set(`${round}/${champ}`,{round,champion:champ,team});allControllers++;
  if(wanted.has(position)){
   const context={round,champion:champ,team,type,position,phase,step:steps};
   const previous=contexts[position];
   if(previous&&(previous.round!==round||previous.champion!==champ||previous.team!==team)){
    (previous.alternatives??=[]).push(context);
   }else contexts[position]=context;
  }
 }
 if(phase===11)break;if(++steps>1000000)throw Error('Replay step limit');call('Game_Visualiser_Timeline_ReplayNext',game,1);
}
fs.writeFileSync(output,JSON.stringify({schema:1,contexts,missing_positions:[...wanted].filter(p=>!contexts[p]),
 total_replay_steps:steps,awaiting_input_snapshots:allControllers,controller_turns:[...turns.values()],rollovers},null,2));
console.log(JSON.stringify({contexts:Object.keys(contexts).length,missing:[...wanted].filter(p=>!contexts[p]).length,turns:turns.size,steps}));
