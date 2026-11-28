const minute = 60_000;
const day = 1440 * minute;
const argentina = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'America/Argentina/Buenos_Aires', weekday: 'short',
    hour: '2-digit', minute: '2-digit', hourCycle: 'h23'
});
const weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

export function diosTubeWindow(now) {
    const local = Object.fromEntries(argentina.formatToParts(new Date(now)).map(p => [p.type, p.value]));
    const sinceMidnight = Number(local.hour) * 60 + Number(local.minute);
    const sinceSunday = weekdays.indexOf(local.weekday);
    let minutesSinceReset = sinceSunday * 1440 + sinceMidnight - 1439;
    if (minutesSinceReset < 0) minutesSinceReset += 7 * 1440;
    const reset = Math.floor(now / minute) * minute - minutesSinceReset * minute;
    return {reset, since: Math.max(reset, now - day), until: now};
}

// Scores use finalized fight telemetry and the pre-fight ratings.
export function diosTubeScore(fight) {
    const clamp = (value, max) => Number.isFinite(value) ? Math.max(0, Math.min(value, max)) : 0;
    const counters = clamp(fight.zCounters, 64);
    const hits = clamp(fight.hits, 500);
    const combo = clamp(fight.longestCombo, 70);
    const winner = clamp(fight.winnerRating, 100_000);
    const loser = clamp(fight.loserRating, 100_000);
    return Math.round(80 * Math.sqrt(counters) + 20 * Math.sqrt(hits) + 4 * combo +
        40 * Math.log1p(loser / 500) + 120 * Math.log1p(Math.max(0, loser - winner) / 750));
}

export function diosTubeTop(fights, now) {
    const {since, until} = diosTubeWindow(now);
    return fights.filter(fight => fight.completedAt >= since && fight.completedAt <= until)
        .map(fight => ({...fight, score: diosTubeScore(fight)}))
        .sort((a, b) => b.score - a.score || b.completedAt - a.completedAt ||
            (a.id < b.id ? -1 : Number(a.id > b.id)))
        .slice(0, 10);
}

export const DIOS_TUBE_SCHEMA = `
CREATE TABLE IF NOT EXISTS dios_tube_replays(
    match_id TEXT PRIMARY KEY REFERENCES ranked_matches(id), score INTEGER NOT NULL CHECK(score>=0)
);
CREATE TABLE IF NOT EXISTS dios_tube_reactions(
    match_id TEXT NOT NULL REFERENCES dios_tube_replays(match_id) ON DELETE CASCADE,
    user_id TEXT NOT NULL REFERENCES online_users(id),
    liked INTEGER NOT NULL DEFAULT 0 CHECK(liked IN (0,1)),
    viewed INTEGER NOT NULL DEFAULT 0 CHECK(viewed IN (0,1)),
    PRIMARY KEY(match_id,user_id)
);
CREATE TABLE IF NOT EXISTS dios_tube_records(
    match_id TEXT NOT NULL REFERENCES ranked_matches(id),
    user_id TEXT NOT NULL REFERENCES online_users(id),
    data TEXT NOT NULL,
    PRIMARY KEY(match_id,user_id)
);
CREATE TABLE IF NOT EXISTS dios_tube_previews(
    match_id TEXT PRIMARY KEY REFERENCES ranked_matches(id), gif TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS dios_tube_preview_parts(
    match_id TEXT NOT NULL REFERENCES dios_tube_previews(match_id) ON DELETE CASCADE,
    part INTEGER NOT NULL, gif TEXT NOT NULL, PRIMARY KEY(match_id,part)
);
`;

export const diosTubeRecordLimit=152000;
export const diosTubePreviewLimit=3400000;
const buildPattern=/^0\.1\.39\.\d+-R\d+(?:\.\d+)?$/;

function randomValid(value) {
    if(typeof value!=='string'||value.length!==3344||!/^[A-Za-z0-9+/]+$/.test(value))return false;
    const state=atob(value);
    const index=state.charCodeAt(2496)+256*state.charCodeAt(2497);
    return index<=625&&state.charCodeAt(2498)===0&&state.charCodeAt(2499)===0;
}

