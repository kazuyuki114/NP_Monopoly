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

// ============ Challenge Operations ============

// Create a new challenge request, returns challenge_id or -1 on error
int db_create_challenge(Database* db, int challenger_id, int challenged_id);

// Respond to a challenge (status: "accepted", "declined", "expired")
int db_respond_challenge(Database* db, int challenge_id, const char* status);

// Get challenge info by ID
// Returns 0 on success, -1 if not found
int db_get_challenge(Database* db, int challenge_id, int* challenger_id, int* challenged_id, char* status);

// Get pending challenges for a user (where they are the challenged)
// Caller must free the returned array
int db_get_pending_challenges(Database* db, int user_id, int** challenge_ids, int* count);

// Cancel expired challenges (older than timeout_seconds)
int db_expire_old_challenges(Database* db, int timeout_seconds);

// ============ Online Players (Extended) ============

// Online player info structure
typedef struct {
    int user_id;
    char username[50];
    int elo_rating;
    char status[20];
} OnlinePlayerInfo;

// Get list of online players with their info
// Caller must free the returned array
int db_get_online_players(Database* db, OnlinePlayerInfo** players, int* count);

// Get online players who are searching for a match
int db_get_searching_players(Database* db, OnlinePlayerInfo** players, int* count);

// Update player's current game ID
int db_set_player_game(Database* db, int user_id, int game_id);

// ============ Matchmaking Queue ============

// Player in matchmaking queue
typedef struct {
    int user_id;
    char username[50];
    int elo_rating;
    int search_start_time;  // Unix timestamp when search started
} MatchmakingPlayer;

// Add player to matchmaking queue (status becomes 'searching')
int db_join_matchmaking(Database* db, int user_id);

// Remove player from matchmaking queue (status becomes 'idle')
int db_leave_matchmaking(Database* db, int user_id);

// Find best match for a player in the queue
// Returns user_id of best opponent, or -1 if no suitable match
int db_find_match(Database* db, int user_id, int elo, int search_time_seconds);

#endif // DATABASE_H

