/*
 * Game Handler Implementation
 * 
 * Handles:
 * - Game end and ELO calculation
 * - Surrender/draw/rematch requests
 * - Match cleanup
 */

#include "game_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "cJSON.h"

// ============ Match Result Handling ============

void handle_game_end(GameServer* server, int match_id, int winner_id, int loser_id, const char* reason) {
    printf("[GAME] Match %d ended: winner=%d, loser=%d, reason=%s\n", 
           match_id, winner_id, loser_id, reason);
    
    // Find both players
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* winner = find_client_by_id(server, winner_id);
    ConnectedClient* loser = find_client_by_id(server, loser_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    // Get game counts for K-factor calculation
    UserInfo winner_info, loser_info;
    db_get_user_info(&server->db, winner_id, &winner_info);
    db_get_user_info(&server->db, loser_id, &loser_info);
    
    // Calculate ELO changes
    EloResult elo_result;
    elo_calculate_match(
        winner_info.elo_rating, 
        loser_info.elo_rating,
        winner_info.total_matches,
        loser_info.total_matches,
        &elo_result
    );
    
    // Update database with new ELOs
    db_update_user_elo(&server->db, winner_id, elo_result.winner_new_elo);
    db_update_user_elo(&server->db, loser_id, elo_result.loser_new_elo);
    
    // Update match result in database
    db_update_match_result(&server->db, match_id, winner_id, 
                           elo_result.winner_new_elo, elo_result.loser_new_elo);
    
    // Update player stats
    db_update_user_stats(&server->db, winner_id, 1);  // Win
    db_update_user_stats(&server->db, loser_id, 0);   // Loss
    
    // Send results to players
    if (winner || loser) {
        send_game_result(server, match_id, winner, loser, &elo_result, reason);
    }
    
    // Update client states if connected
    if (winner) {
        winner->elo_rating = elo_result.winner_new_elo;
        winner->status = PLAYER_IDLE;
        winner->current_match_id = 0;
        db_set_player_online(&server->db, winner_id, "idle");
    }
    
    if (loser) {
        loser->elo_rating = elo_result.loser_new_elo;
        loser->status = PLAYER_IDLE;
        loser->current_match_id = 0;
        db_set_player_online(&server->db, loser_id, "idle");
    }
    
    printf("[GAME] ELO updated: %s %d -> %d (%+d), %s %d -> %d (%+d)\n",
           winner_info.username, elo_result.winner_old_elo, elo_result.winner_new_elo, elo_result.winner_change,
           loser_info.username, elo_result.loser_old_elo, elo_result.loser_new_elo, elo_result.loser_change);
}

void handle_game_draw(GameServer* server, int match_id, int requesting_player_id, int other_player_id) {
    printf("[GAME] Match %d ended in a draw\n", match_id);
    
    // Look up the actual player1_id and player2_id from the match
    // This is critical because the requesting player might not be player1
    int actual_p1_id = 0, actual_p2_id = 0;
    {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT player1_id, player2_id FROM matches WHERE match_id = ?";
        if (sqlite3_prepare_v2(server->db.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, match_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                actual_p1_id = sqlite3_column_int(stmt, 0);
                actual_p2_id = sqlite3_column_int(stmt, 1);
            }
            sqlite3_finalize(stmt);
        }
    }
    
    // Fallback if lookup fails - use passed parameters
    if (actual_p1_id == 0 || actual_p2_id == 0) {
        actual_p1_id = requesting_player_id;
        actual_p2_id = other_player_id;
    }
    
    // Find both players by their actual IDs
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* player1 = find_client_by_id(server, actual_p1_id);
    ConnectedClient* player2 = find_client_by_id(server, actual_p2_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    // Get current ELOs
    UserInfo info1, info2;
    db_get_user_info(&server->db, actual_p1_id, &info1);
    db_get_user_info(&server->db, actual_p2_id, &info2);
    
    // Calculate draw ELO change (positive means player1 gains)
    int elo_change = elo_calculate_draw(info1.elo_rating, info2.elo_rating);
    
    int p1_new_elo = info1.elo_rating + elo_change;
    int p2_new_elo = info2.elo_rating - elo_change;
    
    // Update database - ELOs for each user
    db_update_user_elo(&server->db, actual_p1_id, p1_new_elo);
    db_update_user_elo(&server->db, actual_p2_id, p2_new_elo);
    
    // Update player stats (draw = -1, increments total_matches only)
    db_update_user_stats(&server->db, actual_p1_id, -1);
    db_update_user_stats(&server->db, actual_p2_id, -1);
    
    // Update match result (winner_id = 0 for draw)
    // Now p1_new_elo corresponds to actual player1, p2_new_elo to actual player2
    db_update_match_result(&server->db, match_id, 0, p1_new_elo, p2_new_elo);
    
    // Update client states
    if (player1) {
        player1->elo_rating = p1_new_elo;
        player1->status = PLAYER_IDLE;
        player1->current_match_id = 0;
        db_set_player_online(&server->db, actual_p1_id, "idle");
    }
    
    if (player2) {
        player2->elo_rating = p2_new_elo;
        player2->status = PLAYER_IDLE;
        player2->current_match_id = 0;
        db_set_player_online(&server->db, actual_p2_id, "idle");
    }
    
    // Send draw result to both players
    cJSON* result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "match_id", match_id);
    cJSON_AddBoolToObject(result, "is_draw", 1);
    cJSON_AddStringToObject(result, "reason", "draw");
    
    // Player 1 info
    cJSON_AddNumberToObject(result, "player1_id", actual_p1_id);
    cJSON_AddStringToObject(result, "player1_name", player1 ? player1->username : info1.username);
    cJSON_AddNumberToObject(result, "player1_old_elo", info1.elo_rating);
    cJSON_AddNumberToObject(result, "player1_new_elo", p1_new_elo);
    cJSON_AddNumberToObject(result, "player1_elo_change", elo_change);
    
    // Player 2 info
    cJSON_AddNumberToObject(result, "player2_id", actual_p2_id);
    cJSON_AddStringToObject(result, "player2_name", player2 ? player2->username : info2.username);
    cJSON_AddNumberToObject(result, "player2_old_elo", info2.elo_rating);
    cJSON_AddNumberToObject(result, "player2_new_elo", p2_new_elo);
    cJSON_AddNumberToObject(result, "player2_elo_change", -elo_change);
    
    printf("[GAME] Draw ELO: %s %d -> %d (%+d), %s %d -> %d (%+d)\n",
           player1 ? player1->username : "P1", info1.elo_rating, p1_new_elo, elo_change,
           player2 ? player2->username : "P2", info2.elo_rating, p2_new_elo, -elo_change);
    
    char* result_str = cJSON_PrintUnformatted(result);
    
    if (player1) send_message(player1, MSG_GAME_RESULT, result_str);
    if (player2) send_message(player2, MSG_GAME_RESULT, result_str);
    
    free(result_str);
    cJSON_Delete(result);
}

void handle_surrender(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    (void)msg;  // Unused
    
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status != PLAYER_IN_GAME || client->current_match_id == 0) {
        send_error(client, "Not in a game");
        return;
    }
    
    int match_id = client->current_match_id;
    int loser_id = client->user_id;
    
    printf("[GAME] %s surrendered in match %d\n", client->username, match_id);
    
    // Find the opponent (winner)
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* winner = NULL;
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* c = server->clients[i];
        if (c != client && c->current_match_id == match_id && c->status == PLAYER_IN_GAME) {
            winner = c;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (winner) {
        handle_game_end(server, match_id, winner->user_id, loser_id, "surrender");
    } else {
        // Opponent disconnected, just clean up
        client->status = PLAYER_IDLE;
        client->current_match_id = 0;
        db_set_player_online(&server->db, client->user_id, "idle");
        send_success(client, "Match ended");
    }
}

void handle_draw_offer(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    (void)msg;
    
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status != PLAYER_IN_GAME || client->current_match_id == 0) {
        send_error(client, "Not in a game");
        return;
    }
    
    int match_id = client->current_match_id;
    
    // Find the opponent
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* opponent = NULL;
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* c = server->clients[i];
        if (c != client && c->current_match_id == match_id && c->status == PLAYER_IN_GAME) {
            opponent = c;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (!opponent) {
        send_error(client, "Opponent not found");
        return;
    }
    
    // Send draw offer to opponent
    cJSON* offer = cJSON_CreateObject();
    cJSON_AddNumberToObject(offer, "from_id", client->user_id);
    cJSON_AddStringToObject(offer, "from_name", client->username);
    cJSON_AddStringToObject(offer, "message", "Your opponent offers a draw");
    
    char* offer_str = cJSON_PrintUnformatted(offer);
    send_message(opponent, MSG_GAME_END, offer_str);  // Use MSG_GAME_END for draw offer
    free(offer_str);
    cJSON_Delete(offer);
    
    send_success(client, "Draw offer sent");
}

void handle_draw_response(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status != PLAYER_IN_GAME || client->current_match_id == 0) {
        send_error(client, "Not in a game");
        return;
    }
    
    // Parse response
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* accept_item = cJSON_GetObjectItem(json, "accept");
    int accept = accept_item && cJSON_IsTrue(accept_item);
    cJSON_Delete(json);
    
    int match_id = client->current_match_id;
    
    // Find the opponent
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* opponent = NULL;
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* c = server->clients[i];
        if (c != client && c->current_match_id == match_id && c->status == PLAYER_IN_GAME) {
            opponent = c;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (accept && opponent) {
        // Draw accepted
        handle_game_draw(server, match_id, client->user_id, opponent->user_id);
    } else if (opponent) {
        // Draw declined
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "draw_declined", 1);
        cJSON_AddStringToObject(response, "message", "Draw offer declined");
        
        char* response_str = cJSON_PrintUnformatted(response);
        send_message(opponent, MSG_GAME_END, response_str);
        free(response_str);
        cJSON_Delete(response);
        
        send_success(client, "Draw declined");
    }
}

