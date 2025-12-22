#include "server.h"
#include "matchmaking.h"
#include "game_handler.h"
#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include "cJSON.h"

// Forward declarations for game action handlers
static void handle_roll_dice(GameServer* server, ConnectedClient* client);
static void handle_buy_property(GameServer* server, ConnectedClient* client);
static void handle_skip_property(GameServer* server, ConnectedClient* client);
static void handle_upgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
static void handle_downgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
static void handle_mortgage_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
static void handle_pay_jail_fine(GameServer* server, ConnectedClient* client);
static void handle_pause_game(GameServer* server, ConnectedClient* client);
static void handle_resume_game(GameServer* server, ConnectedClient* client);
static void handle_surrender_game(GameServer* server, ConnectedClient* client);
static void handle_get_history(GameServer* server, ConnectedClient* client);
static void broadcast_game_state(GameServer* server, ActiveGame* game);

static GameServer* global_server = NULL;

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    (void)sig;
    printf("\nShutdown signal received...\n");
    if (global_server) {
        global_server->running = 0;
    }
}

int server_init(GameServer* server, int port, const char* db_file) {
    if (!server) return -1;
    
    // Initialize server structure
    memset(server, 0, sizeof(GameServer));
    server->running = 1;
    server->port = port;
    server->client_count = 0;
    
    // Initialize mutex
    pthread_mutex_init(&server->clients_mutex, NULL);
    
    // Initialize database
    if (db_init(&server->db, db_file) != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return -1;
    }
    
    // Initialize game state manager
    game_state_init();
    
    // Seed random number generator
    srand(time(NULL));
    
    // Create TCP socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        perror("socket");
        db_close(&server->db);
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server->server_socket);
        db_close(&server->db);
        return -1;
    }
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server->server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server->server_socket);
        db_close(&server->db);
        return -1;
    }
    
    // Listen
    if (listen(server->server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server->server_socket);
        db_close(&server->db);
        return -1;
    }
    
    // Set up signal handlers
    global_server = server;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=================================\n");
    printf("  MONOPOLY GAME SERVER\n");
    printf("=================================\n");
    printf("Listening on port %d\n", port);
    printf("Database: %s\n", db_file);
    printf("Press Ctrl+C to stop\n");
    printf("=================================\n\n");
    
    return 0;
}

void server_accept_connection(GameServer* server) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sock = accept(server->server_socket, 
                             (struct sockaddr*)&client_addr, 
                             &addr_len);
    
    if (client_sock < 0) {
        perror("accept");
        return;
    }
    
    pthread_mutex_lock(&server->clients_mutex);
    
    if (server->client_count >= MAX_CLIENTS) {
        printf("[SERVER] Max clients reached, rejecting connection\n");
        close(client_sock);
        pthread_mutex_unlock(&server->clients_mutex);
        return;
    }
    
    // Create new client
    ConnectedClient* client = malloc(sizeof(ConnectedClient));
    memset(client, 0, sizeof(ConnectedClient));
    client->socket_fd = client_sock;
    client->user_id = 0;  // Not logged in yet
    client->status = PLAYER_DISCONNECTED;
    client->last_heartbeat = time(NULL);
    client->is_connected = 1;
    
    server->clients[server->client_count++] = client;
    
    pthread_mutex_unlock(&server->clients_mutex);
    
    printf("[SERVER] New connection from %s:%d (socket %d, total clients: %d)\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port),
           client_sock,
           server->client_count);
}

