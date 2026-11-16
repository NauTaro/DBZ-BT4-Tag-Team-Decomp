import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
import {OnlineCoordinator} from '../online_worker.mjs';
import {launchRanked,rankedMonthAt,rankedMonthStart,rankedMonthEnd,advanceRankedMonth,rankedCollection} from '../ranked_seasons.mjs';
import {rankedRockSeed} from '../ranked_rocks.mjs';

const LAUNCH=Date.parse('2026-11-16T03:00:00Z'),NEXT=Date.parse('2026-12-01T03:00:00Z');
const plan={id:'launch:2026-11-16',not_before:LAUNCH};
function fixture(){
    const db=new DatabaseSync(':memory:');let now=LAUNCH-1000;
    const sql={exec(q,...args){if(!args.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...args);}};
    const c=new OnlineCore(sql,()=>now);
    function tx(fn){db.exec('BEGIN');try{const r=fn();db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}}
    function user(name,rating=0){const u=crypto.randomUUID();c.run('INSERT INTO online_users VALUES(?,?)',u,now);c.command(u,'nickname_set',{name});c.run("INSERT INTO ranked_players VALUES(?,'solo',?,?,0,0)",u,name,rating);return u;}
    function win(u,rating=13,wins=1,losses=0){c.run("UPDATE ranked_players SET rating=? WHERE user_id=?",rating,u);c.run('INSERT OR REPLACE INTO ranked_weekly VALUES(?,?,?,?)',rankedMonthAt(now),u,wins,losses);c.run('INSERT OR REPLACE INTO ranked_streaks VALUES(?,?,?)',u,wins,rating);}
    return {c,db,tx,user,win,now:n=>now=n,launch:()=>tx(()=>launchRanked(c,plan)),command:(u,op,a={})=>tx(()=>c.command(u,op,a))};
}
test('monthly boundary is day one at 00:00 Argentina, including year and leap-year changes',()=>{
    const boundary=Date.parse('2026-12-01T03:00:00Z');
    assert.equal(rankedMonthAt(boundary-1),'2026-11-01');
    assert.equal(rankedMonthAt(boundary),'2026-12-01');
    assert.equal(rankedMonthStart(boundary+1),boundary);
    assert.equal(rankedMonthEnd(boundary-1),boundary);
    assert.equal(rankedMonthAt(Date.parse('2027-01-01T02:59:59Z')),'2026-12-01');
    assert.equal(rankedMonthAt(Date.parse('2027-01-01T03:00:00Z')),'2027-01-01');
    assert.equal(rankedMonthEnd(Date.parse('2028-02-10T12:00:00Z')),Date.parse('2028-03-01T03:00:00Z'));
});