export function pruneDiosTubePreviews(core,now) {
    const {since}=diosTubeWindow(now);
    core.run(`DELETE FROM dios_tube_previews WHERE EXISTS
        (SELECT 1 FROM ranked_matches m WHERE m.id=dios_tube_previews.match_id AND (m.completed<? OR m.void<>0))`,since);
    core.run(`DELETE FROM dios_tube_replays WHERE match_id IN
        (SELECT id FROM ranked_matches WHERE completed<? OR void<>0)`,since);
    core.run(`DELETE FROM dios_tube_records WHERE match_id IN
        (SELECT id FROM ranked_matches WHERE completed<? OR void<>0)`,since);
}

function savePreview(core,user,{id,gif}) {
    if(typeof id!=='string'||id.length>63||typeof gif!=='string'||gif.length<100||gif.length%4||gif.length>diosTubePreviewLimit||
       !/^R0lGODlh[A-Za-z0-9+/]*={0,2}$/.test(gif))throw new Error('BAD_REPLAY_RECORD');
    const header=atob(gif.slice(0,16));
    const w=header.charCodeAt(6),h=header.charCodeAt(8);
    if(header.charCodeAt(7)!==0||header.charCodeAt(9)!==0||!((w===128&&h===72)||(w===160&&h===90)))
        throw new Error('BAD_REPLAY_RECORD');
    const match=core.one(`SELECT m.completed FROM ranked_matches m JOIN dios_tube_records r ON r.match_id=m.id
        WHERE m.id=? AND r.user_id=? AND m.winner IS NOT NULL AND m.void=0`,id,user);
    const {since,until}=diosTubeWindow(core.now());
    if(!match||match.completed<since||match.completed>until)throw new Error('REPLAY_UNAVAILABLE');
    const current=core.one('SELECT gif FROM dios_tube_previews WHERE match_id=?',id);
    if(current&&!current.gif)return {state:'ready'};
    if(current&&w===128)return {state:'ready'};
    core.run(`INSERT INTO dios_tube_previews VALUES(?,'') ON CONFLICT(match_id) DO UPDATE SET gif=''`,id);
    for(let offset=0;offset<gif.length;offset+=500000)
        core.run('INSERT INTO dios_tube_preview_parts VALUES(?,?,?)',id,offset/500000,gif.slice(offset,offset+500000));
    return {state:'ready'};
}

function submitRecord(core,user,args) {
    const {id,build,stage,fighters,frames,inputs,stats,random,time,tracks}=args;
    const bounded=(n,limit)=>Number.isInteger(n)&&n>=0&&n<=limit;
    if(typeof id!=='string'||id.length>63||typeof build!=='string'||!buildPattern.test(build)||
       !bounded(stage,34)||!Array.isArray(fighters)||fighters.length!==2||
       fighters.some(pair=>!Array.isArray(pair)||pair.length!==2||!bounded(pair[0],249)||!bounded(pair[1],31))||
       !bounded(frames,9000)||frames<30||typeof inputs!=='string'||inputs.length!==16*frames||
       !/^[A-Za-z0-9+/]+$/.test(inputs)||!stats||!bounded(stats.hits,1000)||
       !bounded(stats.zCounters,128)||!bounded(stats.longestCombo,100)||
       (tracks!==undefined&&(!Array.isArray(tracks)||tracks.length!==2||tracks.some(n=>!bounded(n,frames)||n<30)||Math.max(...tracks)!==frames))||
       ((random!==undefined||time!==undefined)&&(!randomValid(random)||!bounded(time,7))))throw new Error('BAD_REPLAY_RECORD');
    const match=core.one(`SELECT a,b,winner,completed,void FROM ranked_matches WHERE id=?`,id);
    const {since,until}=diosTubeWindow(core.now());
    if(!match||![match.a,match.b].includes(user)||!match.winner||match.void||
       match.completed<since||match.completed>until||
       core.one('SELECT 1 FROM ranked_abandonments WHERE match_id=? AND confirmed IS NOT NULL',id))
        throw new Error('REPLAY_UNAVAILABLE');
    const canonical=JSON.stringify({build,stage,fighters,frames,inputs,time,random,tracks,
        stats:{hits:stats.hits,zCounters:stats.zCounters,longestCombo:stats.longestCombo}});
    core.run('INSERT OR IGNORE INTO dios_tube_records VALUES(?,?,?)',id,user,canonical);
    const records=core.rows('SELECT data FROM dios_tube_records WHERE match_id=?',id);
    if(records.length<2)return {state:'waiting'};
    const [{stats:firstStats,...first},{stats:secondStats,...second}]=records.map(row=>JSON.parse(row.data));
    if(JSON.stringify(first)!==JSON.stringify(second))return {state:'mismatch'};
    const history=core.rows('SELECT user_id,before_rating FROM ranked_history WHERE match_id=?',id);
    // Rollback can count an event twice in local telemetry. Use the lower observation.
    const score=diosTubeScore({hits:Math.min(firstStats.hits,secondStats.hits),
        zCounters:Math.min(firstStats.zCounters,secondStats.zCounters),longestCombo:Math.min(firstStats.longestCombo,secondStats.longestCombo),
        winnerRating:history.find(row=>row.user_id===match.winner)?.before_rating,
        loserRating:history.find(row=>row.user_id!==match.winner)?.before_rating});
    core.run('INSERT OR IGNORE INTO dios_tube_replays VALUES(?,?)',id,score);
    return {state:'ready'};
}

