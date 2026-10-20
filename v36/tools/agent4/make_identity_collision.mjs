// A valid near-match with unchanged descriptor; only immutable bush layout changes.
// This is a correctness fixture, not a strength/holdout map.
import fs from 'node:fs';import path from 'node:path';import crypto from 'node:crypto';import {WASI} from 'node:wasi';
const root=path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/(?:([A-Z]:))/,'$1')),'../..');
const engine=path.resolve(root,'../development/engine-current.wasm');
const w=new WASI({version:'preview1',args:[],env:{},preopens:{},returnOnExit:true});const m=await WebAssembly.compile(fs.readFileSync(engine));const i=await WebAssembly.instantiate(m,w.getImportObject());w.initialize(i);const e=i.exports;
const out=e.Engine_Malloc(8192),call=(fn,...args)=>{let rc=e[fn](...args);if(rc)throw Error(`${fn} ${rc}`);};
const v=()=>new DataView(e.memory.buffer),u=p=>v().getUint32(p,true);
const folder=path.join(root,'evidence/agent4/identity_maps');fs.mkdirSync(folder,{recursive:true});const rows=[];
for(const name of ['rows','lanes','expanse']){
 const source=path.join(root,'../development/current/maps',name+'.map26a'),raw=fs.readFileSync(source),p=e.Engine_Malloc(raw.length);new Uint8Array(e.memory.buffer).set(raw,p);
 call('MapBuilder_FromFileBytes',p,raw.length,out);const builder=v().getBigUint64(out,true);e.Engine_Free(p);
 call('MapBuilder_GetBitboardMetadata',builder,out);const width=u(out),height=u(out+4);
 let changed=null;
 for(let y=1;y<height-1&&!changed;y++)for(let x=1;x<width-1&&!changed;x++){
  let occupied=false;for(const kind of ['Bush','Wall','Hole','Altar']){call('MapBuilder_Get'+kind,builder,x,y,out);occupied||=!!u(out);}
  if(!occupied){call('MapBuilder_AddBush',builder,x,y);changed=[x,y];}
 }
 if(!changed)throw Error('No free tile');call('MapBuilder_VerifyIntegrity',builder);call('MapBuilder_Encode',builder,out,out+4);
 const bytes=Buffer.from(new Uint8Array(e.memory.buffer,u(out),u(out+4)));const target=path.join(folder,name+'-near-match.map26a');fs.writeFileSync(target,bytes);call('MapBuilder_Destroy',builder);
 rows.push({source,map:target,base:name,changed_bush:changed,source_sha256:crypto.createHash('sha256').update(raw).digest('hex'),sha256:crypto.createHash('sha256').update(bytes).digest('hex'),purpose:'strict unknown fallback on descriptor collision',integrity:'official engine passed'});
}
fs.writeFileSync(path.join(folder,'manifest.json'),JSON.stringify(rows,null,2));console.log(JSON.stringify(rows));
