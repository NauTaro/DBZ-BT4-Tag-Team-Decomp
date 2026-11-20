import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
import {OnlineCoordinator} from '../online_worker.mjs';
import {readFileSync} from 'node:fs';
import {ABANDONMENT_GRACE_MS} from '../ranked_abandonment.mjs';

const profile={mode:'duel',wire:'test-v1',content:'abcd',rules:'S',ruleset:'solo',abandonment_version:2};
function fixture({enabled=true,streak=true}={}){
    const db=new DatabaseSync(':memory:');let now=Date.parse('2026-11-17T15:00:00Z');
    const sql={exec(q,...args){if(!args.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...args);}};
    const c=new OnlineCore(sql,()=>now);
    const config=(key,value)=>c.run('INSERT INTO online_meta VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value',key,value);
    if(enabled)config('ranked_abandonment_v2','1');
    if(streak)config('ranked_scoring','streak');
    function user(name){const id=crypto.randomUUID();c.run('INSERT INTO online_users VALUES(?,?)',id,now);c.command(id,'nickname_set',{name});return id;}
    function call(u,op,args={}){db.exec('BEGIN');try{const r=c.command(u,op,args);db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}}
    function pair(a=user('First'),b=user('Second'),started=true){
        call(a,'ranked_queue',profile);const r=call(b,'ranked_queue',profile);
        const m=c.one('SELECT * FROM ranked_matches WHERE id=?',r.generation);
        if(started){call(m.a,'ranked_heartbeat',{match:m.id});call(m.b,'ranked_heartbeat',{match:m.id});}
        return m;
    }
    const tick=ms=>now+=ms;
    function missing(m,stayer,duration=50000){let reply=call(stayer,'ranked_heartbeat',{match:m.id,peer_gone:true});for(let t=0;t<duration;t+=5000){tick(5000);reply=call(stayer,'ranked_heartbeat',{match:m.id,peer_gone:true});}return reply;}
    const rating=(u,n)=>c.run("UPDATE ranked_players SET rating=? WHERE user_id=? AND ruleset='solo'",n,u);
    const history=(m,u)=>c.one('SELECT * FROM ranked_history WHERE match_id=? AND user_id=?',m.id,u);
    return {db,c,call,config,user,pair,tick,missing,rating,history};
}
for(const side of ['a','b']){
    test(`explicit ${side} departure penalizes only that player, awards the opponent once`,()=>{
        const f=fixture(),m=f.pair(),quitter=m[side],winner=quitter===m.a?m.b:m.a;
        f.rating(quitter,1000);f.rating(winner,500);
        f.c.run('INSERT INTO ranked_streaks VALUES(?,20,640)',quitter);
        f.c.run('INSERT INTO ranked_streaks VALUES(?,3,45)',winner);
        const result=f.call(quitter,'ranked_forfeit',{match:m.id,reason:'local_quit'});
        assert.equal(result.delta,-100);assert.equal(result.rating,900);
        for(let i=0;i<5;i++){
            assert.equal(f.call(winner,'ranked_heartbeat',{match:m.id,peer_gone:true}).delta,19);
            assert.equal(f.call(quitter,'ranked_forfeit',{match:m.id,reason:'local_quit'}).delta,-100);
        }
        assert.equal(f.history(m,winner).after_rating,519);
        assert.equal(f.history(m,quitter).after_rating,900);
        assert.equal(f.c.one('SELECT count(*) n FROM ranked_history WHERE match_id=?',m.id).n,2);
        assert.deepEqual({...f.c.one('SELECT wins,points FROM ranked_streaks WHERE user_id=?',quitter)},{wins:0,points:0});
        assert.equal(f.c.one('SELECT wins FROM ranked_streaks WHERE user_id=?',winner).wins,4);
    });
    test(`unilateral loss of ${side} liveness rewards the remaining player, never deducts from them`,()=>{
        const f=fixture(),m=f.pair(),quitter=m[side],winner=quitter===m.a?m.b:m.a;
        f.rating(quitter,500);f.rating(winner,500);
        const result=f.missing(m,winner);
        assert.equal(result.delta,13);assert.equal(result.rating,513);
        assert.equal(f.history(m,quitter).delta,-100);assert.equal(f.history(m,quitter).after_rating,400);
        assert.equal(f.c.one('SELECT reason FROM ranked_abandonments WHERE match_id=?',m.id).reason,'liveness_timeout');
    });
}
test('the policy defaults off and activation does not reprice existing matches',()=>{
    const f=fixture({enabled:false,streak:false}),m=f.pair();
    assert.equal(f.call(m.a,'ranked_status').forfeit_penalty,0);
    f.config('ranked_abandonment_v2','1');
    assert.equal(f.call(m.a,'ranked_status').forfeit_penalty,100);
    assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).delta,-16);
});