void server_handle_message(GameServer* server, ConnectedClient* client) {
    char buffer[MSG_HEADER_SIZE + MSG_MAX_PAYLOAD];
    
    // First read the header
    int bytes = recv(client->socket_fd, buffer, MSG_HEADER_SIZE, MSG_WAITALL);
    
    if (bytes <= 0) {
        // Client disconnected
        server_disconnect_client(server, client);
        return;
    }
    
    // Parse header to get payload length
    NetworkMessage msg;
    uint32_t* header = (uint32_t*)buffer;
    uint32_t payload_len = ntohl(header[3]);
    
    if (payload_len > 0 && payload_len <= MSG_MAX_PAYLOAD) {
        // Read payload
        bytes = recv(client->socket_fd, buffer + MSG_HEADER_SIZE, payload_len, MSG_WAITALL);
        if (bytes < (int)payload_len) {
            fprintf(stderr, "[SERVER] Incomplete payload received\n");
            return;
        }
    }
    
    // Deserialize full message
    if (msg_deserialize(&msg, buffer, MSG_HEADER_SIZE + payload_len) < 0) {
        fprintf(stderr, "[SERVER] Failed to deserialize message\n");
        return;
    }
    
    // Update heartbeat
    client->last_heartbeat = time(NULL);
    
    // Route message based on type
    switch (msg.type) {
        // === Authentication ===
        case MSG_REGISTER:
            handle_register(server, client, &msg);
            break;
            
        case MSG_LOGIN:
            handle_login(server, client, &msg);
            break;
            
        case MSG_LOGOUT:
            handle_logout(server, client);
            break;
        
        // === Matchmaking ===
        case MSG_GET_ONLINE_PLAYERS:
            handle_get_online_players(server, client);
            break;
            
        case MSG_SEARCH_MATCH:
            handle_search_match(server, client);
            break;
            
        case MSG_CANCEL_SEARCH:
            handle_cancel_search(server, client);
            break;
            
        // === Challenge System ===
        case MSG_SEND_CHALLENGE:
            handle_send_challenge(server, client, &msg);
            break;
            
        case MSG_ACCEPT_CHALLENGE:
            handle_accept_challenge(server, client, &msg);
            break;
            
        case MSG_DECLINE_CHALLENGE:
            handle_decline_challenge(server, client, &msg);
            break;
        
        // === Game Actions ===
        case MSG_ROLL_DICE:
            handle_roll_dice(server, client);
            break;
            
        case MSG_BUY_PROPERTY:
            handle_buy_property(server, client);
            break;
            
        case MSG_SKIP_PROPERTY:
            handle_skip_property(server, client);
            break;
            
        case MSG_UPGRADE_PROPERTY:
            handle_upgrade_property(server, client, &msg);
            break;
            
        case MSG_DOWNGRADE_PROPERTY:
            handle_downgrade_property(server, client, &msg);
            break;
            
        case MSG_MORTGAGE_PROPERTY:
            handle_mortgage_property(server, client, &msg);
            break;
            
        case MSG_PAY_JAIL_FINE:
            handle_pay_jail_fine(server, client);
            break;
            
        case MSG_PAUSE_GAME:
            handle_pause_game(server, client);
            break;
            
        case MSG_RESUME_GAME:
            handle_resume_game(server, client);
            break;
            
        case MSG_SURRENDER:
        case MSG_DECLARE_BANKRUPT:
            handle_surrender_game(server, client);
            break;
            
        case MSG_GAME_END:
            handle_draw_offer(server, client, &msg);
            break;
            
        // === Rematch ===
        case MSG_REMATCH_REQUEST:
            handle_rematch_request(server, client, &msg);
            break;
            
        case MSG_REMATCH_RESPONSE:
            handle_rematch_response(server, client, &msg);
            break;

        case MSG_GET_HISTORY:
            handle_get_history(server, client);
            break;
            
        // === Heartbeat ===
        case MSG_HEARTBEAT:
            send_message(client, MSG_HEARTBEAT_ACK, NULL);
            break;
            
default:
            printf("[SERVER] Unknown message type: %d from socket %d\n", msg.type, client->socket_fd);
            send_error(client, "Unknown message type");
            break;
    }
}

void server_disconnect_client(GameServer* server, ConnectedClient* client) {
    if (!client->is_connected) return;
    
    printf("[SERVER] Client disconnecting: socket %d", client->socket_fd);
    if (client->user_id > 0) {
        printf(" (user: %s)", client->username);
        
        // Clean up user state
        db_set_player_offline(&server->db, client->user_id);
        db_delete_session(&server->db, client->session_id);
    }
    printf("\n");
    
    close(client->socket_fd);
    client->is_connected = 0;
    client->socket_fd = -1;
    
    // Remove from clients array
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i] == client) {
            free(client);
            // Shift remaining clients
            for (int j = i; j < server->client_count - 1; j++) {
                server->clients[j] = server->clients[j + 1];
            }
            server->client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    printf("[SERVER] Clients remaining: %d\n", server->client_count);
}

ConnectedClient* find_client_by_id(GameServer* server, int user_id) {
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i]->user_id == user_id && server->clients[i]->is_connected) {
            return server->clients[i];
        }
    }
    return NULL;
}

ConnectedClient* find_client_by_socket(GameServer* server, int socket_fd) {
    for (int i = 0; i < server->client_count; i++) {
        if (server->clients[i]->socket_fd == socket_fd) {
            return server->clients[i];
        }
    }
    return NULL;
}

