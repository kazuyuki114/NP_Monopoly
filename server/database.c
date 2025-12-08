#include "database.h"
#include <stdio.h>
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
    "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);"
    "CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);";

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

