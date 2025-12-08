#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "cJSON.h"
#include <sys/socket.h>

// Simple password hashing using SHA256
void hash_password(const char* password, char* output) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[SHA256_DIGEST_LENGTH * 2] = '\0';
}

// Generate random session ID (64 hex characters)
void generate_session_id(char* output) {
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < SESSION_ID_LENGTH; i++) {
        output[i] = hex_chars[rand() % 16];
    }
    output[SESSION_ID_LENGTH] = '\0';
}

// Send a message to client
int send_message(ConnectedClient* client, MessageType type, const char* payload) {
    if (!client || client->socket_fd < 0) return -1;
    
    NetworkMessage msg;
    msg_init(&msg, type);
    msg.sender_id = 0;  // From server
    msg.target_id = client->user_id;
    
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
    if (size < 0) return -1;
    
    return send(client->socket_fd, buffer, size, 0);
}

int send_error(ConnectedClient* client, const char* error_msg) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", 0);
    cJSON_AddStringToObject(json, "error", error_msg);
    
    char* str = cJSON_PrintUnformatted(json);
    int result = send_message(client, MSG_ERROR, str);
    
    free(str);
    cJSON_Delete(json);
    return result;
}

int send_success(ConnectedClient* client, const char* msg) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "success", 1);
    if (msg) {
        cJSON_AddStringToObject(json, "message", msg);
    }
    
    char* str = cJSON_PrintUnformatted(json);
    int result = send_message(client, MSG_SUCCESS, str);
    
    free(str);
    cJSON_Delete(json);
    return result;
}

// Handle registration request
void handle_register(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    printf("[AUTH] Register request from socket %d\n", client->socket_fd);
    
    // Parse JSON payload
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON format");
        return;
    }
    
    cJSON* username_item = cJSON_GetObjectItem(json, "username");
    cJSON* password_item = cJSON_GetObjectItem(json, "password");
    cJSON* email_item = cJSON_GetObjectItem(json, "email");
    
    if (!username_item || !cJSON_IsString(username_item) ||
        !password_item || !cJSON_IsString(password_item)) {
        send_error(client, "Missing username or password");
        cJSON_Delete(json);
        return;
    }
    
    const char* username = username_item->valuestring;
    const char* password = password_item->valuestring;
    const char* email = (email_item && cJSON_IsString(email_item)) ? email_item->valuestring : NULL;
    
    // Validate username length
    if (strlen(username) < 3 || strlen(username) > 20) {
        send_error(client, "Username must be 3-20 characters");
        cJSON_Delete(json);
        return;
    }
    
    // Validate password length
    if (strlen(password) < 4) {
        send_error(client, "Password must be at least 4 characters");
        cJSON_Delete(json);
        return;
    }
    
    // Hash password
    char password_hash[65];
    hash_password(password, password_hash);
    
    // Create user in database
    int user_id = db_create_user(&server->db, username, password_hash, email);
    
    if (user_id > 0) {
        // Success
        printf("[AUTH] User registered: %s (id=%d)\n", username, user_id);
        
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", 1);
        cJSON_AddStringToObject(response, "message", "Registration successful! Please login.");
        cJSON_AddNumberToObject(response, "user_id", user_id);
        
        char* response_str = cJSON_PrintUnformatted(response);
        send_message(client, MSG_REGISTER_RESPONSE, response_str);
        
        free(response_str);
        cJSON_Delete(response);
    } else {
        send_error(client, "Username already exists");
    }
    
    cJSON_Delete(json);
}

// Handle login request
void handle_login(GameServer* server, ConnectedClient* client, NetworkMessage* msg) {
    printf("[AUTH] Login request from socket %d\n", client->socket_fd);
    
    // Check if already logged in
    if (client->user_id > 0) {
        send_error(client, "Already logged in");
        return;
    }
    
    // Parse JSON payload
    cJSON* json = cJSON_Parse(msg->payload);
    if (!json) {
        send_error(client, "Invalid JSON format");
        return;
    }
    
    cJSON* username_item = cJSON_GetObjectItem(json, "username");
    cJSON* password_item = cJSON_GetObjectItem(json, "password");
    
    if (!username_item || !cJSON_IsString(username_item) ||
        !password_item || !cJSON_IsString(password_item)) {
        send_error(client, "Missing username or password");
        cJSON_Delete(json);
        return;
    }
    
    const char* username = username_item->valuestring;
    const char* password = password_item->valuestring;
    
    // Hash provided password
    char password_hash[65];
    hash_password(password, password_hash);
    
    // Get user from database
    int user_id;
    char stored_hash[65];
    int elo;
    
    int result = db_get_user_by_username(&server->db, username, &user_id, stored_hash, &elo);
    
    if (result == 0 && strcmp(password_hash, stored_hash) == 0) {
        // Check if user is already logged in from another connection
        pthread_mutex_lock(&server->clients_mutex);
        ConnectedClient* existing = find_client_by_id(server, user_id);
        if (existing && existing != client) {
            pthread_mutex_unlock(&server->clients_mutex);
            send_error(client, "Already logged in from another location");
            cJSON_Delete(json);
            return;
        }
        pthread_mutex_unlock(&server->clients_mutex);
        
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
        
        // Update last login
        db_update_last_login(&server->db, user_id);
        
        // Get full user info
        UserInfo info;
        db_get_user_info(&server->db, user_id, &info);
        
        // Send response
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", 1);
        cJSON_AddNumberToObject(response, "user_id", user_id);
        cJSON_AddStringToObject(response, "username", username);
        cJSON_AddNumberToObject(response, "elo_rating", elo);
        cJSON_AddNumberToObject(response, "total_matches", info.total_matches);
        cJSON_AddNumberToObject(response, "wins", info.wins);
        cJSON_AddNumberToObject(response, "losses", info.losses);
        cJSON_AddStringToObject(response, "session_id", client->session_id);
        
        char* response_str = cJSON_PrintUnformatted(response);
        send_message(client, MSG_LOGIN_RESPONSE, response_str);
        
        free(response_str);
        cJSON_Delete(response);
        
        printf("[AUTH] User logged in: %s (id=%d, elo=%d)\n", username, user_id, elo);
    } else {
        send_error(client, "Invalid username or password");
        printf("[AUTH] Failed login attempt for: %s\n", username);
    }
    
    cJSON_Delete(json);
}

// Handle logout
void handle_logout(GameServer* server, ConnectedClient* client) {
    if (client->user_id > 0) {
        printf("[AUTH] User logging out: %s\n", client->username);
        
        // Delete session
        db_delete_session(&server->db, client->session_id);
        
        // Mark player as offline
        db_set_player_offline(&server->db, client->user_id);
        
        // Send confirmation
        send_success(client, "Logged out successfully");
        
        // Reset client state
        client->user_id = 0;
        client->session_id[0] = '\0';
        client->username[0] = '\0';
        client->elo_rating = 0;
        client->status = PLAYER_DISCONNECTED;
    }
}