test('launch is time guarded, explicit and exactly once, retaining profiles and historical records',()=>{
    const f=fixture(),a=f.user('Alpha',1337),b=f.user('Beta',1000);
    f.c.run('INSERT INTO ranked_streaks VALUES(?,10,220)',a);
    f.c.run("INSERT INTO ranked_history VALUES('old',?,1000,1337,337)",a);
    const room=f.command(a,'create',{mode:'duel',wire:'test',content:'abcd',rules:'S'});
    assert.throws(()=>f.launch(),/RANKED_LAUNCH_TOO_EARLY/);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_resets').n,0);
    assert.equal(f.command(a,'ranked_status').rating,1337);
    f.now(LAUNCH);assert.equal(f.launch().state,'applied');
    assert.equal(f.command(a,'ranked_status').rating,0);assert.equal(f.command(a,'ranked_status').monthly_reset,true);
    assert.equal(f.command(a,'get').generation,room.generation);
    assert.equal(f.c.one('SELECT count(*) n FROM online_names').n,2);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,1);
    assert.equal(f.c.one('SELECT rating FROM ranked_reset_players WHERE user_id=?',a).rating,1337);
    f.win(a,42);assert.equal(f.launch().state,'already_applied');assert.equal(f.command(a,'ranked_status').rating,42);
    assert.throws(()=>f.tx(()=>launchRanked(f.c,{...plan,id:'launch:another'})),/RANKED_ALREADY_LAUNCHED/);
});
test('close archives the standings then zeroes points and streaks without granting Dragon Balls',()=>{
    const f=fixture(),users=['Alpha','Beta','Gamma','Delta'].map(n=>f.user(n));
    f.now(LAUNCH);f.launch();users.forEach((u,i)=>f.win(u,100-i*10,i+1));
    // A cached board cannot determine the podium.
    f.c.run("INSERT INTO ranked_top VALUES('2026-11-01',?,'[]')",LAUNCH);
    f.now(NEXT);const result=f.command(users[0],'ranked_status');
    assert.equal(result.rating,0);assert.equal(result.streak_wins,0);assert.equal(result.streak_points,0);
    assert.equal(result.dragon_balls,0);
    const archive=f.c.one("SELECT * FROM ranked_resets WHERE id='month:2026-11-01'");
    assert.deepEqual(JSON.parse(archive.standings).map(x=>x.rating),[100,90,80,70]);
    f.command(users[1],'ranked_status');f.tx(()=>advanceRankedMonth(f.c));
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_rock_finds').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_resets').n,2);
    assert.equal(f.command(users[3],'ranked_status').dragon_balls,0);
});
test('standings break ties using points, wins, fewer losses and stable account id',()=>{
    const f=fixture(),users=['Alpha','Beta','Gamma','Delta','Zero'].map(n=>f.user(n));
    f.now(LAUNCH);f.launch();
    f.win(users[0],100,3,2);f.win(users[1],100,4,2);f.win(users[2],100,4,1);f.win(users[3],100,4,1);f.win(users[4],0,99);
    f.now(NEXT);f.tx(()=>advanceRankedMonth(f.c));
    const tied=users.slice(2,4).sort();
    const archive=f.c.one("SELECT standings FROM ranked_resets WHERE id='month:2026-11-01'");
    assert.deepEqual(JSON.parse(archive.standings).map(row=>row.user_id),[...tied,users[1],users[0]]);
});
test('one roll per ranked pairing, shared by both clients, with no roll for casual or legacy rooms',t=>{
    const customRandom=crypto.getRandomValues.bind(crypto);
    let draws=0;
    t.mock.method(crypto,'getRandomValues',array=>{
        if(array instanceof Uint32Array){++draws;array[0]=512;return array;}
        return customRandom(array);
    });
    assert.equal(Array.from({length:1024},(_,i)=>rankedRockSeed(i)).filter(Boolean).length,2);
    const f=fixture(),users=['Alpha','Beta','Gamma','Delta'].map(n=>f.user(n));
    f.now(LAUNCH);f.launch();
    const args={mode:'duel',wire:'test:rock1',content:'abcd',rules:'S',ruleset:'solo'};
    f.command(users[2],'queue',args);const casual=f.command(users[3],'queue',args);
    assert.equal(casual.ranked_rock_seed,undefined);
    f.command(users[0],'ranked_queue',args);const ranked=f.command(users[1],'ranked_queue',args);
    assert.equal(ranked.ranked_rock_seed,2);
    assert.equal(f.command(users[0],'get').ranked_rock_seed,2);
    assert.equal(f.command(users[1],'ranked_queue',args).ranked_rock_seed,2);
    assert.equal(draws,1);
    assert.throws(()=>f.command(users[2],'ranked_rock',{match:ranked.generation,event:513}),/NOT_A_PARTICIPANT/);
    assert.throws(()=>f.command(users[2],'ranked_rock',{match:casual.generation,event:513}),/NOT_A_PARTICIPANT/);
    f.command(users[2],'leave',{generation:casual.generation});f.command(users[3],'leave',{generation:casual.generation});
    f.command(users[2],'ranked_queue',{...args,wire:'test'});assert.equal(f.command(users[3],'ranked_queue',{...args,wire:'test'}).ranked_rock_seed,undefined);
    assert.equal(draws,1);
});
test('a find needs both witnesses, is unique per match and stays with the fighter who broke the rock',()=>{
    const f=fixture(),a=f.user('Alpha'),b=f.user('Beta');
    f.now(LAUNCH);f.launch();
    const makeMatch=()=>{
        const id=crypto.randomUUID();
        f.c.run('INSERT INTO ranked_matches(id,season,a,b,admitted,seen_a,seen_b) VALUES(?,?,?,?,?,?,?)',id,'2026-11-01',a,b,LAUNCH,LAUNCH,LAUNCH);
        f.c.run('INSERT INTO ranked_rock_rolls VALUES(?,1)',id);
        return id;
    };
    let match=makeMatch();
    assert.equal(f.command(a,'ranked_rock',{match,event:514}).state,'waiting');
    f.command(a,'ranked_heartbeat',{match});f.command(b,'ranked_heartbeat',{match});
    assert.equal(f.command(a,'ranked_rock',{match,event:514}).state,'waiting');
    assert.equal(f.command(b,'ranked_rock',{match,event:513}).state,'disputed');
    assert.equal(f.command(b,'ranked_rock',{match,event:514}).state,'disputed');
    assert.equal(rankedCollection(f.c,b).dragon_balls,0);
    for(let i=1;i<=8;i++){
        match=makeMatch();
        f.command(a,'ranked_heartbeat',{match});f.command(b,'ranked_heartbeat',{match});
        assert.equal(f.command(b,'ranked_rock',{match,event:514}).state,'waiting');
        const notice=i<8?{finder:'Beta',stars:i}:{};
        assert.deepEqual(f.command(a,'ranked_rock',{match,event:514}),{state:'applied',ball:0,...notice});
        assert.deepEqual(f.command(b,'ranked_rock',{match,event:514}),{state:'applied',ball:i<8?i:0,...notice});
        assert.equal(f.command(b,'ranked_rock',{match,event:513}).state,'disputed');
    }
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_rock_finds').n,8);
    assert.equal(rankedCollection(f.c,a).dragon_balls,0);
    assert.equal(rankedCollection(f.c,b).collection_position,1);
    f.command(b,'nickname_set',{name:'Renamed'});
    f.now(NEXT);assert.equal(f.command(b,'ranked_status').dragon_balls,7);
});
test('only supported clients in the normal Quick Match queue can roll for a public find',t=>{
    const random=crypto.getRandomValues.bind(crypto);
    let draws=0;
    t.mock.method(crypto,'getRandomValues',data=>{
        if(!(data instanceof Uint32Array))return random(data);
        data[0]=512;
        ++draws;
        return data;
    });
    const f=fixture(),[a,b,c,d,e,g]=['Alpha','Beta','Gamma','Delta','Epsilon','Zeta'].map(n=>f.user(n));
    const args={mode:'duel',wire:'test',content:'abcd',rules:'S',rock_version:1};
    const privateRoom=f.command(a,'create',args);
    assert.equal(f.command(b,'join',{...args,code:privateRoom.code}).quick_rock_seed,undefined);
    assert.equal(f.command(a,'quick_rock',{match:privateRoom.generation,event:513}).state,'unavailable');
    f.command(a,'leave',{generation:privateRoom.generation});
    f.command(a,'queue',{...args,rock_version:0});
    const mixed=f.command(b,'queue',args);
    assert.equal(mixed.quick_rock_seed,undefined);
    for(const u of [a,b])f.command(u,'leave',{generation:mixed.generation});
    assert.equal(f.c.one('SELECT count(*) AS n FROM quick_rock_queue').n,0);
    for(const [mode,rules,players] of [['tag','2v2-114',[c,d,e,g]],['tag','ffa6-114',[a,b,c,d,e,g]],['lobby','S',[c,d]]]){
        const queued=players.map(u=>f.command(u,'queue',{...args,mode,rules})).at(-1);
        assert.equal(queued.quick_rock_seed,undefined);
        assert.equal(f.command(c,'quick_rock',{match:queued.generation,event:513}).state,'unavailable');
        for(const u of players)f.command(u,'leave',{generation:queued.generation});
    }
    assert.equal(draws,0);
    for(const rules of ['S','T']){
        f.now(LAUNCH);f.command(a,'queue',{...args,rules});
        f.now(LAUNCH+1);
        const publicRoom=f.command(b,'queue',{...args,rules});
        assert.equal(f.command(a,'get').self,publicRoom.host);
        assert.equal(publicRoom.quick_rock_seed,2);
        assert.equal(f.command(a,'get').quick_rock_seed,2);
        assert.equal(f.command(a,'queue',{...args,rules}).quick_rock_seed,2);
        assert.equal(f.command(c,'join',{...args,code:publicRoom.code,role:'spectator'}).quick_rock_seed,undefined);
        assert.throws(()=>f.command(c,'quick_rock',{match:publicRoom.generation,event:513}),/NOT_A_PARTICIPANT/);
        f.command(a,'leave',{generation:publicRoom.generation});
    }
    assert.equal(draws,2);
});
test('Quick Match needs matching reports and contributes to the same collection as Ranked',()=>{
    const f=fixture(),a=f.user('Alpha'),b=f.user('Beta');
    f.now(LAUNCH);f.launch();
    const ranked=crypto.randomUUID();
    f.c.run('INSERT INTO ranked_matches(id,season,a,b,admitted,started_a,started_b,seen_a,seen_b) VALUES(?,?,?,?,?,?,?,?,?)',
        ranked,'2026-11-01',a,b,LAUNCH,LAUNCH,LAUNCH,LAUNCH,LAUNCH);
    f.c.run('INSERT INTO ranked_rock_rolls VALUES(?,1)',ranked);
    f.command(a,'ranked_rock',{match:ranked,event:514});
    assert.equal(f.command(b,'ranked_rock',{match:ranked,event:514}).ball,1);
    const room=()=>{
        const match=crypto.randomUUID();
        f.c.run('INSERT INTO quick_rock_matches VALUES(?,?,?,?,?)',match,a,b,1,LAUNCH);
        return match;
    };
    let match=room();
    assert.throws(()=>f.command(a,'quick_rock',{match,event:32769}),/BAD_ROCK_EVENT/);
    assert.equal(f.command(a,'quick_rock',{match,event:514}).state,'waiting');
    assert.equal(f.command(b,'quick_rock',{match,event:513}).state,'disputed');
    assert.equal(rankedCollection(f.c,b).dragon_balls,1);
    match=room();
    assert.equal(f.command(a,'quick_rock',{match,event:514}).state,'waiting');
    assert.deepEqual(f.command(b,'quick_rock',{match,event:514}),{state:'applied',ball:2,finder:'Beta',stars:2});
    assert.deepEqual(f.command(a,'quick_rock',{match,event:514}),{state:'applied',ball:0,finder:'Beta',stars:2});
    assert.equal(f.command(b,'quick_rock',{match,event:513}).state,'disputed');
    assert.equal(rankedCollection(f.c,b).dragon_balls,2);
    assert.equal(f.command(b,'ranked_status').rating,0);
    assert.equal(f.c.one('SELECT count(*) AS n FROM quick_rock_finds').n,1);
});
test('months without eligible results and coordinator sleep do not invent podiums',()=>{
    const f=fixture(),u=f.user('Alpha');f.now(LAUNCH);f.launch();
    f.now(Date.parse('2027-03-01T03:00:00Z'));f.tx(()=>advanceRankedMonth(f.c));
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_rock_finds').n,0);
    assert.equal(f.command(u,'ranked_status').season,'2027-03-01');
});
test('reports from the previous scoring epoch cannot change the new month',()=>{
    const f=fixture(),a=f.user('Alpha'),b=f.user('Beta');f.now(LAUNCH);f.launch();
    f.now(NEXT-1000);
    const args={mode:'duel',wire:'test',content:'abcd',rules:'S',ruleset:'solo'};
    f.command(a,'ranked_queue',args);const r=f.command(b,'ranked_queue',args);
    f.command(a,'ranked_heartbeat',{match:r.generation});f.command(b,'ranked_heartbeat',{match:r.generation});
    f.now(NEXT);f.command(a,'ranked_status');
    assert.equal(f.command(a,'ranked_forfeit',{match:r.generation,reason:'local_quit'}).state,'expired');
    assert.equal(f.command(a,'ranked_status').rating,0);assert.equal(f.command(b,'ranked_status').rating,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,0);
    assert.equal(f.command(a,'get').generation,r.generation); // no room disconnect
});
test('the rollover is atomic when the score reset fails',()=>{
    const f=fixture(),a=f.user('Alpha');f.now(LAUNCH);f.launch();f.win(a,55);
    f.c.run("CREATE TRIGGER reject_reset BEFORE UPDATE ON ranked_players WHEN NEW.rating=0 BEGIN SELECT RAISE(ABORT,'test failure'); END");
    f.now(NEXT);assert.throws(()=>f.command(a,'ranked_status'),/test failure/);
    assert.equal(f.c.one('SELECT rating FROM ranked_players WHERE user_id=?',a).rating,55);
    assert.equal(f.c.one("SELECT count(*) n FROM ranked_resets WHERE reason='monthly'").n,0);
    f.c.run('DROP TRIGGER reject_reset');assert.equal(f.command(a,'ranked_status').rating,0);
});
test('the real launch endpoint requires the admin token, time boundary and POST',async()=>{
    const f=fixture(),u=f.user('Alpha',555),alarms=[];
    const coordinator=Object.assign(Object.create(OnlineCoordinator.prototype),{core:f.c,env:{ADMIN_SECRET:'test-only-admin'},
        ctx:{storage:{transactionSync:fn=>f.tx(fn),setAlarm:async t=>alarms.push(t)}}});
    const request=(token,method='POST')=>new Request('https://unit.invalid/v5/admin/ranked-launch',{
        method,headers:{Authorization:'Bearer '+token},...(method==='POST'?{body:JSON.stringify(plan)}:{})});
    assert.equal((await coordinator.fetch(request('wrong'))).status,401);
    assert.equal((await (await coordinator.fetch(request('test-only-admin'))).json()).error,'RANKED_LAUNCH_TOO_EARLY');
    assert.equal((await coordinator.fetch(request('test-only-admin','GET'))).status,400);
    assert.equal(f.command(u,'ranked_status').rating,555);
    f.now(LAUNCH);assert.equal((await (await coordinator.fetch(request('test-only-admin'))).json()).state,'applied');
    assert.equal((await (await coordinator.fetch(request('test-only-admin'))).json()).state,'already_applied');
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_resets').n,1);assert.equal(alarms.length,2);
});

