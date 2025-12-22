/*
 * Network Game Client
 * 
 * Handles network communication during gameplay.
 * Receives game state from server and updates local state.
 */

#include "game_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

// Network game state
static NetGameState netState = NET_GAME_WAITING;
static int myPlayerNum = 0;
static int matchId = 0;
static int opponentId = 0;
static char opponentName[50] = "";
static char myName[50] = "";
static time_t lastHeartbeat = 0;

// Global synced game state
SyncedGameState g_synced_state;

void NetGame_init(ClientState* client, MatchFoundInfo* matchInfo) {
    if (!client || !matchInfo) return;
    
    matchId = matchInfo->match_id;
    myPlayerNum = matchInfo->your_player_num - 1;  // Convert to 0-indexed
    opponentId = matchInfo->opponent_id;
    strncpy(opponentName, matchInfo->opponent_name, sizeof(opponentName) - 1);
    strncpy(myName, client->username, sizeof(myName) - 1);
    
    // Initialize synced state
    memset(&g_synced_state, 0, sizeof(g_synced_state));
    g_synced_state.my_player_index = myPlayerNum;
    g_synced_state.current_player = 0;  // Player 1 goes first
    g_synced_state.state_type = GSTATE_WAITING_ROLL;
    g_synced_state.paused = 0;
    g_synced_state.has_result = 0;
    
    // Set player names
    if (myPlayerNum == 0) {
        strncpy(g_synced_state.players[0].username, myName, sizeof(g_synced_state.players[0].username) - 1);
        strncpy(g_synced_state.players[1].username, opponentName, sizeof(g_synced_state.players[1].username) - 1);
    } else {
        strncpy(g_synced_state.players[0].username, opponentName, sizeof(g_synced_state.players[0].username) - 1);
        strncpy(g_synced_state.players[1].username, myName, sizeof(g_synced_state.players[1].username) - 1);
    }
    
    // Initialize money
    g_synced_state.players[0].money = 1500;
    g_synced_state.players[1].money = 1500;
    
    // Initialize properties
    for (int i = 0; i < 40; i++) {
        g_synced_state.properties[i].owner = -1;
        g_synced_state.properties[i].upgrades = 0;
        g_synced_state.properties[i].mortgaged = 0;
    }
    
    // Player 1 goes first
    if (myPlayerNum == 0) {
        netState = NET_GAME_MY_TURN;
    } else {
        netState = NET_GAME_OPPONENT_TURN;
    }
    
    lastHeartbeat = time(NULL);
    
    printf("[NET_GAME] Initialized: match=%d, player=%d, opponent=%s\n",
           matchId, myPlayerNum + 1, opponentName);
}

