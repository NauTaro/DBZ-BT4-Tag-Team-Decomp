import fs from 'node:fs';
import assert from 'node:assert/strict';
const compiled=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
function room(count){
 const a=new WebAssembly.Instance(compiled,{}).exports;
 const input=new Uint8Array(a.memory.buffer,a.Kernel_Input(),512);
 const output=new Uint8Array(a.memory.buffer,a.Kernel_Output(),512);
 function command(id,cmd,x=0,y=0,z=0,pad=null){
  input.fill(0);const v=new DataView(input.buffer,input.byteOffset,512);
  v.setUint32(0,0x54540008,true);v.setUint32(4,cmd,true);v.setUint32(8,a.Kernel_Epoch(),true);
  v.setUint32(12,x,true);v.setUint32(16,y,true);v.setUint32(20,z,true);
  if(cmd===1||cmd===2||cmd===5||cmd===6)v.setUint32(24,123,true);
  if(cmd===1||cmd===2)input.set(new TextEncoder().encode('Test'+id),32);
  if(pad)input.set(pad,16);
  assert.equal(a.Kernel_Command(id),0,`command ${cmd} member ${id}`);
 }
 command(101,1,2);command(101,13,count/2,count/2);
 for(let n=1;n<count;n++)command(101+n,2);
 command(101,3);
 for(let n=0;n<count;n++)command(101+n,4,n+1,0,n);
 command(101,15,1,0,1);command(101,5);
 for(let n=0;n<count;n++)command(101+n,6);
 command(101,7);for(let n=0;n<count;n++)command(101+n,8);
 command(101,9);a.Kernel_Pacing(1);
 function tick(frame,skip=-1,first=null){
  for(let n=0;n<count;n++)if(n!==skip)command(101+n,11,frame,0,0,n===0&&first?first:neutral());
  const late=a.Kernel_DeadlineFrame();assert.equal(a.Kernel_TakeFrame(),1);
  const v=new DataView(output.buffer,output.byteOffset,512);assert.equal(v.getUint32(12,true),frame);
  return {late,pads:Array.from({length:count},(_,n)=>output.slice(16+n*18,34+n*18))};
 }
 return{a,command,tick};
}
function neutral(){const pad=new Uint8Array(18);pad[4]=pad[5]=127;return pad;}
const r=room(2),charge=neutral();new DataView(charge.buffer).setUint32(0,0x200,true);charge[6]=charge[8]=1;
let row=r.tick(0,-1,charge);assert.equal(row.late,0);
for(let frame=1;frame<=8;frame++){
 row=r.tick(frame,0);assert.equal(row.late,1);
 assert.equal(row.pads[0][4],127);assert.equal(row.pads[0][5],127);
 assert.equal(new DataView(row.pads[0].buffer).getUint32(0,true),frame<=3?0x200:0);
 assert.equal(row.pads[0][6],0);assert.equal(row.pads[0][8],0);
 assert.deepEqual(row.pads[1],neutral());
}
row=r.tick(9,-1,charge);assert.equal(row.late,0);assert.equal(row.pads[0][8],1);
const four=room(4);
for(let frame=0;frame<240;frame++){
 row=four.tick(frame,3);assert.equal(row.late,8);
 assert.deepEqual(row.pads[3],neutral());assert.equal(four.a.Kernel_Phase(),5);
}
console.log(JSON.stringify({passed:true,held_gap_frames:3,one_shot_commands_repeated:false,centred_stick:true,other_clients_advanced_during_outage:240}));
