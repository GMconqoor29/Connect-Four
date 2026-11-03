-- ============================================================================
-- Connect Four Online - PostgreSQL Database Schema (Group 11)
-- Usage: psql -U <username> -d <dbname> -f database/schema.sql
-- ============================================================================

BEGIN;

-- 1. Users table (Human players + System AI Bot accounts)
CREATE TABLE IF NOT EXISTS users (
    id            SERIAL PRIMARY KEY,
    username      VARCHAR(16) NOT NULL UNIQUE,
    password_hash TEXT,                              -- NULL for system Bot accounts
    elo           INTEGER NOT NULL DEFAULT 1200,
    wins          INTEGER NOT NULL DEFAULT 0,        -- PvP wins
    losses        INTEGER NOT NULL DEFAULT 0,        -- PvP losses
    draws         INTEGER NOT NULL DEFAULT 0,        -- PvP draws
    bot_wins      INTEGER NOT NULL DEFAULT 0,        -- vs Bot wins (displayed in "vs Bot" tab)
    bot_losses    INTEGER NOT NULL DEFAULT 0,        -- vs Bot losses
    bot_draws     INTEGER NOT NULL DEFAULT 0,        -- vs Bot draws
    is_bot        BOOLEAN NOT NULL DEFAULT FALSE,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Seed system AI Bot accounts (fixed ELO ratings, cannot log in)
INSERT INTO users (username, password_hash, elo, is_bot) VALUES
    ('BOT_EASY',   NULL,  800, TRUE),
    ('BOT_MEDIUM', NULL, 1200, TRUE),
    ('BOT_HARD',   NULL, 1600, TRUE)
ON CONFLICT (username) DO NOTHING;

-- 2. Matches table (id is the authoritative game_id in protocol_ndjson.md)
CREATE TABLE IF NOT EXISTS matches (
    id               SERIAL PRIMARY KEY,
    red_id           INTEGER NOT NULL REFERENCES users(id),
    yellow_id        INTEGER NOT NULL REFERENCES users(id),
    turn_time        INTEGER NOT NULL DEFAULT 30,
    is_bot_match     BOOLEAN NOT NULL DEFAULT FALSE,
    winner           VARCHAR(6)  CHECK (winner IN ('RED', 'YELLOW', 'DRAW')),
    reason           VARCHAR(12) CHECK (reason IN ('CONNECT4', 'BOARD_FULL', 'RESIGN',
                                                   'DRAW_AGREED', 'DISCONNECT', 'TIMEOUT')),
    red_elo_delta    INTEGER,
    yellow_elo_delta INTEGER,
    started_at       TIMESTAMPTZ NOT NULL DEFAULT now(),
    ended_at         TIMESTAMPTZ                       -- NULL while match is in progress
);

-- 3. Match moves table (ordered move log for replay / GET_GAME)
CREATE TABLE IF NOT EXISTS match_moves (
    match_id INTEGER  NOT NULL REFERENCES matches(id) ON DELETE CASCADE,
    move_no  SMALLINT NOT NULL CHECK (move_no BETWEEN 1 AND 42),
    col      SMALLINT NOT NULL CHECK (col BETWEEN 0 AND 6),
    PRIMARY KEY (match_id, move_no)
);

-- Indexes for fast HISTORY lookup per user
CREATE INDEX IF NOT EXISTS idx_matches_red_ended    ON matches (red_id, ended_at DESC);
CREATE INDEX IF NOT EXISTS idx_matches_yellow_ended ON matches (yellow_id, ended_at DESC);

COMMIT;

