// Read actual completed draft and physical body roster from short raw replays.
import fs from'node:fs';import path from'node:path';import{WASI}from'node:wasi';
const[selectionFile,output]=process.argv.slice(2),root=path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(?:([A-Z]:))/,'$1')),'../..');
const w=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true}),m=await WebAssembly.compile(fs.readFileSync(path.join(root,'../development/engine-current.wasm'))),instance=await WebAssembly.instantiate(m,w.getImportObject());w.initialize(instance);const e=instance.exports;
const out=e.Engine_Malloc(8192),snap=e.Engine_Malloc(184696),pers=e.Engine_Malloc(16),v=()=>new DataView(e.memory.buffer),u=off=>v().getUint32(snap+off,true),i=off=>v().getInt32(snap+off,true),call=(fn,...args)=>{let rc=e[fn](...args);if(rc)throw Error(`${fn} ${rc}`);};v().setUint32(pers,2,true);v().setUint32(pers+4,0xffffffff,true);const records=[];
for(const row of JSON.parse(fs.readFileSync(selectionFile))){const bytes=fs.readFileSync(row.raw),p=e.Engine_Malloc(bytes.length);new Uint8Array(e.memory.buffer).set(bytes,p);call('Game_FromReplayFileBytes',p,bytes.length,out);const game=v().getBigUint64(out,true);e.Engine_Free(p);call('Game_Visualiser_ReadState',game,pers,snap);let steps=0;
 while(u(24)===12||u(120)===0){call('Game_Visualiser_Timeline_ReplayNext',game,1);call('Game_Visualiser_ReadState',game,pers,snap);if(++steps>1000)throw Error('Draft did not finish');}
 if(u(0)!==1||u(4)!==184696)throw Error('Unsupported snapshot ABI');
 const drafts=[0,1].map(t=>Array.from({length:u(179520+t*4)},(_,k)=>i(179528+t*20+k*4))),bodies=Array.from({length:u(120)},(_,k)=>({id:u(144+456*k),team:u(148+456*k),type:u(152+456*k)}));records.push({name:row.name,drafts,bodies});call('Game_Destroy',game);}
fs.writeFileSync(output,JSON.stringify(records,null,2));console.log(JSON.stringify({replays:records.length,physical_bodies:records.reduce((s,r)=>s+r.bodies.length,0)}));
