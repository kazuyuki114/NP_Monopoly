/*
 * Game State Manager Implementation
 * 
 * Server-side game logic for multiplayer Monopoly
 */

#include "game_state.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global state
ActiveGame active_games[MAX_ACTIVE_GAMES];
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

// Property data (simplified - prices for streets)
static const int property_prices[40] = {
    0,    // GO
    60, 0, 60, 0, 200, 100, 0, 100, 120,  // 1-9
    0,    // Jail
    140, 150, 140, 160, 200, 180, 0, 180, 200,  // 11-19
    0,    // Free Parking
    220, 0, 220, 240, 200, 260, 260, 150, 280,  // 21-29
    0,    // Go to Jail
    300, 300, 0, 320, 200, 0, 350, 0, 400  // 31-39
};

static const int property_rents[40][6] = {
    {0,0,0,0,0,0},  // GO
    {2, 10, 30, 90, 160, 250},  // Mediterranean
    {0,0,0,0,0,0},  // Community Chest
    {4, 20, 60, 180, 320, 450},  // Baltic
    {0,0,0,0,0,0},  // Income Tax
    {25, 50, 100, 200, 0, 0},   // Railroad
    {6, 30, 90, 270, 400, 550}, // Oriental
    {0,0,0,0,0,0},  // Chance
    {6, 30, 90, 270, 400, 550}, // Vermont
    {8, 40, 100, 300, 450, 600}, // Connecticut
    {0,0,0,0,0,0},  // Jail
    {10, 50, 150, 450, 625, 750}, // St. Charles
    {4, 10, 0, 0, 0, 0},  // Electric Company (utility)
    {10, 50, 150, 450, 625, 750}, // States
    {12, 60, 180, 500, 700, 900}, // Virginia
    {25, 50, 100, 200, 0, 0},   // Railroad
    {14, 70, 200, 550, 750, 950}, // St. James
    {0,0,0,0,0,0},  // Community Chest
    {14, 70, 200, 550, 750, 950}, // Tennessee
    {16, 80, 220, 600, 800, 1000}, // New York
    {0,0,0,0,0,0},  // Free Parking
    {18, 90, 250, 700, 875, 1050}, // Kentucky
    {0,0,0,0,0,0},  // Chance
    {18, 90, 250, 700, 875, 1050}, // Indiana
    {20, 100, 300, 750, 925, 1100}, // Illinois
    {25, 50, 100, 200, 0, 0},   // Railroad
    {22, 110, 330, 800, 975, 1150}, // Atlantic
    {22, 110, 330, 800, 975, 1150}, // Ventnor
    {4, 10, 0, 0, 0, 0},  // Water Works (utility)
    {24, 120, 360, 850, 1025, 1200}, // Marvin Gardens
    {0,0,0,0,0,0},  // Go to Jail
    {26, 130, 390, 900, 1100, 1275}, // Pacific
    {26, 130, 390, 900, 1100, 1275}, // North Carolina
    {0,0,0,0,0,0},  // Community Chest
    {28, 150, 450, 1000, 1200, 1400}, // Pennsylvania
    {25, 50, 100, 200, 0, 0},   // Railroad
    {0,0,0,0,0,0},  // Chance
    {35, 175, 500, 1100, 1300, 1500}, // Park Place
    {0,0,0,0,0,0},  // Luxury Tax
    {50, 200, 600, 1400, 1700, 2000}  // Boardwalk
};

static const int upgrade_costs[40] = {
    0, 50, 0, 50, 0, 0, 50, 0, 50, 50,
    0, 100, 0, 100, 100, 0, 100, 0, 100, 100,
    0, 150, 0, 150, 150, 0, 150, 150, 0, 150,
    0, 200, 200, 0, 200, 0, 0, 200, 0, 200
};

