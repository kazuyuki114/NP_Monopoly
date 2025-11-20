#define _CRT_SECURE_NO_WARNINGS

#include "Game.h"
#include "BoardData.h"
#include "Cards.h"
#include "Player.h"
#include "Property.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Game constants
#define TOTAL_PLAYERS 2
#define TOTAL_PROPERTIES 40
#define STARTING_MONEY 1500
#define GO_BONUS 200
#define JAIL_POSITION 10
#define JAIL_FINE 50
#define MAX_JAIL_TURNS 3
#define MAX_CONSECUTIVE_DOUBLES 3
#define MAX_HOUSES 5
#define INCOME_TAX 200
#define LUXURY_TAX 100

typedef enum {
  Game_STATE_BEGIN_MOVE,
  Game_STATE_BUY_PROPERTY,
  Game_STATE_PLAYER_IN_DEBT,
  Game_STATE_END
} Game_State;
static int current_move_player = 0;
static Game_Player Game_players[2];
static Game_Prop gProperties[40];
static Game_State gState;
static int lastRoll[2];
static int selectedProperty;
static char *message;
static char *message2;
static int turnsInJail[2]; // Track how many turns each player has been in jail
static int consecutiveDoubles[2]; // Track consecutive doubles for each player
static int
    justLeftJail; // Flag to prevent extra turn when leaving jail with doubles

// Active card display tracking
static int activeCardVisible = 0;
static CardType activeCardType;
static int activeCardIndex;

// Forward declarations
static void Game_remove_money(int player, int amount);
static int Game_isPlayerAMonopolist(int player, Game_Prop_Type type);

char *Game_getFormattedStatus(int player) {
  char *buffer = (char *)malloc(sizeof(char) * 20);

  if (current_move_player == player)
    sprintf(buffer, "%1s%-7s %4i$", ">", Game_players[player].name,
            Game_players[player].money);
  else
    sprintf(buffer, "%1s%-7s %4i$", "", Game_players[player].name,
            Game_players[player].money);
  return buffer;
}

static void updateMessage2(char *newMessage) {
  free(message2);
  message2 = newMessage;
}

void Game_init() {
  int i;

  // Initialize messages as static strings (no malloc needed)
  message = "";
  message2 = (char *)malloc(2);
  message2[0] = '\0';

  // Initialize board and cards using external modules
  BoardData_initializeBoard(gProperties);
  Cards_init();

  srand(time(NULL));
  current_move_player = 0;
  justLeftJail = 0;

  for (i = 0; i < TOTAL_PLAYERS; i++) {
    Game_players[i].id = i;
    Game_players[i].money = STARTING_MONEY;
    Game_players[i].position = 0;
    Game_players[i].jailed = 0;
    turnsInJail[i] = 0;
    consecutiveDoubles[i] = 0;
  }

  (Game_players[0]).name = "Red";
  (Game_players[1]).name = "Blue";

  gState = Game_STATE_BEGIN_MOVE;
}

static void nextPlayer() {
  if (Game_STATE_PLAYER_IN_DEBT != gState) {
    consecutiveDoubles[current_move_player] =
        0;            // Reset doubles count when switching players
    justLeftJail = 0; // Reset jail flag
    current_move_player = (current_move_player + 1) % TOTAL_PLAYERS;
  }
}

static void Game_send_to_jail(int player) {
  char *buffer = (char *)malloc(sizeof(char) * 40);
  sprintf(buffer, "%s sent to jail!", Game_players[player].name);
  updateMessage2(buffer);

  Game_players[player].jailed = 1;
  Game_players[player].position = JAIL_POSITION;
  turnsInJail[player] = 0;
  consecutiveDoubles[player] = 0;
  message = "In Jail! P) Pay $50 or roll doubles";
}

static void Game_pay_jail_fine(int player) {
  if (Game_players[player].money >= JAIL_FINE) {
    Game_remove_money(player, JAIL_FINE);
    Game_players[player].jailed = 0;
    turnsInJail[player] = 0;
    message = "Paid fine! Press SPACE to roll";
  } else {
    message = "Not enough money for fine!";
  }
}

static void Game_process_debts() {
  if (Game_STATE_PLAYER_IN_DEBT != gState)
    return;

  message = "DEBTS! Sell something";

  if (Game_players[current_move_player].money >= 0) {
    gState = Game_STATE_BEGIN_MOVE;
    nextPlayer();
    message = "";
  }
}

