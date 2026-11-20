import test from 'node:test';
import './online_characters.test.mjs';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore,seasonAt,clientAtLeast} from '../online_core.mjs';

function fixture(){
    const db=new DatabaseSync(':memory:');let now=Date.parse('2026-11-11T15:00:00Z');
    const sql={exec(q,...args){if(!args.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...args);}};
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
test('the public directory lists compatible vacant rooms and hides private or expired rooms',()=>{
    const f=fixture(),reader=f.user('Browser');
    const query={wire:'bt4-duel-1429484564',content:'aabbccdd11223344',tag_content:'9988776655443311'};
    const duel={mode:'lobby',wire:query.wire,content:query.content,rules:'S',capacity:3};
    const tag={mode:'tag',wire:'bt4-p2p-2',content:query.tag_content,rules:'tag-114',capacity:2};
    const openHost=f.user('OpenDuels'),tagHost=f.user('OpenTags');
    const openDuel=f.invoke(openHost,'create',{...duel,listed:true});
    const openTag=f.invoke(tagHost,'create',{...tag,listed:true});
    const tour=f.invoke(f.user('OpenTournament'),'create',{...duel,rules:'public-tournament',capacity:34,listed:true});
    const privateRoom=f.invoke(f.user('CodeOnly'),'create',duel);
    f.invoke(f.user('WrongVersion'),'create',{...tag,content:'0123456789abcdef',listed:true});
    f.invoke(f.user('Busy'),'create',{...duel,listed:true});
    f.c.run("UPDATE online_rooms SET joinable=0 WHERE owner=(SELECT user_id FROM online_names WHERE name='Busy')");
    f.invoke(f.user('Expired'),'create',{...duel,listed:true});
    f.c.run("UPDATE online_rooms SET expires=1 WHERE owner=(SELECT user_id FROM online_names WHERE name='Expired')");
    f.invoke(f.user('NotABoolean'),'create',{...duel,listed:'false'});
    const rooms=()=>f.invoke(reader,'public_rooms',query).rooms;
    assert.deepEqual(rooms().map(r=>r.kind).sort(),[0,1,2]);
    assert.equal(rooms().find(r=>r.code===openDuel.code).name,'OpenDuels');
    assert.equal(rooms().find(r=>r.code===tour.code).capacity,34);
    const guest=f.user('Guest');
    f.invoke(guest,'join',{...tag,code:openTag.code});
    assert(!rooms().some(r=>r.code===openTag.code));
    f.invoke(guest,'leave',{generation:openTag.generation});
    assert(rooms().some(r=>r.code===openTag.code));
    f.invoke(tagHost,'lease',{joinable:false});
    assert(!rooms().some(r=>r.code===openTag.code));
    assert.throws(()=>f.invoke(guest,'join',{...tag,code:openTag.code}),/ROOM_BUSY/);
    assert.equal(f.invoke(guest,'join',{...duel,code:privateRoom.code}).members.length,2);
    assert.throws(()=>f.invoke(reader,'public_rooms',{...query,tag_content:'invalid'}),/BAD_PROFILE/);
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
    assert.ok(clientAtLeast('0.1.39.6','0.1.39.5-R5.8'));
    assert.ok(clientAtLeast('0.1.39.6','0.1.39.6'));
    assert.ok(!clientAtLeast('0.1.39.5-R99','0.1.39.6'));
    assert.ok(!clientAtLeast('0.1.39.6-dev','0.1.39.6'));
});
function rankedPair(f){const a=f.user('First'),b=f.user('Second');const args={...profile,ruleset:'solo'};
    f.invoke(a,'ranked_queue',args);const r=f.invoke(b,'ranked_queue',args);
    f.invoke(a,'ranked_heartbeat',{match:r.generation});f.invoke(b,'ranked_heartbeat',{match:r.generation});
    const admitted=f.c.one('SELECT a,b FROM ranked_matches WHERE id=?',r.generation);return {...admitted,id:r.generation};}
test('ranked rooms return the rating of the two competitors only',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);
    f.c.run("UPDATE ranked_players SET rating=1499 WHERE user_id=?",a);
    f.c.run("UPDATE ranked_players SET rating=0 WHERE user_id=?",b);
    const room=f.invoke(a,'get');
    assert.deepEqual(room.members.map(m=>m.rating).sort((x,y)=>x-y),[0,1499]);
    const observer=f.user('Observer');
    const casual=f.invoke(observer,'create',profile);
    assert.equal(casual.members[0].rating,undefined);
    assert.equal(room.generation,id);
});
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
test('pruning retires only transient state after an hour and retains ranking/history',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'});
    f.tick(3599999);f.c.prune();assert.equal(f.c.one('SELECT count(*) n FROM online_rooms').n,1);
    f.tick(1);f.c.prune();assert.equal(f.c.one('SELECT count(*) n FROM online_rooms').n,0);
    assert.equal(f.c.one('SELECT count(*) n FROM online_names').n,2);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history').n,2);
});
test('weekly rollover occurs Sunday 23:59 Argentina, without resetting ratings',()=>{
    const before=Date.parse('2026-11-09T02:58:59Z'),after=Date.parse('2026-11-09T02:59:00Z');
    assert.equal(seasonAt(before),'2026-11-01');assert.equal(seasonAt(after),'2026-11-08');
    const f=fixture(),{a,id}=rankedPair(f);f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'});
    const rating=f.invoke(a,'ranked_status').rating;f.tick(7*86400000);assert.equal(f.invoke(a,'ranked_status').rating,rating);
});
test('private lobbies, duels and Tag hold a one hour lease and tournament discovery seats 22',()=>{
    const f=fixture();
    for(const mode of ['duel','tag','lobby']){
        const host=f.user(mode),capacity=mode==='lobby'?22:mode==='tag'?10:2;
        const room=f.invoke(host,'create',{...profile,mode,capacity});
        if(mode==='lobby'){
            for(let i=1;i<22;i++) assert.equal(f.invoke(f.user('Guest '+i),'join',{...profile,code:room.code}).members.length,i+1);
            assert.throws(()=>f.invoke(f.user('Overflow'),'join',{...profile,code:room.code}),/ROOM_FULL/);
        }
        f.tick(3599999);
        assert.equal(f.invoke(host,'get').generation,room.generation);
        f.invoke(host,'lease',{generation:room.generation});
        f.tick(3599999);f.c.prune();
        assert.equal(f.invoke(host,'get').generation,room.generation);
        f.tick(1);f.c.prune();
        assert.equal(f.invoke(host,'get').state,'none');
    }
});

