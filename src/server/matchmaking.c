/*
 * Matchmaking System Implementation
 * 
 * Handles:
 * - Online player list requests
 * - Match searching with ELO-based matchmaking
 * - Direct player challenges
 * - Match creation and notifications
 */

#include "matchmaking.h"
#include "elo.h"
#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include "cJSON.h"

// ============ Online Players ============

void handle_get_online_players(GameServer* server, ConnectedClient* client) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    printf("[MATCHMAKING] Get online players request from %s\n", client->username);
    
    // Get online players from database
    OnlinePlayerInfo* players = NULL;
    int count = 0;
    
    if (db_get_online_players(&server->db, &players, &count) != 0) {
        send_error(client, "Failed to get online players");
        return;
    }
    
    // Build JSON response
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddNumberToObject(response, "count", count);
    
    cJSON* playerArray = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON* player = cJSON_CreateObject();
        cJSON_AddNumberToObject(player, "user_id", players[i].user_id);
        cJSON_AddStringToObject(player, "username", players[i].username);
        cJSON_AddNumberToObject(player, "elo_rating", players[i].elo_rating);
        cJSON_AddStringToObject(player, "status", players[i].status);
        cJSON_AddItemToArray(playerArray, player);
    }
    cJSON_AddItemToObject(response, "players", playerArray);
    
    char* response_str = cJSON_PrintUnformatted(response);
    send_message(client, MSG_ONLINE_PLAYERS_LIST, response_str);
    
    free(response_str);
    cJSON_Delete(response);
    if (players) free(players);
}

// ============ Match Searching ============

void handle_search_match(GameServer* server, ConnectedClient* client) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status == PLAYER_IN_GAME) {
        send_error(client, "Already in a game");
        return;
    }
    
    if (client->status == PLAYER_SEARCHING) {
        send_error(client, "Already searching for a match");
        return;
    }
    
    printf("[MATCHMAKING] %s started searching for match (ELO: %d)\n", 
           client->username, client->elo_rating);
    
    // Mark as searching
    client->status = PLAYER_SEARCHING;
    client->last_heartbeat = time(NULL);  // Use as search start time
    db_join_matchmaking(&server->db, client->user_id);
    
    // Send confirmation
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddStringToObject(response, "message", "Searching for opponent...");
    cJSON_AddNumberToObject(response, "your_elo", client->elo_rating);
    
    char* response_str = cJSON_PrintUnformatted(response);
    send_message(client, MSG_SEARCH_MATCH, response_str);
    
    free(response_str);
    cJSON_Delete(response);
    
    // Try to match immediately
    matchmaking_try_match_players(server);
}

void handle_cancel_search(GameServer* server, ConnectedClient* client) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status != PLAYER_SEARCHING) {
        send_error(client, "Not searching for a match");
        return;
    }
    
    printf("[MATCHMAKING] %s cancelled match search\n", client->username);
    
    // Stop searching
    client->status = PLAYER_IDLE;
    db_leave_matchmaking(&server->db, client->user_id);
    
    // Send confirmation
    send_success(client, "Match search cancelled");
}

// ============ Challenge System ============

void handle_send_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status == PLAYER_IN_GAME) {
        send_error(client, "You are already in a game");
        return;
    }
    
    // Parse target user ID
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request format");
        return;
    }
    
    cJSON* target_item = cJSON_GetObjectItem(json, "target_id");
    if (!target_item || !cJSON_IsNumber(target_item)) {
        send_error(client, "Missing target_id");
        cJSON_Delete(json);
        return;
    }
    
    int target_id = target_item->valueint;
    cJSON_Delete(json);
    
    // Can't challenge yourself
    if (target_id == client->user_id) {
        send_error(client, "You can't challenge yourself");
        return;
    }
    
    // Find target client
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* target = find_client_by_id(server, target_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (!target) {
        send_error(client, "Player is not online");
        return;
    }
    
    if (target->status == PLAYER_IN_GAME) {
        send_error(client, "Player is already in a game");
        return;
    }
    
    // Create challenge in database
    int challenge_id = db_create_challenge(&server->db, client->user_id, target_id);
    if (challenge_id < 0) {
        send_error(client, "Failed to create challenge");
        return;
    }
    
    printf("[MATCHMAKING] %s challenged %s (challenge_id=%d)\n", 
           client->username, target->username, challenge_id);
    
    // Send confirmation to challenger
    cJSON* response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", 1);
    cJSON_AddStringToObject(response, "message", "Challenge sent!");
    cJSON_AddNumberToObject(response, "challenge_id", challenge_id);
    cJSON_AddNumberToObject(response, "target_id", target_id);
    cJSON_AddStringToObject(response, "target_name", target->username);
    
    char* response_str = cJSON_PrintUnformatted(response);
    send_message(client, MSG_SEND_CHALLENGE, response_str);
    free(response_str);
    cJSON_Delete(response);
    
    // Send challenge notification to target
    cJSON* notification = cJSON_CreateObject();
    cJSON_AddNumberToObject(notification, "challenge_id", challenge_id);
    cJSON_AddNumberToObject(notification, "challenger_id", client->user_id);
    cJSON_AddStringToObject(notification, "challenger_name", client->username);
    cJSON_AddNumberToObject(notification, "challenger_elo", client->elo_rating);
    
    char* notif_str = cJSON_PrintUnformatted(notification);
    send_message(target, MSG_CHALLENGE_REQUEST, notif_str);
    free(notif_str);
    cJSON_Delete(notification);
}