test('legacy clients cannot enter new Ranked matches after the policy is enabled',()=>{
    const f=fixture(),a=f.user('Legacy');
    for(const version of [undefined,0,1,'2',null]){
        assert.throws(()=>f.call(a,'ranked_queue',{...profile,abandonment_version:version}),/UPDATE_REQUIRED/);
        assert.equal(f.c.one('SELECT count(*) n FROM online_queue').n,0);
    }
    assert.equal(f.call(a,'ranked_queue',profile).state,'waiting');
});
test('low or zero quitter balance does not remove the winner reward',()=>{
    for(const streak of [true,false])for(const remaining of [0,27]){
        const f=fixture({streak}),m=f.pair();f.rating(m.a,remaining);f.rating(m.b,0);
        assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).delta,remaining?-remaining:0);
        assert.equal(f.call(m.b,'ranked_heartbeat',{match:m.id}).delta,streak?13:remaining===0?16:17);
    }
});
test('neither player is penalized during selection or failed admission',()=>{
    const f=fixture(),m=f.pair(undefined,undefined,false);
    f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'});
    f.missing(m,m.b);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('transient transport loss cannot identify a heartbeating opponent as the quitter',()=>{
    const f=fixture(),m=f.pair();
    for(let i=0;i<15;i++){
        f.tick(5000);f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true});f.call(m.b,'ranked_heartbeat',{match:m.id});
    }
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('both losing connectivity cannot award a win to the first reconnecting heartbeat',()=>{
    const f=fixture(),m=f.pair();f.missing(m,m.a,20000);f.tick(90000);
    assert.equal(f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true}).state,'waiting');
    f.call(m.b,'ranked_heartbeat',{match:m.id});
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_disconnect_claims').n,0);
});
test('a gap in the witness chain restarts the reconnection grace',()=>{
    const f=fixture(),m=f.pair();f.missing(m,m.a,30000);f.tick(25000);
    assert.equal(f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true}).state,'waiting');
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('an opponent returning before the grace deadline cancels the abandonment claim',()=>{
    const f=fixture(),m=f.pair();f.missing(m,m.a,40000);f.call(m.b,'ranked_heartbeat',{match:m.id});
    f.tick(5000);assert.equal(f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true}).state,'waiting');
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('a pending matching win report can still be resolved if the other player exits',()=>{
    const f=fixture(),m=f.pair();f.rating(m.b,300);
    f.call(m.a,'ranked_report',{match:m.id,outcome:0,digest:'a'.repeat(64)});
    assert.equal(f.missing(m,m.a).delta,13);
    assert.equal(f.history(m,m.b).delta,-100);
});
test('conflicting reports cannot be overridden by a unilateral accusation',()=>{
    const f=fixture(),m=f.pair();
    f.call(m.b,'ranked_report',{match:m.id,outcome:1,digest:'b'.repeat(64)});
    assert.equal(f.missing(m,m.a).state,'waiting');
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('the quitter cannot accuse the connected player to reverse an explicit forfeit',()=>{
    const f=fixture(),m=f.pair();f.rating(m.a,500);f.rating(m.b,500);
    f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true});
    f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'});
    assert.equal(f.call(m.a,'ranked_heartbeat',{match:m.id,peer_gone:true}).delta,-100);
    assert.equal(f.call(m.b,'ranked_heartbeat',{match:m.id,peer_gone:true}).delta,13);
});
test('completed normal games and ties are never converted into abandonments',()=>{
    for(const outcome of [0,1,2]){
        const f=fixture(),m=f.pair();f.rating(m.a,500);f.rating(m.b,500);
        for(const u of [m.a,m.b])f.call(u,'ranked_report',{match:m.id,outcome,digest:'d'.repeat(64)});
        const before=f.history(m,m.a);
        f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'});f.missing(m,m.b);
        assert.deepEqual(f.history(m,m.a),before);
    }
});
test('confirmed repeated forfeits still penalize the quitter and credit the winner',()=>{
    const f=fixture(),a=f.user('Winner'),b=f.user('Quitter');
    let m;
    for(let i=0;i<3;i++){
        m=f.pair(a,b);if(i===0){f.rating(a,500);f.rating(b,500);}
        f.call(b,'ranked_forfeit',{match:m.id,reason:'local_quit'});
        assert.equal(f.history(m,b).delta,-100);
        const result=f.call(a,'ranked_heartbeat',{match:m.id});
        assert.equal(result.delta,13+2*i);
        assert.equal(result.under_review,false);
        f.call(m.a,'leave',{generation:m.id});f.tick(60000);
    }
    assert.equal(f.c.one("SELECT losses FROM ranked_players WHERE user_id=? AND ruleset='solo'",b).losses,3);
});
test('outsiders and malformed departure reasons cannot impose penalties',()=>{
    const f=fixture(),m=f.pair(),outsider=f.user('Outside');
    assert.throws(()=>f.call(outsider,'ranked_forfeit',{match:m.id,reason:'local_quit'}),/NOT_A_PARTICIPANT/);
    f.call(m.a,'ranked_forfeit',{match:m.id,reason:'peer_quit',quitter:m.b});
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});
test('expired matches cannot take points',()=>{
    const f=fixture(),m=f.pair();f.tick(5400001);
    assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).state,'expired');
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
});

