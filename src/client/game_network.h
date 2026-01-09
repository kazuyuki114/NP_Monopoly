#ifndef GAME_NETWORK_H
#define GAME_NETWORK_H

#include "client_network.h"
#include "lobby.h"

// Network game state (client-side connection state)
typedef enum {
    NET_GAME_WAITING,        // Waiting for game to start
    NET_GAME_MY_TURN,        // It's our turn
    NET_GAME_OPPONENT_TURN,  // Waiting for opponent
    NET_GAME_PAUSED,         // Game is paused
    NET_GAME_ENDED           // Game has ended
} NetGameState;

// Server-side game state types (matches server's GameStateType)
typedef enum {
    GSTATE_WAITING_ROLL = 0,
    GSTATE_WAITING_BUY = 1,
    GSTATE_WAITING_DEBT = 2,
    GSTATE_PAUSED = 3,
    GSTATE_ENDED = 4
} GameStateType;

// Game result info
typedef struct {
    int is_draw;
    int winner_id;
    int loser_id;
    char winner_name[50];
    char loser_name[50];
    int winner_elo_change;
    int loser_elo_change;
    int winner_new_elo;
    int loser_new_elo;
    char reason[32];  // "surrender", "bankruptcy", "draw", "disconnect"
} GameResult;

// Game state (shared between network and render)
typedef struct {
    int current_player;
    int my_player_index;  // 0 or 1
    GameStateType state_type;  // What action is expected
    int dice[2];
    char message[128];
    char message2[128];
    int paused;
    int paused_by;  // Which player paused (0 or 1)
    
    // Player info
    struct {
        int user_id;
        char username[50];
        int money;
        int position;
        int jailed;
        int turns_in_jail;
    } players[2];
    
    // Properties
    struct {
        int owner;     // -1, 0, or 1
        int upgrades;
        int mortgaged;
    } properties[40];
    
    // Game result (filled when game ends)
    GameResult result;
    int has_result;
} SyncedGameState;

// Global synced state accessible by renderer
extern SyncedGameState g_synced_state;

// Initialize network game with match info
void NetGame_init(ClientState* client, MatchFoundInfo* matchInfo);

// Process network messages during game
// Returns 1 if game should continue, 0 if game ended
int NetGame_processMessages(ClientState* client);

// Get current network game state
NetGameState NetGame_getState(void);

// Check if it's our turn
int NetGame_isMyTurn(void);

// Get game state type (roll, buy, debt, etc.)
GameStateType NetGame_getStateType(void);

// Check if waiting for buy decision
int NetGame_isWaitingBuy(void);

// Check if game is paused
int NetGame_isPaused(void);

// Check if we paused the game
int NetGame_didWePause(void);

// Get our player number (0 or 1)
int NetGame_getPlayerNum(void);

// Get match ID
int NetGame_getMatchId(void);

// Get player names
const char* NetGame_getPlayerName(int idx);
const char* NetGame_getMyName(void);
const char* NetGame_getOpponentName(void);

// Get opponent's user ID (for rematch)
int NetGame_getOpponentId(void);

// Get game result (if game ended)
const GameResult* NetGame_getResult(void);
int NetGame_hasResult(void);

// Send game actions to server
void NetGame_rollDice(ClientState* client);
void NetGame_buyProperty(ClientState* client);
void NetGame_skipProperty(ClientState* client);
void NetGame_upgradeProperty(ClientState* client, int property_id);
void NetGame_downgradeProperty(ClientState* client, int property_id);
void NetGame_mortgageProperty(ClientState* client, int property_id);
void NetGame_payJailFine(ClientState* client);
void NetGame_declareBankrupt(ClientState* client);
void NetGame_surrender(ClientState* client);
void NetGame_offerDraw(ClientState* client);
int NetGame_hasPendingDrawOffer(void);
int NetGame_isWaitingForDrawResponse(void);
void NetGame_respondToDraw(ClientState* client, int accept);

// Pause/Resume
void NetGame_pause(ClientState* client);
void NetGame_resume(ClientState* client);

// Send heartbeat to prevent timeout
void NetGame_sendHeartbeat(ClientState* client);

// Clean up
void NetGame_close(void);

#endif // GAME_NETWORK_H
