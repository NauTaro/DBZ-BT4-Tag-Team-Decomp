import {rockCount} from './ranked_rocks.mjs';

// Called inside the coordinator's synchronous storage transaction. The old
// scoring mode is unchanged until the explicit launch operation is applied.
const OFFSET=3*3600000;
export function rankedMonthStart(now){
    const local=new Date(now-OFFSET);
    return Date.UTC(local.getUTCFullYear(),local.getUTCMonth(),1)+OFFSET;
}
export function rankedMonthAt(now){return new Date(rankedMonthStart(now)-OFFSET).toISOString().slice(0,10);}
export function rankedMonthEnd(now){
    const local=new Date(now-OFFSET);
    return Date.UTC(local.getUTCFullYear(),local.getUTCMonth()+1,1)+OFFSET;
}
export const monthlyRanked=c=>c.one("SELECT value FROM online_meta WHERE key='ranked_monthly_reset'")?.value==='1';
function meta(c,key,value){c.run('INSERT INTO online_meta VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value',key,String(value));}
export function rankedCollection(c,user){
    const complete=c.one('SELECT completed,position FROM ranked_collections WHERE user_id=?',user);
    return {dragon_balls:rockCount(c,user),
        collection_completed:complete?.completed??null,collection_position:complete?.position??0};
}
function standings(c,season){
    return c.rows(`SELECT p.user_id,p.name,p.rating,w.wins,w.losses FROM ranked_weekly w
        JOIN ranked_players p ON p.user_id=w.user_id AND p.ruleset='solo'
        WHERE w.season=? AND w.wins>0 AND p.rating>0
        ORDER BY p.rating DESC,w.wins DESC,w.losses ASC,p.user_id LIMIT 100`,season);
}
function reset(c,id,season,at,reason,top){
    if(c.one('SELECT id FROM ranked_resets WHERE id=?',id))return false;
    const old=c.one("SELECT value FROM online_meta WHERE key='ranked_season'")?.value??null;
    c.run('INSERT INTO ranked_resets VALUES(?,?,?,?,?,?)',id,at,old,season,reason,JSON.stringify(top));
    c.run(`INSERT INTO ranked_reset_players
        SELECT ?,p.user_id,p.ruleset,p.name,p.rating,p.wins,p.losses,coalesce(s.wins,0),coalesce(s.points,0)
        FROM ranked_players p LEFT JOIN ranked_streaks s ON s.user_id=p.user_id`,id);
    c.run('UPDATE ranked_players SET rating=0');
    c.run('UPDATE ranked_streaks SET wins=0,points=0');
    // Histories, reports, users, names, sessions, rooms and awards are retained.
    c.run('DELETE FROM ranked_top');
    meta(c,'ranked_season',season);meta(c,'ranked_epoch_ms',at);
    return true;
}
export function advanceRankedMonth(c){
    if(!monthlyRanked(c))return false;
    const now=c.now(),season=rankedMonthAt(now);
    const old=c.one("SELECT value FROM online_meta WHERE key='ranked_season'")?.value;
    if(!old)throw new Error('RANKED_SEASON_NOT_INITIALIZED');
    if(old>=season)return false;
    const at=rankedMonthStart(now),top=standings(c,old);
    reset(c,'month:'+old,season,at,'monthly',top);
    return true;
}
export function launchRanked(c,{id,not_before}={}){
    if(typeof id!=='string'||!/^launch:[a-zA-Z0-9._-]{1,80}$/.test(id)||!Number.isSafeInteger(not_before)||not_before<=0)
        throw new Error('BAD_REQUEST');
    const previous=c.one('SELECT * FROM ranked_resets WHERE id=?',id);
    if(previous)return {state:'already_applied',season:previous.new_season,applied_at:previous.applied};
    if(c.now()<not_before)throw new Error('RANKED_LAUNCH_TOO_EARLY');
    if(monthlyRanked(c))throw new Error('RANKED_ALREADY_LAUNCHED');
    const season=rankedMonthAt(c.now());
    reset(c,id,season,c.now(),'launch',[]);
    meta(c,'ranked_scoring','streak');meta(c,'ranked_monthly_reset','1');meta(c,'ranked_weekly_reset','0');meta(c,'ranked_paused','0');
    // Only a pending Ranked search is retired. Existing casual rooms and
    // connections are unrelated to the scoring epoch and remain untouched.
    c.run("DELETE FROM online_queue WHERE wire LIKE 'ranked-v1:%'");
    return {state:'applied',season,applied_at:c.now()};
}

// Explicit operator reset of the current scoring epoch. It does not close
// rooms, revoke sessions or rewrite historical results and collected awards.
export function resetRanked(c,{id,not_before}={}){
    if(typeof id!=='string'||!/^reset:[a-zA-Z0-9._-]{1,80}$/.test(id)||!Number.isSafeInteger(not_before)||not_before<=0)
        throw new Error('BAD_REQUEST');
    const previous=c.one('SELECT * FROM ranked_resets WHERE id=?',id);
    if(previous)return {state:'already_applied',season:previous.new_season,applied_at:previous.applied};
    if(!monthlyRanked(c))throw new Error('RANKED_NOT_LAUNCHED');
    if(c.now()<not_before)throw new Error('RANKED_RESET_TOO_EARLY');
    const season=rankedMonthAt(c.now()),at=c.now();
    reset(c,id,season,at,'manual',standings(c,season));
    return {state:'applied',season,applied_at:at,
        scores:c.one('SELECT count(*) players,min(rating) minimum,max(rating) maximum,sum(rating) total FROM ranked_players'),
        archived:c.one('SELECT count(*) players,sum(rating) total FROM ranked_reset_players WHERE reset_id=?',id)};
}
