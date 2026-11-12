import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore,seasonAt,clientAtLeast} from '../online_core.mjs';

function fixture(){
    const db=new DatabaseSync(':memory:');let now=Date.parse('2026-11-11T15:00:00Z');
    const sql={exec(q,...args){if(!args.length&&!/^SELECT/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...args);}};
    const c=new OnlineCore(sql,()=>now);const ids=[];
    function user(name){const id=crypto.randomUUID();c.run('INSERT INTO online_users VALUES(?,?)',id,now);if(name)c.command(id,'nickname_set',{name});ids.push(id);return id;}
    const invoke=(u,op,a={})=>{db.exec('BEGIN');try{const r=c.command(u,op,a);db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}};
    return {c,db,user,invoke,tick:n=>now+=n,ids};
}
const profile={mode:'duel',wire:'test-v1',content:'abcd',rules:'S'};
test('private rooms work in both hosting directions and isolate outsiders',()=>{
    const f=fixture(),a=f.user('Alpha'),b=f.user('Bravo'),outside=f.user('Charlie');
    for(const [host,guest] of [[a,b],[b,a]]){
        const r=f.invoke(host,'create',profile),j=f.invoke(guest,'join',{...profile,code:r.code});
        assert.equal(j.members.length,2);assert.notEqual(j.self,r.self);
        assert.throws(()=>f.invoke(outside,'signal',{generation:r.generation,to:j.self,channel:'tag',kind:'offer',body:{sdp:'test'}}),/NOT_A_MEMBER/);
        f.invoke(host,'signal',{generation:r.generation,to:j.self,channel:'duel',kind:'udp',body:{addresses:['127.0.0.1:1234'],nonce:'a'.repeat(32)}});
        assert.equal(f.invoke(guest,'get').udp.length,1);
        f.invoke(host,'leave',{generation:r.generation});
    }
});
test('300 players form 150 unique matches without losing their profiles',()=>{
    const f=fixture();const started=performance.now();
    for(let i=0;i<300;i++)f.invoke(f.user('Player'+i),'queue',profile);
    assert.equal(f.c.one('SELECT count(*) n FROM online_rooms').n,150);
    assert.equal(f.c.one('SELECT count(*) n FROM online_members').n,300);
    assert.equal(f.c.one('SELECT count(*) n FROM online_queue').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM online_names').n,300);
    console.log('300-player local SQLite admission ms:',Math.round(performance.now()-started));
});
test('Tag queues isolate team size and content; full rooms reject extra fighters',()=>{
    const f=fixture(),users=Array.from({length:5},(_,i)=>f.user('Fighter'+i));let result;
    for(const u of users.slice(0,4))result=f.invoke(u,'queue',{...profile,mode:'tag',rules:'2v2-114'});
    assert.equal(result.members.length,4);
    assert.throws(()=>f.invoke(users[4],'join',{...profile,code:result.code}),/ROOM_FULL/);
    assert.throws(()=>f.invoke(users[4],'join',{...profile,content:'other',code:result.code}),/INCOMPATIBLE_DATA/);
});
test('nicknames are unique and a failed claim cannot overwrite points',()=>{
    const f=fixture(),a=f.user('Goku'),b=f.user();
    assert.throws(()=>f.invoke(b,'nickname_set',{name:'goku'}),/NICKNAME_TAKEN/);
    assert.equal(f.invoke(a,'nickname_get').name,'Goku');
});
test('legacy nickname formats and numeric prefixes remain accepted',()=>{
    const f=fixture(),u=f.user();assert.equal(f.invoke(u,'nickname_set',{name:'123 Goku!'}).name,'123 Goku!');
    assert.throws(()=>f.invoke(u,'nickname_set',{name:'Player'}),/INVALID_NICKNAME/);
});
test('temporary ranked pause blocks new searches while an admitted battle can still settle',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);f.c.run("INSERT INTO online_meta VALUES('ranked_paused','1')");
    assert.equal(f.invoke(a,'ranked_status').enabled,false);
    assert.throws(()=>f.invoke(f.user('Third'),'ranked_queue',{...profile,ruleset:'solo'}),/RANKED_NOT_ENABLED/);
    assert.equal(f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'}).delta,-16);
    assert.equal(f.invoke(b,'ranked_heartbeat',{match:id,peer_gone:true}).delta,16);
});
test('the minimum client gate compares numeric revisions',()=>{
    assert.ok(clientAtLeast('0.1.39.5-R5.6','0.1.39.5-R5.6'));assert.ok(clientAtLeast('0.1.39.5-R5.10','0.1.39.5-R5.6'));
    assert.ok(!clientAtLeast('0.1.39.5-R5.5','0.1.39.5-R5.6'));assert.ok(!clientAtLeast('garbage','0.1.39.5-R5.6'));
});
function rankedPair(f){const a=f.user('First'),b=f.user('Second');const args={...profile,ruleset:'solo'};
    f.invoke(a,'ranked_queue',args);const r=f.invoke(b,'ranked_queue',args);
    f.invoke(a,'ranked_heartbeat',{match:r.generation});f.invoke(b,'ranked_heartbeat',{match:r.generation});
    const admitted=f.c.one('SELECT a,b FROM ranked_matches WHERE id=?',r.generation);return {...admitted,id:r.generation};}
test('a quitter loses points, the connected player gains, and repeated settlement is idempotent',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);
    assert.equal(f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'}).delta,-16);
    assert.equal(f.invoke(b,'ranked_heartbeat',{match:id,peer_gone:true}).delta,16);
    assert.equal(f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'}).delta,-16);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,2);
    assert.equal(f.invoke(b,'ranked_status',{ruleset:'solo'}).rating,1016);
});
test('missing transport alone cannot punish a still-heartbeating opponent',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);f.tick(20000);
    assert.equal(f.invoke(a,'ranked_heartbeat',{match:id,peer_gone:true}).state,'waiting');
    f.invoke(b,'ranked_heartbeat',{match:id});assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
    f.tick(46000);assert.equal(f.invoke(a,'ranked_heartbeat',{match:id,peer_gone:true}).delta,16);
    assert.equal(f.invoke(b,'ranked_heartbeat',{match:id}).delta,-16);
});
test('normal results require both matching authenticated reports',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f),digest='b'.repeat(64);
    assert.equal(f.invoke(a,'ranked_report',{match:id,outcome:1,digest}).state,'waiting');
    assert.equal(f.invoke(b,'ranked_report',{match:id,outcome:1,digest}).delta,16);
    assert.equal(f.invoke(a,'ranked_report',{match:id,outcome:1,digest}).delta,-16);
});
test('pruning retires only transient state after 30 minutes and retains ranking/history',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'});
    f.tick(1800001);f.c.prune();assert.equal(f.c.one('SELECT count(*) n FROM online_rooms').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM online_names').n,2);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,2);
});
test('weekly rollover occurs Sunday 23:59 Argentina, without resetting ratings',()=>{
    const before=Date.parse('2026-11-09T02:58:59Z'),after=Date.parse('2026-11-09T02:59:00Z');
    assert.equal(seasonAt(before),'2026-11-01');assert.equal(seasonAt(after),'2026-11-08');
    const f=fixture(),{a,id}=rankedPair(f);f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'});
    const rating=f.invoke(a,'ranked_status').rating;f.tick(7*86400000);assert.equal(f.invoke(a,'ranked_status').rating,rating);
});