// Parse game state from server
static void parseGameState(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    // Current player
    cJSON* current = cJSON_GetObjectItem(json, "current_player");
    if (current && cJSON_IsNumber(current)) {
        g_synced_state.current_player = current->valueint;
    }
    
    // Game state type (VERY IMPORTANT for action decisions)
    cJSON* state = cJSON_GetObjectItem(json, "state");
    if (state && cJSON_IsNumber(state)) {
        g_synced_state.state_type = (GameStateType)state->valueint;
        
        // Update net state based on game state
        if (g_synced_state.state_type == GSTATE_ENDED) {
            netState = NET_GAME_ENDED;
        } else if (g_synced_state.state_type == GSTATE_PAUSED) {
            netState = NET_GAME_PAUSED;
        } else if (g_synced_state.current_player == myPlayerNum) {
            netState = NET_GAME_MY_TURN;
        } else {
            netState = NET_GAME_OPPONENT_TURN;
        }
    }
    
    // Paused state
    cJSON* paused = cJSON_GetObjectItem(json, "paused");
    if (paused) {
        g_synced_state.paused = cJSON_IsTrue(paused);
    }
    cJSON* paused_by = cJSON_GetObjectItem(json, "paused_by");
    if (paused_by && cJSON_IsNumber(paused_by)) {
        g_synced_state.paused_by = paused_by->valueint;
    }
    
    // Dice
    cJSON* dice = cJSON_GetObjectItem(json, "dice");
    if (dice && cJSON_IsArray(dice)) {
        cJSON* d1 = cJSON_GetArrayItem(dice, 0);
        cJSON* d2 = cJSON_GetArrayItem(dice, 1);
        if (d1) g_synced_state.dice[0] = d1->valueint;
        if (d2) g_synced_state.dice[1] = d2->valueint;
    }
    
    // Messages
    cJSON* msg = cJSON_GetObjectItem(json, "message");
    if (msg && cJSON_IsString(msg)) {
        strncpy(g_synced_state.message, msg->valuestring, sizeof(g_synced_state.message) - 1);
    }
    cJSON* msg2 = cJSON_GetObjectItem(json, "message2");
    if (msg2 && cJSON_IsString(msg2)) {
        strncpy(g_synced_state.message2, msg2->valuestring, sizeof(g_synced_state.message2) - 1);
    }
    
    // Players
    cJSON* players = cJSON_GetObjectItem(json, "players");
    if (players && cJSON_IsArray(players)) {
        for (int i = 0; i < 2; i++) {
            cJSON* p = cJSON_GetArrayItem(players, i);
            if (!p) continue;
            
            cJSON* uid = cJSON_GetObjectItem(p, "user_id");
            cJSON* name = cJSON_GetObjectItem(p, "username");
            cJSON* money = cJSON_GetObjectItem(p, "money");
            cJSON* pos = cJSON_GetObjectItem(p, "position");
            cJSON* jailed = cJSON_GetObjectItem(p, "jailed");
            cJSON* jail_turns = cJSON_GetObjectItem(p, "turns_in_jail");
            
            if (uid) g_synced_state.players[i].user_id = uid->valueint;
            if (name) strncpy(g_synced_state.players[i].username, name->valuestring, 
                             sizeof(g_synced_state.players[i].username) - 1);
            if (money) g_synced_state.players[i].money = money->valueint;
            if (pos) g_synced_state.players[i].position = pos->valueint;
            if (jailed) g_synced_state.players[i].jailed = cJSON_IsTrue(jailed);
            if (jail_turns) g_synced_state.players[i].turns_in_jail = jail_turns->valueint;
        }
    }
    
    // Properties
    cJSON* properties = cJSON_GetObjectItem(json, "properties");
    if (properties && cJSON_IsArray(properties)) {
        int count = cJSON_GetArraySize(properties);
        for (int i = 0; i < count && i < 40; i++) {
            cJSON* prop = cJSON_GetArrayItem(properties, i);
            if (!prop) continue;
            
            cJSON* owner = cJSON_GetObjectItem(prop, "owner");
            cJSON* upgrades = cJSON_GetObjectItem(prop, "upgrades");
            cJSON* mortgaged = cJSON_GetObjectItem(prop, "mortgaged");
            
            if (owner) g_synced_state.properties[i].owner = owner->valueint;
            if (upgrades) g_synced_state.properties[i].upgrades = upgrades->valueint;
            if (mortgaged) g_synced_state.properties[i].mortgaged = cJSON_IsTrue(mortgaged);
        }
    }
    
    cJSON_Delete(json);
    
    const char* state_names[] = {"WAITING_ROLL", "WAITING_BUY", "WAITING_DEBT", "PAUSED", "ENDED"};
    printf("[NET_GAME] State: %s, player %d's turn, dice=%d+%d\n",
           state_names[g_synced_state.state_type],
           g_synced_state.current_player + 1,
           g_synced_state.dice[0], g_synced_state.dice[1]);
}

// Parse game result from server
static void parseGameResult(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    GameResult* r = &g_synced_state.result;
    
    cJSON* item;
    if ((item = cJSON_GetObjectItem(json, "is_draw"))) r->is_draw = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItem(json, "winner_id"))) r->winner_id = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "loser_id"))) r->loser_id = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "winner_name"))) 
        strncpy(r->winner_name, item->valuestring, sizeof(r->winner_name) - 1);
    if ((item = cJSON_GetObjectItem(json, "loser_name"))) 
        strncpy(r->loser_name, item->valuestring, sizeof(r->loser_name) - 1);
    if ((item = cJSON_GetObjectItem(json, "winner_elo_change"))) r->winner_elo_change = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "loser_elo_change"))) r->loser_elo_change = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "winner_new_elo"))) r->winner_new_elo = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "loser_new_elo"))) r->loser_new_elo = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "reason"))) 
        strncpy(r->reason, item->valuestring, sizeof(r->reason) - 1);
    
    g_synced_state.has_result = 1;
    
    cJSON_Delete(json);
    
    printf("[NET_GAME] Game result: %s wins! (reason: %s)\n", 
           r->winner_name, r->reason);
}

