import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {OnlineCore} from '../online_core.mjs';
import {ONLINE_SCHEMA} from '../online_schema.mjs';
import {characterSelection} from '../online_characters.mjs';

function fixture(old=false){
    const db=new DatabaseSync(':memory:');let now=Date.parse('2026-11-13T12:00:00Z');
    const sql={exec(q,...a){if(!a.length&&!/^(SELECT|PRAGMA table_info)/i.test(q)){db.exec(q);return [];}return db.prepare(q).all(...a);}};
    if(old)db.exec(ONLINE_SCHEMA.replace(', chars TEXT',''));
    const c=new OnlineCore(sql,()=>now);
    const user=name=>{const id=crypto.randomUUID();c.run('INSERT INTO online_users VALUES(?,?)',id,now);c.command(id,'nickname_set',{name});return id;};
    const call=(u,op,a)=>{db.exec('BEGIN');try{const r=c.command(u,op,a);db.exec('COMMIT');return r;}catch(e){db.exec('ROLLBACK');throw e;}};
    return {c,sql,db,user,call,tick:n=>now+=n};
}
const profile={mode:'duel',wire:'test',content:'abcd',rules:'S'};
function ranked(f){const x=f.user('One'),y=f.user('Two');f.call(x,'ranked_queue',{...profile,ruleset:'solo'});const room=f.call(y,'ranked_queue',{...profile,ruleset:'solo'});return f.c.one('SELECT * FROM ranked_matches WHERE id=?',room.generation);}
test('nullable migration upgrades the previous schema and may run repeatedly',()=>{
    const f=fixture(true),m=ranked(f);new OnlineCore(f.sql);new OnlineCore(f.sql);
    assert.equal(f.c.one('SELECT chars FROM ranked_matches WHERE id=?',m.id).chars,null);
});
test('integer engine IDs only; malformed/partial/oversized fields are rejected',()=>{
    assert.equal(characterSelection({}),null);
    assert.deepEqual(characterSelection({char_a:0,char_b:511}),{a:[0],b:[511]});
    for(const id of [-1,512,1.5,'12',null,true,{},Infinity])assert.throws(()=>characterSelection({char_a:id,char_b:0}),/BAD_CHARACTERS/);
    for(const a of [{char_a:1},{chars_a:[1]},{chars_a:[],chars_b:[1]},{char_a:1,char_b:2,chars_a:[1],chars_b:[2]},{chars_a:Array(11).fill(1),chars_b:[2]}])assert.throws(()=>characterSelection(a),/BAD_CHARACTERS/);
});
test('start heartbeat records selection before a forfeit, result digest and Elo are unchanged',()=>{
    const f=fixture(),m=ranked(f),chars={char_a:187,char_b:310};
    assert.equal(f.call(m.a,'ranked_heartbeat',{match:m.id,...chars}).state,'waiting');
    assert.equal(f.c.one('SELECT chars FROM ranked_matches WHERE id=?',m.id).chars,'{"a":[187],"b":[310]}');
    f.call(m.b,'ranked_heartbeat',{match:m.id});
    f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit',...chars});
    assert.equal(f.call(m.b,'ranked_heartbeat',{match:m.id}).delta,16);
    const done=f.c.one('SELECT * FROM ranked_matches WHERE id=?',m.id);
    assert.equal(done.digest,'forfeit:'+m.a);assert.equal(done.winner,m.b);
    const mirror=JSON.parse(f.c.one("SELECT body FROM online_legacy WHERE source='cloudflare' AND row_key=?",m.id).body);
    assert.deepEqual(mirror.chars,{a:[187],b:[310]});assert.equal(mirror.result_digest,done.digest);
});
test('legacy result reports still settle; late metadata does not rewrite digest or points',()=>{
    const f=fixture(),m=ranked(f),digest='a'.repeat(64);
    for(const u of [m.a,m.b])f.call(u,'ranked_heartbeat',{match:m.id});
    for(const u of [m.a,m.b])f.call(u,'ranked_report',{match:m.id,outcome:0,digest});
    f.call(m.b,'ranked_report',{match:m.id,outcome:0,digest,char_a:0,char_b:1});
    const done=f.c.one('SELECT * FROM ranked_matches WHERE id=?',m.id);
    assert.equal(done.digest,digest);assert.deepEqual(JSON.parse(done.chars),{a:[0],b:[1]});
    assert.equal(f.c.one('SELECT count(*) n FROM ranked_history WHERE match_id=?',m.id).n,2);
    f.call(m.a,'ranked_report',{match:m.id,outcome:0,digest,char_a:2,char_b:3});
    assert.equal(f.c.one('SELECT chars FROM ranked_matches WHERE id=?',m.id).chars,done.chars);
});
test('invalid metadata rolls back without poisoning the normal result',()=>{
    const f=fixture(),m=ranked(f);
    assert.throws(()=>f.call(m.a,'ranked_heartbeat',{match:m.id,char_a:512,char_b:1}),/BAD_CHARACTERS/);
    assert.equal(f.c.one('SELECT started_a FROM ranked_matches WHERE id=?',m.id).started_a,null);
    f.call(m.a,'ranked_heartbeat',{match:m.id});f.call(m.b,'ranked_heartbeat',{match:m.id});
    assert.equal(f.call(m.a,'ranked_forfeit',{match:m.id,reason:'local_quit'}).delta,-16);
});
test('casual starts survive room departure, results need participant agreement, rematches have distinct keys',()=>{
    const f=fixture(),a=f.user('Alpha'),b=f.user('Bravo'),outsider=f.user('Outside');
    const room=f.call(a,'create',profile);f.call(b,'join',{...profile,code:room.code});
    const args={generation:room.generation,match:room.generation+':42:1',mode:'duel',char_a:7,char_b:30,names_a:['Alpha'],names_b:['Bravo']};
    assert.throws(()=>f.call(outsider,'match_report',args),/NOT_A_MEMBER/);
    for(const u of [a,b])assert.equal(f.call(u,'match_report',args).state,'recorded');
    f.call(a,'match_report',{...args,match:room.generation+':42:2',char_a:8});
    f.call(a,'leave',{generation:room.generation});
    assert.equal(f.c.one("SELECT count(*) n FROM online_legacy WHERE table_name='online_matches'").n,2);
    for(const u of [a,b])f.call(u,'match_report',{...args,outcome:1,digest:'b'.repeat(64)});
    const body=JSON.parse(f.c.one('SELECT body FROM online_legacy WHERE row_key=?',args.match).body);
    assert.deepEqual(body.chars,{a:[7],b:[30]});assert.deepEqual(body.players_a,[a]);assert.equal(body.outcome,1);
    assert.ok(body.completed);assert.equal(body.character_reports.length,2);
});
test('Tag 2v2, uneven 5v5 with CPUs, duplicates and FFA keep stable player ownership',()=>{
    for(const [left,right,ffa] of [[2,2,false],[5,3,false],[6,0,true]]){
        const f=fixture(),a=f.user('Alpha'),b=f.user('Bravo');
        const room=f.call(a,'create',{...profile,mode:'tag',capacity:10});f.call(b,'join',{...profile,code:room.code});
        const args={generation:room.generation,match:room.generation+':tag:4',mode:'tag',free_for_all:ffa,
            chars_a:Array(left).fill(1),chars_b:Array(right).fill(1),peers_a:[1,...Array(left-1).fill(0)],peers_b:right?[2,...Array(right-1).fill(0)]:[]};
        if(ffa)args.peers_a[1]=2;
        f.call(a,'match_report',args);f.call(b,'match_report',args);
        const body=JSON.parse(f.c.one('SELECT body FROM online_legacy WHERE row_key=?',args.match).body);
        assert.equal(body.chars.a.length,left);assert.equal(body.chars.b.length,right);assert.equal(body.players_a[0],a);
        assert.equal(ffa?body.players_a[1]:body.players_b[0],b);
        assert.throws(()=>f.call(a,'match_report',{...args,match:room.generation+':bad',peers_a:[999,...Array(left-1).fill(0)]}),/BAD_CHARACTERS/);
    }
});
