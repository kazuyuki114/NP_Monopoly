# Monopoly Network Multiplayer Implementation Plan

## Project Overview
Transform the local 2-player monopoly game into a networked multiplayer system with user management, matchmaking, ELO rating, and complete game synchronization.

---

## Current State Analysis

### Existing Components
- **Game.c/Game.h**: Complete monopoly game logic (829 lines)
  - Game states: BEGIN_MOVE, BUY_PROPERTY, PLAYER_IN_DEBT, END
  - Player management (2 players, positions, money, jail status)
  - Property management (40 properties with ownership, upgrades, mortgages)
  - Dice rolling, movement, rent calculation
  - Jail mechanics, bankruptcy handling
  
- **Player.h**: Player structure (id, name, money, jailed, position)
- **Property.h**: Property structure (type, owner, mortgaged, upgrades, price, rent)
- **Render.c**: SDL-based rendering (visual interface)

### What We Need to Build
1. Server with TCP socket handling
2. Database for user accounts and game records
3. Authentication system
4. Matchmaking engine
5. Network protocol for game synchronization
6. Client modifications for network play
7. ELO rating system
8. Match history and statistics

---

## PHASE 1: DATABASE DESIGN & SETUP

### Step 1.1: Design Database Schema

**Technology**: SQLite3 (lightweight, serverless, perfect for this project)

Create `database_schema.sql`:

```sql
-- Users table
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(100) UNIQUE,
    elo_rating INTEGER DEFAULT 1200,
    total_matches INTEGER DEFAULT 0,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP
);

-- Sessions table (for login management)
CREATE TABLE sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id INTEGER NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP,
    is_active BOOLEAN DEFAULT 1,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Online players table (for matchmaking)
CREATE TABLE online_players (
    user_id INTEGER PRIMARY KEY,
    status VARCHAR(20), -- 'idle', 'searching', 'in_game'
    current_game_id INTEGER,
    last_heartbeat TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Matches table
CREATE TABLE matches (
    match_id INTEGER PRIMARY KEY AUTOINCREMENT,
    player1_id INTEGER NOT NULL,
    player2_id INTEGER NOT NULL,
    winner_id INTEGER,
    player1_elo_before INTEGER,
    player2_elo_before INTEGER,
    player1_elo_after INTEGER,
    player2_elo_after INTEGER,
    start_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time TIMESTAMP,
    status VARCHAR(20), -- 'ongoing', 'completed', 'abandoned'
    FOREIGN KEY (player1_id) REFERENCES users(user_id),
    FOREIGN KEY (player2_id) REFERENCES users(user_id),
    FOREIGN KEY (winner_id) REFERENCES users(user_id)
);

-- Game moves log (for replay and validation)
CREATE TABLE game_moves (
    move_id INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id INTEGER NOT NULL,
    player_id INTEGER NOT NULL,
    move_number INTEGER,
    move_type VARCHAR(50), -- 'roll', 'buy', 'upgrade', 'mortgage', etc.
    move_data TEXT, -- JSON data of the move
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (match_id) REFERENCES matches(match_id),
    FOREIGN KEY (player_id) REFERENCES users(user_id)
);

-- Challenge requests table
CREATE TABLE challenge_requests (
    challenge_id INTEGER PRIMARY KEY AUTOINCREMENT,
    challenger_id INTEGER NOT NULL,
    challenged_id INTEGER NOT NULL,
    status VARCHAR(20), -- 'pending', 'accepted', 'declined', 'expired'
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    responded_at TIMESTAMP,
    FOREIGN KEY (challenger_id) REFERENCES users(user_id),
    FOREIGN KEY (challenged_id) REFERENCES users(user_id)
);

-- Create indexes for performance
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_sessions_user ON sessions(user_id);
CREATE INDEX idx_matches_players ON matches(player1_id, player2_id);
CREATE INDEX idx_game_moves_match ON game_moves(match_id);
```

### Step 1.2: Database Interface Implementation

Create `server/database.h`:
```c
#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <pthread.h>

typedef struct {
    sqlite3* db;
    pthread_mutex_t mutex;
} Database;

// Initialization
int db_init(Database* db, const char* filename);
void db_close(Database* db);

// User operations
int db_create_user(Database* db, const char* username, const char* password_hash, const char* email);
int db_get_user(Database* db, const char* username, int* user_id, char* password_hash, int* elo);
int db_update_user_elo(Database* db, int user_id, int new_elo);
int db_update_user_stats(Database* db, int user_id, int wins, int losses);

// Session operations
int db_create_session(Database* db, int user_id, const char* session_id);
int db_validate_session(Database* db, const char* session_id, int* user_id);
int db_delete_session(Database* db, const char* session_id);

// Match operations
int db_create_match(Database* db, int player1_id, int player2_id, int p1_elo, int p2_elo);
int db_update_match_result(Database* db, int match_id, int winner_id, int p1_elo_after, int p2_elo_after);
int db_log_move(Database* db, int match_id, int player_id, int move_num, const char* move_type, const char* move_data);

// Online players
int db_set_player_online(Database* db, int user_id, const char* status);
int db_set_player_offline(Database* db, int user_id);
int db_get_online_players(Database* db, int** user_ids, int* count);

// Challenge operations
int db_create_challenge(Database* db, int challenger_id, int challenged_id);
int db_respond_challenge(Database* db, int challenge_id, const char* status);
int db_get_pending_challenges(Database* db, int user_id, int** challenge_ids, int* count);

#endif
```

---

## PHASE 2: PROTOCOL DESIGN

### Step 2.1: Define Message Protocol