// Check for timed out clients
static void check_client_timeouts(GameServer* server) {
    time_t now = time(NULL);
    
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = server->client_count - 1; i >= 0; i--) {
        ConnectedClient* client = server->clients[i];
        if (client->is_connected && 
            (now - client->last_heartbeat) > HEARTBEAT_TIMEOUT) {
            printf("[SERVER] Client timeout: socket %d\n", client->socket_fd);
            pthread_mutex_unlock(&server->clients_mutex);
            server_disconnect_client(server, client);
            pthread_mutex_lock(&server->clients_mutex);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

void server_run(GameServer* server) {
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
    
    printf("[SERVER] Starting main loop...\n");
    
    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->server_socket, &read_fds);
        max_fd = server->server_socket;
        
        // Add all client sockets
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < server->client_count; i++) {
            int sock = server->clients[i]->socket_fd;
            if (sock > 0) {
                FD_SET(sock, &read_fds);
                if (sock > max_fd) max_fd = sock;
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        // Set timeout for heartbeat checking (1 second)
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (server->running) {
                perror("select");
            }
            break;
        }
        
        // New connection
        if (FD_ISSET(server->server_socket, &read_fds)) {
            server_accept_connection(server);
        }
        
        // Client activity
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < server->client_count; i++) {
            ConnectedClient* client = server->clients[i];
            if (client->socket_fd > 0 && FD_ISSET(client->socket_fd, &read_fds)) {
                pthread_mutex_unlock(&server->clients_mutex);
                server_handle_message(server, client);
                pthread_mutex_lock(&server->clients_mutex);
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        // Periodically check for timeouts
        check_client_timeouts(server);
        
        // Periodically try to match searching players
        static time_t last_matchmaking = 0;
        time_t now = time(NULL);
        if (now - last_matchmaking >= 2) {  // Every 2 seconds
            matchmaking_try_match_players(server);
            last_matchmaking = now;
        }
    }
}

void server_shutdown(GameServer* server) {
    printf("[SERVER] Shutting down...\n");
    
    server->running = 0;
    
    // Disconnect all clients
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = server->client_count - 1; i >= 0; i--) {
        ConnectedClient* client = server->clients[i];
        if (client->is_connected) {
            send_error(client, "Server shutting down");
            close(client->socket_fd);
        }
        free(client);
    }
    server->client_count = 0;
    pthread_mutex_unlock(&server->clients_mutex);
    
    // Close database
    db_close(&server->db);
    
    // Close server socket
    close(server->server_socket);
    
    // Destroy mutex
    pthread_mutex_destroy(&server->clients_mutex);
    
    printf("[SERVER] Shutdown complete\n");
}

int main(int argc, char* argv[]) {
    int port = 8888;
    const char* db_file = "monopoly.db";
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            db_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-p port] [-d database]\n", argv[0]);
            printf("  -p port      Server port (default: 8888)\n");
            printf("  -d database  SQLite database file (default: monopoly.db)\n");
            return 0;
        }
    }
    
    GameServer server;
    
    if (server_init(&server, port, db_file) < 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    server_run(&server);
    server_shutdown(&server);
    
    return 0;
}

// ============ Game Action Handlers ============

// Helper to get player index in game (0 or 1)
static int get_player_index(ActiveGame* game, int user_id) {
    if (game->players[0].user_id == user_id) return 0;
    if (game->players[1].user_id == user_id) return 1;
    return -1;
}

// Broadcast game state to both players
static void broadcast_game_state(GameServer* server, ActiveGame* game) {
    if (!game) return;
    
    char* state_json = game_serialize_state(game);
    if (!state_json) return;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    for (int i = 0; i < 2; i++) {
        ConnectedClient* client = find_client_by_id(server, game->players[i].user_id);
        if (client && client->is_connected) {
            send_message(client, MSG_GAME_STATE, state_json);
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
    
    free(state_json);
    
    // Check if game ended
    if (game->state == GSTATE_ENDED) {
        int winner_id = game_get_winner(game);
        int loser_id = game_get_loser(game);
        int match_id = game->match_id;
        
        if (winner_id > 0 && loser_id > 0) {
            handle_game_end(server, match_id, winner_id, loser_id, "bankruptcy");
            game_destroy(match_id);
        }
    }
}

static void handle_roll_dice(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    if (game->current_player != player_idx) {
        send_message(client, MSG_NOT_YOUR_TURN, "{\"error\":\"Not your turn\"}");
        return;
    }
    
    printf("[GAME] %s rolling dice in match %d\n", client->username, game->match_id);
    
    if (game_roll_dice(game, player_idx) == 0) {
        printf("[GAME] Dice: %d + %d, position: %d\n", 
               game->last_roll[0], game->last_roll[1], 
               game->players[player_idx].position);
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot roll now\"}");
    }
}

static void handle_buy_property(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0 || game->current_player != player_idx) {
        send_message(client, MSG_NOT_YOUR_TURN, "{\"error\":\"Not your turn\"}");
        return;
    }
    
    printf("[GAME] %s buying property in match %d\n", client->username, game->match_id);
    
    if (game_buy_property(game, player_idx) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot buy now\"}");
    }
}

