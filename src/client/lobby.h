#ifndef LOBBY_H
#define LOBBY_H

#include <SDL.h>
#include "client_network.h"

// Maximum online players to display
#define MAX_ONLINE_PLAYERS 20

// Lobby states
typedef enum {
    LOBBY_STATE_CONNECTING,
    LOBBY_STATE_LOGIN,
    LOBBY_STATE_REGISTER,
    LOBBY_STATE_MAIN_MENU,
    LOBBY_STATE_SEARCHING,
    LOBBY_STATE_CHALLENGE_RECEIVED,
    LOBBY_STATE_GAME_RESULT,
    LOBBY_STATE_VIEW_HISTORY,
    LOBBY_STATE_START_GAME,
    LOBBY_STATE_EXIT
} LobbyState;

// Online player info for display
typedef struct {
    int user_id;
    char username[50];
    int elo_rating;
    char status[20];
} LobbyPlayerInfo;

// Challenge info
typedef struct {
    int challenge_id;
    int challenger_id;
    char challenger_name[50];
    int challenger_elo;
} ChallengeInfo;

// Game result info
typedef struct {
    int match_id;
    int is_draw;
    int you_won;
    char opponent_name[50];
    int your_elo_before;
    int your_elo_after;
    int your_elo_change;
    int opponent_elo_before;
    int opponent_elo_after;
    char reason[64];
} GameResultInfo;

// Match found info
typedef struct {
    int match_id;
    int opponent_id;
    char opponent_name[50];
    int opponent_elo;
    int your_player_num;
} MatchFoundInfo;

// Initialize lobby system (SDL window, fonts, etc.)
// Returns 0 on success, -1 on error
int Lobby_init(void);

// Clean up lobby resources
void Lobby_close(void);

// Main lobby loop - returns when game should start or exit
// Returns: LOBBY_STATE_START_GAME if game should start
//          LOBBY_STATE_EXIT if user wants to quit
LobbyState Lobby_run(ClientState* client, const char* server_ip, int port);

// Get connected client state (for passing to game)
ClientState* Lobby_getClientState(void);

// Get match info (after LOBBY_STATE_START_GAME)
MatchFoundInfo* Lobby_getMatchInfo(void);

#endif // LOBBY_H