Create `shared/protocol.h`:

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Message types
typedef enum {
    // Authentication (1-9)
    MSG_REGISTER = 1,
    MSG_REGISTER_RESPONSE = 2,
    MSG_LOGIN = 3,
    MSG_LOGIN_RESPONSE = 4,
    MSG_LOGOUT = 5,
    
    // Lobby & Matchmaking (10-19)
    MSG_GET_ONLINE_PLAYERS = 10,
    MSG_ONLINE_PLAYERS_LIST = 11,
    MSG_SEARCH_MATCH = 12,
    MSG_MATCH_FOUND = 13,
    MSG_CANCEL_SEARCH = 14,
    MSG_SEND_CHALLENGE = 15,
    MSG_CHALLENGE_REQUEST = 16,
    MSG_ACCEPT_CHALLENGE = 17,
    MSG_DECLINE_CHALLENGE = 18,
    
    // Game Actions (20-29)
    MSG_GAME_START = 20,
    MSG_GAME_STATE = 21,
    MSG_ROLL_DICE = 22,
    MSG_BUY_PROPERTY = 23,
    MSG_SKIP_PROPERTY = 24,
    MSG_UPGRADE_PROPERTY = 25,
    MSG_DOWNGRADE_PROPERTY = 26,
    MSG_MORTGAGE_PROPERTY = 27,
    MSG_PAY_JAIL_FINE = 28,
    MSG_DECLARE_BANKRUPT = 29,
    
    // Game End & Rematch (30-39)
    MSG_GAME_END = 30,
    MSG_GAME_RESULT = 31,
    MSG_REMATCH_REQUEST = 32,
    MSG_REMATCH_RESPONSE = 33,
    MSG_REMATCH_CANCELLED = 34,
    
    // Responses & Errors (100+)
    MSG_SUCCESS = 100,
    MSG_ERROR = 101,
    MSG_INVALID_MOVE = 102,
    MSG_NOT_YOUR_TURN = 103,
    MSG_HEARTBEAT = 104,
    MSG_HEARTBEAT_ACK = 105
} MessageType;

// Message structure
typedef struct {
    uint32_t type;           // MessageType
    uint32_t sender_id;      // User ID of sender
    uint32_t target_id;      // User ID of target (0 for server)
    uint32_t payload_length; // Length of payload
    char payload[4096];      // JSON payload
} NetworkMessage;

// Serialization functions
int serialize_message(const NetworkMessage* msg, char* buffer, int buffer_size);
int deserialize_message(NetworkMessage* msg, const char* buffer, int buffer_size);

#endif
```

### Step 2.2: Message Format Examples

```json
// Login Request
{
    "type": "MSG_LOGIN",
    "username": "player1",
    "password": "hashed_password"
}

// Login Response
{
    "type": "MSG_LOGIN_RESPONSE",
    "success": true,
    "user_id": 123,
    "username": "player1",
    "elo_rating": 1350,
    "session_id": "abc123def456..."
}

// Game State Update
{
    "type": "MSG_GAME_STATE",
    "match_id": 42,
    "current_player": 0,
    "game_state": "BEGIN_MOVE",
    "players": [
        {
            "id": 1,
            "name": "player1",
            "money": 1350,
            "position": 5,
            "jailed": false
        },
        {
            "id": 2,
            "name": "player2",
            "money": 1500,
            "position": 0,
            "jailed": false
        }
    ],
    "properties": [
        {"id": 1, "owner": 1, "upgrades": 0, "mortgaged": false},
        // ... other properties
    ],
    "last_roll": [3, 4],
    "message": "Player1's turn"
}

// Roll Dice Request
{
    "type": "MSG_ROLL_DICE",
    "player_id": 1,
    "match_id": 42
}

// Buy Property Request
{
    "type": "MSG_BUY_PROPERTY",
    "player_id": 1,
    "match_id": 42,
    "property_id": 5
}

// Upgrade Property Request
{
    "type": "MSG_UPGRADE_PROPERTY",
    "player_id": 1,
    "match_id": 42,
    "property_id": 6
}

// Game End
{
    "type": "MSG_GAME_END",
    "match_id": 42,
    "winner_id": 1,
    "winner_name": "player1",
    "loser_id": 2,
    "loser_name": "player2",
    "winner_elo_change": +15,
    "loser_elo_change": -15,
    "winner_new_elo": 1365,
    "loser_new_elo": 1335
}
```

---

## PHASE 3: ARCHITECTURE DESIGN

### System Architecture

```
┌─────────────────┐         ┌──────────────────┐         ┌──────────────┐
│   Client 1      │◄───────►│                  │◄───────►│   Database   │
│   (SDL UI)      │  TCP/IP │   Game Server    │         │   (SQLite)   │
│                 │         │                  │         └──────────────┘
│  - Network      │         │  - Auth Module   │
│  - Lobby UI     │         │  - Matchmaking   │
│  - Game Render  │         │  - Game Manager  │
└─────────────────┘         │  - ELO System    │
                            │  - Validator     │
┌─────────────────┐         │  - Logger        │
│   Client 2      │◄───────►│                  │
│   (SDL UI)      │  TCP/IP │  Uses epoll/     │
│                 │         │  select for      │
│  - Network      │         │  concurrency     │
│  - Lobby UI     │         │                  │
│  - Game Render  │         │                  │
└─────────────────┘         └──────────────────┘
```

### Component Breakdown

#### Server Components:
1. **Main Server Loop**: Accept connections, route messages
2. **Authentication Manager**: Handle register/login/logout
3. **Session Manager**: Maintain active sessions
4. **Matchmaking Engine**: Find opponents based on ELO
5. **Game State Manager**: Maintain all active games
6. **Move Validator**: Validate all game moves server-side
7. **ELO Calculator**: Calculate rating changes
8. **Database Interface**: All DB operations
9. **Logger**: Log all game moves

#### Client Components:
1. **Network Layer**: Socket communication
2. **Lobby UI**: Login, player list, matchmaking
3. **Game Renderer**: Existing SDL rendering
4. **Game State**: Mirror server state for display
5. **Input Handler**: Send moves to server

---

## PHASE 4: SERVER IMPLEMENTATION

### Step 4.1: Server Structure Definitions

Create `server/server.h`:

```c
#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include "../shared/protocol.h"
#include "database.h"
#include "../Player.h"
#include "../Property.h"

#define MAX_CLIENTS 100
#define MAX_MATCHES 50

// Player status
typedef enum {
    PLAYER_IDLE,
    PLAYER_SEARCHING,
    PLAYER_IN_GAME
} PlayerStatus;

// Connected client
typedef struct {
    int socket_fd;
    int user_id;
    char username[50];
    char session_id[65];
    int elo_rating;
    PlayerStatus status;
    int current_match_id;
    time_t last_heartbeat;
} ConnectedClient;

// Game state (copied from Game.c)
typedef enum {
    Game_STATE_BEGIN_MOVE,
    Game_STATE_BUY_PROPERTY,
    Game_STATE_PLAYER_IN_DEBT,
    Game_STATE_END
} Game_State;

// Active match
typedef struct {
    int match_id;
    int player1_id;
    int player2_id;
    int player1_socket;
    int player2_socket;
    
    // Game state
    Game_Player players[2];
    Game_Prop properties[40];
    Game_State state;
    int current_player;
    int lastRoll[2];
    int turnsInJail[2];
    int consecutiveDoubles[2];
    int justLeftJail;
    int selectedProperty;
    char message[256];
    char message2[256];
    
    pthread_mutex_t match_mutex;
} ActiveMatch;