// Property types (simplified)
static PropertyType get_property_type(int pos) {
    if (pos == 0) return PROP_GO;
    if (pos == 10) return PROP_JAIL;
    if (pos == 20) return PROP_FREE_PARKING;
    if (pos == 30) return PROP_GOTO_JAIL;
    if (pos == 2 || pos == 17 || pos == 33) return PROP_COMMUNITY_CHEST;
    if (pos == 7 || pos == 22 || pos == 36) return PROP_CHANCE;
    if (pos == 4) return PROP_TAX;  // Income Tax
    if (pos == 38) return PROP_TAX; // Luxury Tax
    if (pos == 5 || pos == 15 || pos == 25 || pos == 35) return PROP_RAILROAD;
    if (pos == 12 || pos == 28) return PROP_UTILITY;
    return PROP_STREET;
}

void game_state_init(void) {
    pthread_mutex_lock(&games_mutex);
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        active_games[i].active = 0;
        active_games[i].match_id = 0;
        pthread_mutex_init(&active_games[i].mutex, NULL);
    }
    pthread_mutex_unlock(&games_mutex);
    printf("[GAME_STATE] Initialized game state manager\n");
}

ActiveGame* game_create(int match_id, int p1_user_id, const char* p1_name,
                        int p2_user_id, const char* p2_name) {
    pthread_mutex_lock(&games_mutex);
    
    ActiveGame* game = NULL;
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (!active_games[i].active) {
            game = &active_games[i];
            break;
        }
    }
    
    if (!game) {
        pthread_mutex_unlock(&games_mutex);
        fprintf(stderr, "[GAME_STATE] No free game slots!\n");
        return NULL;
    }
    
    // Initialize game
    pthread_mutex_lock(&game->mutex);
    
    game->active = 1;
    game->match_id = match_id;
    game->current_player = 0;  // Player 1 goes first
    game->state = GSTATE_WAITING_ROLL;
    game->state_before_pause = GSTATE_WAITING_ROLL;
    game->paused = 0;
    game->paused_by = -1;
    game->last_roll[0] = 0;
    game->last_roll[1] = 0;
    game->just_left_jail = 0;
    game->move_count = 0;
    game->message[0] = '\0';
    game->message2[0] = '\0';
    
    // Initialize players
    game->players[0].user_id = p1_user_id;
    strncpy(game->players[0].username, p1_name, sizeof(game->players[0].username) - 1);
    game->players[0].money = STARTING_MONEY;
    game->players[0].position = 0;
    game->players[0].jailed = 0;
    game->players[0].turns_in_jail = 0;
    game->players[0].consecutive_doubles = 0;
    
    game->players[1].user_id = p2_user_id;
    strncpy(game->players[1].username, p2_name, sizeof(game->players[1].username) - 1);
    game->players[1].money = STARTING_MONEY;
    game->players[1].position = 0;
    game->players[1].jailed = 0;
    game->players[1].turns_in_jail = 0;
    game->players[1].consecutive_doubles = 0;
    
    // Initialize properties
    for (int i = 0; i < TOTAL_PROPERTIES; i++) {
        game->properties[i].owner = -1;
        game->properties[i].upgrades = 0;
        game->properties[i].mortgaged = 0;
    }
    
    pthread_mutex_unlock(&game->mutex);
    pthread_mutex_unlock(&games_mutex);
    
    printf("[GAME_STATE] Created game for match %d: %s vs %s\n", 
           match_id, p1_name, p2_name);
    
    return game;
}

ActiveGame* game_find(int match_id) {
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (active_games[i].active && active_games[i].match_id == match_id) {
            return &active_games[i];
        }
    }
    return NULL;
}

ActiveGame* game_find_by_player(int user_id) {
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (active_games[i].active) {
            if (active_games[i].players[0].user_id == user_id ||
                active_games[i].players[1].user_id == user_id) {
                return &active_games[i];
            }
        }
    }
    return NULL;
}

