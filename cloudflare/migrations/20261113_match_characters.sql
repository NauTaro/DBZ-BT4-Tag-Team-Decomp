-- Existing SQLite storage only. OnlineCore guards this migration with
-- PRAGMA table_info, so normal Worker deployment applies it exactly once.
-- No default/backfill: old matches keep NULL and old result reports remain valid.
ALTER TABLE ranked_matches ADD COLUMN chars TEXT;