// Main server structure
typedef struct {
    int server_socket;
    int running;
    
    ConnectedClient* clients[MAX_CLIENTS];
    int client_count;
    pthread_mutex_t clients_mutex;
    
    ActiveMatch* matches[MAX_MATCHES];
    int match_count;
    pthread_mutex_t matches_mutex;
    
    Database db;
} GameServer;

// Server functions
int server_init(GameServer* server, int port);
void server_run(GameServer* server);
void server_shutdown(GameServer* server);

// Client handling
void handle_new_connection(GameServer* server);
void handle_client_message(GameServer* server, ConnectedClient* client);
void disconnect_client(GameServer* server, ConnectedClient* client);

// Message handlers
void handle_register(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_login(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_logout(GameServer* server, ConnectedClient* client);
void handle_search_match(GameServer* server, ConnectedClient* client);
void handle_send_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_accept_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_decline_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Game handlers
void handle_roll_dice(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_buy_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_skip_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_upgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_mortgage_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_pay_jail_fine(GameServer* server, ConnectedClient* client, NetworkMessage* msg);
void handle_declare_bankrupt(GameServer* server, ConnectedClient* client, NetworkMessage* msg);

// Utility functions
ConnectedClient* find_client_by_id(GameServer* server, int user_id);
ConnectedClient* find_client_by_socket(GameServer* server, int socket_fd);
ActiveMatch* find_match_by_id(GameServer* server, int match_id);
ActiveMatch* find_match_by_player(GameServer* server, int user_id);

#endif
```

### Step 4.2: Server Main Loop

Create `server/server_main.c`:

```c
#include "server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int server_init(GameServer* server, int port) {
    // Initialize server structure
    memset(server, 0, sizeof(GameServer));
    server->running = 1;
    server->client_count = 0;
    server->match_count = 0;
    
    // Initialize mutexes
    pthread_mutex_init(&server->clients_mutex, NULL);
    pthread_mutex_init(&server->matches_mutex, NULL);
    
    // Initialize database
    if (db_init(&server->db, "monopoly.db") != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return -1;
    }
    
    // Create TCP socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        perror("socket");
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server->server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server->server_socket);
        return -1;
    }
    
    // Listen
    if (listen(server->server_socket, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server->server_socket);
        return -1;
    }
    
    printf("Server listening on port %d\n", port);
    return 0;
}

void server_run(GameServer* server) {
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
    
    while (server->running) {
        FD_ZERO(&read_fds);
        FD_SET(server->server_socket, &read_fds);
        max_fd = server->server_socket;
        
        // Add all client sockets
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < server->client_count; i++) {
            int sock = server->clients[i]->socket_fd;
            FD_SET(sock, &read_fds);
            if (sock > max_fd) max_fd = sock;
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        // Set timeout for heartbeat checking
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select");
            break;
        }
        
        // New connection
        if (FD_ISSET(server->server_socket, &read_fds)) {
            handle_new_connection(server);
        }
        
        // Client activity
        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < server->client_count; i++) {
            int sock = server->clients[i]->socket_fd;
            if (FD_ISSET(sock, &read_fds)) {
                handle_client_message(server, server->clients[i]);
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
        // Check for timeouts (heartbeat mechanism)
        check_client_timeouts(server);
    }
}

void handle_new_connection(GameServer* server) {
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
        printf("Max clients reached, rejecting connection\n");
        close(client_sock);
        pthread_mutex_unlock(&server->clients_mutex);
        return;
    }
    
    // Create new client
    ConnectedClient* client = malloc(sizeof(ConnectedClient));
    memset(client, 0, sizeof(ConnectedClient));
    client->socket_fd = client_sock;
    client->user_id = -1; // Not logged in yet
    client->status = PLAYER_IDLE;
    client->last_heartbeat = time(NULL);
    
    server->clients[server->client_count++] = client;
    
    pthread_mutex_unlock(&server->clients_mutex);
    
    printf("New connection from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
}

void handle_client_message(GameServer* server, ConnectedClient* client) {
    NetworkMessage msg;
    char buffer[8192];
    
    int bytes = recv(client->socket_fd, buffer, sizeof(buffer), 0);
    
    if (bytes <= 0) {
        // Client disconnected
        disconnect_client(server, client);
        return;
    }
    
    // Deserialize message
    if (deserialize_message(&msg, buffer, bytes) < 0) {
        fprintf(stderr, "Failed to deserialize message\n");
        return;
    }
    
    // Update heartbeat
    client->last_heartbeat = time(NULL);
    
    // Route message based on type
    switch (msg.type) {
        case MSG_REGISTER:
            handle_register(server, client, &msg);
            break;
        case MSG_LOGIN:
            handle_login(server, client, &msg);
            break;
        case MSG_LOGOUT:
            handle_logout(server, client);
            break;
        case MSG_GET_ONLINE_PLAYERS:
            handle_get_online_players(server, client);
            break;
        case MSG_SEARCH_MATCH:
            handle_search_match(server, client);
            break;
        case MSG_SEND_CHALLENGE:
            handle_send_challenge(server, client, &msg);
            break;
        case MSG_ACCEPT_CHALLENGE:
            handle_accept_challenge(server, client, &msg);
            break;
        case MSG_DECLINE_CHALLENGE:
            handle_decline_challenge(server, client, &msg);
            break;
        case MSG_ROLL_DICE:
            handle_roll_dice(server, client, &msg);
            break;
        case MSG_BUY_PROPERTY:
            handle_buy_property(server, client, &msg);
            break;
        case MSG_SKIP_PROPERTY:
            handle_skip_property(server, client, &msg);
            break;
        case MSG_UPGRADE_PROPERTY:
            handle_upgrade_property(server, client, &msg);
            break;
        case MSG_MORTGAGE_PROPERTY:
            handle_mortgage_property(server, client, &msg);
            break;
        case MSG_PAY_JAIL_FINE:
            handle_pay_jail_fine(server, client, &msg);
            break;
        case MSG_DECLARE_BANKRUPT:
            handle_declare_bankrupt(server, client, &msg);
            break;
        case MSG_HEARTBEAT:
            send_heartbeat_ack(client);
            break;
        default:
            fprintf(stderr, "Unknown message type: %d\n", msg.type);
            break;
    }
}

