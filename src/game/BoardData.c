#include "BoardData.h"
#include "Property.h"

// Board space names for each position
const char *BOARD_SPACE_NAMES[40] = {
    "GO",                    // 0
    "Mediterranean Avenue",  // 1
    "Community Chest",       // 2
    "Baltic Avenue",         // 3
    "Income Tax",            // 4
    "Reading Railroad",      // 5
    "Oriental Avenue",       // 6
    "Chance",                // 7
    "Vermont Avenue",        // 8
    "Connecticut Avenue",    // 9
    "Jail / Just Visiting",  // 10
    "St. Charles Place",     // 11
    "Electric Company",      // 12
    "States Avenue",         // 13
    "Virginia Avenue",       // 14
    "Pennsylvania Railroad", // 15
    "St. James Place",       // 16
    "Community Chest",       // 17
    "Tennessee Avenue",      // 18
    "New York Avenue",       // 19
    "Free Parking",          // 20
    "Kentucky Avenue",       // 21
    "Chance",                // 22
    "Indiana Avenue",        // 23
    "Illinois Avenue",       // 24
    "B&O Railroad",          // 25
    "Atlantic Avenue",       // 26
    "Ventnor Avenue",        // 27
    "Water Works",           // 28
    "Marvin Gardens",        // 29
    "Go To Jail",            // 30
    "Pacific Avenue",        // 31
    "North Carolina Avenue", // 32
    "Community Chest",       // 33
    "Pennsylvania Avenue",   // 34
    "Short Line Railroad",   // 35
    "Chance",                // 36
    "Park Place",            // 37
    "Luxury Tax",            // 38
    "Boardwalk"              // 39
};

const char *BoardData_getSpaceName(int position) {
  if (position >= 0 && position < 40) {
    return BOARD_SPACE_NAMES[position];
  }
  return "Unknown";
}

