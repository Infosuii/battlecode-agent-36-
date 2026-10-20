// Static geometry audit only: no bots, replay timeline, or final outcomes.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {WASI} from 'node:wasi';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const engine=path.resolve(root,'../development/engine-current.wasm');
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const wasi=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});
const inst=await WebAssembly.instantiate(await WebAssembly.compile(fs.readFileSync(engine)),wasi.getImportObject());
wasi.initialize(inst);const e=inst.exports,out=e.Engine_Malloc(8192);
const v=()=>new DataView(e.memory.buffer),u=p=>v().getUint32(p,true),i=p=>v().getInt32(p,true),b=p=>v().getBigUint64(p,true);
const call=(fn,...args)=>{const rc=e[fn](...args);if(rc)throw Error(`${fn} ${rc}`);};
const rows=[];
const sources=[];
for(const stage of ['screen','validation','shift_validation','final_holdout','shift_final']) {
 const manifest=JSON.parse(fs.readFileSync(path.join(root,'maps',stage,'manifest.json')));
 for(const row of manifest.maps) sources.push({stage,...row});
}
for(const file of fs.readdirSync(path.join(root,'../development/current/maps')).filter(f=>f.endsWith('.map26a')))
 sources.push({stage:'official',file:path.join(root,'../development/current/maps',file)});
for(const source of sources){
 const file=path.isAbsolute(source.file)?source.file:path.resolve(root,source.file);
 const raw=fs.readFileSync(file),p=e.Engine_Malloc(raw.length);new Uint8Array(e.memory.buffer).set(raw,p);
 call('Game_FromMapFileBytes',p,raw.length,out,0);const game=b(out);e.Engine_Free(p);
 call('Game_GetBitboardMetadata',game,out);const [width,height,cw,ch,stride]=[i(out),i(out+4),u(out+8),u(out+12),u(out+16)];
 call('Game_GetNumChampsPerTeam',game,out);const picks=u(out);
 call('Game_GetSymmetry',game,out);const symmetry=i(out);
 call('Game_GetTeamASpawnArea',game,out);const spawnA=[0,4,8,12].map(k=>i(out+k));
 const terrain={};
 for(const name of ['Wall','Bush','Hole','Altar']){
  call('Game_Get'+name+'Buffer',game,out);const addr=u(out),tiles=[];
  for(let y=0;y<height;y++)for(let x=0;x<width;x++){
   const word=b(addr+8*((y>>3)*stride+(x>>3))),bit=BigInt((y&7)*8+(x&7));
   if(word>>bit&1n)tiles.push(y*width+x);
  }terrain[name.toLowerCase()]=tiles;
 }
 call('Game_GetNumCamps',game,out);const nc=u(out),handles=new Map(),camps=[];
 for(let k=0;k<nc;k++){
  call('Game_GetCampHandle',game,k,out);const handle=b(out);
  call('Game_GetCamp',game,handle,out);const camp=[i(out),i(out+4),i(out+8)];
  handles.set(handle.toString(),camp);camps.push(camp);
 }
 call('Game_GetNumCampGroups',game,out);const ng=u(out),groups=[];
 for(let k=0;k<ng;k++){
  call('Game_GetCampGroupHandle',game,k,out);const gh=b(out);
  call('Game_GetCampGroup',game,gh,out);const rules=[u(out),u(out+4),u(out+8)],count=u(out+12),members=[];
  for(let q=0;q<count;q++){
   call('Game_GetCampGroupCamp',game,gh,q,out);const camp=handles.get(b(out).toString());
   if(!camp)throw Error('Group camp handle not found');members.push(camp);
  }
  members.sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b)));groups.push({rules,members});
 }
 camps.sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b)));
 groups.sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b)));
 const geometry={width,height,picks,symmetry,spawnA,terrain,camps,groups};
 const signature=hash(JSON.stringify(geometry));
 rows.push({stage:source.stage,name:path.basename(file,'.map26a'),file,file_sha256:hash(raw),seed:source.seed??null,style:source.style??null,
  physical_sha256:signature,geometry});
 call('Game_Destroy',game);
}
const target=path.join(root,'evidence/agent4/physical_map_layouts.json');
fs.writeFileSync(target,JSON.stringify({engine_sha256:hash(fs.readFileSync(engine)),rows,
 scope:'Static geometry only, no bot or performance evaluation. Game editor API representation consistently used for all maps; camp group values are editor-base representation, not runtime SDK counts.'},null,2));
console.log(JSON.stringify({maps:rows.length,out:target}));
