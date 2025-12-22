#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// SQL for creating tables (embedded from schema)
static const char* CREATE_TABLES_SQL = 
    "CREATE TABLE IF NOT EXISTS users ("
    "    user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    username VARCHAR(50) UNIQUE NOT NULL,"
    "    password_hash VARCHAR(64) NOT NULL,"
    "    email VARCHAR(100) UNIQUE,"
    "    elo_rating INTEGER DEFAULT 1200,"
    "    total_matches INTEGER DEFAULT 0,"
    "    wins INTEGER DEFAULT 0,"
    "    losses INTEGER DEFAULT 0,"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    last_login TIMESTAMP"
    ");"
    "CREATE TABLE IF NOT EXISTS sessions ("
    "    session_id VARCHAR(64) PRIMARY KEY,"
    "    user_id INTEGER NOT NULL,"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    expires_at TIMESTAMP,"
    "    is_active BOOLEAN DEFAULT 1,"
    "    FOREIGN KEY (user_id) REFERENCES users(user_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS online_players ("
    "    user_id INTEGER PRIMARY KEY,"
    "    status VARCHAR(20),"
    "    current_game_id INTEGER,"
    "    last_heartbeat TIMESTAMP,"
    "    FOREIGN KEY (user_id) REFERENCES users(user_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS matches ("
    "    match_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    player1_id INTEGER NOT NULL,"
    "    player2_id INTEGER NOT NULL,"
    "    winner_id INTEGER,"
    "    player1_elo_before INTEGER,"
    "    player2_elo_before INTEGER,"
    "    player1_elo_after INTEGER,"
    "    player2_elo_after INTEGER,"
    "    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    end_time TIMESTAMP,"
    "    status VARCHAR(20),"
    "    FOREIGN KEY (player1_id) REFERENCES users(user_id),"
    "    FOREIGN KEY (player2_id) REFERENCES users(user_id),"
    "    FOREIGN KEY (winner_id) REFERENCES users(user_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS game_moves ("
    "    move_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    match_id INTEGER NOT NULL,"
    "    player_id INTEGER NOT NULL,"
    "    move_number INTEGER,"
    "    move_type VARCHAR(50),"
    "    move_data TEXT,"
    "    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    FOREIGN KEY (match_id) REFERENCES matches(match_id),"
    "    FOREIGN KEY (player_id) REFERENCES users(user_id)"
    ");"
    "CREATE TABLE IF NOT EXISTS challenge_requests ("
    "    challenge_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "    challenger_id INTEGER NOT NULL,"
    "    challenged_id INTEGER NOT NULL,"
    "    status VARCHAR(20) DEFAULT 'pending',"
    "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "    responded_at TIMESTAMP,"
    "    FOREIGN KEY (challenger_id) REFERENCES users(user_id),"
    "    FOREIGN KEY (challenged_id) REFERENCES users(user_id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);"
    "CREATE INDEX IF NOT EXISTS idx_challenges_challenged ON challenge_requests(challenged_id);";

int db_init(Database* db, const char* filename) {
    if (!db || !filename) return -1;
    
    pthread_mutex_init(&db->mutex, NULL);
    
    int rc = sqlite3_open(filename, &db->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db->db));
        sqlite3_close(db->db);
        return -1;
    }
    
    // Create tables
    char* err_msg = NULL;
    rc = sqlite3_exec(db->db, CREATE_TABLES_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db->db);
        return -1;
    }
    
    printf("Database initialized successfully: %s\n", filename);
    return 0;
}

void db_close(Database* db) {
    if (db && db->db) {
        sqlite3_close(db->db);
        pthread_mutex_destroy(&db->mutex);
        db->db = NULL;
    }
}

// ============ User Operations ============ 

