// Compact deterministic replay extraction using the official current engine.
// Omniscient snapshots are used only for descriptive evidence, never policy inputs.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';
import {WASI} from 'node:wasi';
const source=process.argv[2],dest=process.argv[3];
if(!source||!dest)throw Error('usage: fast_decode.mjs replay output.json.gz');
const start=performance.now(),raw=fs.readFileSync(source);
const mod=await WebAssembly.compile(fs.readFileSync(new URL('engine-current.wasm',import.meta.url)));
const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
const inst=await WebAssembly.instantiate(mod,wasi.getImportObject());wasi.initialize(inst);const e=inst.exports;
const snapshotSize=e.Game_Visualiser_SnapshotSize();
// SDK 3.0.2's official PanelChrome parser supports both layouts; table offsets are unchanged.
if(![184696,184760].includes(snapshotSize))throw Error('Unsupported snapshot size '+snapshotSize);
const p=e.Engine_Malloc(raw.length),out=e.Engine_Malloc(8192),snap=e.Engine_Malloc(snapshotSize),pers=e.Engine_Malloc(16);
new Uint8Array(e.memory.buffer).set(raw,p);let v=new DataView(e.memory.buffer);
v.setUint32(pers,2,true);v.setUint32(pers+4,0xffffffff,true);
const check=(rc)=>{if(rc)throw Error('engine error '+rc);};
check(e.Game_FromReplayFileBytes(p,raw.length,out));v=new DataView(e.memory.buffer);const h=v.getBigUint64(out,true);
const u=o=>v.getUint32(snap+o,true),i=o=>v.getInt32(snap+o,true),f=o=>v.getFloat32(snap+o,true);
check(e.Game_GetBitboardMetadata(h,out));v=new DataView(e.memory.buffer);
const width=v.getInt32(out,true),height=v.getInt32(out+4,true),cw=v.getUint32(out+8,true),ch=v.getUint32(out+12,true),stride=v.getUint32(out+16,true);
check(e.Game_GetBushBuffer(h,out));v=new DataView(e.memory.buffer);let bushptr=v.getUint32(out,true);
const bushes=Buffer.from(new Uint8Array(e.memory.buffer,bushptr,stride*ch*8));
const inBush=(x,y)=>{if(x<0||y<0||x>=width||y>=height)return false;let word=Math.floor(y/8)*stride+Math.floor(x/8),bit=(y%8)*8+x%8;return Boolean(bushes.readBigUInt64LE(word*8)&(1n<<BigInt(bit)));};
check(e.Game_Visualiser_ReadState(h,pers,snap));v=new DataView(e.memory.buffer);
if(u(0)!==1||u(4)!==snapshotSize)throw Error('Invalid ABI header');
// Physical bodies and runtime camp tables are populated only when drafting ends.
let prefaceSteps=0;
while(u(24)===12||u(120)===0){check(e.Game_Visualiser_Timeline_ReplayNext(h,1));check(e.Game_Visualiser_ReadState(h,pers,snap));v=new DataView(e.memory.buffer);if(++prefaceSteps>1000)throw Error('Draft never completed');}
const n=u(120),nc=u(124),ng=u(128),C=24;
if(n<6||n>16||nc===0||ng===0)throw Error('Invalid runtime tables');
let before=new Float64Array(16*C),after=new Float64Array(16*C),campHP=new Float32Array(nc),campMask=new Uint32Array(nc),groupState=new Uint32Array(ng);
const campGroup=new Uint32Array(nc),campXY=new Int32Array(nc*2),dealers=Array.from({length:ng},()=>[new Set(),new Set()]);
const metrics=Array.from({length:2},()=>({actions:{},casts_at_camps:0,casts_at_enemies:0,camp_stuns_observed:0,camp_damage_taken:0,champion_damage_taken:0,entity_deaths:0,linked_deaths:0,gems_lost_on_death:0,captured_gems:0,captures:0,multi_dealer_captures:0,solo_collector_captures:0,wards:0,bush_wards:0,locks:0,priority_calls:0,priority_bands:{},low_hp_contact_turns:0,low_hp_away_or_recall_turns:0,spell_openers:0,fight_openers:0,bags:[],first_bank_round:null,recast_gaps:[],banked_curve:[],monster_items_final_equiv:0}));
const lastCast=Array.from({length:16},()=>[-10000,-10000]),turns=Array(16).fill(null);
const events=[],roundStates=[],deaths=[],purchases=[],transitions=[],gemFlows=[];
function readChamps(target){
 for(let k=0;k<n;k++){let o=140+456*k,id=u(o+4),a=id*C;
  target[a]=u(o+8);target[a+1]=u(o+12);target[a+2]=i(o+20);target[a+3]=i(o+24);target[a+4]=f(o+28);target[a+5]=f(o+32);target[a+6]=u(o+224);target[a+7]=u(o+232);target[a+8]=u(o+236);target[a+9]=f(o+112);target[a+10]=u(o+92);target[a+11]=u(o+96);target[a+12]=u(o+440);target[a+13]=i(o+436);target[a+14]=f(o+148);target[a+15]=f(o+132);target[a+16]=i(o+444);target[a+17]=u(o+16);target[a+18]=u(o+428);target[a+19]=u(184504+12*id);target[a+20]=u(184508+12*id);target[a+21]=u(184512+12*id);
 }
}
readChamps(before);
for(let k=0;k<nc;k++){let o=7436+232*k;campHP[k]=f(o+28);campMask[k]=u(o+40);campGroup[k]=u(o+224);campXY[2*k]=i(o+20);campXY[2*k+1]=i(o+24);}
for(let g=0;g<ng;g++)groupState[g]=u(123436+20*g);
const cheb=(x,y,a,b)=>Math.max(Math.abs(x-a),Math.abs(y-b));
let steps=prefaceSteps,round=u(8),phase=u(24),lastContact=-10000;
function saveRound(r,state){
 for(let k=0;k<n;k++){let o=140+456*k,id=u(o+4),a=id*C;roundStates.push([r,id,...state.subarray(a,a+18)]);}
 metrics[0].banked_curve.push([r,u(32)]);metrics[1].banked_curve.push([r,u(40)]);
}
while(phase!==11&&steps<1000000){
 check(e.Game_Visualiser_Timeline_ReplayNext(h,1));check(e.Game_Visualiser_ReadState(h,pers,snap));v=new DataView(e.memory.buffer);
 readChamps(after);const newRound=u(8),kind=u(68),ak=u(72),actor=u(76),slot=u(108),ability=slot===0xffffffff?-1:u(112),fx=i(80),fy=i(84),tx=i(88),ty=i(92);
 const side=ak===1&&actor<16?after[actor*C]:-1,prev=actor*C;
 if(newRound!==round){saveRound(round,before);round=newRound;}
 const m=side>=0&&side<=1?metrics[side]:null;
 if(m){m.actions[kind]=(m.actions[kind]||0)+1;
  if(!turns[actor]||turns[actor].round!==round){
   let nearest=999;for(let j=0;j<n;j++){let id=u(140+j*456+4),a=id*C;if(before[a]!==side&&before[a+6])nearest=Math.min(nearest,cheb(before[prev+2],before[prev+3],before[a+2],before[a+3]));}
   turns[actor]={round,eligible:before[prev+6]&&before[prev+4]<.35*before[prev+5]&&nearest<=6,away:false,recall:false};
  }
  if(kind===4&&turns[actor].eligible){let d0=999,d1=999;for(let j=0;j<n;j++){let id=u(140+j*456+4),a=id*C;if(before[a]!==side&&before[a+6]){d0=Math.min(d0,cheb(fx,fy,before[a+2],before[a+3]));d1=Math.min(d1,cheb(tx,ty,before[a+2],before[a+3]));}}turns[actor].away ||= d1>d0;}
  if(kind===11)turns[actor].recall=true;
  if(kind===8&&turns[actor].eligible){m.low_hp_contact_turns++;m.low_hp_away_or_recall_turns+=Number(turns[actor].away||turns[actor].recall);turns[actor]=null;}
  if(kind===9){m.priority_calls++;let band=Math.floor(i(116)/100)*100;m.priority_bands[band]=(m.priority_bands[band]||0)+1;}
  if(kind===14){m.wards++;m.bush_wards+=Number(inBush(tx,ty));}
  if(kind===20)m.locks++;
  if(kind===12&&before[prev+8]>0){m.bags.push(before[prev+8]);m.first_bank_round ??= round;}
  if(kind===6&&slot<2){let last=lastCast[actor][slot];if(last>=0)m.recast_gaps.push(round-last);lastCast[actor][slot]=round;}
 }
 let campDamage=[],champDamage=[],newStuns=0;
 for(let k=0;k<nc;k++){
  let o=7436+k*232,hp=f(o+28),mask=u(o+40),delta=hp-campHP[k];
  if(delta<-.01){campDamage.push([k,-delta]);if(m&&campGroup[k]<ng)dealers[campGroup[k]][side].add(actor);}
  if((mask&2)&&!(campMask[k]&2)&&hp>0)newStuns++;
  campHP[k]=hp;campMask[k]=mask;
 }
 let logicalDead=[false,false];
 for(let k=0;k<n;k++){
  let id=u(140+k*456+4),a=id*C,t=after[a];
  if(before[a+6]&&after[a+6]&&after[a+4]<before[a+4]-.01){let damage=before[a+4]-after[a+4];champDamage.push([id,damage]);if(ak===2)metrics[t].camp_damage_taken+=damage;else if(m&&side!==t)metrics[t].champion_damage_taken+=damage;}
  if(before[a+6]&&!after[a+6]){metrics[t].entity_deaths++;metrics[t].gems_lost_on_death+=before[a+8];deaths.push([round,id,t,before[a+2],before[a+3],before[a+8],ak,actor]);if(after[a+1]===3||after[a+1]===4)logicalDead[t]=true;else metrics[t].linked_deaths++;}
  const delta=after[a+8]-before[a+8];if(delta)gemFlows.push([round,id,t,delta,kind]);
 }
 for(let t=0;t<2;t++)metrics[t].linked_deaths+=Number(logicalDead[t]);
 if(m&&newStuns)m.camp_stuns_observed+=newStuns;
 if(m&&kind===6){if(campDamage.length||newStuns)m.casts_at_camps++;if(champDamage.some(([id])=>after[id*C]!==side))m.casts_at_enemies++;}
 if(m&&champDamage.some(([id])=>after[id*C]!==side)){if(round-lastContact>6){m.fight_openers++;m.spell_openers+=Number(kind===6);}lastContact=round;}
 for(let g=0;g<ng;g++){
  let o=123436+20*g,state=u(o);if(state===groupState[g])continue;
  transitions.push([round,g,groupState[g],state,u(o+8)]);
  if(groupState[g]===2&&state===3){
   for(let t=0;t<2;t++){let gain=0,collectors=0;for(let k=0;k<n;k++){let id=u(140+k*456+4),a=id*C;if(after[a]===t&&after[a+8]>before[a+8]){gain+=after[a+8]-before[a+8];collectors++;}}
    if(gain){metrics[t].captured_gems+=gain;metrics[t].captures++;metrics[t].multi_dealer_captures+=Number(dealers[g][t].size>=2);metrics[t].solo_collector_captures+=Number(collectors===1);}
   }
  }
  if(state===1){dealers[g][0].clear();dealers[g][1].clear();}groupState[g]=state;
 }
 if(m&&kind===13){let o=140+456*Array.from({length:n},(_,k)=>k).find(k=>u(140+456*k+4)===actor);purchases.push([round,actor,side,before[prev+7],after[prev+7],Array.from({length:42},(_,j)=>u(o+240+j*4))]);}
 if(kind!==0&&kind!==3&&kind!==8&&kind!==17)events.push([steps,round,kind,ak,actor,side,fx,fy,tx,ty,slot===0xffffffff?null:slot,ability,i(116),m?before[prev+4]:null,m?before[prev+8]:null,campDamage,champDamage]);
 let swap=before;before=after;after=swap;phase=u(24);steps++;
}
if(phase!==11)throw Error('Incomplete replay');saveRound(round,before);
const drafts=[[],[]];for(let t=0;t<2;t++)for(let k=0;k<u(179520+t*4);k++)drafts[t].push(i(179528+t*20+k*4));
const champs=[];
for(let k=0;k<n;k++){let o=140+k*456,id=u(o+4),t=u(o+8),items=Array.from({length:42},(_,j)=>u(o+240+j*4));metrics[t].monster_items_final_equiv+=items.slice(0,6).reduce((s,x,j)=>s+x*(j%2?5:1),0);champs.push({id,team:t,type:u(o+12),items,kills:u(184504+12*id),deaths:u(184508+12*id),assists:u(184512+12*id),gems:u(o+236),gold:u(o+232)});}
const result={schema:1,engine:'1.0.1',snapshot_bytes:snapshotSize,abi:1,source:path.basename(source),sha256:crypto.createHash('sha256').update(raw).digest('hex'),width,height,champs_per_team:drafts[0].length,camps:nc,groups:ng,steps,rounds:round,outcome:u(28),scores:[u(32),u(40)],drafts,champions:champs,metrics,events,round_states:roundStates,deaths,purchases,group_transitions:transitions,gem_flows:gemFlows,seconds:(performance.now()-start)/1000,definitions:{first_bank_round:'tribute start; completed tribute is verified separately by score flows',camp_stuns_observed:'new camp stun buff on a champion action',multi_dealer_captures:'direct-damage distinct champions since camp respawn, per winning side',low_hp_away_or_recall:'end-turn sample below 35% at first observed action with an enemy within six; any away movement or recall during that turn',spell_openers:'first direct cross-team HP loss after more than six rounds without direct cross-team HP loss; shields and delayed damage may be absent'}};
fs.mkdirSync(path.dirname(dest),{recursive:true});fs.writeFileSync(dest+'.part',zlib.gzipSync(JSON.stringify(result),{level:1}));fs.renameSync(dest+'.part',dest);
console.log(JSON.stringify({source:result.source,steps,rounds:round,scores:result.scores,seconds:result.seconds}));
