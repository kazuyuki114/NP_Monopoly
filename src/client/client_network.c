#include "client_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "cJSON.h"

// Last error message from operations
static char last_error_msg[256] = "";

const char* client_get_last_error(void) {
    return last_error_msg;
}

void client_init(ClientState* state) {
    memset(state, 0, sizeof(ClientState));
    state->socket_fd = -1;
    state->connected = 0;
    state->logged_in = 0;
}

int client_connect(ClientState* state, const char* server_ip, int port) {
    if (!state || !server_ip) return -1;
    
    // Create socket
    state->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (state->socket_fd < 0) {
        perror("socket");
        return -1;
    }
    
    // Server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address: %s\n", server_ip);
        close(state->socket_fd);
        state->socket_fd = -1;
        return -1;
    }
    
    // Connect
    if (connect(state->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(state->socket_fd);
        state->socket_fd = -1;
        return -1;
    }
    
    state->connected = 1;
    printf("[CLIENT] Connected to %s:%d\n", server_ip, port);
    
    return 0;
}

void client_disconnect(ClientState* state) {
    if (!state) return;
    
    if (state->socket_fd >= 0) {
        // Send logout if logged in
        if (state->logged_in) {
            client_logout(state);
        }
        close(state->socket_fd);
        state->socket_fd = -1;
    }
    
    state->connected = 0;
    state->logged_in = 0;
    printf("[CLIENT] Disconnected\n");
}

int client_is_connected(ClientState* state) {
    return state && state->connected && state->socket_fd >= 0;
}

int client_send(ClientState* state, MessageType type, const char* payload) {
    if (!client_is_connected(state)) {
        fprintf(stderr, "[CLIENT] Not connected\n");
        return -1;
    }
    
    NetworkMessage msg;
    msg_init(&msg, type);
    msg.sender_id = state->user_id;
    msg.target_id = 0;  // To server
    
    if (payload) {
        msg.payload_length = strlen(payload);
        if (msg.payload_length > MSG_MAX_PAYLOAD - 1) {
            msg.payload_length = MSG_MAX_PAYLOAD - 1;
        }
        memcpy(msg.payload, payload, msg.payload_length);
        msg.payload[msg.payload_length] = '\0';
    }
    
    char buffer[MSG_HEADER_SIZE + MSG_MAX_PAYLOAD];
    int size = msg_serialize(&msg, buffer, sizeof(buffer));
    if (size < 0) {
        fprintf(stderr, "[CLIENT] Failed to serialize message\n");
        return -1;
    }
    
    int sent = send(state->socket_fd, buffer, size, 0);
    if (sent != size) {
        perror("send");
        return -1;
    }
    
    return 0;
}

int client_receive(ClientState* state, NetworkMessage* msg) {
    if (!client_is_connected(state) || !msg) return -1;
    
    char buffer[MSG_HEADER_SIZE + MSG_MAX_PAYLOAD];
    
    // Read header
    int bytes = recv(state->socket_fd, buffer, MSG_HEADER_SIZE, MSG_WAITALL);
    if (bytes <= 0) {
        if (bytes == 0) {
            printf("[CLIENT] Server closed connection\n");
        } else {
            perror("recv header");
        }
        state->connected = 0;
        return -1;
    }
    
    // Parse header to get payload length
    uint32_t* header = (uint32_t*)buffer;
    uint32_t payload_len = ntohl(header[3]);
    
    if (payload_len > 0 && payload_len <= MSG_MAX_PAYLOAD) {
        bytes = recv(state->socket_fd, buffer + MSG_HEADER_SIZE, payload_len, MSG_WAITALL);
        if (bytes < (int)payload_len) {
            fprintf(stderr, "[CLIENT] Incomplete payload\n");
            return -1;
        }
    }
    
    // Deserialize
    if (msg_deserialize(msg, buffer, MSG_HEADER_SIZE + payload_len) < 0) {
        fprintf(stderr, "[CLIENT] Failed to deserialize message\n");
        return -1;
    }
    
    return 0;
}

