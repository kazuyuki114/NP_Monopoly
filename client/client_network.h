#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include "../shared/protocol.h"

// Client connection state
typedef struct {
    int socket_fd;
    int connected;
    int logged_in;
    
    // User info (set after login)
    int user_id;
    char username[50];
    char session_id[65];
    int elo_rating;
    int total_matches;
    int wins;
    int losses;
    
    // Game state
    int in_game;
    int current_match_id;
} ClientState;

// ============ Connection ============

// Initialize client state
void client_init(ClientState* state);

// Connect to server
// Returns 0 on success, -1 on error
int client_connect(ClientState* state, const char* server_ip, int port);

// Disconnect from server
void client_disconnect(ClientState* state);

// Check if connected
int client_is_connected(ClientState* state);

// ============ Communication ============

// Send a message to server
// Returns 0 on success, -1 on error
int client_send(ClientState* state, MessageType type, const char* payload);

// Receive a message from server (blocking)
// Returns 0 on success, -1 on error
int client_receive(ClientState* state, NetworkMessage* msg);

// Check if data is available to read (non-blocking)
// Returns 1 if data available, 0 if not, -1 on error
int client_data_available(ClientState* state);

// ============ Authentication ============

// Register a new account
// Returns 0 on success, -1 on error
int client_register(ClientState* state, const char* username, const char* password, const char* email);

// Login to existing account
// Returns 0 on success, -1 on error
int client_login(ClientState* state, const char* username, const char* password);

// Logout
int client_logout(ClientState* state);

// ============ Utility ============

// Send heartbeat
int client_send_heartbeat(ClientState* state);

// Parse login response and update state
int client_parse_login_response(ClientState* state, const char* payload);

// Get error message from response
const char* client_get_error(const char* payload);

#endif // CLIENT_NETWORK_H