void server_shutdown(GameServer* server) {
    printf("Shutting down server...\n");
    
    server->running = 0;
    
    // Disconnect all clients
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < server->client_count; i++) {
        close(server->clients[i]->socket_fd);
        free(server->clients[i]);
    }
    pthread_mutex_unlock(&server->clients_mutex);
    
    // Clean up matches
    pthread_mutex_lock(&server->matches_mutex);
    for (int i = 0; i < server->match_count; i++) {
        pthread_mutex_destroy(&server->matches[i]->match_mutex);
        free(server->matches[i]);
    }
    pthread_mutex_unlock(&server->matches_mutex);
    
    // Close database
    db_close(&server->db);
    
    // Close server socket
    close(server->server_socket);
    
    // Destroy mutexes
    pthread_mutex_destroy(&server->clients_mutex);
    pthread_mutex_destroy(&server->matches_mutex);
    
    printf("Server shutdown complete\n");
}

int main(int argc, char* argv[]) {
    int port = 8888;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    GameServer server;
    
    if (server_init(&server, port) < 0) {
        return 1;
    }
    
    printf("Monopoly Server started on port %d\n", port);
    printf("Press Ctrl+C to stop\n");
    
    server_run(&server);
    server_shutdown(&server);
    
    return 0;
}
```

### Step 4.3: Authentication Implementation

Create `server/auth.c`:

```c
#include "server.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <cjson/cJSON.h>

// Simple password hashing (use bcrypt in production!)
void hash_password(const char* password, char* output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[SHA256_DIGEST_LENGTH * 2] = '\0';
}

void generate_session_id(char* output) {
    // Generate random session ID
    for (int i = 0; i < 64; i++) {
        output[i] = "0123456789abcdef"[rand() % 16];
    }
    output[64] = '\0';
}

void handle_register(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON");
        return;
    }
    
    cJSON* username_item = cJSON_GetObjectItem(json, "username");
    cJSON* password_item = cJSON_GetObjectItem(json, "password");
    cJSON* email_item = cJSON_GetObjectItem(json, "email");
    
    if (!username_item || !password_item) {
        send_error(client, "Missing username or password");
        cJSON_Delete(json);
        return;
    }
    
    const char* username = username_item->valuestring;
    const char* password = password_item->valuestring;
    const char* email = email_item ? email_item->valuestring : NULL;
    
    // Hash password
    char password_hash[256];
    hash_password(password, password_hash);
    
    // Create user in database
    int result = db_create_user(&server->db, username, password_hash, email);
    
    if (result == 0) {
        // Success
        cJSON* response = cJSON_CreateObject();
        cJSON_AddNumberToObject(response, "type", MSG_REGISTER_RESPONSE);
        cJSON_AddBoolToObject(response, "success", 1);
        cJSON_AddStringToObject(response, "message", "Registration successful");
        
        char* response_str = cJSON_Print(response);
        send_message(client, MSG_REGISTER_RESPONSE, response_str);
        
        free(response_str);
        cJSON_Delete(response);
    } else {
        send_error(client, "Username already exists");
    }
    
    cJSON_Delete(json);
}

void handle_login(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON");
        return;
    }
    
    cJSON* username_item = cJSON_GetObjectItem(json, "username");
    cJSON* password_item = cJSON_GetObjectItem(json, "password");
    
    if (!username_item || !password_item) {
        send_error(client, "Missing username or password");
        cJSON_Delete(json);
        return;
    }
    
    const char* username = username_item->valuestring;
    const char* password = password_item->valuestring;
    
    // Hash password
    char password_hash[256];
    hash_password(password, password_hash);
    
    // Get user from database
    int user_id;
    char stored_hash[256];
    int elo;
    
    int result = db_get_user(&server->db, username, &user_id, stored_hash, &elo);
    
    if (result == 0 && strcmp(password_hash, stored_hash) == 0) {
        // Login successful
        generate_session_id(client->session_id);
        client->user_id = user_id;
        client->elo_rating = elo;
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->status = PLAYER_IDLE;
        
        // Create session in database
        db_create_session(&server->db, user_id, client->session_id);
        
        // Mark player as online
        db_set_player_online(&server->db, user_id, "idle");
        
        // Send response
        cJSON* response = cJSON_CreateObject();
        cJSON_AddNumberToObject(response, "type", MSG_LOGIN_RESPONSE);
        cJSON_AddBoolToObject(response, "success", 1);
        cJSON_AddNumberToObject(response, "user_id", user_id);
        cJSON_AddStringToObject(response, "username", username);
        cJSON_AddNumberToObject(response, "elo_rating", elo);
        cJSON_AddStringToObject(response, "session_id", client->session_id);
        
        char* response_str = cJSON_Print(response);
        send_message(client, MSG_LOGIN_RESPONSE, response_str);
        
        free(response_str);
        cJSON_Delete(response);
        
        printf("User %s logged in (ELO: %d)\n", username, elo);
    } else {
        send_error(client, "Invalid username or password");
    }
    
    cJSON_Delete(json);
}

void handle_logout(GameServer* server, ConnectedClient* client) {
    if (client->user_id > 0) {
        // Delete session
        db_delete_session(&server->db, client->session_id);
        
        // Mark player as offline
        db_set_player_offline(&server->db, client->user_id);
        
        printf("User %s logged out\n", client->username);
        
        // Reset client state
        client->user_id = -1;
        client->session_id[0] = '\0';
        client->status = PLAYER_IDLE;
    }
}
```

### Step 4.4: Matchmaking Implementation

Create `server/matchmaking.c`:

```c
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <cjson/cJSON.h>

void handle_search_match(GameServer* server, ConnectedClient* client) {
    if (client->status != PLAYER_IDLE) {
        send_error(client, "Already in game or searching");
        return;
    }
    
    // Mark player as searching
    client->status = PLAYER_SEARCHING;
    db_set_player_online(&server->db, client->user_id, "searching");
    
    printf("User %s searching for match (ELO: %d)\n", client->username, client->elo_rating);
    
    // Find opponent with similar ELO (within ±100 points)
    ConnectedClient* opponent = NULL;
    int min_diff = 100;
    
    pthread_mutex_lock(&server->clients_mutex);
    
    for (int i = 0; i < server->client_count; i++) {
        ConnectedClient* other = server->clients[i];
        
        if (other->status == PLAYER_SEARCHING && 
            other->user_id != client->user_id) {
            
            int elo_diff = abs(other->elo_rating - client->elo_rating);
            
            if (elo_diff <= min_diff) {
                opponent = other;
                break; // Found good match
            }
        }
    }
    
    pthread_mutex_unlock(&server->clients_mutex);
    
    // If opponent found, create match
    if (opponent) {
        printf("Match found: %s vs %s\n", client->username, opponent->username);
        create_match(server, client, opponent);
    } else {
        // Send "searching..." message
        send_message(client, MSG_SEARCH_MATCH, "{\"status\": \"searching\"}");
    }
}

