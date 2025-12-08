#pragma once
#include "Property.h"

// Board space names (for reference and debugging)
extern const char *BOARD_SPACE_NAMES[40];

// Rent increase rate when passing GO (e.g., 0.10 = 10% increase)
// Change this value to adjust the inflation rate
#define RENT_INCREASE_RATE 0.10f

// Initialize all board spaces with their properties
void BoardData_initializeBoard(Game_Prop *properties);

// Helper to get a property by position
const char *BoardData_getSpaceName(int position);

// Increase all property rents by the given rate (e.g., 0.10 for 10%)
void BoardData_increaseAllRents(Game_Prop *properties, float rate);