void handle_rematch_request(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    (void)msg;
    
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    // Parse target from last match
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* target_item = cJSON_GetObjectItem(json, "opponent_id");
    if (!target_item || !cJSON_IsNumber(target_item)) {
        send_error(client, "Missing opponent_id");
        cJSON_Delete(json);
        return;
    }
    
    int opponent_id = target_item->valueint;
    cJSON_Delete(json);
    
    // Find opponent
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* opponent = find_client_by_id(server, opponent_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (!opponent) {
        send_error(client, "Opponent is not online");
        return;
    }
    
    if (opponent->status == PLAYER_IN_GAME) {
        send_error(client, "Opponent is already in a game");
        return;
    }
    
    // Send rematch request to opponent
    cJSON* request = cJSON_CreateObject();
    cJSON_AddNumberToObject(request, "from_id", client->user_id);
    cJSON_AddStringToObject(request, "from_name", client->username);
    cJSON_AddNumberToObject(request, "from_elo", client->elo_rating);
    cJSON_AddStringToObject(request, "message", "Your opponent wants a rematch!");
    
    char* request_str = cJSON_PrintUnformatted(request);
    send_message(opponent, MSG_REMATCH_REQUEST, request_str);
    free(request_str);
    cJSON_Delete(request);
    
    send_success(client, "Rematch request sent");
}

void handle_rematch_response(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    // Parse response
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* accept_item = cJSON_GetObjectItem(json, "accept");
    cJSON* opponent_item = cJSON_GetObjectItem(json, "opponent_id");
    
    int accept = accept_item && cJSON_IsTrue(accept_item);
    int opponent_id = (opponent_item && cJSON_IsNumber(opponent_item)) ? opponent_item->valueint : 0;
    cJSON_Delete(json);
    
    if (opponent_id == 0) {
        send_error(client, "Missing opponent_id");
        return;
    }
    
    // Find opponent
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* opponent = find_client_by_id(server, opponent_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (!opponent) {
        send_error(client, "Opponent is not online");
        return;
    }
    
    if (accept) {
        // Create new match
        if (client->status == PLAYER_IN_GAME || opponent->status == PLAYER_IN_GAME) {
            send_error(client, "One player is already in a game");
            return;
        }
        
        // Use matchmaking's create_match
        extern int create_match(GameServer* server, ConnectedClient* p1, ConnectedClient* p2);
        extern void send_match_found(ConnectedClient* p1, ConnectedClient* p2, int match_id);
        
        int match_id = create_match(server, opponent, client);
        if (match_id >= 0) {
            send_match_found(opponent, client, match_id);
        } else {
            send_error(client, "Failed to create rematch");
            send_error(opponent, "Failed to create rematch");
        }
    } else {
        // Rematch declined
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "declined", 1);
        cJSON_AddStringToObject(response, "message", "Rematch declined");
        
        char* response_str = cJSON_PrintUnformatted(response);
        send_message(opponent, MSG_REMATCH_RESPONSE, response_str);
        free(response_str);
        cJSON_Delete(response);
    }
}

// ============ Utility ============

void send_game_result(GameServer* server, int match_id, 
                      ConnectedClient* winner, ConnectedClient* loser, 
                      EloResult* elo_result, const char* reason) {
    (void)server;
    
    // Create result JSON
    cJSON* result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "match_id", match_id);
    cJSON_AddBoolToObject(result, "is_draw", 0);
    cJSON_AddStringToObject(result, "reason", reason);
    
    if (winner) {
        cJSON_AddNumberToObject(result, "winner_id", winner->user_id);
        cJSON_AddStringToObject(result, "winner_name", winner->username);
    }
    if (loser) {
        cJSON_AddNumberToObject(result, "loser_id", loser->user_id);
        cJSON_AddStringToObject(result, "loser_name", loser->username);
    }
    
    cJSON_AddNumberToObject(result, "winner_elo_before", elo_result->winner_old_elo);
    cJSON_AddNumberToObject(result, "winner_elo_after", elo_result->winner_new_elo);
    cJSON_AddNumberToObject(result, "winner_elo_change", elo_result->winner_change);
    
    cJSON_AddNumberToObject(result, "loser_elo_before", elo_result->loser_old_elo);
    cJSON_AddNumberToObject(result, "loser_elo_after", elo_result->loser_new_elo);
    cJSON_AddNumberToObject(result, "loser_elo_change", elo_result->loser_change);
    
    char* result_str = cJSON_PrintUnformatted(result);
    
    // Send to both players
    if (winner) send_message(winner, MSG_GAME_RESULT, result_str);
    if (loser) send_message(loser, MSG_GAME_RESULT, result_str);
    
    free(result_str);
    cJSON_Delete(result);
}

void cleanup_match(GameServer* server, int match_id) {
    // Reset all players in this match to idle
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* c = server->clients[i];
        if (c->current_match_id == match_id) {
            c->status = PLAYER_IDLE;
            c->current_match_id = 0;
            db_set_player_online(&server->db, c->user_id, "idle");
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

