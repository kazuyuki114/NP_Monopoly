#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <pthread.h>

// Database handle with thread safety
typedef struct {
    sqlite3* db;
    pthread_mutex_t mutex;
} Database;

// User info structure
typedef struct {
    int user_id;
    char username[50];
    int elo_rating;
    int total_matches;
    int wins;
    int losses;
} UserInfo;

// ============ Initialization ============

// Initialize database connection and create tables if needed
// Returns 0 on success, -1 on error
int db_init(Database* db, const char* filename);

// Close database connection
void db_close(Database* db);

// ============ User Operations ============

// Create a new user
// Returns user_id on success, -1 on error (e.g., username exists)
int db_create_user(Database* db, const char* username, const char* password_hash, const char* email);

// Get user by username
// Returns 0 on success, -1 if not found
int db_get_user_by_username(Database* db, const char* username, int* user_id, char* password_hash, int* elo);

// Get user info by user_id
// Returns 0 on success, -1 if not found
int db_get_user_info(Database* db, int user_id, UserInfo* info);

// Update user ELO rating
int db_update_user_elo(Database* db, int user_id, int new_elo);

// Update user stats (wins/losses)
int db_update_user_stats(Database* db, int user_id, int is_win);

// Update last login time
int db_update_last_login(Database* db, int user_id);

// ============ Session Operations ============

// Create a new session
// Returns 0 on success
int db_create_session(Database* db, int user_id, const char* session_id);

// Validate session and get user_id
// Returns 0 on success (valid session), -1 on invalid/expired
int db_validate_session(Database* db, const char* session_id, int* user_id);

// Delete/invalidate session
int db_delete_session(Database* db, const char* session_id);

// Delete all sessions for a user
int db_delete_user_sessions(Database* db, int user_id);

// ============ Online Players ============

// Set player online with status
int db_set_player_online(Database* db, int user_id, const char* status);

// Set player offline
int db_set_player_offline(Database* db, int user_id);

// Update player heartbeat
int db_update_heartbeat(Database* db, int user_id);

// Get online players count
int db_get_online_count(Database* db);

// ============ Match Operations ============

// Create a new match, returns match_id or -1 on error
int db_create_match(Database* db, int player1_id, int player2_id, int p1_elo, int p2_elo);

// Update match result
int db_update_match_result(Database* db, int match_id, int winner_id, int p1_elo_after, int p2_elo_after);

// Log a game move
int db_log_move(Database* db, int match_id, int player_id, int move_num, const char* move_type, const char* move_data);

#endif // DATABASE_H