int NetGame_processMessages(ClientState* client) {
    if (!client_is_connected(client)) {
        printf("[NET_GAME] Disconnected from server\n");
        netState = NET_GAME_ENDED;
        return 0;
    }
    
    // Send heartbeat every 15 seconds (more frequent to avoid timeout)
    time_t now = time(NULL);
    if (now - lastHeartbeat >= 15) {
        NetGame_sendHeartbeat(client);
        lastHeartbeat = now;
    }
    
    // Check for incoming messages
    while (client_data_available(client) > 0) {
        NetworkMessage msg;
        if (client_receive(client, &msg) == 0) {
            printf("[NET_GAME] Received message type: %d\n", msg.type);
            
            switch (msg.type) {
                case MSG_GAME_STATE:
                    parseGameState(msg.payload);
                    break;
                    
                case MSG_GAME_RESULT:
                    printf("[NET_GAME] Game result received\n");
                    parseGameResult(msg.payload);
                    netState = NET_GAME_ENDED;
                    return 0;
                    
                case MSG_NOT_YOUR_TURN:
                    printf("[NET_GAME] Not your turn!\n");
                    break;
                    
                case MSG_INVALID_MOVE:
                    {
                        cJSON* json = cJSON_Parse(msg.payload);
                        if (json) {
                            cJSON* error = cJSON_GetObjectItem(json, "error");
                            if (error && cJSON_IsString(error)) {
                                printf("[NET_GAME] Invalid move: %s\n", error->valuestring);
                                // Show error in game message
                                strncpy(g_synced_state.message2, error->valuestring, 
                                       sizeof(g_synced_state.message2) - 1);
                            }
                            cJSON_Delete(json);
                        }
                    }
                    break;
                    
                case MSG_ERROR:
                    {
                        cJSON* json = cJSON_Parse(msg.payload);
                        if (json) {
                            cJSON* error = cJSON_GetObjectItem(json, "error");
                            if (error && cJSON_IsString(error)) {
                                printf("[NET_GAME] Error: %s\n", error->valuestring);
                            }
                            cJSON_Delete(json);
                        }
                    }
                    break;
                    
                case MSG_HEARTBEAT_ACK:
                    // Heartbeat acknowledged
                    break;
                    
                default:
                    printf("[NET_GAME] Unknown message type: %d\n", msg.type);
                    break;
            }
        }
    }
    
    return (netState != NET_GAME_ENDED) ? 1 : 0;
}

NetGameState NetGame_getState(void) {
    return netState;
}

int NetGame_isMyTurn(void) {
    return (netState == NET_GAME_MY_TURN) && 
           (g_synced_state.current_player == myPlayerNum) &&
           !g_synced_state.paused;
}

GameStateType NetGame_getStateType(void) {
    return g_synced_state.state_type;
}

int NetGame_isWaitingBuy(void) {
    return g_synced_state.state_type == GSTATE_WAITING_BUY;
}

int NetGame_isPaused(void) {
    return g_synced_state.paused || netState == NET_GAME_PAUSED;
}

int NetGame_didWePause(void) {
    return g_synced_state.paused && (g_synced_state.paused_by == myPlayerNum);
}

int NetGame_getPlayerNum(void) {
    return myPlayerNum;
}

int NetGame_getMatchId(void) {
    return matchId;
}

const char* NetGame_getPlayerName(int idx) {
    if (idx >= 0 && idx < 2) {
        return g_synced_state.players[idx].username;
    }
    return "";
}

const char* NetGame_getMyName(void) {
    return myName;
}

const char* NetGame_getOpponentName(void) {
    return opponentName;
}

const GameResult* NetGame_getResult(void) {
    return &g_synced_state.result;
}

int NetGame_hasResult(void) {
    return g_synced_state.has_result;
}

