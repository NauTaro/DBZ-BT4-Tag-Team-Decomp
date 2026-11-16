export const RANKED_ROCK_WIRE = ':rock1';
export const RANKED_ROCK_ODDS = 512;
const MAX_ROCK_EVENT = 256 * 64 * 2;

export function rankedRockSeed(draw) {
    return draw % RANKED_ROCK_ODDS === 0 ? Math.floor(draw / RANKED_ROCK_ODDS) + 1 : 0;
}

export function rollRankedRock(core, match) {
    const draw = crypto.getRandomValues(new Uint32Array(1))[0];
    const seed = rankedRockSeed(draw);
    if (seed) core.run('INSERT INTO ranked_rock_rolls(match_id,seed) VALUES(?,?)', match, seed);
}

export function rollQuickRock(core, match, a, b) {
    const seed = rankedRockSeed(crypto.getRandomValues(new Uint32Array(1))[0]);
    if (seed) core.run('INSERT INTO quick_rock_matches VALUES(?,?,?,?,?)', match, a, b, seed, core.now());
}

export function rockCount(core, user) {
    return core.one(`SELECT
        (SELECT count(*) FROM ranked_rock_finds WHERE user_id=? AND ball>0) +
        (SELECT count(*) FROM quick_rock_finds WHERE user_id=? AND ball>0) AS balls`, user, user).balls;
}

function finishFind(core, table, match, collector, event) {
    const count = rockCount(core, collector);
    const ball = count < 7 ? count + 1 : 0;
    const now = core.now();
    core.run(`INSERT INTO ${table} VALUES(?,?,?,?,?)`, match, collector, event, ball, now);
    if (ball === 7) core.run('INSERT OR IGNORE INTO ranked_collections(user_id,completed) VALUES(?,?)', collector, now);
    return {user_id:collector, ball, event};
}

function findReply(core, found, reporter, event) {
    if (found.event !== event) return {state:'disputed'};
    return {state:'applied',ball:found.user_id === reporter ? found.ball : 0,
        ...(found.ball ? {finder:core.name(found.user_id),stars:found.ball} : {})};
}

export function claimRankedRock(core, match, reporter, event) {
    if (!Number.isInteger(event) || event < 1 || event > MAX_ROCK_EVENT) throw new Error('BAD_ROCK_EVENT');
    if (!core.one('SELECT seed FROM ranked_rock_rolls WHERE match_id=?', match.id)) return {state:'unavailable'};
    if (match.void || match.admitted < core.now() - 5400000) return {state:'expired'};

    const collected = core.one('SELECT user_id,ball,event FROM ranked_rock_finds WHERE match_id=?', match.id);
    if (collected) return findReply(core, collected, reporter, event);
    if (match.started_a == null || match.started_b == null) {
        // A normal result may settle before its independent start messages arrive.
        if (!match.completed || !/^[a-f0-9]{64}$/.test(match.digest ?? '')) return {state:'waiting'};
    }
    const old = core.one('SELECT event FROM ranked_rock_claims WHERE match_id=? AND user_id=?', match.id, reporter);
    if (old && old.event !== event) return {state:'disputed'};
    core.run('INSERT OR IGNORE INTO ranked_rock_claims VALUES(?,?,?)', match.id, reporter, event);
    const opponent = reporter === match.a ? match.b : match.a;
    const witness = core.one('SELECT event FROM ranked_rock_claims WHERE match_id=? AND user_id=?', match.id, opponent);
    if (!witness) return {state:'waiting'};
    if (witness.event !== event) return {state:'disputed'};

    const collector = ((event - 1) & 1) ? match.b : match.a;
    return findReply(core, finishFind(core, 'ranked_rock_finds', match.id, collector, event), reporter, event);
}

export function claimQuickRock(core, reporter, {match, event}) {
    const round = core.one('SELECT * FROM quick_rock_matches WHERE match_id=?', match);
    if (!round) return {state:'unavailable'};
    if (reporter !== round.a && reporter !== round.b) throw new Error('NOT_A_PARTICIPANT');
    if (round.admitted < core.now() - 5400000) return {state:'expired'};
    if (!Number.isInteger(event) || event < 1 || event > MAX_ROCK_EVENT) throw new Error('BAD_ROCK_EVENT');
    const found = core.one('SELECT user_id,ball,event FROM quick_rock_finds WHERE match_id=?', match);
    if (found) return findReply(core, found, reporter, event);
    const old = core.one('SELECT event FROM quick_rock_claims WHERE match_id=? AND user_id=?', match, reporter);
    if (old && old.event !== event) return {state:'disputed'};
    core.run('INSERT OR IGNORE INTO quick_rock_claims VALUES(?,?,?)', match, reporter, event);
    const opponent = reporter === round.a ? round.b : round.a;
    const witness = core.one('SELECT event FROM quick_rock_claims WHERE match_id=? AND user_id=?', match, opponent);
    if (!witness) return {state:'waiting'};
    if (witness.event !== event) return {state:'disputed'};
    const collector = ((event - 1) & 1) ? round.b : round.a;
    return findReply(core, finishFind(core, 'quick_rock_finds', match, collector, event), reporter, event);
}