static void Game_pay_player(int payer, int receiever, int sum) {
  char *buffer = (char *)malloc(sizeof(char) * 30);
  sprintf(buffer, "%s paid %s $%i", Game_players[payer].name,
          Game_players[receiever].name, sum);
  updateMessage2(buffer);

  Game_players[payer].money -= sum;
  Game_players[receiever].money += sum;
  if (Game_players[payer].money < 0) {
    gState = Game_STATE_PLAYER_IN_DEBT;
    Game_process_debts();
  }
}

static void Game_remove_money(int player, int amount) {
  char *buffer = (char *)malloc(sizeof(char) * 30);
  sprintf(buffer, "%s paid $%i", Game_players[player].name, amount);
  updateMessage2(buffer);

  Game_players[player].money -= amount;

  if (Game_players[player].money < 0) {
    gState = Game_STATE_PLAYER_IN_DEBT;
    Game_process_debts();
  }
}

static void Game_roll_dice() {
  int i;
  for (i = 0; i < 2; i++)
    lastRoll[i] = rand() % 6 + 1;
}

static int Game_isPlayerAMonopolist(int player, Game_Prop_Type type);

static void Game_player_land(int newposition) {
  /* if a buyable property */
  if (gProperties[newposition].price > 0 &&
      gProperties[newposition].mortgaged == 0) {
    if (gProperties[newposition].owner == -1) {
      gState = Game_STATE_BUY_PROPERTY;
    } else if (gProperties[newposition].owner != current_move_player) {
      if (gProperties[newposition].type != Game_PT_UTILITY &&
          gProperties[newposition].type != Game_PT_RAILROAD) {
        int mult = 1;
        if (Game_isPlayerAMonopolist(gProperties[newposition].owner,
                                     gProperties[newposition].type) &&
            gProperties[newposition].upgrades == 0)
          mult = 2;
        Game_pay_player(current_move_player, gProperties[newposition].owner,
                        mult *
                            gProperties[newposition]
                                .rentCost[gProperties[newposition].upgrades]);
      }
      if (gProperties[newposition].type == Game_PT_RAILROAD) {
        int owner = gProperties[newposition].owner;
        int i, m = 1;
        for (i = 5; i <= 35; i += 10) {
          if (gProperties[i].owner == owner && gProperties[i].mortgaged == 0)
            m *= 2;
        }
        Game_pay_player(current_move_player, owner, 25 * m / 2);
      }
      if (gProperties[newposition].type == Game_PT_UTILITY) {
        int owner = gProperties[newposition].owner;
        int i, m = 0;
        for (i = 12; i <= 38; i += 16) {
          if (gProperties[i].owner == owner && gProperties[i].mortgaged == 0)
            m += 1;
        }

        i = (1 == m ? 4 : 10) * (lastRoll[0] + lastRoll[1]);
        Game_pay_player(current_move_player, owner, i);
      }
    }
  } else {
    switch (gProperties[newposition].type) {
    case Game_PT_GO:
      // Landing on GO gives bonus (in addition to passing GO bonus)
      Game_players[current_move_player].money += GO_BONUS;
      message = "Landed on GO! Collect $200";
      break;
    case Game_PT_JAIL:
      // Just visiting - no penalty
      message = "Just Visiting Jail";
      break;
    case Game_PT_CHANCE: {
      CardEffect effect = Cards_drawChance();
      message = effect.message;

      // Set active card for UI display
      activeCardVisible = 1;
      activeCardType = effect.cardType;
      activeCardIndex = effect.cardIndex;

      // Handle money changes
      if (effect.moneyChange > 0) {
        Game_players[current_move_player].money += effect.moneyChange;
      } else if (effect.moneyChange < 0) {
        Game_remove_money(current_move_player, -effect.moneyChange);
      }

      // Handle position changes
      if (effect.newPosition == -3) {
        // Go back 3 spaces
        Game_players[current_move_player].position =
            (Game_players[current_move_player].position - 3 +
             TOTAL_PROPERTIES) %
            TOTAL_PROPERTIES;
        Game_player_land(Game_players[current_move_player].position);
      } else if (effect.newPosition >= 0) {
        // Move to specific position
        int oldPos = Game_players[current_move_player].position;
        Game_players[current_move_player].position = effect.newPosition;
        // Check if passed GO (only if moving forward)
        if (effect.newPosition < oldPos && effect.newPosition != 0) {
          Game_players[current_move_player].money += GO_BONUS;
        }
        Game_player_land(Game_players[current_move_player].position);
      }

      // Handle advance to nearest property type
      if (effect.advanceToNearest != 0) {
        do {
          Game_players[current_move_player].position =
              (Game_players[current_move_player].position + 1) %
              TOTAL_PROPERTIES;
          if (Game_players[current_move_player].position == 0) {
            Game_players[current_move_player].money += GO_BONUS;
          }
        } while (gProperties[Game_players[current_move_player].position].type !=
                 effect.advanceToNearest);
        Game_player_land(Game_players[current_move_player].position);
      }

      // Handle Go to Jail
      if (effect.goToJail) {
        Game_send_to_jail(current_move_player);
      }

      // Handle Get Out of Jail Free
      if (effect.getOutOfJailFree) {
        Cards_giveGetOutOfJailFree(current_move_player);
      }

      // Handle property repairs
      if (effect.propertyRepairs) {
        int totalCost = 0;
        for (int i = 0; i < TOTAL_PROPERTIES; i++) {
          if (gProperties[i].owner == current_move_player) {
            if (gProperties[i].upgrades == 5) {
              // Hotel
              totalCost += effect.hotelRepairCost;
            } else if (gProperties[i].upgrades > 0) {
              // Houses
              totalCost += gProperties[i].upgrades * effect.houseRepairCost;
            }
          }
        }
        if (totalCost > 0) {
          Game_remove_money(current_move_player, totalCost);
        }
      }

      // Handle collect from players (multiplayer - simplified for 2 players)
      if (effect.collectFromPlayers > 0) {
        for (int i = 0; i < TOTAL_PLAYERS; i++) {
          if (i != current_move_player) {
            Game_pay_player(i, current_move_player, effect.collectFromPlayers);
          }
        }
      }

      // Handle pay to players (multiplayer - simplified for 2 players)
      if (effect.payToPlayers > 0) {
        for (int i = 0; i < TOTAL_PLAYERS; i++) {
          if (i != current_move_player) {
            Game_pay_player(current_move_player, i, effect.payToPlayers);
          }
        }
      }
    } break;
    case Game_PT_COMMUNITY_CHEST: {
      CardEffect effect = Cards_drawCommunityChest();
      message = effect.message;

      // Set active card for UI display
      activeCardVisible = 1;
      activeCardType = effect.cardType;
      activeCardIndex = effect.cardIndex;

      // Handle money changes
      if (effect.moneyChange > 0) {
        Game_players[current_move_player].money += effect.moneyChange;
      } else if (effect.moneyChange < 0) {
        Game_remove_money(current_move_player, -effect.moneyChange);
      }

      // Handle position changes
      if (effect.newPosition >= 0) {
        int oldPos = Game_players[current_move_player].position;
        Game_players[current_move_player].position = effect.newPosition;
        // Check if passed GO (only if moving forward)
        if (effect.newPosition < oldPos && effect.newPosition != 0) {
          Game_players[current_move_player].money += GO_BONUS;
        }
        Game_player_land(Game_players[current_move_player].position);
      }

      // Handle Go to Jail
      if (effect.goToJail) {
        Game_send_to_jail(current_move_player);
      }

      // Handle Get Out of Jail Free
      if (effect.getOutOfJailFree) {
        Cards_giveGetOutOfJailFree(current_move_player);
      }

      // Handle property repairs
      if (effect.propertyRepairs) {
        int totalCost = 0;
        for (int i = 0; i < TOTAL_PROPERTIES; i++) {
          if (gProperties[i].owner == current_move_player) {
            if (gProperties[i].upgrades == 5) {
              // Hotel
              totalCost += effect.hotelRepairCost;
            } else if (gProperties[i].upgrades > 0) {
              // Houses
              totalCost += gProperties[i].upgrades * effect.houseRepairCost;
            }
          }
        }
        if (totalCost > 0) {
          Game_remove_money(current_move_player, totalCost);
        }
      }

      // Handle collect from players (multiplayer - simplified for 2 players)
      if (effect.collectFromPlayers > 0) {
        for (int i = 0; i < TOTAL_PLAYERS; i++) {
          if (i != current_move_player) {
            Game_pay_player(i, current_move_player, effect.collectFromPlayers);
          }
        }
      }

      // Handle pay to players (multiplayer - simplified for 2 players)
      if (effect.payToPlayers > 0) {
        for (int i = 0; i < TOTAL_PLAYERS; i++) {
          if (i != current_move_player) {
            Game_pay_player(current_move_player, i, effect.payToPlayers);
          }
        }
      }
    } break;
    case Game_PT_TAX_INCOME:
      Game_remove_money(current_move_player, INCOME_TAX);
      break;
    case Game_PT_TAX_LUXURY:
      Game_remove_money(current_move_player, LUXURY_TAX);
      break;
    case Game_PT_GOTO_JAIL:
      Game_send_to_jail(current_move_player);
      break;
    default:
      break;
    }
  }
}

