#include "Cards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Card indices for the 16 Chance and 16 Community Chest cards
static int chanceCards[16];
static int communityChestCards[16];
static int chanceIndex = 0;
static int communityChestIndex = 0;

// Get Out of Jail Free cards held by players
static int playerGetOutOfJailFreeCards[2] = {0, 0}; // For 2 players

// Chance card descriptions
static const char *CHANCE_DESCRIPTIONS[16] = {
    "Advance to GO (Collect $200)",
    "Advance to Illinois Avenue",
    "Advance to St. Charles Place",
    "Advance token to nearest Utility",
    "Advance token to nearest Railroad",
    "Advance token to nearest Railroad",
    "Bank pays you dividend of $50",
    "Get Out of Jail Free",
    "Go Back 3 Spaces",
    "Go to Jail",
    "Make general repairs - $25 per house, $100 per hotel",
    "Pay poor tax of $15",
    "Take a trip to Reading Railroad",
    "Take a walk on the Boardwalk",
    "You have been elected Chairman of the Board - Pay each player $50",
    "Your building loan matures - Collect $150"};

// Community Chest card descriptions
static const char *COMMUNITY_CHEST_DESCRIPTIONS[16] = {
    "Advance to GO (Collect $200)",
    "Bank error in your favor - Collect $200",
    "Doctor's fee - Pay $50",
    "From sale of stock you get $50",
    "Get Out of Jail Free",
    "Go to Jail",
    "Grand Opera Night - Collect $50 from every player",
    "Holiday Fund matures - Collect $100",
    "Income tax refund - Collect $20",
    "It is your birthday - Collect $10 from each player",
    "Life insurance matures - Collect $100",
    "Hospital fees - Pay $100",
    "School fees - Pay $150",
    "Receive for services $25",
    "You are assessed for street repairs - $40 per house, $115 per hotel",
    "You have won second prize in a beauty contest - Collect $10"};

// Fisher-Yates shuffle algorithm
static void shuffleDeck(int *deck, int size) {
  for (int i = size - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int temp = deck[i];
    deck[i] = deck[j];
    deck[j] = temp;
  }
}

void Cards_init() {
  // Initialize card decks with indices 0-15
  for (int i = 0; i < 16; i++) {
    chanceCards[i] = i;
    communityChestCards[i] = i;
  }

  // Shuffle both decks
  Cards_shuffle();

  // Reset indices
  chanceIndex = 0;
  communityChestIndex = 0;

  // Reset Get Out of Jail Free cards
  playerGetOutOfJailFreeCards[0] = 0;
  playerGetOutOfJailFreeCards[1] = 0;
}

void Cards_shuffle() {
  shuffleDeck(chanceCards, 16);
  shuffleDeck(communityChestCards, 16);
}

CardEffect Cards_drawChance() {
  CardEffect effect = {0};
  effect.newPosition = -1; // -1 means no position change

  // Get next card and advance index
  int cardIndex = chanceCards[chanceIndex];
  chanceIndex = (chanceIndex + 1) % 16; // Wrap around to beginning

  // Set card info for UI display
  effect.cardIndex = cardIndex;
  effect.cardType = CARD_CHANCE;

  // Allocate message buffer
  effect.message = (char *)malloc(80);

  switch (cardIndex) {
  case 0: // Advance to GO
    sprintf(effect.message, "CHANCE: Advance to GO, collect $200");
    effect.newPosition = 0;
    effect.moneyChange = 200;
    break;

  case 1: // Advance to Illinois Avenue (Position 24)
    sprintf(effect.message, "CHANCE: Advance to Illinois Avenue");
    effect.newPosition = 24;
    break;

  case 2: // Advance to St. Charles Place (Position 11)
    sprintf(effect.message, "CHANCE: Advance to St. Charles Place");
    effect.newPosition = 11;
    break;

  case 3: // Advance to nearest Utility
    sprintf(effect.message, "CHANCE: Advance to nearest Utility");
    effect.advanceToNearest = Game_PT_UTILITY;
    break;

  case 4: // Advance to nearest Railroad (1st copy)
  case 5: // Advance to nearest Railroad (2nd copy)
    sprintf(effect.message, "CHANCE: Advance to nearest Railroad");
    effect.advanceToNearest = Game_PT_RAILROAD;
    break;

  case 6: // Bank pays dividend
    sprintf(effect.message, "CHANCE: Bank pays you dividend of $50");
    effect.moneyChange = 50;
    break;

  case 7: // Get Out of Jail Free
    sprintf(effect.message, "CHANCE: Get Out of Jail Free");
    effect.getOutOfJailFree = 1;
    break;

  case 8: // Go back 3 spaces
    sprintf(effect.message, "CHANCE: Go back 3 spaces");
    effect.newPosition = -3; // Special value meaning "go back 3"
    break;

  case 9: // Go to Jail
    sprintf(effect.message, "CHANCE: Go to Jail!");
    effect.goToJail = 1;
    break;

  case 10: // Property repairs
    sprintf(effect.message,
            "CHANCE: Make general repairs - $25 per house, $100 per hotel");
    effect.propertyRepairs = 1;
    effect.houseRepairCost = 25;
    effect.hotelRepairCost = 100;
    break;

  case 11: // Pay poor tax
    sprintf(effect.message, "CHANCE: Pay poor tax of $15");
    effect.moneyChange = -15;
    break;

  case 12: // Reading Railroad (Position 5)
    sprintf(effect.message, "CHANCE: Take a trip to Reading Railroad");
    effect.newPosition = 5;
    break;

  case 13: // Boardwalk (Position 39)
    sprintf(effect.message, "CHANCE: Take a walk on the Boardwalk");
    effect.newPosition = 39;
    break;

  case 14: // Pay each player $50
    sprintf(effect.message, "CHANCE: Elected Chairman - Pay each player $50");
    effect.payToPlayers = 50;
    break;

  case 15: // Collect $150
    sprintf(effect.message, "CHANCE: Building loan matures - Collect $150");
    effect.moneyChange = 150;
    break;
  }

  return effect;
}