int client_data_available(ClientState* state) {
    if (!client_is_connected(state)) return -1;
    
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(state->socket_fd, &read_fds);
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    int result = select(state->socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (result < 0) return -1;
    
    return FD_ISSET(state->socket_fd, &read_fds) ? 1 : 0;
}

int client_register(ClientState* state, const char* username, const char* password, const char* email) {
    if (!client_is_connected(state)) return -1;
    
    // Create JSON payload
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "username", username);
    cJSON_AddStringToObject(json, "password", password);
    if (email) {
        cJSON_AddStringToObject(json, "email", email);
    }
    
    char* payload = cJSON_PrintUnformatted(json);
    int result = client_send(state, MSG_REGISTER, payload);
    
    free(payload);
    cJSON_Delete(json);
    
    if (result < 0) return -1;
    
    // Wait for response
    NetworkMessage response;
    if (client_receive(state, &response) < 0) return -1;
    
    // Parse response
    cJSON* resp_json = cJSON_Parse(response.payload);
    if (!resp_json) return -1;
    
    cJSON* success = cJSON_GetObjectItem(resp_json, "success");
    int is_success = success && cJSON_IsTrue(success);
    
    if (!is_success) {
        cJSON* error = cJSON_GetObjectItem(resp_json, "error");
        if (error && cJSON_IsString(error)) {
            strncpy(last_error_msg, error->valuestring, sizeof(last_error_msg) - 1);
            printf("[CLIENT] Registration failed: %s\n", error->valuestring);
        } else {
            strncpy(last_error_msg, "Unknown error", sizeof(last_error_msg) - 1);
        }
    } else {
        printf("[CLIENT] Registration successful!\n");
    }
    
    cJSON_Delete(resp_json);
    return is_success ? 0 : -1;
}

int client_login(ClientState* state, const char* username, const char* password) {
    if (!client_is_connected(state)) return -1;
    if (state->logged_in) {
        printf("[CLIENT] Already logged in\n");
        return -1;
    }
    
    // Create JSON payload
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "username", username);
    cJSON_AddStringToObject(json, "password", password);
    
    char* payload = cJSON_PrintUnformatted(json);
    int result = client_send(state, MSG_LOGIN, payload);
    
    free(payload);
    cJSON_Delete(json);
    
    if (result < 0) return -1;
    
    // Wait for response
    NetworkMessage response;
    if (client_receive(state, &response) < 0) return -1;
    
    // Check message type
    if (response.type == MSG_ERROR) {
        cJSON* resp_json = cJSON_Parse(response.payload);
        if (resp_json) {
            cJSON* error = cJSON_GetObjectItem(resp_json, "error");
            if (error && cJSON_IsString(error)) {
                strncpy(last_error_msg, error->valuestring, sizeof(last_error_msg) - 1);
                printf("[CLIENT] Login failed: %s\n", error->valuestring);
            } else {
                strncpy(last_error_msg, "Unknown error", sizeof(last_error_msg) - 1);
            }
            cJSON_Delete(resp_json);
        }
        return -1;
    }
    
    if (response.type == MSG_LOGIN_RESPONSE) {
        return client_parse_login_response(state, response.payload);
    }
    
    return -1;
}

int client_parse_login_response(ClientState* state, const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return -1;
    
    cJSON* success = cJSON_GetObjectItem(json, "success");
    if (!success || !cJSON_IsTrue(success)) {
        cJSON* error = cJSON_GetObjectItem(json, "error");
        if (error && cJSON_IsString(error)) {
            printf("[CLIENT] Login failed: %s\n", error->valuestring);
        }
        cJSON_Delete(json);
        return -1;
    }
    
    // Parse user info
    cJSON* user_id = cJSON_GetObjectItem(json, "user_id");
    cJSON* username = cJSON_GetObjectItem(json, "username");
    cJSON* session_id = cJSON_GetObjectItem(json, "session_id");
    cJSON* elo = cJSON_GetObjectItem(json, "elo_rating");
    cJSON* matches = cJSON_GetObjectItem(json, "total_matches");
    cJSON* wins = cJSON_GetObjectItem(json, "wins");
    cJSON* losses = cJSON_GetObjectItem(json, "losses");
    
    if (user_id && cJSON_IsNumber(user_id)) {
        state->user_id = user_id->valueint;
    }
    if (username && cJSON_IsString(username)) {
        strncpy(state->username, username->valuestring, sizeof(state->username) - 1);
    }
    if (session_id && cJSON_IsString(session_id)) {
        strncpy(state->session_id, session_id->valuestring, sizeof(state->session_id) - 1);
    }
    if (elo && cJSON_IsNumber(elo)) {
        state->elo_rating = elo->valueint;
    }
    if (matches && cJSON_IsNumber(matches)) {
        state->total_matches = matches->valueint;
    }
    if (wins && cJSON_IsNumber(wins)) {
        state->wins = wins->valueint;
    }
    if (losses && cJSON_IsNumber(losses)) {
        state->losses = losses->valueint;
    }
    
    state->logged_in = 1;
    
    printf("[CLIENT] Login successful!\n");
    printf("  User: %s (ID: %d)\n", state->username, state->user_id);
    printf("  ELO: %d\n", state->elo_rating);
    printf("  Record: %d wins / %d losses (%d total)\n", 
           state->wins, state->losses, state->total_matches);
    
    cJSON_Delete(json);
    return 0;
}