void Game_cycle() {
  int roll;
  int isDoubles;

  Game_roll_dice();
  roll = lastRoll[0] + lastRoll[1];
  isDoubles = (lastRoll[0] == lastRoll[1]);

  // Handle player in jail
  if (Game_players[current_move_player].jailed) {
    turnsInJail[current_move_player]++;

    if (isDoubles) {
      // Rolled doubles - get out of jail and move
      Game_players[current_move_player].jailed = 0;
      turnsInJail[current_move_player] = 0;
      justLeftJail = 1; // Mark that player just left jail
      message = "Rolled doubles! Out of jail!";

      if (Game_players[current_move_player].position + roll >= TOTAL_PROPERTIES)
        Game_players[current_move_player].money += GO_BONUS;
      Game_players[current_move_player].position =
          (Game_players[current_move_player].position + roll) %
          TOTAL_PROPERTIES;
      Game_player_land(Game_players[current_move_player].position);

      // Continue to handle consequences (buying property, etc.)
      // Turn will end based on normal logic but won't give extra turn for
      // doubles
    } else if (turnsInJail[current_move_player] >= MAX_JAIL_TURNS) {
      // Third turn in jail - must pay fine
      if (Game_players[current_move_player].money >= JAIL_FINE) {
        Game_remove_money(current_move_player, JAIL_FINE);
        Game_players[current_move_player].jailed = 0;
        turnsInJail[current_move_player] = 0;
        justLeftJail = 1; // Mark that player just left jail
        message = "3rd turn! Paid fine, rolled";

        if (Game_players[current_move_player].position + roll >=
            TOTAL_PROPERTIES)
          Game_players[current_move_player].money += GO_BONUS;
        Game_players[current_move_player].position =
            (Game_players[current_move_player].position + roll) %
            TOTAL_PROPERTIES;
        Game_player_land(Game_players[current_move_player].position);

        // Continue to handle consequences
      } else {
        message = "3rd turn but no money for fine!";
        gState = Game_STATE_PLAYER_IN_DEBT;
      }
    } else {
      // Failed to roll doubles, still in jail
      char *buffer = (char *)malloc(sizeof(char) * 50);
      sprintf(buffer, "No doubles. In jail %d/3 turns",
              turnsInJail[current_move_player]);
      message = buffer;
      nextPlayer();
    }
    return;
  }

  // Normal movement (not in jail)
  if (isDoubles) {
    consecutiveDoubles[current_move_player]++;
    if (consecutiveDoubles[current_move_player] >= MAX_CONSECUTIVE_DOUBLES) {
      // Three doubles in a row - go to jail
      Game_send_to_jail(current_move_player);
      nextPlayer();
      return;
    }
  } else {
    consecutiveDoubles[current_move_player] = 0;
  }

  if (Game_players[current_move_player].position + roll >= TOTAL_PROPERTIES)
    Game_players[current_move_player].money += GO_BONUS;
  Game_players[current_move_player].position =
      (Game_players[current_move_player].position + roll) % TOTAL_PROPERTIES;
  Game_player_land(Game_players[current_move_player].position);

  // End turn if not doubles, or if just left jail (no extra turn for doubles
  // when leaving jail)
  if (gState == Game_STATE_BEGIN_MOVE && (!isDoubles || justLeftJail)) {
    consecutiveDoubles[current_move_player] = 0;
    nextPlayer();
  }
}