CardEffect Cards_drawCommunityChest() {
  CardEffect effect = {0};
  effect.newPosition = -1; // -1 means no position change

  // Get next card and advance index
  int cardIndex = communityChestCards[communityChestIndex];
  communityChestIndex = (communityChestIndex + 1) % 16; // Wrap around

  // Set card info for UI display
  effect.cardIndex = cardIndex;
  effect.cardType = CARD_COMMUNITY_CHEST;

  // Allocate message buffer
  effect.message = (char *)malloc(80);

  switch (cardIndex) {
  case 0: // Advance to GO
    sprintf(effect.message, "COMMUNITY CHEST: Advance to GO, collect $200");
    effect.newPosition = 0;
    effect.moneyChange = 200;
    break;

  case 1: // Bank error
    sprintf(effect.message,
            "COMMUNITY CHEST: Bank error in your favor - Collect $200");
    effect.moneyChange = 200;
    break;

  case 2: // Doctor's fee
    sprintf(effect.message, "COMMUNITY CHEST: Doctor's fee - Pay $50");
    effect.moneyChange = -50;
    break;

  case 3: // Stock sale
    sprintf(effect.message, "COMMUNITY CHEST: From sale of stock you get $50");
    effect.moneyChange = 50;
    break;

  case 4: // Get Out of Jail Free
    sprintf(effect.message, "COMMUNITY CHEST: Get Out of Jail Free");
    effect.getOutOfJailFree = 1;
    break;

  case 5: // Go to Jail
    sprintf(effect.message, "COMMUNITY CHEST: Go to Jail!");
    effect.goToJail = 1;
    break;

  case 6: // Grand Opera Night
    sprintf(
        effect.message,
        "COMMUNITY CHEST: Grand Opera Night - Collect $50 from each player");
    effect.collectFromPlayers = 50;
    break;

  case 7: // Holiday Fund
    sprintf(effect.message,
            "COMMUNITY CHEST: Holiday Fund matures - Collect $100");
    effect.moneyChange = 100;
    break;

  case 8: // Income tax refund
    sprintf(effect.message, "COMMUNITY CHEST: Income tax refund - Collect $20");
    effect.moneyChange = 20;
    break;

  case 9: // Birthday
    sprintf(
        effect.message,
        "COMMUNITY CHEST: It is your birthday - Collect $10 from each player");
    effect.collectFromPlayers = 10;
    break;

  case 10: // Life insurance
    sprintf(effect.message,
            "COMMUNITY CHEST: Life insurance matures - Collect $100");
    effect.moneyChange = 100;
    break;

  case 11: // Hospital fees
    sprintf(effect.message, "COMMUNITY CHEST: Hospital fees - Pay $100");
    effect.moneyChange = -100;
    break;

  case 12: // School fees
    sprintf(effect.message, "COMMUNITY CHEST: School fees - Pay $150");
    effect.moneyChange = -150;
    break;

  case 13: // Services rendered
    sprintf(effect.message, "COMMUNITY CHEST: Receive for services $25");
    effect.moneyChange = 25;
    break;

  case 14: // Street repairs
    sprintf(effect.message,
            "COMMUNITY CHEST: Street repairs - $40 per house, $115 per hotel");
    effect.propertyRepairs = 1;
    effect.houseRepairCost = 40;
    effect.hotelRepairCost = 115;
    break;

  case 15: // Beauty contest
    sprintf(effect.message,
            "COMMUNITY CHEST: Won beauty contest - Collect $10");
    effect.moneyChange = 10;
    break;
  }

  return effect;
}

const char *Cards_getChanceDescription(int cardIndex) {
  if (cardIndex >= 0 && cardIndex < 16) {
    return CHANCE_DESCRIPTIONS[cardIndex];
  }
  return "Unknown card";
}

const char *Cards_getCommunityChestDescription(int cardIndex) {
  if (cardIndex >= 0 && cardIndex < 16) {
    return COMMUNITY_CHEST_DESCRIPTIONS[cardIndex];
  }
  return "Unknown card";
}

// Get Out of Jail Free card management
int Cards_hasGetOutOfJailFree(int player) {
  if (player >= 0 && player < 2) {
    return playerGetOutOfJailFreeCards[player];
  }
  return 0;
}

void Cards_giveGetOutOfJailFree(int player) {
  if (player >= 0 && player < 2) {
    playerGetOutOfJailFreeCards[player]++;
  }
}

void Cards_useGetOutOfJailFree(int player) {
  if (player >= 0 && player < 2 && playerGetOutOfJailFreeCards[player] > 0) {
    playerGetOutOfJailFreeCards[player]--;
  }
}
