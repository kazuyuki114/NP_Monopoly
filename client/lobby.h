#ifndef LOBBY_H
#define LOBBY_H

#include <SDL.h>
#include "client_network.h"

// Lobby states
typedef enum {
    LOBBY_STATE_CONNECTING,
    LOBBY_STATE_LOGIN,
    LOBBY_STATE_REGISTER,
    LOBBY_STATE_MAIN_MENU,
    LOBBY_STATE_SEARCHING,
    LOBBY_STATE_START_GAME,
    LOBBY_STATE_EXIT
} LobbyState;

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

#endif // LOBBY_H