void Game_buyProperty() {
  int newposition = Game_getPlayerPosition(current_move_player);
  if (gProperties[newposition].price > 0) {
    if (gProperties[newposition].owner == -1) {
      gState = Game_STATE_BEGIN_MOVE;
      Game_remove_money(current_move_player, gProperties[newposition].price);
      gProperties[newposition].owner = current_move_player;
    }
  }
}

int Game_getTotalPlayers() { return TOTAL_PLAYERS; }

int Game_getPlayerPosition(int playerid) {
  return Game_players[playerid].position;
}

int Game_getPropOwner(int propid) {
  if (gProperties[propid].price > 0) {
    return gProperties[propid].owner;
  } else
    return -1;
}

static void Game_mortageProp(int propid) {
  if (gProperties[propid].owner == current_move_player) {
    int i;
    if (gProperties[propid].upgrades > 0) {
      message = "Can't mortage with houses";
      return;
    }

    for (i = 0; i < TOTAL_PROPERTIES; i++) {
      if (gProperties[i].type == gProperties[propid].type) {
        if (gProperties[i].upgrades > 0) {
          message = "Destroy other houses first";
          return;
        }
      }
    }

    if (gProperties[propid].mortgaged == 0 &&
        gProperties[propid].upgrades == 0) {
      Game_players[current_move_player].money += gProperties[propid].price / 2;
      gProperties[propid].mortgaged = 1;
    } else {
      if (Game_players[current_move_player].money >=
          gProperties[propid].price / 2 * 1.1) {
        gProperties[propid].mortgaged = 0;
        Game_remove_money(current_move_player,
                          (int)(gProperties[propid].price / 2 * 1.1));
      }
    }
  } else {
    message = "Can't mortage that";
  }
}

