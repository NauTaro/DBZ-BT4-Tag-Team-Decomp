import {ONLINE_SCHEMA} from './online_schema.mjs';
const bytes=s=>new TextEncoder().encode(s).length;
const error=code=>{throw new Error(code);};
const UUID=/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/;
export function clientAtLeast(actual,minimum){
    const parse=v=>typeof v==='string'&&v.match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)-R(\d+)(?:\.(\d+))?$/);
    const a=parse(actual),b=parse(minimum);if(!a||!b)return false;
    for(let i=1;i<=6;i++){const x=Number(a[i]||0),y=Number(b[i]||0);if(!Number.isSafeInteger(x)||!Number.isSafeInteger(y))return false;if(x!==y)return x>y;}
    return true;
}
export function seasonAt(now){
    // Argentina, UTC-3: the weekly board rolls over Sunday at 23:59, points persist.
    const shifted=now-3*3600000+60000, d=new Date(shifted);
    const start=Date.UTC(d.getUTCFullYear(),d.getUTCMonth(),d.getUTCDate()-(d.getUTCDay()+6)%7-1);
    return new Date(start).toISOString().slice(0,10);
}
export class OnlineCore {
    constructor(sql,now=()=>Date.now(),uuid=()=>crypto.randomUUID()){
        this.sql=sql;this.now=now;this.uuid=uuid;this.events=[];
        sql.exec('PRAGMA foreign_keys=ON');sql.exec(ONLINE_SCHEMA);
    }
    rows(q,...args){return [...this.sql.exec(q,...args)];}
    one(q,...args){return this.rows(q,...args)[0];}
    run(q,...args){this.sql.exec(q,...args);}
    name(user){return this.one('SELECT name FROM online_names WHERE user_id=?',user)?.name;}
    member(user){return this.one('SELECT m.*,r.generation,r.owner,r.expires FROM online_members m JOIN online_rooms r ON r.id=m.room WHERE m.user_id=?',user);}
    descriptor(user){
        const m=this.member(user);if(!m||m.expires<=this.now())return {version:1,state:'none'};
        const r=this.one('SELECT * FROM online_rooms WHERE id=?',m.room);
        const members=this.rows('SELECT peer,name,role,udp FROM online_members WHERE room=? ORDER BY peer',r.id);
        return {version:1,room:r.id,code:r.code,generation:r.generation,mode:r.mode,wire:r.wire,content:r.content,
            rules:r.rules,host:1,self:m.peer,capacity:r.capacity,joinable:!!r.joinable,expires_at:new Date(r.expires).toISOString(),
            members:members.map(({peer,name,role})=>({peer,name,role})),udp:members.filter(p=>p.udp).map(p=>({peer:p.peer,addresses:JSON.parse(p.udp)}))};
    }
    changed(room){for(const p of this.rows('SELECT user_id FROM online_members WHERE room=?',room))this.events.push({user:p.user_id,event:'bt4.room',body:this.descriptor(p.user_id)});}
    close(room){
        for(const p of this.rows('SELECT m.user_id,r.generation FROM online_members m JOIN online_rooms r ON r.id=m.room WHERE room=?',room))
            this.events.push({user:p.user_id,event:'bt4.closed',body:{room,generation:p.generation}});
        this.run('DELETE FROM online_signals WHERE room=?',room);this.run('DELETE FROM online_members WHERE room=?',room);this.run('DELETE FROM online_rooms WHERE id=?',room);
    }
    prune(){
        const now=this.now();for(const r of this.rows('SELECT id FROM online_rooms WHERE expires<=?',now))this.close(r.id);
        for(const [table,column] of [['online_queue','expires'],['online_signals','expires'],['online_presence','expires'],['online_sessions','expires']])this.run(`DELETE FROM ${table} WHERE ${column}<=?`,now);
        // Profiles, registered names, match results and rating history are never pruned.
    }
    profile(a){
        if(!['duel','tag','lobby'].includes(a.mode)||typeof a.wire!=='string'||!a.wire.length||a.wire.length>64||
            typeof a.content!=='string'||!a.content.length||a.content.length>128||typeof a.rules!=='string'||!a.rules.length||a.rules.length>128)error('BAD_PROFILE');
    }
    create(user,a){
        this.profile(a);const current=this.member(user);
        if(current){const r=this.descriptor(user);if(current.owner===user&&r.mode===a.mode&&r.wire===a.wire&&r.content===a.content&&r.rules===a.rules)return r;error('ALREADY_IN_ROOM');}
        const name=this.name(user);if(!name)error('NICKNAME_REQUIRED');
        const capacity=a.capacity??(a.mode==='tag'?10:2);if(!Number.isInteger(capacity)||capacity<2||capacity>40||(a.mode==='tag'&&capacity>10))error('BAD_CAPACITY');
        const id=this.uuid(),generation=this.uuid();let code;
        const alphabet='ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
        do {const random=new Uint8Array(6);crypto.getRandomValues(random);code=Array.from(random,x=>alphabet[x%alphabet.length]).join('');}
        while(this.one('SELECT id FROM online_rooms WHERE code=?',code));
        this.run('INSERT INTO online_rooms VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)',id,code,generation,user,a.mode,a.wire,a.content,a.rules,capacity,a.listed?1:0,1,this.now()+1800000,2);
        this.run('INSERT INTO online_members(user_id,room,peer,name,role) VALUES(?,?,1,?,?)',user,id,name,'player');
        this.run('DELETE FROM online_queue WHERE user_id=?',user);return this.descriptor(user);
    }
    join(user,a){
        if(typeof a.code!=='string'||!/^[A-HJ-NP-Z2-9]{6}$/.test(a.code.toUpperCase()))error('BAD_CODE');
        const r=this.one('SELECT * FROM online_rooms WHERE code=?',a.code.toUpperCase());if(!r||r.expires<=this.now())error('ROOM_NOT_FOUND');
        if(r.wire!==a.wire||r.content!==a.content)error('INCOMPATIBLE_DATA');
        const existing=this.member(user);if(existing){if(existing.room===r.id)return this.descriptor(user);error('ALREADY_IN_ROOM');}
        const name=this.name(user);if(!name)error('NICKNAME_REQUIRED');const role=a.role??'player';
        if(!['player','spectator'].includes(role)||(r.mode==='tag'&&role!=='player'))error('BAD_PROFILE');
        if(!r.joinable)error('ROOM_BUSY');
        const total=this.one('SELECT count(*) n FROM online_members WHERE room=?',r.id).n;
        const players=this.one("SELECT count(*) n FROM online_members WHERE room=? AND role='player'",r.id).n;
        if(total>=r.capacity||(r.mode==='duel'&&role==='player'&&players>=2))error('ROOM_FULL');
        this.run('INSERT INTO online_members(user_id,room,peer,name,role) VALUES(?,?,?,?,?)',user,r.id,r.next_peer,name,role);
        this.run('UPDATE online_rooms SET next_peer=next_peer+1,expires=? WHERE id=?',this.now()+1800000,r.id);
        this.run('DELETE FROM online_queue WHERE user_id=?',user);this.changed(r.id);return this.descriptor(user);
    }
    queue(user,a,ranked=false){
        this.profile(a);if(ranked&&(a.ruleset!=='solo'||a.mode!=='duel'||a.rules!=='S'))error('BAD_RULESET');
        if(this.member(user)){const r=this.descriptor(user);if(r.mode===a.mode&&r.wire===(ranked?'ranked-v1:':'')+a.wire&&r.content===a.content&&r.rules===a.rules)return r;error('ALREADY_IN_ROOM');}
        const name=this.name(user);if(!name)error('NICKNAME_REQUIRED');
        const wire=ranked?'ranked-v1:'+a.wire:a.wire;
        const desired=a.mode==='tag'?({'ffa6-114':6,'2v2-114':4,'5v5-114':10}[a.rules]||0):2;
        if(!desired)error('BAD_PROFILE');
        this.run('INSERT INTO online_queue VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(user_id) DO UPDATE SET expires=excluded.expires',user,a.mode,wire,a.content,a.rules,name,this.now()+60000,this.now());
        // Changing queue requires an explicit cancellation; it cannot mutate an existing match.
        const own=this.one('SELECT * FROM online_queue WHERE user_id=?',user);
        if(own.mode!==a.mode||own.wire!==wire||own.content!==a.content||own.rules!==a.rules)error('ALREADY_QUEUED');
        const waiting=this.rows('SELECT user_id FROM online_queue WHERE mode=? AND wire=? AND content=? AND rules=? AND expires>? ORDER BY made,user_id LIMIT ?',a.mode,wire,a.content,a.rules,this.now(),desired);
        if(waiting.length<desired)return {version:1,state:'waiting'};
        const host=waiting[0].user_id;
        const first=this.create(host,{...a,wire,capacity:a.mode==='duel'?40:desired,listed:false});
        for(const peer of waiting.slice(1))this.join(peer.user_id,{code:first.code,wire,content:a.content,role:'player'});
        if(ranked){
            const now=this.now(),season=seasonAt(now),b=waiting[1].user_id;
            for(const u of [host,b])this.run("INSERT INTO ranked_players(user_id,ruleset,name) VALUES(?,'solo',?) ON CONFLICT(user_id,ruleset) DO UPDATE SET name=excluded.name",u,this.name(u));
            this.run('INSERT INTO ranked_matches(id,season,a,b,admitted,seen_a,seen_b) VALUES(?,?,?,?,?,?,?)',first.generation,season,host,b,now,now,now);
        }
        this.changed(first.room);return {...this.descriptor(user),...(ranked?{ranked:true,ruleset:'solo'}:{})};
    }
    signal(user,a){
        const m=this.member(user);if(!m||m.generation!==a.generation)error('NOT_A_MEMBER');
        const other=this.one('SELECT user_id FROM online_members WHERE room=? AND peer=? AND user_id<>?',m.room,a.to,user);if(!other)error('BAD_RECIPIENT');
        if(!['duel','tag','voice'].includes(a.channel)||!a.body||typeof a.body!=='object'||Array.isArray(a.body)||bytes(JSON.stringify(a.body))>16000)error('BAD_SIGNAL');
        const b=a.body,keys=Object.keys(b).filter(k=>k!=='revision').sort().join(',');
        if(a.kind==='udp'){
            if(a.channel!=='duel'||keys!=='addresses,nonce'||!Array.isArray(b.addresses)||b.addresses.length<1||b.addresses.length>4||
                b.addresses.some(x=>typeof x!=='string'||x.length>96||!/^[a-fA-F0-9.:\[\]]+$/.test(x))||!/^[a-f0-9]{32,64}$/.test(b.nonce))error('BAD_SIGNAL');
            this.run('UPDATE online_members SET udp=? WHERE user_id=?',JSON.stringify(b.addresses),user);
        }else if(['offer','answer'].includes(a.kind)){
            if(a.channel==='duel'||keys!=='sdp'||typeof b.sdp!=='string'||!b.sdp.length||b.sdp.length>14000)error('BAD_SIGNAL');
        }else if(a.kind==='candidate'){
            if(a.channel==='duel'||keys!=='candidate,mid'||typeof b.candidate!=='string'||!b.candidate.length||b.candidate.length>2048||typeof b.mid!=='string'||b.mid.length>32)error('BAD_SIGNAL');
        }else if(a.kind==='restart'){if(a.channel!=='voice'||Object.keys(b).length)error('BAD_SIGNAL');}
        else error('SIGNAL_KIND_NOT_ALLOWED');
        if(a.channel==='voice'&&a.kind!=='restart'&&(!Number.isInteger(b.revision)||b.revision<1||b.revision>2147483647))error('BAD_SIGNAL');
        const body={version:1,room:m.room,generation:m.generation,from:m.peer,to:a.to,channel:a.channel,kind:a.kind,body:b};
        if(this.one('SELECT count(*) n FROM online_signals WHERE recipient=? AND expires>?',other.user_id,this.now()).n>=256)error('RATE_LIMITED');
        this.run('INSERT INTO online_signals(room,generation,recipient,body,expires) VALUES(?,?,?,?,?)',m.room,m.generation,other.user_id,JSON.stringify(body),this.now()+60000);
        this.events.push({user:other.user_id,event:'bt4.signal',body});return {version:1,state:'sent'};
    }
    settlement(m,winner,digest){
        if(m.completed)return;
        const a=this.one("SELECT rating FROM ranked_players WHERE user_id=? AND ruleset='solo'",m.a).rating;
        const b=this.one("SELECT rating FROM ranked_players WHERE user_id=? AND ruleset='solo'",m.b).rating;
        const score=winner===m.a?1:winner===m.b?0:.5;
        let gain=Math.round(32*(score-1/(1+10**((b-a)/400))));gain=Math.max(-a,b-100000,Math.min(b,100000-a,gain));
        for(const [u,before,delta] of [[m.a,a,gain],[m.b,b,-gain]]){
            const win=winner===u?1:0,loss=winner&&winner!==u?1:0;
            this.run("UPDATE ranked_players SET rating=rating+?,wins=wins+?,losses=losses+? WHERE user_id=? AND ruleset='solo'",delta,win,loss,u);
            this.run('INSERT INTO ranked_history VALUES(?,?,?,?,?)',m.id,u,before,before+delta,delta);
            this.run('INSERT INTO ranked_weekly VALUES(?,?,?,?) ON CONFLICT(season,user_id) DO UPDATE SET wins=wins+excluded.wins,losses=losses+excluded.losses',m.season,u,win,loss);
        }
        this.run('UPDATE ranked_matches SET completed=?,winner=?,digest=? WHERE id=?',this.now(),winner,digest,m.id);
    }
    ranked(user,op,a){
        if(a.ruleset&&a.ruleset!=='solo')error('BAD_RULESET');const season=seasonAt(this.now());
        const paused=this.one("SELECT value FROM online_meta WHERE key='ranked_paused'")?.value==='1';
        if(paused&&op==='queue')error('RANKED_NOT_ENABLED');
        if(op==='queue')return this.queue(user,a,true);
        if(op==='status')return {version:1,enabled:!paused,season,...(this.one("SELECT rating,wins,losses FROM ranked_players WHERE user_id=? AND ruleset='solo'",user)||{rating:1000,wins:0,losses:0})};
        if(op==='top'){
            let top=this.one('SELECT * FROM ranked_top WHERE season=?',season);
            if(!top||top.updated<=this.now()-600000){
                const rows=this.rows("SELECT p.name,p.rating,w.wins,w.losses FROM ranked_weekly w JOIN ranked_players p ON p.user_id=w.user_id AND p.ruleset='solo' WHERE w.season=? ORDER BY p.rating DESC,p.user_id LIMIT 100",season);
                top={updated:this.now(),entries:JSON.stringify(rows)};this.run('INSERT INTO ranked_top VALUES(?,?,?) ON CONFLICT(season) DO UPDATE SET updated=excluded.updated,entries=excluded.entries',season,top.updated,top.entries);
            }return {season,updated_at:new Date(top.updated).toISOString(),entries:JSON.parse(top.entries)};
        }
        if(typeof a.match!=='string'||!UUID.test(a.match))error('BAD_REQUEST');
        const m=this.one('SELECT * FROM ranked_matches WHERE id=?',a.match);if(!m||![m.a,m.b].includes(user))error('NOT_A_PARTICIPANT');
        const result=state=>({state,...(this.one('SELECT after_rating rating,delta FROM ranked_history WHERE match_id=? AND user_id=?',m.id,user)||{})});
        if(m.completed)return result('already_applied');
        const now=this.now();if(m.void||m.admitted<now-5400000)return result('expired');
        if(op==='report'){
            if(!Number.isInteger(a.outcome)||a.outcome<0||a.outcome>2||typeof a.digest!=='string'||!/^[a-f0-9]{64}$/.test(a.digest))error('BAD_RESULT');
            const old=this.one('SELECT * FROM ranked_reports WHERE match_id=? AND user_id=?',m.id,user);
            if(old&&(old.digest!==a.digest||old.outcome!==a.outcome))error('RESULT_CONFLICT');
            this.run('INSERT OR IGNORE INTO ranked_reports VALUES(?,?,?,?)',m.id,user,a.outcome,a.digest);
            const other=this.one('SELECT * FROM ranked_reports WHERE match_id=? AND user_id<>?',m.id,user);if(!other)return result('waiting');
            if(other.digest!==a.digest||other.outcome!==a.outcome)return result('disputed');
            this.settlement(m,a.outcome===0?m.a:a.outcome===1?m.b:null,a.digest);return result('applied');
        }
        if(!['heartbeat','forfeit'].includes(op))error('BAD_REQUEST');
        const side=user===m.a?'a':'b';
        if(op==='heartbeat'){
            this.run(`UPDATE ranked_matches SET seen_${side}=?,started_${side}=coalesce(started_${side},?) WHERE id=?`,now,now,m.id);
            m['seen_'+side]=now;m['started_'+side]=m['started_'+side]??now;
        }
        if(m.started_a==null||m.started_b==null)return result('waiting');
        let quitter=null;
        if(op==='forfeit'&&a.reason==='local_quit')quitter=user;
        else if(op==='heartbeat'&&a.peer_gone===true){
            const other=side==='a'?'b':'a';if(m['seen_'+other]<now-45000)quitter=other==='a'?m.a:m.b;
        }
        if(!quitter)return result('waiting');
        const winner=quitter===m.a?m.b:m.a;
        const conflicting=this.rows('SELECT outcome FROM ranked_reports WHERE match_id=?',m.id).some(r=>r.outcome!==(winner===m.a?0:1));
        if(conflicting&&!(op==='forfeit'&&a.reason==='local_quit'))return result('waiting');
        this.settlement(m,winner,'forfeit:'+quitter);return result('applied');
    }
    command(user,op,a={}){
        this.events=[];
        if(!UUID.test(user)||!this.one('SELECT id FROM online_users WHERE id=?',user))error('AUTH_REQUIRED');
        if(!a||typeof a!=='object'||Array.isArray(a)||bytes(JSON.stringify(a))>18000)error('BAD_REQUEST');
        if(op==='nickname_get')return this.name(user)?{state:'ready',name:this.name(user)}:{state:'required'};
        if(op==='nickname_set'){
            if(typeof a.name!=='string')error('INVALID_NICKNAME');const name=a.name.normalize('NFKC').trim();
            if(!name.length||bytes(name)>19||/\p{Cc}/u.test(name)||name.toLocaleLowerCase('en-US')==='player')error('INVALID_NICKNAME');
            const normalized=name.toLocaleLowerCase('en-US'),existing=this.one('SELECT user_id FROM online_names WHERE normalized=?',normalized);
            if(existing&&existing.user_id!==user)error('NICKNAME_TAKEN');
            this.run('INSERT INTO online_names VALUES(?,?,?) ON CONFLICT(user_id) DO UPDATE SET name=excluded.name,normalized=excluded.normalized',user,name,normalized);
            this.run('UPDATE ranked_players SET name=? WHERE user_id=?',name,user);return {state:'ready',name};
        }
        if(op.startsWith('ranked_'))return this.ranked(user,op.slice(7),a);
        if(op==='create')return this.create(user,a);
        if(op==='join')return this.join(user,a);
        if(op==='queue')return this.queue(user,a);
        if(op==='get')return this.descriptor(user);
        if(op==='signal')return this.signal(user,a);
        if(op==='cancel'){this.run('DELETE FROM online_queue WHERE user_id=?',user);return {state:'cancelled'};}
        if(op==='leave'){
            const m=this.member(user);if(!m||a.generation&&a.generation!==m.generation)return {state:'left'};
            if(user===m.owner)this.close(m.room);else{this.run('DELETE FROM online_members WHERE user_id=?',user);this.changed(m.room);}return {state:'left'};
        }
        if(op==='lease'){
            const m=this.member(user);if(!m)error('NOT_A_MEMBER');
            if(typeof a.joinable==='boolean'&&m.owner!==user)error('NOT_OWNER');
            this.run('UPDATE online_rooms SET expires=? WHERE id=?',this.now()+1800000,m.room);
            if(typeof a.joinable==='boolean')this.run('UPDATE online_rooms SET joinable=? WHERE id=?',a.joinable?1:0,m.room);return this.descriptor(user);
        }
        if(op==='get_signals'){
            const m=this.member(user);if(!m||m.generation!==a.generation)error('NOT_A_MEMBER');
            const after=Number.isSafeInteger(a.after)&&a.after>=0?a.after:0;
            const packets=this.rows('SELECT id,body FROM online_signals WHERE recipient=? AND generation=? AND id>? AND expires>? ORDER BY id LIMIT 64',user,m.generation,after,this.now());
            let size=0,cursor=after;const signals=[];
            for(const packet of packets){const n=bytes(packet.body);if(signals.length&&size+n>14500)break;size+=n;signals.push(JSON.parse(packet.body));cursor=packet.id;}
            return {cursor,signals};
        }
        if(op==='presence'){
            if(a.active)this.run('INSERT INTO online_presence VALUES(?,?) ON CONFLICT(user_id) DO UPDATE SET expires=excluded.expires',user,this.now()+360000);
            else this.run('DELETE FROM online_presence WHERE user_id=?',user);
            return {online:this.one('SELECT count(*) n FROM online_presence WHERE expires>?',this.now()).n};
        }
        if(op==='counts')return Object.fromEntries(this.rows('SELECT rules,count(*) n FROM online_queue WHERE wire=? AND content=? AND expires>? GROUP BY rules',a.wire,a.content,this.now()).map(r=>[({'ffa6-114':'F6','2v2-114':'T2','5v5-114':'T5'}[r.rules]||r.rules),r.n]));
        if(op==='voice'){
            if(typeof a.token!=='string'||!/^[a-f0-9]{64}$/.test(a.token)||![0,1].includes(a.seat))error('BAD_REQUEST');
            const r=this.one("SELECT id,code,owner FROM online_rooms WHERE mode='duel' AND wire='bt4-voice-direct-1' AND content=? AND expires>?",a.token,this.now());
            if(!r){if(a.seat===1)return {state:'waiting'};return this.create(user,{mode:'duel',wire:'bt4-voice-direct-1',content:a.token,rules:'voice-direct',capacity:2});}
            if(this.member(user)?.room===r.id)return this.descriptor(user);
            if(a.seat===0||r.owner===user)error('NOT_OWNER');return this.join(user,{code:r.code,wire:'bt4-voice-direct-1',content:a.token});
        }
        if(op==='list')return {rooms:this.rows('SELECT code,mode,rules,capacity FROM online_rooms WHERE mode=? AND wire=? AND content=? AND listed=1 AND joinable=1 AND expires>? ORDER BY id LIMIT 40',a.mode,a.wire,a.content,this.now())};
        error('BAD_REQUEST');
    }
}
