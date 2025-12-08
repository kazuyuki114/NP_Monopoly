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

// Fixed-size message header for network transmission
#define MSG_HEADER_SIZE 16
#define MSG_MAX_PAYLOAD 4096

// Message structure
typedef struct {
    uint32_t type;           // MessageType
    uint32_t sender_id;      // User ID of sender (0 if not logged in)
    uint32_t target_id;      // User ID of target (0 for server)
    uint32_t payload_length; // Length of payload
    char payload[MSG_MAX_PAYLOAD];  // JSON payload
} NetworkMessage;

// Initialize a message
void msg_init(NetworkMessage* msg, MessageType type);

// Serialization functions
// Returns total bytes written, or -1 on error
int msg_serialize(const NetworkMessage* msg, char* buffer, int buffer_size);

// Deserialize from buffer
// Returns 0 on success, -1 on error
int msg_deserialize(NetworkMessage* msg, const char* buffer, int buffer_size);

// Helper to get the total message size (header + payload)
int msg_total_size(const NetworkMessage* msg);

// Debug: print message info
void msg_print(const NetworkMessage* msg);

#endif // PROTOCOL_H