test('an abandonment from the previous month cannot debit the new month',()=>{
    const f=fixture();
    f.config('ranked_monthly_reset','1');f.config('ranked_season','2026-11-01');
    const m=f.pair();f.rating(m.a,500);f.rating(m.b,600);
    f.tick(Date.parse('2026-12-01T03:00:00Z')-f.c.now());
    assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).state,'expired');
    for(const u of [m.a,m.b])assert.equal(f.call(u,'ranked_status').rating,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
    assert.equal(f.c.one('SELECT confirmed FROM ranked_abandonments WHERE match_id=?',m.id).confirmed,null);
});

test('activation requires the admin credential, POST and a boolean without resetting scores',async()=>{
    const f=fixture({enabled:false}),m=f.pair();f.rating(m.a,432);f.rating(m.b,765);
    const coordinator=Object.assign(Object.create(OnlineCoordinator.prototype),{core:f.c,env:{ADMIN_SECRET:'unit-admin'},ctx:{storage:{transactionSync(fn){f.db.exec('BEGIN');try{const r=fn();f.db.exec('COMMIT');return r;}catch(e){f.db.exec('ROLLBACK');throw e;}}}}});
    const request=(token,method='POST',body={enabled:true})=>new Request('https://unit.invalid/v5/admin/ranked-abandonment',{
        method,headers:{Authorization:'Bearer '+token},...(method==='POST'?{body:JSON.stringify(body)}:{})});
    assert.equal((await coordinator.fetch(request('wrong'))).status,401);
    assert.equal((await coordinator.fetch(request('unit-admin','GET'))).status,400);
    assert.equal((await coordinator.fetch(request('unit-admin','POST',{enabled:1}))).status,400);
    assert.equal(f.call(m.a,'ranked_status').forfeit_penalty,0);
    const activated=await (await coordinator.fetch(request('unit-admin'))).json();
    assert.deepEqual(activated,{enabled:true,penalty:100,grace_seconds:45,applies_to:'new_matches'});
    assert.equal(f.call(m.a,'ranked_status').rating,432);assert.equal(f.call(m.b,'ranked_status').rating,765);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_abandonments').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
    await coordinator.fetch(request('unit-admin','POST',{enabled:false}));
    assert.equal(f.call(m.a,'ranked_status').forfeit_penalty,0);
});

