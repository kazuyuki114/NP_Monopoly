#include "Game.h"
#include "Render.h"

int main(int argc, char *argv[]) {
  (void)argc; // Unused parameter
  (void)argv; // Unused parameter

  // Initialize SDL and rendering system
  if (Render_init() != 0) {
    return 1; // Initialization failed
  }

  // Initialize game state
  Game_init();

  // Main game loop
  Render_processEventsAndRender();

  // Clean up
  Render_close();

  return 0;
}

