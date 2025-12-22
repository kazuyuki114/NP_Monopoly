#ifndef MATCHMAKING_H
#define MATCHMAKING_H

#include "server.h"

// Challenge timeout in seconds (60 seconds to respond)
#define CHALLENGE_TIMEOUT 60

// ============ Matchmaking Handlers ============

// Handle request to get list of online players
void handle_get_online_players(GameServer* server, ConnectedClient* client);

// Handle request to start searching for a match
void handle_search_match(GameServer* server, ConnectedClient* client);

// Handle request to cancel match search
void handle_cancel_search(GameServer* server, ConnectedClient* client);

// ============ Challenge Handlers ============

// Handle sending a challenge to another player
void handle_send_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle accepting a challenge
void handle_accept_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle declining a challenge
void handle_decline_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// ============ Match Creation ============

// Create a new match between two players
// Returns match_id on success, -1 on error
int create_match(GameServer* server, ConnectedClient* player1, ConnectedClient* player2);

// ============ Matchmaking Engine ============

// Attempt to find and create matches for all searching players
// Called periodically from server main loop
void matchmaking_try_match_players(GameServer* server);

// Send match found notification to both players
void send_match_found(ConnectedClient* player1, ConnectedClient* player2, int match_id);

#endif // MATCHMAKING_H