static int Game_isPlayerAMonopolist(int player, Game_Prop_Type type) {
  int result = 1;
  int i;

  if (Game_PT_UTILITY == type || Game_PT_RAILROAD == type)
    return 0;

  for (i = 0; i < TOTAL_PROPERTIES; i++) {
    if (gProperties[i].type == type)
      if (gProperties[i].owner != player) {
        return 0;
      }
  }

  return result;
}

static int Game_isLegitUpgrade(int player, int propid) {
  int highest_upgrade = 0;
  int lowest_upgrade = MAX_HOUSES;
  int i;
  int this_upgrade = gProperties[propid].upgrades;
  Game_Prop_Type p_type = gProperties[propid].type;

  if (Game_isPlayerAMonopolist(player, p_type)) {
    for (i = 0; i < TOTAL_PROPERTIES; i++) {
      if (gProperties[i].type == p_type) {
        if (gProperties[i].mortgaged == 1)
          return 0;
        highest_upgrade = gProperties[i].upgrades > highest_upgrade
                              ? gProperties[i].upgrades
                              : highest_upgrade;
        lowest_upgrade = gProperties[i].upgrades < lowest_upgrade
                             ? gProperties[i].upgrades
                             : lowest_upgrade;
      }
    }
  }

  if (this_upgrade == lowest_upgrade)
    return 1;
  if (highest_upgrade == lowest_upgrade)
    return 1;
  return 0;
}

static int Game_isLegitDowngrade(int player, int propid) {
  int highest_upgrade = 0;
  int lowest_upgrade = MAX_HOUSES;
  int i;
  int this_upgrade = gProperties[propid].upgrades;
  Game_Prop_Type p_type = gProperties[propid].type;

  if (Game_isPlayerAMonopolist(player, p_type)) {
    for (i = 0; i < TOTAL_PROPERTIES; i++) {
      if (gProperties[i].type == p_type) {
        if (gProperties[i].mortgaged == 1)
          return 0;
        highest_upgrade = gProperties[i].upgrades > highest_upgrade
                              ? gProperties[i].upgrades
                              : highest_upgrade;
        lowest_upgrade = gProperties[i].upgrades < lowest_upgrade
                             ? gProperties[i].upgrades
                             : lowest_upgrade;
      }
    }
  }

  if (this_upgrade == highest_upgrade)
    return 1;
  if (highest_upgrade == lowest_upgrade)
    return 1;
  return 0;
}

static void Game_upgradeProp(int propid, int flag) {
  /*upgrade*/
  if (flag >= 0) {
    if (gProperties[propid].owner == current_move_player &&
        gProperties[propid].mortgaged == 0 &&
        gProperties[propid].upgrades < MAX_HOUSES) {

      if (Game_isLegitUpgrade(current_move_player, propid)) {
        if (Game_players[current_move_player].money >=
            gProperties[propid].upgradeCost) {
          gProperties[propid].upgrades++;
          Game_remove_money(current_move_player,
                            gProperties[propid].upgradeCost);
        } else {
          message = "Not enough $";
        }
      } else {
        message = "Not allowed to build";
      }
    } else
      message = "Can't build there";
  }
  /*downgrade*/
  else {
    if (gProperties[propid].owner == current_move_player &&
        gProperties[propid].mortgaged == 0 && gProperties[propid].upgrades > 0 &&
        Game_isLegitDowngrade(current_move_player, propid)) {
      gProperties[propid].upgrades--;
      Game_players[current_move_player].money +=
          gProperties[propid].upgradeCost / 2;
    } else {
      message = "Can't destroy there";
    }
  }
}