void game_destroy(int match_id) {
    pthread_mutex_lock(&games_mutex);
    for (int i = 0; i < MAX_ACTIVE_GAMES; i++) {
        if (active_games[i].active && active_games[i].match_id == match_id) {
            pthread_mutex_lock(&active_games[i].mutex);
            active_games[i].active = 0;
            active_games[i].match_id = 0;
            pthread_mutex_unlock(&active_games[i].mutex);
            printf("[GAME_STATE] Destroyed game for match %d\n", match_id);
            break;
        }
    }
    pthread_mutex_unlock(&games_mutex);
}

// Helper: send player to jail
static void send_to_jail(ActiveGame* game, int player_idx) {
    game->players[player_idx].jailed = 1;
    game->players[player_idx].position = JAIL_POSITION;
    game->players[player_idx].turns_in_jail = 0;
    game->players[player_idx].consecutive_doubles = 0;
    snprintf(game->message, sizeof(game->message), "%s sent to jail!", 
             game->players[player_idx].username);
}

// Helper: next player's turn
static void next_player(ActiveGame* game) {
    game->players[game->current_player].consecutive_doubles = 0;
    game->just_left_jail = 0;
    game->current_player = 1 - game->current_player;
    game->state = GSTATE_WAITING_ROLL;
}

// Helper: check if player owns all of a color group
static int owns_monopoly(ActiveGame* game, int player_idx, int prop_id) {
    // Simplified check - would need full color group data
    // For now, just return 0 (no monopoly bonus)
    (void)game; (void)player_idx; (void)prop_id;
    return 0;
}

// Helper: handle landing on a property
static void handle_landing(ActiveGame* game, int player_idx, int position) {
    PropertyType type = get_property_type(position);
    GamePlayerState* player = &game->players[player_idx];
    
    switch (type) {
        case PROP_GO:
            player->money += GO_BONUS;
            snprintf(game->message, sizeof(game->message), "Landed on GO! Collect $200");
            break;
            
        case PROP_STREET:
        case PROP_RAILROAD:
        case PROP_UTILITY:
            if (game->properties[position].owner == -1) {
                // Unowned - can buy
                if (player->money >= property_prices[position]) {
                    game->state = GSTATE_WAITING_BUY;
                    snprintf(game->message, sizeof(game->message), 
                             "Buy for $%d? (SPACE=buy, N=skip)", property_prices[position]);
                }
            } else if (game->properties[position].owner != player_idx) {
                // Owned by opponent - pay rent
                int owner = game->properties[position].owner;
                int rent = property_rents[position][game->properties[position].upgrades];
                
                // Double rent for monopoly (no houses)
                if (owns_monopoly(game, owner, position) && 
                    game->properties[position].upgrades == 0) {
                    rent *= 2;
                }
                
                player->money -= rent;
                game->players[owner].money += rent;
                snprintf(game->message, sizeof(game->message), 
                         "Paid $%d rent to %s", rent, game->players[owner].username);
                
                if (player->money < 0) {
                    game->state = GSTATE_WAITING_DEBT;
                }
            }
            break;
            
        case PROP_TAX:
            if (position == 4) {
                player->money -= 200;  // Income Tax
                snprintf(game->message, sizeof(game->message), "Income Tax: $200");
            } else {
                player->money -= 100;  // Luxury Tax
                snprintf(game->message, sizeof(game->message), "Luxury Tax: $100");
            }
            if (player->money < 0) {
                game->state = GSTATE_WAITING_DEBT;
            }
            break;
            
        case PROP_GOTO_JAIL:
            send_to_jail(game, player_idx);
            break;
            
        case PROP_JAIL:
            snprintf(game->message, sizeof(game->message), "Just Visiting Jail");
            break;
            
        case PROP_CHANCE:
        case PROP_COMMUNITY_CHEST:
            // Simplified - just give/take random amount
            {
                int amount = (rand() % 200) - 50;
                player->money += amount;
                if (amount >= 0) {
                    snprintf(game->message, sizeof(game->message), 
                             "Card: Received $%d", amount);
                } else {
                    snprintf(game->message, sizeof(game->message), 
                             "Card: Pay $%d", -amount);
                }
            }
            break;
            
        case PROP_FREE_PARKING:
            snprintf(game->message, sizeof(game->message), "Free Parking");
            break;
    }
}