test('temporary release access can back up approved tables without exposing sessions or accepting SQL',async()=>{
    const f=fixture();f.user('Alpha',555);
    const env={ADMIN_SECRET:'test-admin',RELEASE_ADMIN_SECRET:'test-release'};
    const coordinator=Object.assign(Object.create(OnlineCoordinator.prototype),{core:f.c,env});
    const call=(path,token,body={})=>coordinator.fetch(new Request('https://unit.invalid/v5/admin/'+path,{
        method:'POST',headers:{Authorization:'Bearer '+token},body:JSON.stringify(body)}));
    assert.equal((await call('ranked-audit','wrong')).status,401);
    const audit=await (await call('ranked-audit','test-release')).json();
    assert.equal(audit.users,1);assert.equal(audit.scores.total,555);
    assert.equal((await call('release-snapshot','test-admin',{table:'online_users',offset:0})).status,401);
    for(const table of ['online_sessions','online_users;DROP TABLE online_users'])
        assert.equal((await call('release-snapshot','test-release',{table,offset:0})).status,400);
    assert.equal((await call('release-snapshot','test-release',{table:'online_users',offset:-1})).status,400);
    const snapshot=await (await call('release-snapshot','test-release',{table:'ranked_players',offset:0})).json();
    assert.equal(snapshot.rows.length,1);assert.equal(snapshot.rows[0].rating,555);
    assert.equal(f.c.one('SELECT count(*) n FROM online_users').n,1);
    delete env.RELEASE_ADMIN_SECRET;
    assert.equal((await call('ranked-audit','test-release')).status,401);
    assert.equal((await call('ranked-audit','test-admin')).status,200);
});
