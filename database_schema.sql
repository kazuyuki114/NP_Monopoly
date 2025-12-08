-- Monopoly Network Game Database Schema
-- SQLite3

-- Users table
CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(64) NOT NULL,
    email VARCHAR(100) UNIQUE,
    elo_rating INTEGER DEFAULT 1200,
    total_matches INTEGER DEFAULT 0,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- Sessions table (for login management)
CREATE TABLE IF NOT EXISTS sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id INTEGER NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    is_active BOOLEAN DEFAULT 1,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Online players table (for matchmaking)
CREATE TABLE IF NOT EXISTS online_players (
    user_id INTEGER PRIMARY KEY,
    status VARCHAR(20), -- 'idle', 'searching', 'in_game'
    current_game_id INTEGER,
    last_heartbeat TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Matches table
CREATE TABLE IF NOT EXISTS matches (
    match_id INTEGER PRIMARY KEY AUTOINCREMENT,
    player1_id INTEGER NOT NULL,
    player2_id INTEGER NOT NULL,
    winner_id INTEGER,
    player1_elo_before INTEGER,
    player2_elo_before INTEGER,
    player1_elo_after INTEGER,
    player2_elo_after INTEGER,
    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time TIMESTAMP,
    status VARCHAR(20), -- 'ongoing', 'completed', 'abandoned'
    FOREIGN KEY (player1_id) REFERENCES users(user_id),
    FOREIGN KEY (player2_id) REFERENCES users(user_id),
    FOREIGN KEY (winner_id) REFERENCES users(user_id)
);

-- Game moves log (for replay and validation)
CREATE TABLE IF NOT EXISTS game_moves (
    move_id INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id INTEGER NOT NULL,
    player_id INTEGER NOT NULL,
    move_number INTEGER,
    move_type VARCHAR(50), -- 'roll', 'buy', 'upgrade', 'mortgage', etc.
    move_data TEXT, -- JSON data of the move
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (match_id) REFERENCES matches(match_id),
    FOREIGN KEY (player_id) REFERENCES users(user_id)
);

-- Challenge requests table
CREATE TABLE IF NOT EXISTS challenge_requests (
    challenge_id INTEGER PRIMARY KEY AUTOINCREMENT,
    challenger_id INTEGER NOT NULL,
    challenged_id INTEGER NOT NULL,
    status VARCHAR(20), -- 'pending', 'accepted', 'declined', 'expired'
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP,
    FOREIGN KEY (challenger_id) REFERENCES users(user_id),
    FOREIGN KEY (challenged_id) REFERENCES users(user_id)
);

-- Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_active ON sessions(is_active);
CREATE INDEX IF NOT EXISTS idx_matches_players ON matches(player1_id, player2_id);
CREATE INDEX IF NOT EXISTS idx_game_moves_match ON game_moves(match_id);
CREATE INDEX IF NOT EXISTS idx_online_status ON online_players(status);

