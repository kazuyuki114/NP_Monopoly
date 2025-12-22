#ifndef GAME_HANDLER_H
#define GAME_HANDLER_H

#include "server.h"
#include "elo.h"

// ============ Match Result Handling ============

// Handle game end and calculate ELO changes
// winner_id: The user_id of the winner
// loser_id: The user_id of the loser
// match_id: The match being ended
// reason: Why the game ended ("bankruptcy", "surrender", "disconnect", "timeout")
void handle_game_end(GameServer* server, int match_id, int winner_id, int loser_id, const char* reason);

// Handle a draw/tie (for games that support it)
void handle_game_draw(GameServer* server, int match_id, int player1_id, int player2_id);

// Handle player surrender
void handle_surrender(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle draw offer
void handle_draw_offer(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle draw response (accept/decline)
void handle_draw_response(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle rematch request
void handle_rematch_request(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Handle rematch response
void handle_rematch_response(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// ============ Utility ============

// Send game result to both players
void send_game_result(GameServer* server, int match_id, 
                      ConnectedClient* winner, ConnectedClient* loser, 
                      EloResult* elo_result, const char* reason);

// Clean up match state after game ends
void cleanup_match(GameServer* server, int match_id);

#endif // GAME_HANDLER_H