test('the standalone SQL migration is repeatable and retains existing match data',()=>{
    const f=fixture(),m=f.pair();f.rating(m.a,123);
    const before={...f.c.one('SELECT * FROM ranked_matches WHERE id=?',m.id)};
    const sql=readFileSync(new URL('../migrations/20261117_ranked_abandonment.sql',import.meta.url),'utf8');
    f.db.exec(sql);f.db.exec(sql);
    assert.deepEqual({...f.c.one('SELECT * FROM ranked_matches WHERE id=?',m.id)},before);
    assert.equal(f.call(m.a,'ranked_status').rating,123);
});
test('the policy persists across coordinator reconstruction and respects rating bounds',()=>{
    const f=fixture(),m=f.pair();f.rating(m.a,100);f.rating(m.b,99999);
    const restored=new OnlineCore(f.c.sql,f.c.now);assert.equal(restored.ranked(m.b,'status',{}).forfeit_penalty,100);
    f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'});
    assert.equal(f.history(m,m.a).after_rating,0);assert.equal(f.history(m,m.b).after_rating,100000);
});
test('rating-boundary matrix never deducts from the remaining player in either hosting direction',()=>{
    for(const streak of [true,false])for(const side of ['a','b'])for(const qa of [0,99,100,100000])for(const wa of [0,500,100000]){
        const f=fixture({streak}),m=f.pair(),quitter=m[side],winner=quitter===m.a?m.b:m.a;
        f.rating(quitter,qa);f.rating(winner,wa);
        f.call(quitter,'ranked_forfeit',{match:m.id,reason:'local_quit'});
        const wh=f.history(m,winner),qh=f.history(m,quitter);
        assert.ok(wh.delta>=0,JSON.stringify({streak,side,qa,wa,wh}));
        assert.equal(wh.before_rating,wa);assert.ok(wh.after_rating>=wa&&wh.after_rating<=100000);
        assert.equal(qh.after_rating,Math.max(0,qa-100));
        assert.equal(f.c.one('SELECT winner FROM ranked_matches WHERE id=?',m.id).winner,winner);
    }
});
test('a failed write rolls back both players and the penalty audit together',()=>{
    const f=fixture(),m=f.pair();f.rating(m.a,500);f.rating(m.b,500);
    f.db.exec("CREATE TRIGGER stop_history BEFORE INSERT ON ranked_history BEGIN SELECT RAISE(ABORT,'write failed');END;");
    assert.throws(()=>f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}),/write failed/);
    assert.equal(f.c.one("SELECT rating FROM ranked_players WHERE user_id=? AND ruleset='solo'",m.a).rating,500);
    assert.equal(f.c.one("SELECT rating FROM ranked_players WHERE user_id=? AND ruleset='solo'",m.b).rating,500);
    assert.equal(f.c.one('SELECT confirmed FROM ranked_abandonments WHERE match_id=?',m.id).confirmed,null);
    assert.equal(f.c.one('SELECT completed FROM ranked_matches WHERE id=?',m.id).completed,null);
    f.db.exec('DROP TRIGGER stop_history');
    assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).delta,-100);
    assert.equal(f.history(m,m.b).delta,13);
});
