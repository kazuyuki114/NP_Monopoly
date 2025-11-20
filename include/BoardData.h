#pragma once
#include "Property.h"

// Board space names (for reference and debugging)
extern const char *BOARD_SPACE_NAMES[40];

// Initialize all board spaces with their properties
void BoardData_initializeBoard(Game_Prop *properties);

// Helper to get a property by position
const char *BoardData_getSpaceName(int position);