void BoardData_initializeBoard(Game_Prop *properties) {
  int i;

  // Initialize all spaces to default values
  for (i = 0; i < 40; ++i) {
    properties[i].id = i;
    properties[i].mortgaged = 0;
    properties[i].owner = -1;
    properties[i].upgrades = 0;
    properties[i].price = 0;
    properties[i].upgradeCost = 0;
    for (int j = 0; j < 6; ++j) {
      properties[i].rentCost[j] = 0;
    }
  }

  // Special spaces (no price)
  properties[0].type = Game_PT_GO;
  properties[10].type = Game_PT_JAIL;
  properties[20].type = Game_PT_FREE_PARK;
  properties[30].type = Game_PT_GOTO_JAIL;

  // Tax spaces
  properties[4].type = Game_PT_TAX_INCOME;
  properties[38].type = Game_PT_TAX_LUXURY;

  // Chance spaces
  properties[7].type = Game_PT_CHANCE;
  properties[22].type = Game_PT_CHANCE;
  properties[36].type = Game_PT_CHANCE;

  // Community Chest spaces
  properties[2].type = Game_PT_COMMUNITY_CHEST;
  properties[17].type = Game_PT_COMMUNITY_CHEST;
  properties[33].type = Game_PT_COMMUNITY_CHEST;

  // Railroads (all $200)
  for (i = 5; i <= 35; i += 10) {
    properties[i].type = Game_PT_RAILROAD;
    properties[i].price = 200;
  }

  // Utilities (both $150)
  properties[12].type = Game_PT_UTILITY;
  properties[12].price = 150;
  properties[28].type = Game_PT_UTILITY;
  properties[28].price = 150;

  // ===== PURPLE PROPERTIES =====
  // Mediterranean Avenue (Position 1)
  properties[1].type = Game_PT_PURPLE;
  properties[1].price = 60;
  properties[1].rentCost[0] = 2;
  properties[1].rentCost[1] = 10;
  properties[1].rentCost[2] = 30;
  properties[1].rentCost[3] = 90;
  properties[1].rentCost[4] = 160;
  properties[1].rentCost[5] = 250;
  properties[1].upgradeCost = 50;

  // Baltic Avenue (Position 3)
  properties[3].type = Game_PT_PURPLE;
  properties[3].price = 60;
  properties[3].rentCost[0] = 4;
  properties[3].rentCost[1] = 20;
  properties[3].rentCost[2] = 60;
  properties[3].rentCost[3] = 180;
  properties[3].rentCost[4] = 320;
  properties[3].rentCost[5] = 450;
  properties[3].upgradeCost = 50;

  // ===== LIGHT BLUE PROPERTIES =====
  // Oriental Avenue (Position 6)
  properties[6].type = Game_PT_LIGHTBLUE;
  properties[6].price = 100;
  properties[6].rentCost[0] = 6;
  properties[6].rentCost[1] = 30;
  properties[6].rentCost[2] = 90;
  properties[6].rentCost[3] = 270;
  properties[6].rentCost[4] = 400;
  properties[6].rentCost[5] = 550;
  properties[6].upgradeCost = 50;

  // Vermont Avenue (Position 8)
  properties[8].type = Game_PT_LIGHTBLUE;
  properties[8].price = 100;
  properties[8].rentCost[0] = 6;
  properties[8].rentCost[1] = 30;
  properties[8].rentCost[2] = 90;
  properties[8].rentCost[3] = 270;
  properties[8].rentCost[4] = 400;
  properties[8].rentCost[5] = 550;
  properties[8].upgradeCost = 50;

  // Connecticut Avenue (Position 9)
  properties[9].type = Game_PT_LIGHTBLUE;
  properties[9].price = 120;
  properties[9].rentCost[0] = 8;
  properties[9].rentCost[1] = 40;
  properties[9].rentCost[2] = 100;
  properties[9].rentCost[3] = 300;
  properties[9].rentCost[4] = 450;
  properties[9].rentCost[5] = 600;
  properties[9].upgradeCost = 50;

  // ===== MAGENTA/PINK PROPERTIES =====
  // St. Charles Place (Position 11)
  properties[11].type = Game_PT_MAGNETA;
  properties[11].price = 140;
  properties[11].rentCost[0] = 10;
  properties[11].rentCost[1] = 50;
  properties[11].rentCost[2] = 150;
  properties[11].rentCost[3] = 450;
  properties[11].rentCost[4] = 625;
  properties[11].rentCost[5] = 750;
  properties[11].upgradeCost = 100;

  // States Avenue (Position 13)
  properties[13].type = Game_PT_MAGNETA;
  properties[13].price = 140;
  properties[13].rentCost[0] = 10;
  properties[13].rentCost[1] = 50;
  properties[13].rentCost[2] = 150;
  properties[13].rentCost[3] = 450;
  properties[13].rentCost[4] = 625;
  properties[13].rentCost[5] = 750;
  properties[13].upgradeCost = 100;

  // Virginia Avenue (Position 14)
  properties[14].type = Game_PT_MAGNETA;
  properties[14].price = 160;
  properties[14].rentCost[0] = 12;
  properties[14].rentCost[1] = 60;
  properties[14].rentCost[2] = 180;
  properties[14].rentCost[3] = 500;
  properties[14].rentCost[4] = 700;
  properties[14].rentCost[5] = 900;
  properties[14].upgradeCost = 100;

  // ===== ORANGE PROPERTIES =====
  // St. James Place (Position 16)
  properties[16].type = Game_PT_ORANGE;
  properties[16].price = 180;
  properties[16].rentCost[0] = 14;
  properties[16].rentCost[1] = 70;
  properties[16].rentCost[2] = 200;
  properties[16].rentCost[3] = 550;
  properties[16].rentCost[4] = 750;
  properties[16].rentCost[5] = 950;
  properties[16].upgradeCost = 100;

  // Tennessee Avenue (Position 18)
  properties[18].type = Game_PT_ORANGE;
  properties[18].price = 180;
  properties[18].rentCost[0] = 14;
  properties[18].rentCost[1] = 70;
  properties[18].rentCost[2] = 200;
  properties[18].rentCost[3] = 550;
  properties[18].rentCost[4] = 750;
  properties[18].rentCost[5] = 950;
  properties[18].upgradeCost = 100;

  // New York Avenue (Position 19)
  properties[19].type = Game_PT_ORANGE;
  properties[19].price = 200;
  properties[19].rentCost[0] = 16;
  properties[19].rentCost[1] = 80;
  properties[19].rentCost[2] = 220;
  properties[19].rentCost[3] = 600;
  properties[19].rentCost[4] = 800;
  properties[19].rentCost[5] = 1000;
  properties[19].upgradeCost = 100;

  // ===== RED PROPERTIES =====
  // Kentucky Avenue (Position 21)
  properties[21].type = Game_PT_RED;
  properties[21].price = 220;
  properties[21].rentCost[0] = 18;
  properties[21].rentCost[1] = 90;
  properties[21].rentCost[2] = 250;
  properties[21].rentCost[3] = 700;
  properties[21].rentCost[4] = 875;
  properties[21].rentCost[5] = 1050;
  properties[21].upgradeCost = 150;

  // Indiana Avenue (Position 23)
  properties[23].type = Game_PT_RED;
  properties[23].price = 220;
  properties[23].rentCost[0] = 18;
  properties[23].rentCost[1] = 90;
  properties[23].rentCost[2] = 250;
  properties[23].rentCost[3] = 700;
  properties[23].rentCost[4] = 875;
  properties[23].rentCost[5] = 1050;
  properties[23].upgradeCost = 150;

  // Illinois Avenue (Position 24)
  properties[24].type = Game_PT_RED;
  properties[24].price = 240;
  properties[24].rentCost[0] = 20;
  properties[24].rentCost[1] = 100;
  properties[24].rentCost[2] = 300;
  properties[24].rentCost[3] = 750;
  properties[24].rentCost[4] = 925;
  properties[24].rentCost[5] = 1100;
  properties[24].upgradeCost = 150;

  // ===== YELLOW PROPERTIES =====
  // Atlantic Avenue (Position 26)
  properties[26].type = Game_PT_YELLOW;
  properties[26].price = 260;
  properties[26].rentCost[0] = 22;
  properties[26].rentCost[1] = 110;
  properties[26].rentCost[2] = 330;
  properties[26].rentCost[3] = 800;
  properties[26].rentCost[4] = 975;
  properties[26].rentCost[5] = 1150;
  properties[26].upgradeCost = 150;

  // Ventnor Avenue (Position 27)
  properties[27].type = Game_PT_YELLOW;
  properties[27].price = 260;
  properties[27].rentCost[0] = 22;
  properties[27].rentCost[1] = 110;
  properties[27].rentCost[2] = 330;
  properties[27].rentCost[3] = 800;
  properties[27].rentCost[4] = 975;
  properties[27].rentCost[5] = 1150;
  properties[27].upgradeCost = 150;

  // Marvin Gardens (Position 29)
  properties[29].type = Game_PT_YELLOW;
  properties[29].price = 280;
  properties[29].rentCost[0] = 24;
  properties[29].rentCost[1] = 120;
  properties[29].rentCost[2] = 360;
  properties[29].rentCost[3] = 850;
  properties[29].rentCost[4] = 1025;
  properties[29].rentCost[5] = 1200;
  properties[29].upgradeCost = 150;

  // ===== GREEN PROPERTIES =====
  // Pacific Avenue (Position 31)
  properties[31].type = Game_PT_GREEN;
  properties[31].price = 300;
  properties[31].rentCost[0] = 26;
  properties[31].rentCost[1] = 130;
  properties[31].rentCost[2] = 390;
  properties[31].rentCost[3] = 900;
  properties[31].rentCost[4] = 1100;
  properties[31].rentCost[5] = 1275;
  properties[31].upgradeCost = 200;

  // North Carolina Avenue (Position 32)
  properties[32].type = Game_PT_GREEN;
  properties[32].price = 300;
  properties[32].rentCost[0] = 26;
  properties[32].rentCost[1] = 130;
  properties[32].rentCost[2] = 390;
  properties[32].rentCost[3] = 900;
  properties[32].rentCost[4] = 1100;
  properties[32].rentCost[5] = 1275;
  properties[32].upgradeCost = 200;

  // Pennsylvania Avenue (Position 34)
  properties[34].type = Game_PT_GREEN;
  properties[34].price = 320;
  properties[34].rentCost[0] = 28;
  properties[34].rentCost[1] = 150;
  properties[34].rentCost[2] = 450;
  properties[34].rentCost[3] = 1000;
  properties[34].rentCost[4] = 1200;
  properties[34].rentCost[5] = 1400;
  properties[34].upgradeCost = 200;

  // ===== BLUE PROPERTIES =====
  // Park Place (Position 37)
  properties[37].type = Game_PT_BLUE;
  properties[37].price = 350;
  properties[37].rentCost[0] = 35;
  properties[37].rentCost[1] = 175;
  properties[37].rentCost[2] = 500;
  properties[37].rentCost[3] = 1100;
  properties[37].rentCost[4] = 1300;
  properties[37].rentCost[5] = 1500;
  properties[37].upgradeCost = 200;

  // Boardwalk (Position 39)
  properties[39].type = Game_PT_BLUE;
  properties[39].price = 400;
  properties[39].rentCost[0] = 50;
  properties[39].rentCost[1] = 200;
  properties[39].rentCost[2] = 600;
  properties[39].rentCost[3] = 1400;
  properties[39].rentCost[4] = 1700;
  properties[39].rentCost[5] = 2000;
  properties[39].upgradeCost = 200;
}