int game_roll_dice(ActiveGame* game, int player_idx) {
    if (!game || game->current_player != player_idx) {
        return -1;
    }
    
    if (game->state != GSTATE_WAITING_ROLL) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    // Roll dice
    int die1 = (rand() % 6) + 1;
    int die2 = (rand() % 6) + 1;
    int total = die1 + die2;
    int is_doubles = (die1 == die2);
    
    game->last_roll[0] = die1;
    game->last_roll[1] = die2;
    game->move_count++;
    
    GamePlayerState* player = &game->players[player_idx];
    
    // Handle jail
    if (player->jailed) {
        player->turns_in_jail++;
        
        if (is_doubles) {
            // Rolled doubles - get out
            player->jailed = 0;
            player->turns_in_jail = 0;
            game->just_left_jail = 1;
            snprintf(game->message, sizeof(game->message), "Rolled doubles! Out of jail!");
        } else if (player->turns_in_jail >= MAX_JAIL_TURNS) {
            // 3rd turn - must pay
            if (player->money >= JAIL_FINE) {
                player->money -= JAIL_FINE;
                player->jailed = 0;
                player->turns_in_jail = 0;
                game->just_left_jail = 1;
                snprintf(game->message, sizeof(game->message), "3rd turn - paid $50 fine");
            } else {
                game->state = GSTATE_WAITING_DEBT;
                snprintf(game->message, sizeof(game->message), "Can't afford $50 fine!");
                pthread_mutex_unlock(&game->mutex);
                return 0;
            }
        } else {
            // Still in jail
            snprintf(game->message, sizeof(game->message), 
                     "In jail %d/3 turns. P to pay $50", player->turns_in_jail);
            next_player(game);
            pthread_mutex_unlock(&game->mutex);
            return 0;
        }
    }
    
    // Check for 3 doubles
    if (is_doubles && !game->just_left_jail) {
        player->consecutive_doubles++;
        if (player->consecutive_doubles >= 3) {
            send_to_jail(game, player_idx);
            next_player(game);
            pthread_mutex_unlock(&game->mutex);
            return 0;
        }
    } else {
        player->consecutive_doubles = 0;
    }
    
    // Move player
    int old_pos = player->position;
    player->position = (player->position + total) % TOTAL_PROPERTIES;
    
    // Check if passed GO
    if (player->position < old_pos && player->position != 0) {
        player->money += GO_BONUS;
    }
    
    // Handle landing
    handle_landing(game, player_idx, player->position);
    
    // If not buying, check for end of turn
    if (game->state == GSTATE_WAITING_ROLL) {
        if (!is_doubles || game->just_left_jail) {
            next_player(game);
        }
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_buy_property(ActiveGame* game, int player_idx) {
    if (!game || game->current_player != player_idx || game->state != GSTATE_WAITING_BUY) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    GamePlayerState* player = &game->players[player_idx];
    int pos = player->position;
    int price = property_prices[pos];
    
    if (player->money >= price && game->properties[pos].owner == -1) {
        player->money -= price;
        game->properties[pos].owner = player_idx;
        snprintf(game->message, sizeof(game->message), "Bought property for $%d", price);
    }
    
    game->state = GSTATE_WAITING_ROLL;
    
    // Check for doubles
    if (game->last_roll[0] != game->last_roll[1] || game->just_left_jail) {
        next_player(game);
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_skip_property(ActiveGame* game, int player_idx) {
    if (!game || game->current_player != player_idx || game->state != GSTATE_WAITING_BUY) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    snprintf(game->message, sizeof(game->message), "Declined to buy");
    game->state = GSTATE_WAITING_ROLL;
    
    if (game->last_roll[0] != game->last_roll[1] || game->just_left_jail) {
        next_player(game);
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_pay_jail_fine(ActiveGame* game, int player_idx) {
    if (!game || game->current_player != player_idx) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    GamePlayerState* player = &game->players[player_idx];
    
    if (player->jailed && player->money >= JAIL_FINE) {
        player->money -= JAIL_FINE;
        player->jailed = 0;
        player->turns_in_jail = 0;
        snprintf(game->message, sizeof(game->message), "Paid $50 fine - out of jail!");
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_declare_bankrupt(ActiveGame* game, int player_idx) {
    if (!game) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    game->state = GSTATE_ENDED;
    game->players[player_idx].money = -1;  // Mark as bankrupt
    snprintf(game->message, sizeof(game->message), 
             "%s is bankrupt! %s wins!", 
             game->players[player_idx].username,
             game->players[1 - player_idx].username);
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_upgrade_property(ActiveGame* game, int player_idx, int prop_id) {
    if (!game || prop_id < 0 || prop_id >= TOTAL_PROPERTIES) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    PropertyState* prop = &game->properties[prop_id];
    GamePlayerState* player = &game->players[player_idx];
    int cost = upgrade_costs[prop_id];
    
    if (prop->owner == player_idx && !prop->mortgaged && 
        prop->upgrades < 5 && player->money >= cost && cost > 0) {
        player->money -= cost;
        prop->upgrades++;
        snprintf(game->message, sizeof(game->message), "Built house for $%d", cost);
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_downgrade_property(ActiveGame* game, int player_idx, int prop_id) {
    if (!game || prop_id < 0 || prop_id >= TOTAL_PROPERTIES) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    PropertyState* prop = &game->properties[prop_id];
    GamePlayerState* player = &game->players[player_idx];
    int cost = upgrade_costs[prop_id];
    
    if (prop->owner == player_idx && prop->upgrades > 0) {
        player->money += cost / 2;
        prop->upgrades--;
        snprintf(game->message, sizeof(game->message), "Sold house for $%d", cost / 2);
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_mortgage_property(ActiveGame* game, int player_idx, int prop_id) {
    if (!game || prop_id < 0 || prop_id >= TOTAL_PROPERTIES) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    PropertyState* prop = &game->properties[prop_id];
    GamePlayerState* player = &game->players[player_idx];
    int price = property_prices[prop_id];
    
    if (prop->owner == player_idx) {
        if (!prop->mortgaged && prop->upgrades == 0) {
            // Mortgage
            player->money += price / 2;
            prop->mortgaged = 1;
            snprintf(game->message, sizeof(game->message), "Mortgaged for $%d", price / 2);
        } else if (prop->mortgaged && player->money >= (int)(price * 0.55)) {
            // Unmortgage
            int cost = (int)(price * 0.55);
            player->money -= cost;
            prop->mortgaged = 0;
            snprintf(game->message, sizeof(game->message), "Unmortgaged for $%d", cost);
        }
    }
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_pause(ActiveGame* game, int player_idx) {
    if (!game || game->paused || game->state == GSTATE_ENDED) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    game->paused = 1;
    game->paused_by = player_idx;
    game->state_before_pause = game->state;
    game->state = GSTATE_PAUSED;
    snprintf(game->message, sizeof(game->message), "Game paused by %s",
             game->players[player_idx].username);
    
    printf("[GAME_STATE] Game %d paused by player %d\n", game->match_id, player_idx);
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_resume(ActiveGame* game, int player_idx) {
    if (!game || !game->paused) {
        return -1;
    }
    
    // Only the player who paused can resume
    if (game->paused_by != player_idx) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    game->paused = 0;
    game->state = game->state_before_pause;
    snprintf(game->message, sizeof(game->message), "Game resumed");
    
    printf("[GAME_STATE] Game %d resumed by player %d\n", game->match_id, player_idx);
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

int game_surrender(ActiveGame* game, int player_idx) {
    if (!game || game->state == GSTATE_ENDED) {
        return -1;
    }
    
    pthread_mutex_lock(&game->mutex);
    
    game->state = GSTATE_ENDED;
    game->paused = 0;
    game->players[player_idx].money = -1;  // Mark as loser
    snprintf(game->message, sizeof(game->message), 
             "%s surrendered! %s wins!", 
             game->players[player_idx].username,
             game->players[1 - player_idx].username);
    
    printf("[GAME_STATE] Game %d: %s surrendered\n", game->match_id, 
           game->players[player_idx].username);
    
    pthread_mutex_unlock(&game->mutex);
    return 0;
}

char* game_serialize_state(ActiveGame* game) {
    if (!game) return NULL;
    
    pthread_mutex_lock(&game->mutex);
    
    cJSON* json = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(json, "match_id", game->match_id);
    cJSON_AddNumberToObject(json, "current_player", game->current_player);
    cJSON_AddNumberToObject(json, "state", game->state);
    cJSON_AddNumberToObject(json, "move_count", game->move_count);
    cJSON_AddBoolToObject(json, "paused", game->paused);
    cJSON_AddNumberToObject(json, "paused_by", game->paused_by);
    
    // Dice
    cJSON* dice = cJSON_CreateArray();
    cJSON_AddItemToArray(dice, cJSON_CreateNumber(game->last_roll[0]));
    cJSON_AddItemToArray(dice, cJSON_CreateNumber(game->last_roll[1]));
    cJSON_AddItemToObject(json, "dice", dice);
    
    // Messages
    cJSON_AddStringToObject(json, "message", game->message);
    cJSON_AddStringToObject(json, "message2", game->message2);
    
    // Players
    cJSON* players = cJSON_CreateArray();
    for (int i = 0; i < 2; i++) {
        cJSON* p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "user_id", game->players[i].user_id);
        cJSON_AddStringToObject(p, "username", game->players[i].username);
        cJSON_AddNumberToObject(p, "money", game->players[i].money);
        cJSON_AddNumberToObject(p, "position", game->players[i].position);
        cJSON_AddBoolToObject(p, "jailed", game->players[i].jailed);
        cJSON_AddNumberToObject(p, "turns_in_jail", game->players[i].turns_in_jail);
        cJSON_AddItemToArray(players, p);
    }
    cJSON_AddItemToObject(json, "players", players);
    
    // Properties
    cJSON* properties = cJSON_CreateArray();
    for (int i = 0; i < TOTAL_PROPERTIES; i++) {
        cJSON* prop = cJSON_CreateObject();
        cJSON_AddNumberToObject(prop, "owner", game->properties[i].owner);
        cJSON_AddNumberToObject(prop, "upgrades", game->properties[i].upgrades);
        cJSON_AddBoolToObject(prop, "mortgaged", game->properties[i].mortgaged);
        cJSON_AddItemToArray(properties, prop);
    }
    cJSON_AddItemToObject(json, "properties", properties);
    
    char* result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    pthread_mutex_unlock(&game->mutex);
    
    return result;
}

int game_get_winner(ActiveGame* game) {
    if (!game || game->state != GSTATE_ENDED) {
        return -1;
    }
    
    // The player with money >= 0 wins
    for (int i = 0; i < 2; i++) {
        if (game->players[i].money >= 0) {
            return game->players[i].user_id;
        }
    }
    return -1;
}

int game_get_loser(ActiveGame* game) {
    if (!game || game->state != GSTATE_ENDED) {
        return -1;
    }
    
    // The player with money < 0 (bankrupt) loses
    for (int i = 0; i < 2; i++) {
        if (game->players[i].money < 0) {
            return game->players[i].user_id;
        }
    }
    return -1;
}