static void handle_skip_property(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0 || game->current_player != player_idx) {
        send_message(client, MSG_NOT_YOUR_TURN, "{\"error\":\"Not your turn\"}");
        return;
    }
    
    printf("[GAME] %s skipping property in match %d\n", client->username, game->match_id);
    
    if (game_skip_property(game, player_idx) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot skip now\"}");
    }
}

static void handle_upgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    // Parse property ID
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* prop_item = cJSON_GetObjectItem(json, "property_id");
    int prop_id = prop_item ? prop_item->valueint : -1;
    cJSON_Delete(json);
    
    if (game_upgrade_property(game, player_idx, prop_id) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot upgrade\"}");
    }
}

static void handle_downgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* prop_item = cJSON_GetObjectItem(json, "property_id");
    int prop_id = prop_item ? prop_item->valueint : -1;
    cJSON_Delete(json);
    
    if (game_downgrade_property(game, player_idx, prop_id) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot downgrade\"}");
    }
}

static void handle_mortgage_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid request");
        return;
    }
    
    cJSON* prop_item = cJSON_GetObjectItem(json, "property_id");
    int prop_id = prop_item ? prop_item->valueint : -1;
    cJSON_Delete(json);
    
    if (game_mortgage_property(game, player_idx, prop_id) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot mortgage\"}");
    }
}

static void handle_pay_jail_fine(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0 || game->current_player != player_idx) {
        send_message(client, MSG_NOT_YOUR_TURN, "{\"error\":\"Not your turn\"}");
        return;
    }
    
    printf("[GAME] %s paying jail fine in match %d\n", client->username, game->match_id);
    
    if (game_pay_jail_fine(game, player_idx) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot pay fine\"}");
    }
}

static void handle_pause_game(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    printf("[GAME] %s pausing match %d\n", client->username, game->match_id);
    
    if (game_pause(game, player_idx) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot pause now\"}");
    }
}

static void handle_resume_game(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    printf("[GAME] %s resuming match %d\n", client->username, game->match_id);
    
    if (game_resume(game, player_idx) == 0) {
        broadcast_game_state(server, game);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Only the player who paused can resume\"}");
    }
}

static void handle_surrender_game(GameServer* server, ConnectedClient* client) {
    if (!client->user_id || client->status != PLAYER_IN_GAME) {
        send_error(client, "Not in a game");
        return;
    }
    
    ActiveGame* game = game_find_by_player(client->user_id);
    if (!game) {
        send_error(client, "Game not found");
        return;
    }
    
    int player_idx = get_player_index(game, client->user_id);
    if (player_idx < 0) {
        send_error(client, "Not a player in this game");
        return;
    }
    
    printf("[GAME] %s surrendered in match %d\n", client->username, game->match_id);
    
    if (game_surrender(game, player_idx) == 0) {
        // Game ended - send result to both players
        int winner_id = game->players[1 - player_idx].user_id;
        int loser_id = game->players[player_idx].user_id;
        int match_id = game->match_id;
        
        handle_game_end(server, match_id, winner_id, loser_id, "surrender");
        game_destroy(match_id);
    } else {
        send_message(client, MSG_INVALID_MOVE, "{\"error\":\"Cannot surrender\"}");
    }
}

static void handle_get_history(GameServer* server, ConnectedClient* client) {
    MatchHistoryEntry* history = NULL;
    int count = 0;
    
    if (db_get_user_match_history(&server->db, client->user_id, &history, &count) != 0) {
        send_error(client, "Failed to fetch history");
        return;
    }
    
    // Create JSON response
    cJSON* root = cJSON_CreateArray();
    
    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "match_id", history[i].match_id);
        cJSON_AddNumberToObject(item, "opponent_id", history[i].opponent_id);
        cJSON_AddStringToObject(item, "opponent_name", history[i].opponent_name);
        cJSON_AddNumberToObject(item, "is_win", history[i].is_win);
        cJSON_AddNumberToObject(item, "elo_change", history[i].elo_change);
        cJSON_AddStringToObject(item, "timestamp", history[i].timestamp);
        cJSON_AddItemToArray(root, item);
    }
    
    if (history) {
        free(history);
    }
    
    char* json_str = cJSON_PrintUnformatted(root);
    
    if (json_str) {
        if (strlen(json_str) < MSG_MAX_PAYLOAD) {
            send_message(client, MSG_HISTORY_LIST, json_str);
        } else {
             send_error(client, "History too large");
        }
        free(json_str);
    } else {
         send_message(client, MSG_HISTORY_LIST, "[]");
    }
    
    cJSON_Delete(root);
}