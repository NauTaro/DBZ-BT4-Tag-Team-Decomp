-- Prepared locally. Does not enable streak scoring or reset any existing score.
CREATE TABLE IF NOT EXISTS ranked_streaks(
    user_id TEXT PRIMARY KEY,
    wins INTEGER NOT NULL,
    points INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS ranked_streak_matches(match_id TEXT PRIMARY KEY);
CREATE TABLE IF NOT EXISTS ranked_reviews(
    match_id TEXT PRIMARY KEY,
    created INTEGER NOT NULL,
    reason TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS ranked_matches_pair_completed
    ON ranked_matches(a,b,completed DESC);
