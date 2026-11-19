import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
const profile={mode:'duel',wire:'hotfix',content:'abcd',rules:'S',ruleset:'solo',abandonment_version:2};
function fixture(){
 const db=new DatabaseSync(':memory:');let now=Date.parse('2026-11-19T15:00:00Z');
 const sql={exec(q,...a){if(!a.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...a);}};
 const c=new OnlineCore(sql,()=>now);
 c.run("INSERT INTO online_meta VALUES('ranked_scoring','streak')");c.run("INSERT INTO online_meta VALUES('ranked_abandonment_v2','1')");
 function user(name,rating=1000){const u=crypto.randomUUID();c.run('INSERT INTO online_users VALUES(?,?)',u,now);c.command(u,'nickname_set',{name});c.run("INSERT INTO ranked_players(user_id,ruleset,name,rating) VALUES(?,'solo',?,?)",u,name,rating);return u;}
 function call(u,op,args={}){db.exec('BEGIN');try{let r=c.command(u,op,args);db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}}
 function pair(a,b){call(a,'ranked_queue',profile);const r=call(b,'ranked_queue',profile);const m=c.one('SELECT * FROM ranked_matches WHERE id=?',r.generation);assert.ok(m);for(const u of [a,b])call(u,'ranked_heartbeat',{match:m.id});return m;}
 function finish(m,winner){const outcome=winner===m.a?0:winner===m.b?1:2,digest='e'.repeat(64);for(const u of [m.a,m.b])call(u,'ranked_report',{match:m.id,outcome,digest});}
 return {c,user,call,pair,finish,tick:n=>now+=n};
}
test('normal losses never tax accumulated streak gains, in either seat and across rating gaps',()=>{
 for(const reverse of [false,true])for(const streak of [0,3,20,10000])for(const difference of [-500,0,500]){
  const f=fixture(),loser=f.user('Loser',1000),winner=f.user('Winner',1000+difference);
  f.c.run('INSERT INTO ranked_streaks VALUES(?,?,?)',loser,streak,streak?640:0);
  const m=reverse?f.pair(loser,winner):f.pair(winner,loser);f.finish(m,winner);
  const h=f.c.one('SELECT * FROM ranked_history WHERE match_id=? AND user_id=?',m.id,loser);
  assert.equal(h.delta,difference===0?-16:difference===500?-2:-30);
  assert.equal(f.call(loser,'ranked_status').streak_wins,0);
  assert.equal(f.call(winner,'ranked_heartbeat',{match:m.id}).delta,13);
  for(let i=0;i<3;i++)f.call(loser,'ranked_heartbeat',{match:m.id});
  assert.equal(f.call(loser,'ranked_status').rating,h.after_rating);
 }
});
test('six agreed normal wins against the same opponent are scored, not automatically held',()=>{
 const f=fixture(),a=f.user('Winner'),b=f.user('Opponent');
 for(let i=0;i<6;i++){
  const m=f.pair(a,b);f.finish(m,a);
  const result=f.call(a,'ranked_heartbeat',{match:m.id});
  assert.equal(result.delta,13+2*i);assert.equal(result.under_review,false);
  f.call(m.a,'leave',{generation:m.id});f.tick(60000);
 }
 assert.equal(f.c.one('SELECT count(*) n FROM ranked_reviews').n,0);
});
test('requeue waits instead of instantly rematching, then creates a new independently scored generation',()=>{
 const f=fixture(),a=f.user('Alpha'),b=f.user('Beta');
 const old=f.pair(a,b);f.finish(old,a);f.call(old.a,'leave',{generation:old.id});
 assert.equal(f.call(a,'ranked_queue',profile).state,'waiting');
 assert.equal(f.call(b,'ranked_queue',profile).state,'waiting');
 f.tick(30001);const next=f.call(a,'ranked_queue',profile);
 assert.ok(next.generation&&next.generation!==old.id);
 const m=f.c.one('SELECT * FROM ranked_matches WHERE id=?',next.generation);
 for(const u of [a,b])f.call(u,'ranked_heartbeat',{match:m.id});
 f.finish(m,b);assert.equal(f.call(b,'ranked_heartbeat',{match:m.id}).delta,13);
 assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,4);
});
test('a third queued opponent can play while a just-finished pairing is cooling down',()=>{
 const f=fixture(),a=f.user('Alpha'),b=f.user('Beta'),c=f.user('Gamma');
 const old=f.pair(a,b);f.finish(old,a);f.call(old.a,'leave',{generation:old.id});
 f.call(a,'ranked_queue',profile);f.call(b,'ranked_queue',profile);
 f.call(c,'ranked_queue',profile);
 assert.equal(f.c.one('SELECT count(*) n FROM ranked_matches').n,2);
 const m=f.c.one('SELECT * FROM ranked_matches WHERE id<>?',old.id);
 assert.ok([m.a,m.b].includes(c));
});
