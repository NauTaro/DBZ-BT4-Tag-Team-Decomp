-- Opt-in policy for newly admitted matches. No points, users or existing matches are changed.
CREATE TABLE IF NOT EXISTS ranked_abandonments(match_id TEXT PRIMARY KEY REFERENCES ranked_matches(id),penalty INTEGER NOT NULL CHECK(penalty=100),quitter TEXT,reason TEXT CHECK(reason IN ('local_quit','liveness_timeout')),confirmed INTEGER,penalty_applied INTEGER,reward_applied INTEGER);
CREATE TABLE IF NOT EXISTS ranked_disconnect_claims(match_id TEXT NOT NULL REFERENCES ranked_matches(id),claimant TEXT NOT NULL,began INTEGER NOT NULL,last_seen INTEGER NOT NULL,PRIMARY KEY(match_id,claimant));
CREATE INDEX IF NOT EXISTS ranked_disconnect_claims_seen ON ranked_disconnect_claims(last_seen);
