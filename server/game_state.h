/*
 * Game State Manager
 * 
 * Manages active games on the server, including:
 * - Game state for each active match
 * - Turn management
 * - Move validation
 * - State synchronization between players
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "server.h"
#include <pthread.h>

// Constants
#define MAX_ACTIVE_GAMES 25
#define TOTAL_PROPERTIES 40
#define STARTING_MONEY 1500
#define GO_BONUS 200
#define JAIL_POSITION 10
#define JAIL_FINE 50
#define MAX_JAIL_TURNS 3

// Property types
typedef enum {
    PROP_GO,
    PROP_STREET,
    PROP_COMMUNITY_CHEST,
    PROP_TAX,
    PROP_RAILROAD,
    PROP_CHANCE,
    PROP_JAIL,
    PROP_UTILITY,
    PROP_FREE_PARKING,
    PROP_GOTO_JAIL
} PropertyType;

// Property state
typedef struct {
    int owner;        // -1 = unowned, 0 = player1, 1 = player2
    int upgrades;     // 0-5 (5 = hotel)
    int mortgaged;    // 0 or 1
} PropertyState;

// Player state in a game
typedef struct {
    int user_id;
    char username[50];
    int money;
    int position;
    int jailed;
    int turns_in_jail;
    int consecutive_doubles;
} GamePlayerState;

// Game state
typedef enum {
    GSTATE_WAITING_ROLL = 0,
    GSTATE_WAITING_BUY = 1,
    GSTATE_WAITING_DEBT = 2,
    GSTATE_PAUSED = 3,
    GSTATE_ENDED = 4
} GameStateType;

// Active game structure
typedef struct {
    int match_id;
    int active;
    
    GamePlayerState players[2];
    PropertyState properties[TOTAL_PROPERTIES];
    
    int current_player;    // 0 or 1
    GameStateType state;
    GameStateType state_before_pause;  // State before pausing
    int paused;
    int paused_by;         // Which player paused (0 or 1)
    int last_roll[2];
    int just_left_jail;
    int move_count;
    
    char message[128];
    char message2[128];
    
    pthread_mutex_t mutex;
} ActiveGame;

// Global games array
extern ActiveGame active_games[MAX_ACTIVE_GAMES];
extern pthread_mutex_t games_mutex;

// ============ Game Management ============

// Initialize game state system
void game_state_init(void);

// Create a new game for a match
// Returns pointer to game, or NULL on failure
ActiveGame* game_create(int match_id, int p1_user_id, const char* p1_name, 
                        int p2_user_id, const char* p2_name);

// Find game by match_id
ActiveGame* game_find(int match_id);

// Find game by user_id
ActiveGame* game_find_by_player(int user_id);

// End and cleanup a game
void game_destroy(int match_id);

// ============ Game Actions ============

// Roll dice and move player
// Returns: 0 on success, -1 on error
int game_roll_dice(ActiveGame* game, int player_idx);

// Buy the current property
int game_buy_property(ActiveGame* game, int player_idx);

// Skip buying property
int game_skip_property(ActiveGame* game, int player_idx);

// Upgrade property (build house/hotel)
int game_upgrade_property(ActiveGame* game, int player_idx, int prop_id);

// Downgrade property (sell house)
int game_downgrade_property(ActiveGame* game, int player_idx, int prop_id);

// Mortgage/unmortgage property
int game_mortgage_property(ActiveGame* game, int player_idx, int prop_id);

// Pay jail fine
int game_pay_jail_fine(ActiveGame* game, int player_idx);

// Declare bankruptcy
int game_declare_bankrupt(ActiveGame* game, int player_idx);

// Pause game (only the player who paused can resume)
int game_pause(ActiveGame* game, int player_idx);

// Resume game
int game_resume(ActiveGame* game, int player_idx);

// Surrender - opponent wins immediately
int game_surrender(ActiveGame* game, int player_idx);

// ============ State Serialization ============

// Serialize game state to JSON string
// Caller must free the returned string
char* game_serialize_state(ActiveGame* game);

// Get the winner/loser when game ends
// Returns -1 if game hasn't ended
int game_get_winner(ActiveGame* game);
int game_get_loser(ActiveGame* game);

#endif // GAME_STATE_H

