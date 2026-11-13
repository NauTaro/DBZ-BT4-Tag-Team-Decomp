import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
import {importRows} from '../online_migration.mjs';
import {createSession,verifyAccess,issueAccess,sha256} from '../online_auth.mjs';
function fixture(){
 const db=new DatabaseSync(':memory:');let time=Date.now();
 const sql={exec(q,...a){if(!a.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...a);}};
 const c=new OnlineCore(sql,()=>time);let batch=0;
 const apply=(table,rows,sync=false)=>{db.exec('BEGIN');try{const r=importRows(c,{source:'secondary',table,rows,batch:'test'+batch++,digest:'digest'},sync);db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}};
 return {c,apply,tick:n=>time+=n};
}
test('migration keeps old progress and adds late old-server results to newly earned points exactly once',()=>{
 const f=fixture(),user=crypto.randomUUID();f.apply('users',[{id:user,created_at:new Date().toISOString()}]);
 const row={user_id:user,ruleset:'solo',name:'Goku',rating:1240,wins:20,losses:5,updated_at:'2026-11-11T10:00:00Z'};
 f.apply('online_nicknames',[{user_id:user,name:'Goku'}]);f.apply('ranked_players',[row]);
 // A new Cloudflare match awards 16 points before the previous server drains.
 f.c.run("UPDATE ranked_players SET rating=rating+16,wins=wins+1 WHERE user_id=?",user);
 f.c.run("INSERT INTO online_meta VALUES('admission','open')");
 assert.throws(()=>f.apply('ranked_players',[row]),/IMPORT_REQUIRES_CLOSED/);
 const late={...row,rating:1258,wins:21,updated_at:'2026-11-11T10:05:00Z'};
 f.apply('ranked_players',[late],true);f.apply('ranked_players',[late],true);
 assert.equal(f.c.one('SELECT rating FROM ranked_players WHERE user_id=?',user).rating,1274);
 assert.equal(f.c.one('SELECT wins FROM ranked_players WHERE user_id=?',user).wins,22);
 f.apply('ranked_players',[row],true);assert.equal(f.c.one('SELECT rating FROM ranked_players WHERE user_id=?',user).rating,1274);
 assert.equal(f.c.name(user),'Goku');
});
test('late legacy snapshots do not overwrite a nickname changed in Cloudflare',()=>{
 const f=fixture(),u=crypto.randomUUID();f.apply('users',[{id:u,created_at:new Date().toISOString()}]);
 f.apply('online_nicknames',[{user_id:u,name:'Goku'}]);f.c.command(u,'nickname_set',{name:'Kakarot'});
 f.apply('online_nicknames',[{user_id:u,name:'Goku'}],true);assert.equal(f.c.name(u),'Kakarot');
});
test('refresh lost-response grace preserves identity, and forged/expired tokens are rejected',async()=>{
 const f=fixture(),u=crypto.randomUUID(),secret='s'.repeat(48);f.apply('users',[{id:u,created_at:new Date().toISOString()}]);
 const a=await createSession(f.c,u,secret);const oldHash=await sha256(a.refresh_token);
 const b=await createSession(f.c,u,secret,oldHash);assert.equal(await verifyAccess(b.access_token,secret),u);
 assert.equal(f.c.one('SELECT user_id FROM online_sessions WHERE hash=? AND expires>?',oldHash,f.c.now()).user_id,u);
 f.tick(120001);assert.equal(f.c.one('SELECT user_id FROM online_sessions WHERE hash=? AND expires>?',oldHash,f.c.now()),undefined);
 assert.equal(await verifyAccess(b.access_token,'t'.repeat(48)),null);
 const expired=await issueAccess(u,secret,Date.now()-1000000);assert.equal(await verifyAccess(expired,secret),null);
});
