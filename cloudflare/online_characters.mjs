// Numeric engine roster IDs, matching Tt_ChooseSeat in port/src/tag_team.c.
// Optional metadata: it is deliberately excluded from every result digest.
export const ROSTER_ID_LIMIT=512;
export function characterSelection(args){
    const keys=['char_a','char_b','chars_a','chars_b'];
    if(!keys.some(k=>Object.hasOwn(args,k)))return null;
    const scalar=Object.hasOwn(args,'char_a')||Object.hasOwn(args,'char_b');
    const array=Object.hasOwn(args,'chars_a')||Object.hasOwn(args,'chars_b');
    if(scalar&&array)throw new Error('BAD_CHARACTERS');
    const a=scalar?[args.char_a]:args.chars_a,b=scalar?[args.char_b]:args.chars_b;
    if(!Array.isArray(a)||!Array.isArray(b)||!a.length||a.length>10||b.length>7||a.length+b.length>14||
        (!b.length&&args.free_for_all!==true)||[...a,...b].some(id=>!Number.isInteger(id)||id<0||id>=ROSTER_ID_LIMIT))throw new Error('BAD_CHARACTERS');
    return {a,b};
}

export function migrateCharacters(sql){
    if(![...sql.exec('PRAGMA table_info(ranked_matches)')].some(c=>c.name==='chars'))
        sql.exec('ALTER TABLE ranked_matches ADD COLUMN chars TEXT');
}

export function mirrorRanked(core,m){
    const chars=m.chars?JSON.parse(m.chars):null;
    // Live Cloudflare rows have their own provenance. Imported bodies retain
    // every existing field when their corresponding match receives metadata.
    const rows=[];
    for(const source of ['primary','secondary','cloudflare'])for(const key of [m.id,JSON.stringify([m.id,null,null,null,null])]){
        const row=core.one("SELECT source,row_key,body FROM online_legacy WHERE source=? AND table_name='ranked_matches' AND row_key=?",source,key);
        if(row)rows.push(row);
    }
    if(!rows.some(r=>r.source==='cloudflare'))rows.push({source:'cloudflare',row_key:m.id,body:'{}'});
    for(const row of rows){
        const body={...JSON.parse(row.body),id:m.id,player_a:m.a,player_b:m.b,started_a:m.started_a,
            started_b:m.started_b,completed:m.completed,winner:m.winner,result_digest:m.digest,chars};
        core.run('INSERT INTO online_legacy VALUES(?,?,?,?) ON CONFLICT(source,table_name,row_key) DO UPDATE SET body=excluded.body',row.source,'ranked_matches',row.row_key,JSON.stringify(body));
    }
}

export function saveRankedCharacters(core,m,args){
    const chars=characterSelection(args);if(!chars)return;
    if(chars.a.length!==1||chars.b.length!==1)throw new Error('BAD_CHARACTERS');
    // Never let a late or contradictory reporter replace a recorded selection.
    // Metadata disagreement must not prevent settlement of an existing result.
    if(!m.chars&&(m.started_a!=null||m.started_b!=null)){
        m.chars=JSON.stringify(chars);core.run('UPDATE ranked_matches SET chars=? WHERE id=?',m.chars,m.id);
    }
}

export function reportOnlineMatch(core,user,args){
    const chars=characterSelection(args);
    if(!chars||typeof args.match!=='string'||args.match.length>160||typeof args.generation!=='string'||
        !args.match.startsWith(args.generation+':')||!['duel','tag'].includes(args.mode))throw new Error('BAD_CHARACTERS');
    const key=args.match,old=core.one("SELECT body FROM online_legacy WHERE source='cloudflare' AND table_name='online_matches' AND row_key=?",key);
    let body=old?JSON.parse(old.body):null;
    if(!body){
        const room=core.member(user);
        if(!room||room.generation!==args.generation)throw new Error('NOT_A_MEMBER');
        const r=core.one('SELECT mode,owner FROM online_rooms WHERE id=?',room.room);
        if((args.mode==='tag')!==(r.mode==='tag'))throw new Error('BAD_CHARACTERS');
        if(args.mode==='tag'&&(chars.a.length+chars.b.length>10||(!args.free_for_all&&(chars.a.length>5||chars.b.length>5))))throw new Error('BAD_CHARACTERS');
        const members=core.rows('SELECT user_id,peer,name,role FROM online_members WHERE room=?',room.room);
        const resolve=(side)=>{
            const ids=args['peers_'+side],names=args['names_'+side],list=chars[side];
            if(args.mode==='tag'){
                if(!Array.isArray(ids)||ids.length!==list.length)throw new Error('BAD_CHARACTERS');
                return ids.map(id=>{if(id===0)return null;const p=members.find(p=>p.peer===id&&p.role==='player');if(!Number.isSafeInteger(id)||!p)throw new Error('BAD_CHARACTERS');return p.user_id;});
            }
            if(!Array.isArray(names)||names.length!==1)throw new Error('BAD_CHARACTERS');
            const p=members.find(p=>p.name===names[0]&&p.role==='player');if(!p)throw new Error('BAD_CHARACTERS');
            return list.map(()=>p.user_id);
        };
        const players_a=resolve('a'),players_b=resolve('b');
        if(![...players_a,...players_b].includes(user)&&r.owner!==user)throw new Error('NOT_A_PARTICIPANT');
        body={id:key,generation:args.generation,mode:args.mode,free_for_all:args.free_for_all===true,chars,
            players_a,players_b,started_a:core.now(),started_b:core.now(),character_reports:[],result_reports:{}};
    }
    if(![...body.players_a,...body.players_b].includes(user))throw new Error('NOT_A_PARTICIPANT');
    if(JSON.stringify(chars)!==JSON.stringify(body.chars))return {state:'characters_disputed'};
    if(!body.character_reports.includes(user))body.character_reports.push(user);
    if(Object.hasOwn(args,'outcome')){
        if(!Number.isInteger(args.outcome)||args.outcome<0||args.outcome>2||typeof args.digest!=='string'||!/^[a-f0-9]{64}$/.test(args.digest))throw new Error('BAD_RESULT');
        if(!body.result_reports[user])body.result_reports[user]={outcome:args.outcome,digest:args.digest};
        const humans=[...new Set([...body.players_a,...body.players_b].filter(Boolean))];
        if(humans.every(u=>body.result_reports[u]?.outcome===args.outcome&&body.result_reports[u]?.digest===args.digest)){
            body.completed??=core.now();body.outcome=args.outcome;body.result_digest=args.digest;
        }
    }
    core.run("INSERT INTO online_legacy VALUES('cloudflare','online_matches',?,?) ON CONFLICT(source,table_name,row_key) DO UPDATE SET body=excluded.body",key,JSON.stringify(body));
    return {state:'recorded'};
}
