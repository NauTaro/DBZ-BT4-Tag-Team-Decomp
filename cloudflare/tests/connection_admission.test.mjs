import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
import worker,{OnlineCoordinator} from '../online_worker.mjs';
import {createSession,sha256,verifyAccess} from '../online_auth.mjs';
function fixture(){
 const db=new DatabaseSync(':memory:');let now=Date.now();
 const sql={exec(q,...a){if(!a.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...a);}};
 const core=new OnlineCore(sql,()=>now),secret='admission-test-only-'.repeat(3);
 core.run("INSERT INTO online_meta VALUES('admission','open')");
 const coordinator=Object.assign(Object.create(OnlineCoordinator.prototype),{core,env:{AUTH_SECRET:secret},authTail:Promise.resolve()});
 async function user(){const id=crypto.randomUUID();core.run('INSERT INTO online_users VALUES(?,?)',id,now);return createSession(core,id,secret);}
 return {core,coordinator,secret,user,tick:n=>now+=n};
}
test('300 existing accounts sharing an IP renew; one noisy account cannot exhaust another account budget',async()=>{
 const f=fixture(),users=[];
 for(let n=0;n<300;n++)users.push(await f.user());
 for(let n=0;n<300;n++){
  const r=await f.coordinator.auth('/v5/auth/refresh',{refresh_token:users[n].refresh_token},'shared-ip');
  assert.equal(r.user.id,users[n].user.id);assert.equal(r.refresh_token,users[n].refresh_token);
 }
 for(let n=1;n<30;n++)await f.coordinator.auth('/v5/auth/refresh',{refresh_token:users[0].refresh_token},'shared-ip');
 await assert.rejects(()=>f.coordinator.auth('/v5/auth/refresh',{refresh_token:users[0].refresh_token},'shared-ip'),/RATE_LIMITED/);
 assert.equal((await f.coordinator.auth('/v5/auth/refresh',{refresh_token:users[1].refresh_token},'shared-ip')).user.id,users[1].user.id);
 assert.equal(f.core.one('SELECT count(*) n FROM online_sessions').n,300);
});
test('a lost renewal response does not expire the saved recovery credential after two minutes',async()=>{
 const f=fixture(),saved=await f.user(),hash=await sha256(saved.refresh_token);
 await createSession(f.core,saved.user.id,f.secret,hash,saved.refresh_token);
 for(const gap of [120001,3600000,20*86400000]){
  f.tick(gap);
  const recovered=await createSession(f.core,saved.user.id,f.secret,hash,saved.refresh_token);
  assert.equal(recovered.refresh_token,saved.refresh_token);assert.equal(await verifyAccess(recovered.access_token,f.secret,f.core.now()),saved.user.id);
 }
 assert.equal(f.core.one('SELECT count(*) n FROM online_users').n,1);
 assert.equal(f.core.one('SELECT count(*) n FROM online_sessions').n,1);
});
test('unknown, expired and cross-account recovery credentials remain rejected',async()=>{
 const f=fixture(),a=await f.user(),b=await f.user(),hash=await sha256(a.refresh_token);
 await assert.rejects(()=>createSession(f.core,b.user.id,f.secret,hash,a.refresh_token),/AUTH_REQUIRED/);
 await assert.rejects(()=>createSession(f.core,a.user.id,f.secret,hash,'wrong-secret'),/AUTH_REQUIRED/);
 f.tick(30*86400000+1);
 await assert.rejects(()=>createSession(f.core,a.user.id,f.secret,hash,a.refresh_token),/AUTH_REQUIRED/);
 assert.equal(f.core.one('SELECT count(*) n FROM online_users').n,2);
});
test('expanded ingress still rejects invalid credentials and bounds a shared-IP flood',async()=>{
 const f=fixture();
 for(let n=0;n<600;n++)await assert.rejects(()=>f.coordinator.auth('/v5/auth/refresh',{refresh_token:'invalid'},'bad-ip'),/AUTH_REQUIRED/);
 await assert.rejects(()=>f.coordinator.auth('/v5/auth/refresh',{refresh_token:'invalid'},'bad-ip'),/RATE_LIMITED/);
 assert.equal(f.core.one('SELECT count(*) n FROM online_users').n,0);
});
test('the existing panel TURN route retains its credential and POST checks',async()=>{
 for(const method of ['GET','POST']){
  const r=await worker.fetch(new Request('https://test.invalid/v5/panel/turn',{method}),{PANEL_SECRET:'test-only'});
  assert.equal(r.status,401);assert.equal((await r.json()).error,'AUTH_REQUIRED');
 }
});
