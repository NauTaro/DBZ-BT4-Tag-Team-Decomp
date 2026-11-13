// Persistent data lives in SQLite, not the JavaScript heap. No fight frames are stored here.
export const ONLINE_SCHEMA = `
CREATE TABLE IF NOT EXISTS online_users(id TEXT PRIMARY KEY, created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS online_aliases(source TEXT NOT NULL, subject TEXT NOT NULL, user_id TEXT NOT NULL REFERENCES online_users(id), PRIMARY KEY(source,subject));
CREATE TABLE IF NOT EXISTS online_sessions(hash TEXT PRIMARY KEY, user_id TEXT NOT NULL REFERENCES online_users(id), expires INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS online_sessions_expiry ON online_sessions(expires);
CREATE TABLE IF NOT EXISTS online_names(user_id TEXT PRIMARY KEY REFERENCES online_users(id), name TEXT NOT NULL, normalized TEXT NOT NULL UNIQUE);
CREATE TABLE IF NOT EXISTS online_rooms(id TEXT PRIMARY KEY, code TEXT NOT NULL UNIQUE, generation TEXT NOT NULL UNIQUE, owner TEXT NOT NULL, mode TEXT NOT NULL, wire TEXT NOT NULL, content TEXT NOT NULL, rules TEXT NOT NULL, capacity INTEGER NOT NULL, listed INTEGER NOT NULL, joinable INTEGER NOT NULL, expires INTEGER NOT NULL, next_peer INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS online_rooms_expiry ON online_rooms(expires);
CREATE TABLE IF NOT EXISTS online_members(user_id TEXT PRIMARY KEY, room TEXT NOT NULL REFERENCES online_rooms(id) ON DELETE CASCADE, peer INTEGER NOT NULL, name TEXT NOT NULL, role TEXT NOT NULL, udp TEXT, UNIQUE(room,peer));
CREATE TABLE IF NOT EXISTS online_queue(user_id TEXT PRIMARY KEY, mode TEXT NOT NULL, wire TEXT NOT NULL, content TEXT NOT NULL, rules TEXT NOT NULL, name TEXT NOT NULL, expires INTEGER NOT NULL, made INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS online_queue_matching ON online_queue(mode,wire,content,rules,made);
CREATE TABLE IF NOT EXISTS online_signals(id INTEGER PRIMARY KEY AUTOINCREMENT, room TEXT NOT NULL, generation TEXT NOT NULL, recipient TEXT NOT NULL, body TEXT NOT NULL, expires INTEGER NOT NULL);
CREATE INDEX IF NOT EXISTS online_signals_recipient ON online_signals(recipient,id);
CREATE TABLE IF NOT EXISTS online_presence(user_id TEXT PRIMARY KEY, expires INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS ranked_players(user_id TEXT NOT NULL, ruleset TEXT NOT NULL, name TEXT NOT NULL, rating INTEGER NOT NULL DEFAULT 1000 CHECK(rating BETWEEN 0 AND 100000), wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(user_id,ruleset));
CREATE TABLE IF NOT EXISTS ranked_matches(id TEXT PRIMARY KEY, season TEXT NOT NULL, a TEXT NOT NULL, b TEXT NOT NULL, admitted INTEGER NOT NULL, started_a INTEGER, started_b INTEGER, seen_a INTEGER NOT NULL, seen_b INTEGER NOT NULL, completed INTEGER, winner TEXT, digest TEXT, void INTEGER NOT NULL DEFAULT 0, chars TEXT);
CREATE TABLE IF NOT EXISTS ranked_reports(match_id TEXT NOT NULL, user_id TEXT NOT NULL, outcome INTEGER NOT NULL, digest TEXT NOT NULL, PRIMARY KEY(match_id,user_id));
CREATE TABLE IF NOT EXISTS ranked_history(match_id TEXT NOT NULL, user_id TEXT NOT NULL, before_rating INTEGER NOT NULL, after_rating INTEGER NOT NULL, delta INTEGER NOT NULL, PRIMARY KEY(match_id,user_id));
CREATE TABLE IF NOT EXISTS ranked_weekly(season TEXT NOT NULL, user_id TEXT NOT NULL, wins INTEGER NOT NULL DEFAULT 0, losses INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(season,user_id));
CREATE TABLE IF NOT EXISTS ranked_top(season TEXT PRIMARY KEY, updated INTEGER NOT NULL, entries TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS online_meta(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS online_import_batches(id TEXT PRIMARY KEY, source TEXT NOT NULL, digest TEXT NOT NULL, imported INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS online_legacy(source TEXT NOT NULL, table_name TEXT NOT NULL, row_key TEXT NOT NULL, body TEXT NOT NULL, PRIMARY KEY(source,table_name,row_key));
CREATE INDEX IF NOT EXISTS online_legacy_tablename ON online_legacy(table_name);
CREATE TABLE IF NOT EXISTS online_budgets(subject TEXT NOT NULL, operation TEXT NOT NULL, minute INTEGER NOT NULL, n INTEGER NOT NULL, PRIMARY KEY(subject,operation,minute));
CREATE TABLE IF NOT EXISTS online_turn(user_id TEXT NOT NULL, generation TEXT NOT NULL, body TEXT NOT NULL, expires INTEGER NOT NULL, PRIMARY KEY(user_id,generation));
`;