static void Game_goBankrupt() {
  if (Game_players[current_move_player].money < 0) {
    char *f = (char *)malloc(sizeof(char) * 40);
    int t = (current_move_player + 1) % TOTAL_PLAYERS;
    sprintf(f, "%s lost! %s won!", Game_players[current_move_player].name,
            Game_players[t].name);
    message = f;
    gState = Game_STATE_END;
  }
}

void Game_receiveinput(SDL_Keycode key) {
  if (key == SDLK_SPACE) {
    message = "";
    // Clear card display when space is pressed
    Game_clearActiveCard();
    
    switch (gState) {
    case Game_STATE_BEGIN_MOVE:
      Game_cycle();
      break;
    case Game_STATE_BUY_PROPERTY:
      Game_buyProperty();
      if (lastRoll[0] != lastRoll[1] || justLeftJail)
        nextPlayer();
      break;
    case Game_STATE_PLAYER_IN_DEBT:
      Game_process_debts();
      break;
    default:
      break;
    }
  }
  if (key == SDLK_p) {
    // Pay jail fine
    if (Game_players[current_move_player].jailed) {
      Game_pay_jail_fine(current_move_player);
    }
  }
  if (key == SDLK_g) {
    // Use Get Out of Jail Free card
    if (Game_players[current_move_player].jailed &&
        Cards_hasGetOutOfJailFree(current_move_player)) {
      Cards_useGetOutOfJailFree(current_move_player);
      Game_players[current_move_player].jailed = 0;
      turnsInJail[current_move_player] = 0;
      message = "Used Get Out of Jail Free card!";
    }
  }
  if (key == SDLK_n) {
    // Decline property purchase (skip buying)
    if (gState == Game_STATE_BUY_PROPERTY) {
      message = "Declined to buy property";
      gState = Game_STATE_BEGIN_MOVE;
      if (lastRoll[0] != lastRoll[1] || justLeftJail)
        nextPlayer();
    }
  }
  if (key == SDLK_m) {
    if (selectedProperty >= 0 && selectedProperty < 40) {
      Game_mortageProp(selectedProperty);
    }
  }
  if (key == SDLK_x) {
    Game_goBankrupt();
  }
  if (key == SDLK_b) {
    Game_upgradeProp(selectedProperty, 1);
  }
  if (key == SDLK_d) {
    Game_upgradeProp(selectedProperty, -1);
  }
}

void Game_getLastRoll(int *a, int *b) {
  *a = lastRoll[0];
  *b = lastRoll[1];
}

char *Game_getText(int line) {
  if (Game_STATE_END == gState) {
    if (line == 0)
      return message;
    else
      return NULL;
  }

  switch (line) {
  case 4:
    if (Game_players[current_move_player].jailed) {
      return "SPACE) Try roll doubles (IN JAIL!)";
    }
    if (Game_STATE_BEGIN_MOVE == gState)
      return "SPACE) Roll and jump";
    if (Game_STATE_BUY_PROPERTY == gState)
      return "SPACE) Buy property | N) Skip";
    if (Game_players[current_move_player].money < 0)
      return "    X) Go Bankrupt";
    return "";
    break;
  case 1:
    if (Game_players[current_move_player].jailed) {
      if (Cards_hasGetOutOfJailFree(current_move_player)) {
        return "    P) Pay $50 | G) Use GOOJF card";
      }
      return "    P) Pay $50 fine (IN JAIL)";
    }
    return "    M) Mortage";
    break;
  case 2:
    return "    B) Build";
    break;
  case 3:
    return "    D) Destroy";
    break;
  case 5:
    return message2;
    break;
  case 0:
    return message;
    break;
  }

  return NULL;
}

void Game_selectProperty(int propid) {
  if (propid > 0 && propid < TOTAL_PROPERTIES)
    selectedProperty = propid;
}

int Game_getPropLevel(int id) { return gProperties[id].upgrades; }

int Game_getPropMortageStatus(int id) { return gProperties[id].mortgaged; }

int Game_isPlayerJailed(int playerid) { return Game_players[playerid].jailed; }

// Card display functions
int Game_hasActiveCard(void) { return activeCardVisible; }

CardType Game_getActiveCardType(void) { return activeCardType; }

int Game_getActiveCardIndex(void) { return activeCardIndex; }

void Game_clearActiveCard(void) { activeCardVisible = 0; }