void handle_accept_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    if (client->status == PLAYER_IN_GAME) {
        send_error(client, "You are already in a game");
        return;
    }
    
    // Parse challenge ID
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request format");
        return;
    }
    
    cJSON* challenge_item = cJSON_GetObjectItem(json, "challenge_id");
    if (!challenge_item || !cJSON_IsNumber(challenge_item)) {
        send_error(client, "Missing challenge_id");
        cJSON_Delete(json);
        return;
    }
    
    int challenge_id = challenge_item->valueint;
    cJSON_Delete(json);
    
    // Get challenge info
    int challenger_id, challenged_id;
    char status[20];
    
    if (db_get_challenge(&server->db, challenge_id, &challenger_id, &challenged_id, status) != 0) {
        send_error(client, "Challenge not found");
        return;
    }
    
    // Verify this client is the challenged player
    if (challenged_id != client->user_id) {
        send_error(client, "This challenge is not for you");
        return;
    }
    
    // Check challenge is still pending
    if (strcmp(status, "pending") != 0) {
        send_error(client, "Challenge is no longer pending");
        return;
    }
    
    // Find challenger
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* challenger = find_client_by_id(server, challenger_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (!challenger) {
        send_error(client, "Challenger is no longer online");
        db_respond_challenge(&server->db, challenge_id, "expired");
        return;
    }
    
    if (challenger->status == PLAYER_IN_GAME) {
        send_error(client, "Challenger is already in another game");
        db_respond_challenge(&server->db, challenge_id, "expired");
        return;
    }
    
    printf("[MATCHMAKING] %s accepted challenge from %s\n", 
           client->username, challenger->username);
    
    // Update challenge status
    db_respond_challenge(&server->db, challenge_id, "accepted");
    
    // Stop both players from searching if they were
    if (client->status == PLAYER_SEARCHING) {
        db_leave_matchmaking(&server->db, client->user_id);
    }
    if (challenger->status == PLAYER_SEARCHING) {
        db_leave_matchmaking(&server->db, challenger->user_id);
    }
    
    // Create the match
    int match_id = create_match(server, challenger, client);
    if (match_id < 0) {
        send_error(client, "Failed to create match");
        send_error(challenger, "Failed to create match");
        return;
    }
    
    // Notify both players
    send_match_found(challenger, client, match_id);
}

void handle_decline_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id) {
        send_error(client, "Not logged in");
        return;
    }
    
    // Parse challenge ID
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request format");
        return;
    }
    
    cJSON* challenge_item = cJSON_GetObjectItem(json, "challenge_id");
    if (!challenge_item || !cJSON_IsNumber(challenge_item)) {
        send_error(client, "Missing challenge_id");
        cJSON_Delete(json);
        return;
    }
    
    int challenge_id = challenge_item->valueint;
    cJSON_Delete(json);
    
    // Get challenge info
    int challenger_id, challenged_id;
    char status[20];
    
    if (db_get_challenge(&server->db, challenge_id, &challenger_id, &challenged_id, status) != 0) {
        send_error(client, "Challenge not found");
        return;
    }
    
    // Verify this client is the challenged player
    if (challenged_id != client->user_id) {
        send_error(client, "This challenge is not for you");
        return;
    }
    
    printf("[MATCHMAKING] %s declined challenge from user %d\n", 
           client->username, challenger_id);
    
    // Update challenge status
    db_respond_challenge(&server->db, challenge_id, "declined");
    
    // Send confirmation
    send_success(client, "Challenge declined");
    
    // Notify challenger
    pthread_mutex_lock(&server->clients_mutex);
    ConnectedClient* challenger = find_client_by_id(server, challenger_id);
    pthread_mutex_unlock(&server->clients_mutex);
    
    if (challenger) {
        cJSON* notification = cJSON_CreateObject();
        cJSON_AddNumberToObject(notification, "challenge_id", challenge_id);
        cJSON_AddNumberToObject(notification, "declined_by_id", client->user_id);
        cJSON_AddStringToObject(notification, "declined_by_name", client->username);
        cJSON_AddStringToObject(notification, "message", "Your challenge was declined");
        
        char* notif_str = cJSON_PrintUnformatted(notification);
        send_message(challenger, MSG_DECLINE_CHALLENGE, notif_str);
        free(notif_str);
        cJSON_Delete(notification);
    }
}

