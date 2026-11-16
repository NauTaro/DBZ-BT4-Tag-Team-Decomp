-- Additive only: does not reset scores, award balls or enable Ranked.
CREATE TABLE IF NOT EXISTS ranked_resets(id TEXT PRIMARY KEY, applied INTEGER NOT NULL, old_season TEXT, new_season TEXT NOT NULL, reason TEXT NOT NULL, standings TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS ranked_reset_players(reset_id TEXT NOT NULL REFERENCES ranked_resets(id),user_id TEXT NOT NULL,ruleset TEXT NOT NULL,name TEXT NOT NULL,rating INTEGER NOT NULL,wins INTEGER NOT NULL,losses INTEGER NOT NULL,streak_wins INTEGER NOT NULL,streak_points INTEGER NOT NULL,PRIMARY KEY(reset_id,user_id,ruleset));
CREATE TABLE IF NOT EXISTS ranked_rock_rolls(match_id TEXT PRIMARY KEY REFERENCES ranked_matches(id),seed INTEGER NOT NULL CHECK(seed BETWEEN 1 AND 8388608));
CREATE TABLE IF NOT EXISTS ranked_rock_claims(match_id TEXT NOT NULL REFERENCES ranked_rock_rolls(match_id),user_id TEXT NOT NULL,event INTEGER NOT NULL,PRIMARY KEY(match_id,user_id));
CREATE TABLE IF NOT EXISTS ranked_rock_finds(match_id TEXT PRIMARY KEY REFERENCES ranked_rock_rolls(match_id),user_id TEXT NOT NULL,event INTEGER NOT NULL,ball INTEGER NOT NULL CHECK(ball BETWEEN 0 AND 7),found INTEGER NOT NULL);
CREATE UNIQUE INDEX IF NOT EXISTS ranked_rock_collection ON ranked_rock_finds(user_id,ball) WHERE ball>0;
CREATE TABLE IF NOT EXISTS quick_rock_queue(user_id TEXT PRIMARY KEY REFERENCES online_queue(user_id) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS quick_rock_matches(match_id TEXT PRIMARY KEY,a TEXT NOT NULL,b TEXT NOT NULL,seed INTEGER NOT NULL,admitted INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS quick_rock_claims(match_id TEXT NOT NULL REFERENCES quick_rock_matches(match_id),user_id TEXT NOT NULL,event INTEGER NOT NULL,PRIMARY KEY(match_id,user_id));
CREATE TABLE IF NOT EXISTS quick_rock_finds(match_id TEXT PRIMARY KEY REFERENCES quick_rock_matches(match_id),user_id TEXT NOT NULL,event INTEGER NOT NULL,ball INTEGER NOT NULL CHECK(ball BETWEEN 0 AND 7),found INTEGER NOT NULL);
CREATE UNIQUE INDEX IF NOT EXISTS quick_rock_collection ON quick_rock_finds(user_id,ball) WHERE ball>0;
CREATE TABLE IF NOT EXISTS ranked_collections(position INTEGER PRIMARY KEY AUTOINCREMENT,user_id TEXT NOT NULL UNIQUE,completed INTEGER NOT NULL);