int client_logout(ClientState* state) {
    if (!client_is_connected(state) || !state->logged_in) return -1;
    
    client_send(state, MSG_LOGOUT, NULL);
    
    // Reset state
    state->logged_in = 0;
    state->user_id = 0;
    state->username[0] = '\0';
    state->session_id[0] = '\0';
    state->elo_rating = 0;
    
    printf("[CLIENT] Logged out\n");
    return 0;
}

int client_send_heartbeat(ClientState* state) {
    return client_send(state, MSG_HEARTBEAT, NULL);
}

const char* client_get_error(const char* payload) {
    static char error_msg[256];
    error_msg[0] = '\0';
    
    cJSON* json = cJSON_Parse(payload);
    if (json) {
        cJSON* error = cJSON_GetObjectItem(json, "error");
        if (error && cJSON_IsString(error)) {
            strncpy(error_msg, error->valuestring, sizeof(error_msg) - 1);
        }
        cJSON_Delete(json);
    }
    
    return error_msg;
}

// ============ Matchmaking ============

int client_get_online_players(ClientState* state) {
    if (!client_is_connected(state) || !state->logged_in) {
        printf("[CLIENT] Not connected or not logged in\n");
        return -1;
    }
    
    return client_send(state, MSG_GET_ONLINE_PLAYERS, NULL);
}

int client_search_match(ClientState* state) {
    if (!client_is_connected(state) || !state->logged_in) {
        printf("[CLIENT] Not connected or not logged in\n");
        return -1;
    }
    
    if (state->in_game) {
        printf("[CLIENT] Already in a game\n");
        return -1;
    }
    
    printf("[CLIENT] Searching for match...\n");
    return client_send(state, MSG_SEARCH_MATCH, NULL);
}

int client_cancel_search(ClientState* state) {
    if (!client_is_connected(state) || !state->logged_in) {
        return -1;
    }
    
    printf("[CLIENT] Cancelling match search\n");
    return client_send(state, MSG_CANCEL_SEARCH, NULL);
}

// ============ Challenge System ============

int client_send_challenge(ClientState* state, int target_id) {
    if (!client_is_connected(state) || !state->logged_in) {
        printf("[CLIENT] Not connected or not logged in\n");
        return -1;
    }
    
    if (state->in_game) {
        printf("[CLIENT] Already in a game\n");
        return -1;
    }
    
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "target_id", target_id);
    
    char* payload = cJSON_PrintUnformatted(json);
    int result = client_send(state, MSG_SEND_CHALLENGE, payload);
    
    free(payload);
    cJSON_Delete(json);
    
    if (result == 0) {
        printf("[CLIENT] Challenge sent to user %d\n", target_id);
    }
    
    return result;
}

int client_accept_challenge(ClientState* state, int challenge_id) {
    if (!client_is_connected(state) || !state->logged_in) {
        printf("[CLIENT] Not connected or not logged in\n");
        return -1;
    }
    
    if (state->in_game) {
        printf("[CLIENT] Already in a game\n");
        return -1;
    }
    
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "challenge_id", challenge_id);
    
    char* payload = cJSON_PrintUnformatted(json);
    int result = client_send(state, MSG_ACCEPT_CHALLENGE, payload);
    
    free(payload);
    cJSON_Delete(json);
    
    if (result == 0) {
        printf("[CLIENT] Challenge %d accepted\n", challenge_id);
    }
    
    return result;
}

int client_decline_challenge(ClientState* state, int challenge_id) {
    if (!client_is_connected(state) || !state->logged_in) {
        printf("[CLIENT] Not connected or not logged in\n");
        return -1;
    }
    
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "challenge_id", challenge_id);
    
    char* payload = cJSON_PrintUnformatted(json);
    int result = client_send(state, MSG_DECLINE_CHALLENGE, payload);
    
    free(payload);
    cJSON_Delete(json);
    
    if (result == 0) {
        printf("[CLIENT] Challenge %d declined\n", challenge_id);
    }
    
    return result;
}

int client_refresh_stats(ClientState* state) {
    // After a game ends, the stats are updated from the game result.
    // This function can be called to verify stats with server if needed.
    // For now, stats are updated directly from game result.
    if (!client_is_connected(state) || !state->logged_in) {
        return -1;
    }
    
    // Clear in_game flag
    state->in_game = 0;
    state->current_match_id = 0;
    
    printf("[CLIENT] Stats refreshed: ELO=%d, W=%d, L=%d, Total=%d\n",
           state->elo_rating, state->wins, state->losses, state->total_matches);
    
    return 0;
}

