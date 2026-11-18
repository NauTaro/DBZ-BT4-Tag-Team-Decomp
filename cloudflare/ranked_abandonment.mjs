// Authenticated match liveness can prove absence, not why someone's connection stopped.
export const ABANDONMENT_PENALTY=100;
export const ABANDONMENT_GRACE_MS=45000;
export const WITNESS_MAX_GAP_MS=20000;
export const abandonmentEnabled=c=>c.one("SELECT value FROM online_meta WHERE key='ranked_abandonment_v2'")?.value==='1';
export function admitAbandonment(c,match){
    if(abandonmentEnabled(c))c.run('INSERT INTO ranked_abandonments(match_id,penalty) VALUES(?,?)',match,ABANDONMENT_PENALTY);
}
export function abandonmentProof(c,m,user,op,args,previousSeen,now){
    if(!c.one('SELECT 1 FROM ranked_abandonments WHERE match_id=?',m.id))return null;
    if(op==='forfeit'&&args.reason==='local_quit')return {quitter:user,reason:'local_quit'};
    if(op!=='heartbeat')return null;
    // Any fresh heartbeat from an accused player cancels the other player's witness chain.
    c.run('DELETE FROM ranked_disconnect_claims WHERE match_id=? AND claimant<>?',m.id,user);
    if(args.peer_gone!==true){
        c.run('DELETE FROM ranked_disconnect_claims WHERE match_id=? AND claimant=?',m.id,user);
        return null;
    }
    let claim=c.one('SELECT * FROM ranked_disconnect_claims WHERE match_id=? AND claimant=?',m.id,user);
    if(!claim||now-previousSeen>WITNESS_MAX_GAP_MS||now-claim.last_seen>WITNESS_MAX_GAP_MS){
        c.run('INSERT INTO ranked_disconnect_claims VALUES(?,?,?,?) ON CONFLICT(match_id,claimant) DO UPDATE SET began=excluded.began,last_seen=excluded.last_seen',m.id,user,now,now);
        return null;
    }
    c.run('UPDATE ranked_disconnect_claims SET last_seen=? WHERE match_id=? AND claimant=?',now,m.id,user);
    const other=user===m.a?'b':'a';
    if(now-claim.began<ABANDONMENT_GRACE_MS||now-m['seen_'+other]<ABANDONMENT_GRACE_MS)return null;
    return {quitter:other==='a'?m.a:m.b,reason:'liveness_timeout'};
}
export function confirmAbandonment(c,m,proof,now){
    c.run('UPDATE ranked_abandonments SET quitter=?,reason=?,confirmed=? WHERE match_id=? AND confirmed IS NULL',proof.quitter,proof.reason,now,m.id);
}
export function confirmedAbandonment(c,m,winner,digest){
    const r=c.one('SELECT * FROM ranked_abandonments WHERE match_id=? AND confirmed IS NOT NULL',m.id);
    return r&&r.quitter!==winner&&[m.a,m.b].includes(r.quitter)&&[m.a,m.b].includes(winner)&&digest==='forfeit:'+r.quitter?r:null;
}
export function eloForfeitReward(winnerRating,loserRating){
    // The reward isn't funded by the quitter's remaining balance.
    return Math.min(100000-winnerRating,Math.max(1,Math.round(32*(1-1/(1+10**((loserRating-winnerRating)/400))))));
}