void create_match(GameServer* server, ConnectedClient* p1, ConnectedClient* p2) {
    // Create match record in database
    int match_id = db_create_match(&server->db, 
                                    p1->user_id, p2->user_id,
                                    p1->elo_rating, p2->elo_rating);
    
    if (match_id < 0) {
        send_error(p1, "Failed to create match");
        send_error(p2, "Failed to create match");
        return;
    }
    
    // Create active match structure
    pthread_mutex_lock(&server->matches_mutex);
    
    if (server->match_count >= MAX_MATCHES) {
        send_error(p1, "Server full");
        send_error(p2, "Server full");
        pthread_mutex_unlock(&server->matches_mutex);
        return;
    }
    
    ActiveMatch* match = malloc(sizeof(ActiveMatch));
    memset(match, 0, sizeof(ActiveMatch));
    
    match->match_id = match_id;
    match->player1_id = p1->user_id;
    match->player2_id = p2->user_id;
    match->player1_socket = p1->socket_fd;
    match->player2_socket = p2->socket_fd;
    
    pthread_mutex_init(&match->match_mutex, NULL);
    
    // Initialize game state (similar to Game_init())
    init_match_game_state(match);
    
    // Set player names
    match->players[0].id = 0;
    match->players[0].name = p1->username;
    match->players[0].money = 1500;
    match->players[0].position = 0;
    match->players[0].jailed = 0;
    
    match->players[1].id = 1;
    match->players[1].name = p2->username;
    match->players[1].money = 1500;
    match->players[1].position = 0;
    match->players[1].jailed = 0;
    
    // Add to active matches
    server->matches[server->match_count++] = match;
    
    pthread_mutex_unlock(&server->matches_mutex);
    
    // Update player statuses
    p1->status = PLAYER_IN_GAME;
    p2->status = PLAYER_IN_GAME;
    p1->current_match_id = match_id;
    p2->current_match_id = match_id;
    
    db_set_player_online(&server->db, p1->user_id, "in_game");
    db_set_player_online(&server->db, p2->user_id, "in_game");
    
    // Notify both players that game is starting
    send_game_start(match, p1->socket_fd, p2->socket_fd);
    
    printf("Match %d created: %s vs %s\n", match_id, p1->username, p2->username);
}

void handle_send_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON");
        return;
    }
    
    cJSON* target_item = cJSON_GetObjectItem(json, "target_id");
    if (!target_item) {
        send_error(client, "Missing target_id");
        cJSON_Delete(json);
        return;
    }
    
    int target_id = target_item->valueint;
    
    // Find target client
    ConnectedClient* target = find_client_by_id(server, target_id);
    
    if (!target) {
        send_error(client, "Player not online");
        cJSON_Delete(json);
        return;
    }
    
    if (target->status != PLAYER_IDLE) {
        send_error(client, "Player busy");
        cJSON_Delete(json);
        return;
    }
    
    // Create challenge in database
    int challenge_id = db_create_challenge(&server->db, client->user_id, target_id);
    
    // Send challenge notification to target
    cJSON* challenge_msg = cJSON_CreateObject();
    cJSON_AddNumberToObject(challenge_msg, "type", MSG_CHALLENGE_REQUEST);
    cJSON_AddNumberToObject(challenge_msg, "challenge_id", challenge_id);
    cJSON_AddNumberToObject(challenge_msg, "challenger_id", client->user_id);
    cJSON_AddStringToObject(challenge_msg, "challenger_name", client->username);
    cJSON_AddNumberToObject(challenge_msg, "challenger_elo", client->elo_rating);
    
    char* challenge_str = cJSON_Print(challenge_msg);
    send_message(target, MSG_CHALLENGE_REQUEST, challenge_str);
    
    free(challenge_str);
    cJSON_Delete(challenge_msg);
    cJSON_Delete(json);
}

void handle_accept_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON");
        return;
    }
    
    cJSON* challenge_item = cJSON_GetObjectItem(json, "challenge_id");
    if (!challenge_item) {
        send_error(client, "Missing challenge_id");
        cJSON_Delete(json);
        return;
    }
    
    int challenge_id = challenge_item->valueint;
    
    // Update challenge status
    db_respond_challenge(&server->db, challenge_id, "accepted");
    
    // Get challenger
    // (Would need to add db function to get challenger_id from challenge_id)
    // For now, assume we get it from the message
    cJSON* challenger_item = cJSON_GetObjectItem(json, "challenger_id");
    int challenger_id = challenger_item->valueint;
    
    ConnectedClient* challenger = find_client_by_id(server, challenger_id);
    
    if (!challenger) {
        send_error(client, "Challenger no longer online");
        cJSON_Delete(json);
        return;
    }
    
    // Create match
    create_match(server, challenger, client);
    
    cJSON_Delete(json);
}

void handle_decline_challenge(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) return;
    
    cJSON* challenge_item = cJSON_GetObjectItem(json, "challenge_id");
    if (!challenge_item) {
        cJSON_Delete(json);
        return;
    }
    
    int challenge_id = challenge_item->valueint;
    
    // Update challenge status
    db_respond_challenge(&server->db, challenge_id, "declined");
    
    // Notify challenger
    // ... (implementation left for you)
    
    cJSON_Delete(json);
}
```

### Step 4.5: Game Logic Integration

Create `server/game_handler.c`:

```c
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <cjson/cJSON.h>

// Import game logic functions from Game.c
// These would need to be extracted and made available

void handle_roll_dice(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    // Find active match
    ActiveMatch* match = find_match_by_player(server, client->user_id);
    if (!match) {
        send_error(client, "Not in a match");
        return;
    }
    
    pthread_mutex_lock(&match->match_mutex);
    
    // Validate it's player's turn
    int player_index = (match->player1_id == client->user_id) ? 0 : 1;
    if (match->current_player != player_index) {
        send_error(client, "Not your turn!");
        pthread_mutex_unlock(&match->match_mutex);
        return;
    }
    
    // Validate game state
    if (match->state != Game_STATE_BEGIN_MOVE) {
        send_error(client, "Cannot roll dice now!");
        pthread_mutex_unlock(&match->match_mutex);
        return;
    }
    
    // Execute move using game logic (from Game.c)
    // This would call a modified version of Game_cycle()
    execute_game_cycle(match);
    
    // Log move to database
    char move_data[256];
    sprintf(move_data, "{\"roll\": [%d, %d]}", match->lastRoll[0], match->lastRoll[1]);
    db_log_move(&server->db, match->match_id, client->user_id, 
                get_move_number(match), "roll", move_data);
    
    pthread_mutex_unlock(&match->match_mutex);
    
    // Broadcast updated game state to both players
    broadcast_game_state(match);
    
    // Check if game ended
    if (match->state == Game_STATE_END) {
        handle_game_end(server, match);
    }
}