// ============ Match Creation ============

int create_match(GameServer* server, ConnectedClient* player1, ConnectedClient* player2) {
    // Create match in database
    int match_id = db_create_match(&server->db, 
                                    player1->user_id, player2->user_id,
                                    player1->elo_rating, player2->elo_rating);
    
    if (match_id < 0) {
        fprintf(stderr, "[MATCHMAKING] Failed to create match in database\n");
        return -1;
    }
    
    // Create game state
    ActiveGame* game = game_create(match_id, 
                                   player1->user_id, player1->username,
                                   player2->user_id, player2->username);
    if (!game) {
        fprintf(stderr, "[MATCHMAKING] Failed to create game state\n");
        return -1;
    }
    
    // Update player statuses
    player1->status = PLAYER_IN_GAME;
    player2->status = PLAYER_IN_GAME;
    player1->current_match_id = match_id;
    player2->current_match_id = match_id;
    
    // Update database
    db_set_player_game(&server->db, player1->user_id, match_id);
    db_set_player_game(&server->db, player2->user_id, match_id);
    
    printf("[MATCHMAKING] Match %d created: %s (ELO %d) vs %s (ELO %d)\n",
           match_id,
           player1->username, player1->elo_rating,
           player2->username, player2->elo_rating);
    
    return match_id;
}

void send_match_found(ConnectedClient* player1, ConnectedClient* player2, int match_id) {
    // Build match info for player 1
    cJSON* msg1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(msg1, "match_id", match_id);
    cJSON_AddNumberToObject(msg1, "opponent_id", player2->user_id);
    cJSON_AddStringToObject(msg1, "opponent_name", player2->username);
    cJSON_AddNumberToObject(msg1, "opponent_elo", player2->elo_rating);
    cJSON_AddNumberToObject(msg1, "your_player_num", 1);  // Player 1 goes first
    cJSON_AddStringToObject(msg1, "message", "Match found! You go first.");
    
    char* str1 = cJSON_PrintUnformatted(msg1);
    send_message(player1, MSG_MATCH_FOUND, str1);
    free(str1);
    cJSON_Delete(msg1);
    
    // Build match info for player 2
    cJSON* msg2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(msg2, "match_id", match_id);
    cJSON_AddNumberToObject(msg2, "opponent_id", player1->user_id);
    cJSON_AddStringToObject(msg2, "opponent_name", player1->username);
    cJSON_AddNumberToObject(msg2, "opponent_elo", player1->elo_rating);
    cJSON_AddNumberToObject(msg2, "your_player_num", 2);
    cJSON_AddStringToObject(msg2, "message", "Match found! Opponent goes first.");
    
    char* str2 = cJSON_PrintUnformatted(msg2);
    send_message(player2, MSG_MATCH_FOUND, str2);
    free(str2);
    cJSON_Delete(msg2);
}

// ============ Matchmaking Engine ============

void matchmaking_try_match_players(GameServer* server) {
    // Get all searching clients
    pthread_mutex_lock(&server->clients_mutex);
    
    // Find pairs of searching players
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* player1 = server->clients[i];
        
        if (!player1->is_connected || player1->status != PLAYER_SEARCHING) {
            continue;
        }
        
        time_t now = time(NULL);
        int search_time = (int)(now - player1->last_heartbeat);
        
        // Look for opponent
        ConnectedClient* best_match = NULL;
        int best_elo_diff = 99999;
        
        for (int j = i + 1; j < server->client_count; j++) {
            ConnectedClient* player2 = server->clients[j];
            
            if (!player2->is_connected || player2->status != PLAYER_SEARCHING) {
                continue;
            }
            
            int elo_diff = abs(player1->elo_rating - player2->elo_rating);
            int p2_search_time = (int)(now - player2->last_heartbeat);
            int max_search_time = (search_time > p2_search_time) ? search_time : p2_search_time;
            
            // Check if within allowed range
            if (elo_is_good_match(player1->elo_rating, player2->elo_rating, max_search_time)) {
                if (elo_diff < best_elo_diff) {
                    best_match = player2;
                    best_elo_diff = elo_diff;
                }
            }
        }
        
        // If we found a match, create it
        if (best_match) {
            pthread_mutex_unlock(&server->clients_mutex);
            
            int match_id = create_match(server, player1, best_match);
            if (match_id >= 0) {
                send_match_found(player1, best_match, match_id);
            }
            
            // Restart search from beginning since array may have changed
            pthread_mutex_lock(&server->clients_mutex);
            i = -1;  // Will be incremented to 0
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
}

