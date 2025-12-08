#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

void msg_init(NetworkMessage* msg, MessageType type) {
    memset(msg, 0, sizeof(NetworkMessage));
    msg->type = type;
}

int msg_serialize(const NetworkMessage* msg, char* buffer, int buffer_size) {
    if (!msg || !buffer) return -1;
    
    int total_size = MSG_HEADER_SIZE + msg->payload_length;
    if (buffer_size < total_size) return -1;
    
    // Convert to network byte order (big endian)
    uint32_t* header = (uint32_t*)buffer;
    header[0] = htonl(msg->type);
    header[1] = htonl(msg->sender_id);
    header[2] = htonl(msg->target_id);
    header[3] = htonl(msg->payload_length);
    
    // Copy payload
    if (msg->payload_length > 0) {
        memcpy(buffer + MSG_HEADER_SIZE, msg->payload, msg->payload_length);
    }
    
    return total_size;
}

int msg_deserialize(NetworkMessage* msg, const char* buffer, int buffer_size) {
    if (!msg || !buffer) return -1;
    if (buffer_size < MSG_HEADER_SIZE) return -1;
    
    // Read header (convert from network byte order)
    const uint32_t* header = (const uint32_t*)buffer;
    msg->type = ntohl(header[0]);
    msg->sender_id = ntohl(header[1]);
    msg->target_id = ntohl(header[2]);
    msg->payload_length = ntohl(header[3]);
    
    // Validate payload length
    if (msg->payload_length > MSG_MAX_PAYLOAD) return -1;
    if (buffer_size < (int)(MSG_HEADER_SIZE + msg->payload_length)) return -1;
    
    // Copy payload
    if (msg->payload_length > 0) {
        memcpy(msg->payload, buffer + MSG_HEADER_SIZE, msg->payload_length);
    }
    msg->payload[msg->payload_length] = '\0';  // Null terminate
    
    return 0;
}

int msg_total_size(const NetworkMessage* msg) {
    return MSG_HEADER_SIZE + msg->payload_length;
}

void msg_print(const NetworkMessage* msg) {
    printf("Message[type=%u, sender=%u, target=%u, len=%u]\n",
           msg->type, msg->sender_id, msg->target_id, msg->payload_length);
    if (msg->payload_length > 0) {
        printf("  Payload: %s\n", msg->payload);
    }
}

