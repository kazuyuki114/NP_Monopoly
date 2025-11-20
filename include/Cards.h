#pragma once
#include "Player.h"
#include "Property.h"

// Card types
typedef enum { CARD_CHANCE, CARD_COMMUNITY_CHEST } CardType;

// Card effect result
typedef struct {
  char *message;              // Message to display
  int cardIndex;              // Which card (0-15) for UI display
  CardType cardType;          // CARD_CHANCE or CARD_COMMUNITY_CHEST
  int moneyChange;            // Money to add/subtract (positive = add, negative =
                              // subtract)
  int newPosition;            // New position (-1 = no change)
  int goToJail;               // 1 = send to jail, 0 = no
  int collectFromPlayers;     // Money to collect from each player (0 = none)
  int payToPlayers;           // Money to pay to each player (0 = none)
  int getOutOfJailFree;       // 1 = give Get Out of Jail Free card, 0 = no
  int propertyRepairs;        // 1 = assess property repairs, 0 = no
  int houseRepairCost;        // Cost per house for repairs
  int hotelRepairCost;        // Cost per hotel for repairs
  Game_Prop_Type advanceToNearest; // Property type to advance to (0 = none)
} CardEffect;

// Forward declarations for game state access
typedef struct GameContext GameContext;

// Initialize card system
void Cards_init();

// Draw and execute a Chance card
CardEffect Cards_drawChance();

// Draw and execute a Community Chest card
CardEffect Cards_drawCommunityChest();

// Get card description for UI (for specific card index)
const char *Cards_getChanceDescription(int cardIndex);
const char *Cards_getCommunityChestDescription(int cardIndex);

// Shuffle decks
void Cards_shuffle();

// Get Out of Jail Free card management
int Cards_hasGetOutOfJailFree(int player);
void Cards_giveGetOutOfJailFree(int player);
void Cards_useGetOutOfJailFree(int player);