// Helper to send game action
static void sendGameAction(ClientState* client, MessageType type, const char* extra_json) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "match_id", matchId);
    
    if (extra_json) {
        cJSON* extra = cJSON_Parse(extra_json);
        if (extra) {
            cJSON* item;
            cJSON_ArrayForEach(item, extra) {
                cJSON_AddItemToObject(json, item->string, cJSON_Duplicate(item, 1));
            }
            cJSON_Delete(extra);
        }
    }
    
    char* payload = cJSON_PrintUnformatted(json);
    client_send(client, type, payload);
    free(payload);
    cJSON_Delete(json);
}

void NetGame_sendHeartbeat(ClientState* client) {
    client_send(client, MSG_HEARTBEAT, NULL);
}

void NetGame_rollDice(ClientState* client) {
    if (!NetGame_isMyTurn()) {
        printf("[NET_GAME] Not your turn to roll\n");
        return;
    }
    
    if (g_synced_state.state_type != GSTATE_WAITING_ROLL) {
        printf("[NET_GAME] Not in rolling state (current: %d)\n", g_synced_state.state_type);
        return;
    }
    
    printf("[NET_GAME] Rolling dice...\n");
    sendGameAction(client, MSG_ROLL_DICE, NULL);
}

void NetGame_buyProperty(ClientState* client) {
    if (!NetGame_isMyTurn()) {
        printf("[NET_GAME] Not your turn\n");
        return;
    }
    
    if (g_synced_state.state_type != GSTATE_WAITING_BUY) {
        printf("[NET_GAME] Not in buying state\n");
        return;
    }
    
    printf("[NET_GAME] Buying property...\n");
    sendGameAction(client, MSG_BUY_PROPERTY, NULL);
}

void NetGame_skipProperty(ClientState* client) {
    if (!NetGame_isMyTurn()) {
        printf("[NET_GAME] Not your turn\n");
        return;
    }
    
    if (g_synced_state.state_type != GSTATE_WAITING_BUY) {
        printf("[NET_GAME] Not in buying state\n");
        return;
    }
    
    printf("[NET_GAME] Skipping property...\n");
    sendGameAction(client, MSG_SKIP_PROPERTY, NULL);
}

void NetGame_upgradeProperty(ClientState* client, int property_id) {
    char extra[64];
    snprintf(extra, sizeof(extra), "{\"property_id\": %d}", property_id);
    sendGameAction(client, MSG_UPGRADE_PROPERTY, extra);
}

void NetGame_downgradeProperty(ClientState* client, int property_id) {
    char extra[64];
    snprintf(extra, sizeof(extra), "{\"property_id\": %d}", property_id);
    sendGameAction(client, MSG_DOWNGRADE_PROPERTY, extra);
}

void NetGame_mortgageProperty(ClientState* client, int property_id) {
    char extra[64];
    snprintf(extra, sizeof(extra), "{\"property_id\": %d}", property_id);
    sendGameAction(client, MSG_MORTGAGE_PROPERTY, extra);
}

void NetGame_payJailFine(ClientState* client) {
    if (!NetGame_isMyTurn()) {
        printf("[NET_GAME] Not your turn\n");
        return;
    }
    
    printf("[NET_GAME] Paying jail fine...\n");
    sendGameAction(client, MSG_PAY_JAIL_FINE, NULL);
}

void NetGame_declareBankrupt(ClientState* client) {
    printf("[NET_GAME] Declaring bankruptcy...\n");
    sendGameAction(client, MSG_DECLARE_BANKRUPT, NULL);
}

void NetGame_surrender(ClientState* client) {
    printf("[NET_GAME] Surrendering...\n");
    sendGameAction(client, MSG_SURRENDER, NULL);
}

void NetGame_offerDraw(ClientState* client) {
    printf("[NET_GAME] Offering draw...\n");
    sendGameAction(client, MSG_GAME_END, NULL);
}

void NetGame_pause(ClientState* client) {
    printf("[NET_GAME] Pausing game...\n");
    sendGameAction(client, MSG_PAUSE_GAME, NULL);
}

void NetGame_resume(ClientState* client) {
    printf("[NET_GAME] Resuming game...\n");
    sendGameAction(client, MSG_RESUME_GAME, NULL);
}

void NetGame_close(void) {
    netState = NET_GAME_WAITING;
    myPlayerNum = 0;
    matchId = 0;
    opponentId = 0;
    opponentName[0] = '\0';
    myName[0] = '\0';
    memset(&g_synced_state, 0, sizeof(g_synced_state));
}