int db_create_user(Database* db, const char* username, const char* password_hash, const char* email) {
    if (!db || !username || !password_hash) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO users (username, password_hash, email) VALUES (?, ?, ?)";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_STATIC);
    if (email) {
        sqlite3_bind_text(stmt, 3, email, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        pthread_mutex_unlock(&db->mutex);
        return -1;  // Probably duplicate username
    }
    
    int user_id = (int)sqlite3_last_insert_rowid(db->db);
    pthread_mutex_unlock(&db->mutex);
    
    printf("Created user: %s (id=%d)\n", username, user_id);
    return user_id;
}

int db_get_user_by_username(Database* db, const char* username, int* user_id, char* password_hash, int* elo) {
    if (!db || !username) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT user_id, password_hash, elo_rating FROM users WHERE username = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (user_id) *user_id = sqlite3_column_int(stmt, 0);
        if (password_hash) {
            const char* hash = (const char*)sqlite3_column_text(stmt, 1);
            strcpy(password_hash, hash ? hash : "");
        }
        if (elo) *elo = sqlite3_column_int(stmt, 2);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    return -1;  // Not found
}

int db_get_user_info(Database* db, int user_id, UserInfo* info) {
    if (!db || !info) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT user_id, username, elo_rating, total_matches, wins, losses "
                      "FROM users WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        info->user_id = sqlite3_column_int(stmt, 0);
        const char* name = (const char*)sqlite3_column_text(stmt, 1);
        strncpy(info->username, name ? name : "", sizeof(info->username) - 1);
        info->elo_rating = sqlite3_column_int(stmt, 2);
        info->total_matches = sqlite3_column_int(stmt, 3);
        info->wins = sqlite3_column_int(stmt, 4);
        info->losses = sqlite3_column_int(stmt, 5);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    return -1;
}

int db_update_user_elo(Database* db, int user_id, int new_elo) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET elo_rating = ? WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, new_elo);
    sqlite3_bind_int(stmt, 2, user_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_user_stats(Database* db, int user_id, int is_win) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    const char* sql = is_win 
        ? "UPDATE users SET total_matches = total_matches + 1, wins = wins + 1 WHERE user_id = ?"
        : "UPDATE users SET total_matches = total_matches + 1, losses = losses + 1 WHERE user_id = ?";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_last_login(Database* db, int user_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// ============ Session Operations ============ 

int db_create_session(Database* db, int user_id, const char* session_id) {
    if (!db || !session_id) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO sessions (session_id, user_id, expires_at) "
                      "VALUES (?, ?, datetime('now', '+24 hours'))";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, user_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_validate_session(Database* db, const char* session_id, int* user_id) {
    if (!db || !session_id) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT user_id FROM sessions "
                      "WHERE session_id = ? AND is_active = 1 "
                      "AND (expires_at IS NULL OR expires_at > datetime('now'))";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (user_id) *user_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    return -1;
}

int db_delete_session(Database* db, const char* session_id) {
    if (!db || !session_id) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE sessions SET is_active = 0 WHERE session_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_user_sessions(Database* db, int user_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE sessions SET is_active = 0 WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// ============ Online Players ============ 

int db_set_player_online(Database* db, int user_id, const char* status) {
    if (!db || !status) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO online_players (user_id, status, last_heartbeat) "
                      "VALUES (?, ?, datetime('now'))";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, status, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_player_offline(Database* db, int user_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM online_players WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_heartbeat(Database* db, int user_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE online_players SET last_heartbeat = datetime('now') WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_online_count(Database* db) {
    if (!db) return 0;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT COUNT(*) FROM online_players";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return count;
}

// ============ Match Operations ============ 

int db_create_match(Database* db, int player1_id, int player2_id, int p1_elo, int p2_elo) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO matches (player1_id, player2_id, player1_elo_before, player2_elo_before, status) "
                      "VALUES (?, ?, ?, ?, 'ongoing')";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, player1_id);
    sqlite3_bind_int(stmt, 2, player2_id);
    sqlite3_bind_int(stmt, 3, p1_elo);
    sqlite3_bind_int(stmt, 4, p2_elo);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    int match_id = (int)sqlite3_last_insert_rowid(db->db);
    pthread_mutex_unlock(&db->mutex);
    
    return match_id;
}

int db_update_match_result(Database* db, int match_id, int winner_id, int p1_elo_after, int p2_elo_after) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE matches SET winner_id = ?, player1_elo_after = ?, player2_elo_after = ?, "
                      "status = 'completed', end_time = datetime('now') WHERE match_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, winner_id);
    sqlite3_bind_int(stmt, 2, p1_elo_after);
    sqlite3_bind_int(stmt, 3, p2_elo_after);
    sqlite3_bind_int(stmt, 4, match_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_log_move(Database* db, int match_id, int player_id, int move_num, const char* move_type, const char* move_data) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO game_moves (match_id, player_id, move_number, move_type, move_data) "
                      "VALUES (?, ?, ?, ?, ?)";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, match_id);
    sqlite3_bind_int(stmt, 2, player_id);
    sqlite3_bind_int(stmt, 3, move_num);
    sqlite3_bind_text(stmt, 4, move_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, move_data, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// ============ Challenge Operations ============ 

int db_create_challenge(Database* db, int challenger_id, int challenged_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "INSERT INTO challenge_requests (challenger_id, challenged_id, status) "
                      "VALUES (?, ?, 'pending')";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, challenger_id);
    sqlite3_bind_int(stmt, 2, challenged_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    int challenge_id = (int)sqlite3_last_insert_rowid(db->db);
    pthread_mutex_unlock(&db->mutex);
    
    printf("[DB] Challenge created: %d -> %d (id=%d)\n", challenger_id, challenged_id, challenge_id);
    return challenge_id;
}

int db_respond_challenge(Database* db, int challenge_id, const char* status) {
    if (!db || !status) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE challenge_requests SET status = ?, responded_at = datetime('now') "
                      "WHERE challenge_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, challenge_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_challenge(Database* db, int challenge_id, int* challenger_id, int* challenged_id, char* status) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT challenger_id, challenged_id, status FROM challenge_requests WHERE challenge_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, challenge_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (challenger_id) *challenger_id = sqlite3_column_int(stmt, 0);
        if (challenged_id) *challenged_id = sqlite3_column_int(stmt, 1);
        if (status) {
            const char* s = (const char*)sqlite3_column_text(stmt, 2);
            strcpy(status, s ? s : "");
        }
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    return -1;
}

int db_get_pending_challenges(Database* db, int user_id, int** challenge_ids, int* count) {
    if (!db || !challenge_ids || !count) return -1;
    
    *challenge_ids = NULL;
    *count = 0;
    
    pthread_mutex_lock(&db->mutex);
    
    // First count challenges
    sqlite3_stmt* stmt;
    const char* sql_count = "SELECT COUNT(*) FROM challenge_requests "
                            "WHERE challenged_id = ? AND status = 'pending'";
    
    int rc = sqlite3_prepare_v2(db->db, sql_count, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (*count == 0) {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    // Allocate array
    *challenge_ids = malloc(sizeof(int) * (*count));
    if (!*challenge_ids) {
        pthread_mutex_unlock(&db->mutex);
        *count = 0;
        return -1;
    }
    
    // Get challenge IDs
    const char* sql_get = "SELECT challenge_id FROM challenge_requests "
                          "WHERE challenged_id = ? AND status = 'pending' "
                          "ORDER BY created_at DESC";
    
    rc = sqlite3_prepare_v2(db->db, sql_get, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(*challenge_ids);
        *challenge_ids = NULL;
        *count = 0;
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < *count) {
        (*challenge_ids)[i++] = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return 0;
}

int db_expire_old_challenges(Database* db, int timeout_seconds) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), 
             "UPDATE challenge_requests SET status = 'expired' "
             "WHERE status = 'pending' AND created_at < datetime('now', '-%d seconds')",
             timeout_seconds);
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// ============ Online Players (Extended) ============ 

int db_get_online_players(Database* db, OnlinePlayerInfo** players, int* count) {
    if (!db || !players || !count) return -1;
    
    *players = NULL;
    *count = 0;
    
    pthread_mutex_lock(&db->mutex);
    
    // Count first
    sqlite3_stmt* stmt;
    const char* sql_count = "SELECT COUNT(*) FROM online_players op "
                            "JOIN users u ON op.user_id = u.user_id";
    
    int rc = sqlite3_prepare_v2(db->db, sql_count, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (*count == 0) {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    // Allocate
    *players = malloc(sizeof(OnlinePlayerInfo) * (*count));
    if (!*players) {
        pthread_mutex_unlock(&db->mutex);
        *count = 0;
        return -1;
    }
    
    // Get players
    const char* sql_get = "SELECT op.user_id, u.username, u.elo_rating, op.status "
                          "FROM online_players op "
                          "JOIN users u ON op.user_id = u.user_id "
                          "ORDER BY u.elo_rating DESC";
    
    rc = sqlite3_prepare_v2(db->db, sql_get, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(*players);
        *players = NULL;
        *count = 0;
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < *count) {
        (*players)[i].user_id = sqlite3_column_int(stmt, 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt, 1);
        strncpy((*players)[i].username, name ? name : "", sizeof((*players)[i].username) - 1);
        
        (*players)[i].elo_rating = sqlite3_column_int(stmt, 2);
        
        const char* status = (const char*)sqlite3_column_text(stmt, 3);
        strncpy((*players)[i].status, status ? status : "", sizeof((*players)[i].status) - 1);
        
        i++;
    }
    
    *count = i;  // Update to actual count
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return 0;
}

int db_get_searching_players(Database* db, OnlinePlayerInfo** players, int* count) {
    if (!db || !players || !count) return -1;
    
    *players = NULL;
    *count = 0;
    
    pthread_mutex_lock(&db->mutex);
    
    // Count searching players
    sqlite3_stmt* stmt;
    const char* sql_count = "SELECT COUNT(*) FROM online_players op "
                            "JOIN users u ON op.user_id = u.user_id "
                            "WHERE op.status = 'searching'";
    
    int rc = sqlite3_prepare_v2(db->db, sql_count, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (*count == 0) {
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    // Allocate
    *players = malloc(sizeof(OnlinePlayerInfo) * (*count));
    if (!*players) {
        pthread_mutex_unlock(&db->mutex);
        *count = 0;
        return -1;
    }
    
    // Get searching players
    const char* sql_get = "SELECT op.user_id, u.username, u.elo_rating, op.status "
                          "FROM online_players op "
                          "JOIN users u ON op.user_id = u.user_id "
                          "WHERE op.status = 'searching' "
                          "ORDER BY u.elo_rating";
    
    rc = sqlite3_prepare_v2(db->db, sql_get, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        free(*players);
        *players = NULL;
        *count = 0;
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < *count) {
        (*players)[i].user_id = sqlite3_column_int(stmt, 0);
        
        const char* name = (const char*)sqlite3_column_text(stmt, 1);
        strncpy((*players)[i].username, name ? name : "", sizeof((*players)[i].username) - 1);
        
        (*players)[i].elo_rating = sqlite3_column_int(stmt, 2);
        
        const char* status = (const char*)sqlite3_column_text(stmt, 3);
        strncpy((*players)[i].status, status ? status : "", sizeof((*players)[i].status) - 1);
        
        i++;
    }
    
    *count = i;
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return 0;
}

int db_set_player_game(Database* db, int user_id, int game_id) {
    if (!db) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE online_players SET current_game_id = ?, status = 'in_game' WHERE user_id = ?";
    
    int rc = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, game_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// ============ Matchmaking Queue ============ 

int db_join_matchmaking(Database* db, int user_id) {
    if (!db) return -1;
    
    // Set player status to searching
    return db_set_player_online(db, user_id, "searching");
}

int db_leave_matchmaking(Database* db, int user_id) {
    if (!db) return -1;
    
    // Set player status to idle
    return db_set_player_online(db, user_id, "idle");
}

int db_find_match(Database* db, int user_id, int elo, int search_time_seconds) {
    if (!db) return -1;
    
    // Get list of all searching players
    OnlinePlayerInfo* players = NULL;
    int count = 0;
    
    if (db_get_searching_players(db, &players, &count) != 0) {
        return -1;
    }
    
    if (count == 0) {
        return -1;
    }
    
    // Calculate allowed ELO range based on search time
    // Range expands over time
    int base_range = 100;
    int extra_range = (search_time_seconds / 10) * 25;  // +25 ELO every 10 seconds
    int max_range = base_range + extra_range;
    if (max_range > 500) max_range = 500;  // Cap at 500
    
    int best_match = -1;
    int best_elo_diff = 99999;
    
    for (int i = 0; i < count; i++) {
        // Skip self
        if (players[i].user_id == user_id) continue;
        
        int elo_diff = abs(players[i].elo_rating - elo);
        
        // Check if within allowed range
        if (elo_diff <= max_range && elo_diff < best_elo_diff) {
            best_match = players[i].user_id;
            best_elo_diff = elo_diff;
        }
    }
    
    free(players);
    
    return best_match;
}


int db_get_user_match_history(Database* db, int user_id, MatchHistoryEntry** history, int* count) {
    if (!db || !history || !count) return -1;
    
    pthread_mutex_lock(&db->mutex);
    
    // First count the matches
    const char* count_sql = "SELECT COUNT(*) FROM matches WHERE (player1_id = ? OR player2_id = ?) AND status = 'completed'";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db->db, count_sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *count = sqlite3_column_int(stmt, 0);
    } else {
        *count = 0;
    }
    sqlite3_finalize(stmt);
    
    if (*count == 0) {
        *history = NULL;
        pthread_mutex_unlock(&db->mutex);
        return 0;
    }
    
    // Limit to 20 most recent matches
    if (*count > 20) *count = 20;
    
    *history = malloc(sizeof(MatchHistoryEntry) * (*count));
    if (!*history) {
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    const char* sql = 
        "SELECT "
        "    m.match_id, "
        "    CASE WHEN m.player1_id = ?1 THEN m.player2_id ELSE m.player1_id END as opponent_id, "
        "    u.username, "
        "    CASE WHEN m.winner_id = ?1 THEN 1 WHEN m.winner_id IS NULL THEN -1 ELSE 0 END as is_win, "
        "    CASE WHEN m.player1_id = ?1 THEN m.player1_elo_after - m.player1_elo_before ELSE m.player2_elo_after - m.player2_elo_before END as elo_change, "
        "    m.start_time "
        "FROM matches m "
        "JOIN users u ON u.user_id = (CASE WHEN m.player1_id = ?1 THEN m.player2_id ELSE m.player1_id END) "
        "WHERE (m.player1_id = ?1 OR m.player2_id = ?1) AND m.status = 'completed' "
        "ORDER BY m.start_time DESC LIMIT 20";
        
    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(*history);
        *history = NULL;
        pthread_mutex_unlock(&db->mutex);
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < *count) {
        MatchHistoryEntry* entry = &(*history)[i];
        
        entry->match_id = sqlite3_column_int(stmt, 0);
        entry->opponent_id = sqlite3_column_int(stmt, 1);
        
        const char* username = (const char*)sqlite3_column_text(stmt, 2);
        strncpy(entry->opponent_name, username ? username : "Unknown", sizeof(entry->opponent_name) - 1);
        
        entry->is_win = sqlite3_column_int(stmt, 3);
        entry->elo_change = sqlite3_column_int(stmt, 4);
        
        const char* time_str = (const char*)sqlite3_column_text(stmt, 5);
        strncpy(entry->timestamp, time_str ? time_str : "", sizeof(entry->timestamp) - 1);
        
        i++;
    }
    
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db->mutex);
    return 0;
}