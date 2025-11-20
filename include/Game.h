#pragma once
#include <SDL.h>
#include "Cards.h"

char *Game_getFormattedStatus(int player);

void Game_init();

void Game_cycle();

int Game_getTotalPlayers();

int Game_getPlayerPosition(int playerid);

int Game_getPropOwner(int propid);

void Game_receiveinput(SDL_Keycode);

void Game_getLastRoll(int *a, int *b);

char *Game_getText(int line);

int Game_getPropLevel(int);

int Game_getPropMortageStatus(int);

void Game_selectProperty(int propid);

int Game_isPlayerJailed(int playerid);

// Card display functions
int Game_hasActiveCard(void);               // Returns 1 if card should be displayed
CardType Game_getActiveCardType(void);      // CARD_CHANCE or CARD_COMMUNITY_CHEST
int Game_getActiveCardIndex(void);          // 0-15 card index
void Game_clearActiveCard(void);            // Clear card display