void handle_buy_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    ActiveMatch* match = find_match_by_player(server, client->user_id);
    if (!match) {
        send_error(client, "Not in a match");
        return;
    }
    
    pthread_mutex_lock(&match->match_mutex);
    
    int player_index = (match->player1_id == client->user_id) ? 0 : 1;
    if (match->current_player != player_index) {
        send_error(client, "Not your turn!");
        pthread_mutex_unlock(&match->match_mutex);
        return;
    }
    
    if (match->state != Game_STATE_BUY_PROPERTY) {
        send_error(client, "Cannot buy property now!");
        pthread_mutex_unlock(&match->match_mutex);
        return;
    }
    
    // Execute buy (from Game.c Game_buyProperty)
    execute_buy_property(match);
    
    // Log move
    int prop_id = match->players[player_index].position;
    char move_data[256];
    sprintf(move_data, "{\"property_id\": %d}", prop_id);
    db_log_move(&server->db, match->match_id, client->user_id,
                get_move_number(match), "buy", move_data);
    
    pthread_mutex_unlock(&match->match_mutex);
    
    broadcast_game_state(match);
}

void handle_upgrade_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    ActiveMatch* match = find_match_by_player(server, client->user_id);
    if (!match) {
        send_error(client, "Not in a match");
        return;
    }
    
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON");
        return;
    }
    
    cJSON* prop_item = cJSON_GetObjectItem(json, "property_id");
    if (!prop_item) {
        send_error(client, "Missing property_id");
        cJSON_Delete(json);
        return;
    }
    
    int property_id = prop_item->valueint;
    
    pthread_mutex_lock(&match->match_mutex);
    
    int player_index = (match->player1_id == client->user_id) ? 0 : 1;
    if (match->current_player != player_index) {
        send_error(client, "Not your turn!");
        pthread_mutex_unlock(&match->match_mutex);
        cJSON_Delete(json);
        return;
    }
    
    // Execute upgrade (from Game.c Game_upgradeProp)
    execute_upgrade_property(match, property_id, 1);
    
    // Log move
    char move_data[256];
    sprintf(move_data, "{\"property_id\": %d}", property_id);
    db_log_move(&server->db, match->match_id, client->user_id,
                get_move_number(match), "upgrade", move_data);
    
    pthread_mutex_unlock(&match->match_mutex);
    
    broadcast_game_state(match);
    cJSON_Delete(json);
}

void handle_mortgage_property(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    // Similar to upgrade, but calls mortgage logic
    // Implementation follows same pattern...
}

void handle_declare_bankrupt(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    ActiveMatch* match = find_match_by_player(server, client->user_id);
    if (!match) {
        send_error(client, "Not in a match");
        return;
    }
    
    pthread_mutex_lock(&match->match_mutex);
    
    int player_index = (match->player1_id == client->user_id) ? 0 : 1;
    
    // Force bankruptcy
    match->players[player_index].money = -1;
    match->state = Game_STATE_END;
    
    pthread_mutex_unlock(&match->match_mutex);
    
    // Handle game end
    handle_game_end(server, match);
}

void broadcast_game_state(ActiveMatch* match) {
    // Create JSON with complete game state
    cJSON* state = cJSON_CreateObject();
    cJSON_AddNumberToObject(state, "type", MSG_GAME_STATE);
    cJSON_AddNumberToObject(state, "match_id", match->match_id);
    cJSON_AddNumberToObject(state, "current_player", match->current_player);
    cJSON_AddNumberToObject(state, "game_state", match->state);
    
    // Add players
    cJSON* players = cJSON_CreateArray();
    for (int i = 0; i < 2; i++) {
        cJSON* player = cJSON_CreateObject();
        cJSON_AddNumberToObject(player, "id", match->players[i].id);
        cJSON_AddStringToObject(player, "name", match->players[i].name);
        cJSON_AddNumberToObject(player, "money", match->players[i].money);
        cJSON_AddNumberToObject(player, "position", match->players[i].position);
        cJSON_AddBoolToObject(player, "jailed", match->players[i].jailed);
        cJSON_AddItemToArray(players, player);
    }
    cJSON_AddItemToObject(state, "players", players);
    
    // Add properties
    cJSON* properties = cJSON_CreateArray();
    for (int i = 0; i < 40; i++) {
        cJSON* prop = cJSON_CreateObject();
        cJSON_AddNumberToObject(prop, "id", match->properties[i].id);
        cJSON_AddNumberToObject(prop, "owner", match->properties[i].owner);
        cJSON_AddNumberToObject(prop, "upgrades", match->properties[i].upgrades);
        cJSON_AddBoolToObject(prop, "mortgaged", match->properties[i].mortaged);
        cJSON_AddItemToArray(properties, prop);
    }
    cJSON_AddItemToObject(state, "properties", properties);
    
    // Add last roll
    cJSON* last_roll = cJSON_CreateArray();
    cJSON_AddItemToArray(last_roll, cJSON_CreateNumber(match->lastRoll[0]));
    cJSON_AddItemToArray(last_roll, cJSON_CreateNumber(match->lastRoll[1]));
    cJSON_AddItemToObject(state, "last_roll", last_roll);
    
    // Add messages
    cJSON_AddStringToObject(state, "message", match->message);
    cJSON_AddStringToObject(state, "message2", match->message2);
    
    char* state_str = cJSON_Print(state);
    
    // Send to both players
    send_message_to_socket(match->player1_socket, MSG_GAME_STATE, state_str);
    send_message_to_socket(match->player2_socket, MSG_GAME_STATE, state_str);
    
    free(state_str);
    cJSON_Delete(state);
}
```

### Step 4.6: ELO Rating System

Create `server/elo.c`:

```c
#include "server.h"
#include <math.h>
#include <stdio.h>

#define K_FACTOR 32

