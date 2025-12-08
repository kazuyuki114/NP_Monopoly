#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <time.h>
#include "../shared/protocol.h"
#include "database.h"

#define MAX_CLIENTS 100
#define MAX_MATCHES 50
#define SESSION_ID_LENGTH 64
#define HEARTBEAT_TIMEOUT 60  // seconds

// Player status
typedef enum {
    PLAYER_DISCONNECTED = 0,
    PLAYER_IDLE,
    PLAYER_SEARCHING,
    PLAYER_IN_GAME
} PlayerStatus;

// Connected client structure
typedef struct {
    int socket_fd;
    int user_id;
    char username[50];
    char session_id[SESSION_ID_LENGTH + 1];
    int elo_rating;
    PlayerStatus status;
    int current_match_id;
    time_t last_heartbeat;
    int is_connected;
} ConnectedClient;

// Main server structure
typedef struct {
    int server_socket;
    int running;
    int port;
    
    ConnectedClient* clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t clients_mutex;
    
    Database db;
} GameServer;

// ============ Server Core ============

// Initialize server on given port
int server_init(GameServer* server, int port, const char* db_file);

// Main server loop (blocking)
void server_run(GameServer* server);

// Shutdown server gracefully
void server_shutdown(GameServer* server);

// ============ Connection Handling ============

// Accept new client connection
void server_accept_connection(GameServer* server);

// Handle incoming message from client
void server_handle_message(GameServer* server, ConnectedClient* client);

// Disconnect a client
void server_disconnect_client(GameServer* server, ConnectedClient* client);

// ============ Message Sending ============

// Send a message to a specific client
int send_message(ConnectedClient* client, MessageType type, const char* payload);

// Send error message
int send_error(ConnectedClient* client, const char* error_msg);

// Send success message
int send_success(ConnectedClient* client, const char* msg);

// ============ Authentication Handlers ============

void handle_register(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_login(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_logout(GameServer* server, ConnectedClient* client);

// ============ Utility Functions ============

// Find client by user_id
ConnectedClient* find_client_by_id(GameServer* server, int user_id);

// Find client by socket fd
ConnectedClient* find_client_by_socket(GameServer* server, int socket_fd);

// Generate a random session ID
void generate_session_id(char* output);

// Simple password hashing (SHA256)
void hash_password(const char* password, char* output);

#endif // SERVER_H