function active(core,build=null) {
    const {since,until}=diosTubeWindow(core.now());
    return core.rows(`
        SELECT r.match_id AS id, a.name AS player_a, b.name AS player_b,
           EXISTS(SELECT 1 FROM dios_tube_previews p WHERE p.match_id=r.match_id) AS has_preview
        FROM dios_tube_replays r JOIN ranked_matches m ON m.id=r.match_id
        LEFT JOIN dios_tube_records record ON record.match_id=m.id AND record.user_id=m.a
        LEFT JOIN online_names a ON a.user_id=m.a
        LEFT JOIN online_names b ON b.user_id=m.b
        WHERE m.completed BETWEEN ? AND ? AND m.winner IS NOT NULL AND m.void=0
          AND (? IS NULL OR json_extract(record.data,'$.build')=?)
        ORDER BY r.score DESC,m.completed DESC,r.match_id LIMIT 10
    `,since,until,build,build);
}

function publicCounters(core,id,user) {
    return core.one(`
        SELECT COALESCE(SUM(liked),0) AS likes,COALESCE(SUM(viewed),0) AS views,
            COALESCE(MAX(CASE WHEN user_id=? THEN liked END),0) AS liked
        FROM dios_tube_reactions WHERE match_id=?
    `,user,id);
}

export function diosTubeCommand(core,user,op,args) {
    if(op==='dios_tube_submit')return submitRecord(core,user,args);
    if(op==='dios_tube_preview_save')return savePreview(core,user,args);
    const build=args.build??null;
    if(build!==null&&(typeof build!=='string'||!buildPattern.test(build)))throw new Error('BAD_REQUEST');
    const entries=active(core,build);
    if(op==='dios_tube_list'){
        return {replays:entries.map(entry=>({...entry,...publicCounters(core,entry.id,user)}))};
    }
    if(typeof args.id!=='string'||!entries.some(entry=>entry.id===args.id)){
        throw new Error('REPLAY_UNAVAILABLE');
    }
    if(op==='dios_tube_preview_get'){
        const preview=core.one('SELECT gif FROM dios_tube_previews WHERE match_id=?',args.id);
        if(!preview)throw new Error('REPLAY_UNAVAILABLE');
        const gif=preview.gif||core.rows('SELECT gif FROM dios_tube_preview_parts WHERE match_id=? ORDER BY part',args.id).map(row=>row.gif).join('');
        return {id:args.id,gif};
    }
    if(op==='dios_tube_fetch'){
        const row=core.one('SELECT data FROM dios_tube_records WHERE match_id=? LIMIT 1',args.id);
        if(!row)throw new Error('REPLAY_UNAVAILABLE');
        const {stats,...record}=JSON.parse(row.data);
        return {id:args.id,...record};
    }
    if(op==='dios_tube_like'){
        if(typeof args.liked!=='boolean')throw new Error('BAD_REQUEST');
        core.run(`INSERT INTO dios_tube_reactions(match_id,user_id,liked) VALUES(?,?,?)
            ON CONFLICT(match_id,user_id) DO UPDATE SET liked=excluded.liked`,args.id,user,Number(args.liked));
    }else if(op==='dios_tube_view'){
        core.run(`INSERT INTO dios_tube_reactions(match_id,user_id,viewed) VALUES(?,?,1)
            ON CONFLICT(match_id,user_id) DO UPDATE SET viewed=1`,args.id,user);
    }else throw new Error('BAD_REQUEST');
    return publicCounters(core,args.id,user);
}