void calculate_elo_change(int winner_elo, int loser_elo, 
                          int* winner_change, int* loser_change) {
    // Expected scores
    double expected_winner = 1.0 / (1.0 + pow(10.0, (loser_elo - winner_elo) / 400.0));
    double expected_loser = 1.0 / (1.0 + pow(10.0, (winner_elo - loser_elo) / 400.0));
    
    // Actual scores: 1 for win, 0 for loss
    *winner_change = (int)(K_FACTOR * (1.0 - expected_winner));
    *loser_change = (int)(K_FACTOR * (0.0 - expected_loser));
}

void handle_game_end(GameServer* server, ActiveMatch* match) {
    // Determine winner
    int winner_id, loser_id;
    int winner_index, loser_index;
    
    if (match->players[0].money < 0) {
        winner_id = match->player2_id;
        loser_id = match->player1_id;
        winner_index = 1;
        loser_index = 0;
    } else {
        winner_id = match->player1_id;
        loser_id = match->player2_id;
        winner_index = 0;
        loser_index = 1;
    }
    
    // Get current ELOs
    ConnectedClient* winner_client = find_client_by_id(server, winner_id);
    ConnectedClient* loser_client = find_client_by_id(server, loser_id);
    
    int winner_elo = winner_client->elo_rating;
    int loser_elo = loser_client->elo_rating;
    
    // Calculate ELO changes
    int winner_change, loser_change;
    calculate_elo_change(winner_elo, loser_elo, &winner_change, &loser_change);
    
    int new_winner_elo = winner_elo + winner_change;
    int new_loser_elo = loser_elo + loser_change;
    
    // Update database
    db_update_match_result(&server->db, match->match_id, winner_id, 
                          (winner_index == 0) ? new_winner_elo : new_loser_elo,
                          (winner_index == 1) ? new_winner_elo : new_loser_elo);
    
    db_update_user_elo(&server->db, winner_id, new_winner_elo);
    db_update_user_elo(&server->db, loser_id, new_loser_elo);
    
    // Update client ELOs
    winner_client->elo_rating = new_winner_elo;
    loser_client->elo_rating = new_loser_elo;
    
    // Update player statuses
    winner_client->status = PLAYER_IDLE;
    loser_client->status = PLAYER_IDLE;
    winner_client->current_match_id = 0;
    loser_client->current_match_id = 0;
    
    db_set_player_online(&server->db, winner_id, "idle");
    db_set_player_online(&server->db, loser_id, "idle");
    
    // Send game result to both players
    cJSON* result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "type", MSG_GAME_RESULT);
    cJSON_AddNumberToObject(result, "match_id", match->match_id);
    cJSON_AddNumberToObject(result, "winner_id", winner_id);
    cJSON_AddStringToObject(result, "winner_name", match->players[winner_index].name);
    cJSON_AddNumberToObject(result, "loser_id", loser_id);
    cJSON_AddStringToObject(result, "loser_name", match->players[loser_index].name);
    cJSON_AddNumberToObject(result, "winner_elo_change", winner_change);
    cJSON_AddNumberToObject(result, "loser_elo_change", loser_change);
    cJSON_AddNumberToObject(result, "winner_new_elo", new_winner_elo);
    cJSON_AddNumberToObject(result, "loser_new_elo", new_loser_elo);
    
    char* result_str = cJSON_Print(result);
    
    send_message_to_socket(match->player1_socket, MSG_GAME_RESULT, result_str);
    send_message_to_socket(match->player2_socket, MSG_GAME_RESULT, result_str);
    
    free(result_str);
    cJSON_Delete(result);
    
    printf("Match %d ended: %s defeated %s (ELO: %+d vs %+d)\n",
           match->match_id, 
           match->players[winner_index].name,
           match->players[loser_index].name,
           winner_change, loser_change);
    
    // Remove match from active matches
    pthread_mutex_lock(&server->matches_mutex);
    for (int i = 0; i < server->match_count; i++) {
        if (server->matches[i] == match) {
            // Shift remaining matches
            for (int j = i; j < server->match_count - 1; j++) {
                server->matches[j] = server->matches[j + 1];
            }
            server->match_count--;
            break;
        }
    }
    pthread_mutex_unlock(&server->matches_mutex);
    
    // Free match
    pthread_mutex_destroy(&match->match_mutex);
    free(match);
}
```

---

## PHASE 5: CLIENT MODIFICATION

### Step 5.1: Client Network Layer

Create `client/client_network.h`:

```c
#ifndef CLIENT_NETWORK_H
#define CLIENT_NETWORK_H

#include "../shared/protocol.h"

typedef struct {
    int socket_fd;
    int user_id;
    char username[50];
    char session_id[65];
    int elo_rating;
    int in_game;
    int current_match_id;
} ClientState;

// Connection
int client_connect(ClientState* state, const char* server_ip, int port);
void client_disconnect(ClientState* state);

// Authentication
int client_register(ClientState* state, const char* username, const char* password);
int client_login(ClientState* state, const char* username, const char* password);
void client_logout(ClientState* state);

// Communication
int client_send_message(ClientState* state, NetworkMessage* msg);
int client_receive_message(ClientState* state, NetworkMessage* msg);

// Game actions
void client_search_match(ClientState* state);
void client_send_challenge(ClientState* state, int target_id);
void client_accept_challenge(ClientState* state, int challenge_id);
void client_decline_challenge(ClientState* state, int challenge_id);

void client_roll_dice(ClientState* state);
void client_buy_property(ClientState* state);
void client_skip_property(ClientState* state);
void client_upgrade_property(ClientState* state, int property_id);
void client_mortgage_property(ClientState* state, int property_id);
void client_pay_jail_fine(ClientState* state);
void client_declare_bankrupt(ClientState* state);

#endif
```

### Step 5.2: Modify Game.c for Network Mode

The key is to separate:
1. **Game state** (now updated from server)
2. **Input handling** (send to server, not process locally)
3. **Rendering** (keep existing SDL code)

Modifications needed:
- Add `Game_update_from_server(NetworkMessage* msg)` to update state
- Modify `Game_receiveinput()` to send network messages instead of processing locally
- Keep all rendering code unchanged

### Step 5.3: Create Lobby UI

Create `client/lobby.h` and `client/lobby.c`:

```c
// lobby.h
#ifndef LOBBY_H
#define LOBBY_H

#include "client_network.h"
#include <SDL.h>

typedef enum {
    LOBBY_STATE_LOGIN,
    LOBBY_STATE_REGISTER,
    LOBBY_STATE_MAIN,
    LOBBY_STATE_SEARCHING,
    LOBBY_STATE_IN_GAME
} LobbyState;