function finishRanked(f, first, second, winner, forfeit=false){
    const args={...profile,ruleset:'solo'};
    f.invoke(first,'ranked_queue',args);
    const {generation:match}=f.invoke(second,'ranked_queue',args);
    const {a,b}=f.c.one('SELECT a,b FROM ranked_matches WHERE id=?',match);
    f.invoke(a,'ranked_heartbeat',{match});
    f.invoke(b,'ranked_heartbeat',{match});
    const digest='c'.repeat(64),outcome=winner===a?0:winner===b?1:2;
    if(forfeit)f.invoke(winner===a?b:a,'ranked_forfeit',{match,reason:'local_quit'});
    else{
        f.invoke(a,'ranked_report',{match,outcome,digest});
        f.invoke(b,'ranked_report',{match,outcome,digest});
    }
    const result=f.invoke(first,'ranked_heartbeat',{match});
    f.invoke(a,'leave',{generation:match});
    f.tick(60000);
    return {result,match};
}

test('streaks preserve win rewards but losses depend on opponent rating',()=>{
    const f=fixture(),a=f.user('Rising');
    f.c.run("INSERT INTO ranked_players(user_id,ruleset,name,rating) VALUES(?,'solo','Rising',1000)",a);
    f.c.run("INSERT INTO online_meta VALUES('ranked_scoring','streak')");
    for(let i=0;i<20;i++){
        const opponent=f.user('Opponent'+i);
        assert.equal(finishRanked(f,a,opponent,a).result.delta,13+2*i);
    }
    let status=f.invoke(a,'ranked_status');
    assert.equal(status.rating,1640);
    assert.equal(status.streak_wins,20);
    assert.equal(status.streak_points,640);
    const victor=f.user('Rival');
    const result=finishRanked(f,a,victor,victor).result;
    assert.equal(result.delta,-32);
    assert.equal(result.rating,1608);
    status=f.invoke(a,'ranked_status');
    assert.equal(status.streak_wins,0);
    assert.equal(status.streak_points,0);
    assert.equal(finishRanked(f,a,f.user('Fresh'),a).result.delta,13);
});

test('new streak players start at zero without resetting existing accounts',()=>{
    const f=fixture(),a=f.user('NewA'),b=f.user('NewB');
    f.c.run("INSERT INTO online_meta VALUES('ranked_scoring','streak')");
    assert.equal(f.invoke(a,'ranked_status').rating,0);
    const result=finishRanked(f,a,b,a).result;
    assert.equal(result.rating,13);
    assert.equal(f.invoke(b,'ranked_status').rating,0);
});

test('repeat departures settle the winner points and record the result without a review',()=>{
    const f=fixture(),a=f.user('Stayer'),b=f.user('Leaver');
    f.c.run("INSERT INTO online_meta VALUES('ranked_scoring','streak')");
    assert.equal(finishRanked(f,a,b,a,true).result.delta,13);
    assert.equal(finishRanked(f,b,a,a,true).result.delta,0);
    const before=f.invoke(a,'ranked_status');
    const {match,result}=finishRanked(f,a,b,a,true);
    assert.equal(result.under_review,false);
    assert.equal(result.delta,17);
    assert.equal(f.c.one('SELECT reason FROM ranked_reviews WHERE match_id=?',match),undefined);
    assert.equal(f.invoke(a,'ranked_status').rating,before.rating+17);
    assert.equal(f.invoke(a,'ranked_status').streak_wins,before.streak_wins+1);
    assert.equal(f.invoke(b,'ranked_heartbeat',{match}).delta,0);
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history WHERE match_id=?',match).n,2);
});

test('streak draws preserve the streak and older admitted games settle under Elo',()=>{
    const f=fixture(),{a,b,id}=rankedPair(f);
    f.c.run("INSERT INTO online_meta VALUES('ranked_scoring','streak')");
    assert.equal(f.invoke(a,'ranked_forfeit',{match:id,reason:'local_quit'}).delta,-16);
    f.invoke(a,'leave',{generation:id});
    f.tick(30001);
    assert.equal(finishRanked(f,a,b,a).result.delta,13);
    assert.equal(finishRanked(f,a,b,null).result.delta,0);
    assert.equal(f.invoke(a,'ranked_status').streak_wins,1);
});