void lobby_init();
void lobby_render(ClientState* client, LobbyState state);
void lobby_handle_input(ClientState* client, SDL_Event* event, LobbyState* state);
void lobby_cleanup();

#endif
```

---

## PHASE 6: IMPLEMENTATION ORDER & TIMELINE

### Week 1-2: Foundation (Core Infrastructure)
**Goal**: Basic server-client communication working

1. ☐ Create project structure (server/, client/, shared/ directories)
2. ☐ Set up database schema (create tables)
3. ☐ Implement database.c/database.h (all DB operations)
4. ☐ Implement shared/protocol.h (message serialization)
5. ☐ Create basic server socket handling (server_main.c)
6. ☐ Create basic client socket handling (client_network.c)
7. ☐ Test: Client can connect to server

**Deliverable**: Client connects to server, sends/receives messages

### Week 3-4: Authentication & Sessions
**Goal**: Users can register and login

8. ☐ Implement password hashing (use OpenSSL)
9. ☐ Implement handle_register() on server
10. ☐ Implement handle_login() on server
11. ☐ Implement session management
12. ☐ Create login/register UI on client
13. ☐ Test: User can register, logout, login again

**Deliverable**: Working authentication system

### Week 5-6: Matchmaking System
**Goal**: Players can find each other and start games

14. ☐ Implement online players tracking
15. ☐ Implement handle_search_match()
16. ☐ Implement ELO-based matchmaking algorithm
17. ☐ Implement create_match()
18. ☐ Implement challenge system (send/accept/decline)
19. ☐ Create lobby UI showing online players
20. ☐ Test: Two clients can be matched together

**Deliverable**: Working matchmaking system

### Week 7-8: Game Logic Integration
**Goal**: Players can play a complete game

21. ☐ Extract game logic from Game.c into reusable functions
22. ☐ Implement server-side game state management
23. ☐ Implement handle_roll_dice()
24. ☐ Implement handle_buy_property()
25. ☐ Implement handle_upgrade_property()
26. ☐ Implement handle_mortgage_property()
27. ☐ Implement broadcast_game_state()
28. ☐ Modify client Game.c to update from network
29. ☐ Test: Complete game playable over network

**Deliverable**: Full game working over network

### Week 9-10: Game Features & Polish
**Goal**: All game features working

30. ☐ Implement win/loss detection
31. ☐ Implement ELO calculation (elo.c)
32. ☐ Implement handle_game_end()
33. ☐ Implement game move logging
34. ☐ Implement bankruptcy handling
35. ☐ Implement rematch system
36. ☐ Add game result notifications
37. ☐ Test: Full game cycle with ELO updates

**Deliverable**: Complete game with ratings

### Week 11-12: Testing & Deployment
**Goal**: Production-ready system

38. ☐ Add error handling throughout
39. ☐ Implement reconnection logic
40. ☐ Implement heartbeat system (detect disconnects)
41. ☐ Add match history viewer
42. ☐ Add statistics display
43. ☐ Stress testing (multiple concurrent games)
44. ☐ Security audit (SQL injection, input validation)
45. ☐ Performance optimization
46. ☐ Write user manual/documentation

**Deliverable**: Production-ready multiplayer monopoly

---

## PHASE 7: TECHNICAL CONSIDERATIONS

### Security Checklist
- [ ] All passwords hashed with bcrypt/SHA256
- [ ] All moves validated server-side
- [ ] SQL injection prevention (prepared statements)
- [ ] Session timeout implementation
- [ ] Rate limiting on login attempts
- [ ] Input sanitization
- [ ] Consider SSL/TLS for production

### Reliability Checklist
- [ ] Heartbeat every 30 seconds
- [ ] Handle disconnections gracefully
- [ ] Allow reconnection to ongoing games
- [ ] Save game state periodically
- [ ] Timeout inactive players
- [ ] Handle server shutdown gracefully

### Performance Checklist
- [ ] Use non-blocking I/O (epoll/select)
- [ ] Minimize database writes
- [ ] Cache frequently accessed data
- [ ] Profile and optimize hot paths
- [ ] Test with 20+ concurrent games

### Code Quality Checklist
- [ ] Comprehensive error handling
- [ ] Memory leak testing (valgrind)
- [ ] Thread safety verification
- [ ] Code documentation
- [ ] Unit tests for game logic
- [ ] Integration tests

---

## DIRECTORY STRUCTURE

```
NP_Monopoly/
├── server/
│   ├── server_main.c       # Main server loop
│   ├── server.h            # Server structures
│   ├── auth.c              # Authentication
│   ├── matchmaking.c       # Matchmaking engine
│   ├── game_handler.c      # Game move handlers
│   ├── elo.c               # ELO calculation
│   ├── database.c          # Database operations
│   ├── database.h
│   └── Makefile
│
├── client/
│   ├── client_main.c       # Main client loop
│   ├── client_network.c    # Network communication
│   ├── client_network.h
│   ├── lobby.c             # Lobby UI
│   ├── lobby.h
│   ├── Game.c              # Game logic (modified)
│   ├── Game.h
│   ├── Render.c            # SDL rendering
│   ├── Player.h
│   ├── Property.h
│   └── Makefile
│
├── shared/
│   ├── protocol.h          # Message protocol
│   └── protocol.c          # Serialization
│
├── database_schema.sql     # Database schema
├── monopoly.db             # SQLite database
├── plan.md                 # This file
└── README.md               # User guide
```

---

## DEPENDENCIES

### Server Dependencies:
- **libsqlite3-dev**: Database
- **libssl-dev**: Password hashing
- **libcjson-dev**: JSON parsing
- **pthread**: Threading

Install on Ubuntu/Debian:
```bash
sudo apt-get install libsqlite3-dev libssl-dev libcjson-dev build-essential
```

### Client Dependencies:
- **libsdl2-dev**: Graphics
- **libsdl2-ttf-dev**: Text rendering
- **libcjson-dev**: JSON parsing

Install on Ubuntu/Debian:
```bash
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libcjson-dev build-essential
```

---

## BUILD INSTRUCTIONS

### Server:
```bash
cd server/
make
./monopoly_server 8888
```

### Client:
```bash
cd client/
make
./monopoly_client 127.0.0.1 8888
```

---

## TESTING STRATEGY

### Unit Tests:
1. Database operations (create user, get user, etc.)
2. ELO calculation
3. Game logic functions
4. Message serialization/deserialization

### Integration Tests:
1. Login flow
2. Matchmaking flow
3. Complete game flow
4. Disconnect/reconnect

---


