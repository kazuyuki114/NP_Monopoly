#define _CRT_SECURE_NO_WARNINGS

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Game.h"
#include "Render.h"
#include "Cards.h"

static SDL_Window *gWindow;
static SDL_Renderer *gRenderer;
static TTF_Font *gFont;
static TTF_Font *gFontSmall; // Smaller font for sidebar

static SDL_Texture *background;
static SDL_Texture *player;
static SDL_Texture *ownerBanner;
static SDL_Texture *diceSheet;
static SDL_Texture *house;
static SDL_Texture *cross;

static int SCREEN_WIDTH = 1000;  // Increased from 800 to add sidebar
static int SCREEN_HEIGHT = 800;
static int BOARD_WIDTH = 800;    // Board is 800x800 (not affected by sidebar)
static int BOARD_HEIGHT = 800;
static int SIDEBAR_X = 810;      // Sidebar starts at x=810
static int SIDEBAR_WIDTH = 190;  // Sidebar is 190px wide

// Player animation tracking
static float playerAnimX[2] = {0, 0};
static float playerAnimY[2] = {0, 0};
static int playerTargetX[2] = {0, 0};
static int playerTargetY[2] = {0, 0};
static const float ANIM_SPEED = 8.0f; // Pixels per frame

static SDL_Texture *loadTexture(const char *, SDL_Renderer *const);

static int xCross = -1000;
static int yCross = -1000;

int Render_init(void) {
  int returnValue = 0;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
    returnValue = 1;
  } else {
    // Set texture filtering to linear
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
      printf("Warning: Linear texture filtering not enabled!");
    }

    // Create window
    gWindow = SDL_CreateWindow("Monopoly", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
                               SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (gWindow == NULL) {
      printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
      returnValue = 1;
    } else {
      // Create renderer for window
      gRenderer = SDL_CreateRenderer(
          gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
      if (gRenderer == NULL) {
        printf("Renderer could not be created! SDL Error: %s\n",
               SDL_GetError());
        returnValue = 1;
      } else {
        // Initialize renderer color
        SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
      }
    }
  }

  if (TTF_Init() == -1) {
    printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
    returnValue = 1;
  }

  gFont = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 32);
  if (gFont == NULL) {
    printf("Failed to load lazy font! SDL_ttf Error: %s\n", TTF_GetError());
    returnValue = 1;
  }

  gFontSmall = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 16);
  if (gFontSmall == NULL) {
    printf("Failed to load small font! SDL_ttf Error: %s\n", TTF_GetError());
    returnValue = 1;
  }

  background = loadTexture("assets/images/monopoly.bmp", gRenderer);
  player = loadTexture("assets/images/player.bmp", gRenderer);
  ownerBanner = loadTexture("assets/images/ownerbanner.bmp", gRenderer);
  diceSheet = loadTexture("assets/images/dice.bmp", gRenderer);
  house = loadTexture("assets/images/house.bmp", gRenderer);
  cross = loadTexture("assets/images/cross.bmp", gRenderer);

  return returnValue;
}

void Render_close(void) {
  /*SDL_DestroyTexture( gTexture );
  gTexture = NULL;*/

  SDL_DestroyTexture(background);
  SDL_DestroyTexture(player);
  SDL_DestroyTexture(ownerBanner);
  SDL_DestroyTexture(diceSheet);
  SDL_DestroyTexture(house);
  SDL_DestroyTexture(cross);

  // Destroy window
  SDL_DestroyRenderer(gRenderer);
  SDL_DestroyWindow(gWindow);
  gWindow = NULL;
  gRenderer = NULL;

  // Quit SDL subsystems
  SDL_Quit();
}

static SDL_Texture *loadTexture(const char *file,
                                SDL_Renderer *const renderer) {
  SDL_Surface *loadedImage = NULL;
  SDL_Texture *texture = NULL;
  loadedImage = SDL_LoadBMP(file);
  if (loadedImage != NULL) {
    SDL_SetColorKey(loadedImage, 1, 0x323232);
    texture = SDL_CreateTextureFromSurface(renderer, loadedImage);

    SDL_FreeSurface(loadedImage);
  } else
    return NULL;
  /*logSDLError("Image could not be loaded!");*/
  return texture;
}

static void renderText(const char *text, int x, int y, SDL_Color color) {
  // SDL_Color color = {0, 255, 0};  // this is the color in rgb format, maxing
  // out all would give you the color white, and it will be your text's color
  SDL_Rect Message_rect; // create a rect
  SDL_Surface *surfaceMessage = TTF_RenderText_Blended(gFont, text, color);
  SDL_Texture *Message = SDL_CreateTextureFromSurface(
      gRenderer, surfaceMessage); // now you can convert it into a texture
  SDL_FreeSurface(surfaceMessage);

  Message_rect.x = x; // controls the rect's x coordinate
  Message_rect.y = y; // controls the rect's y coordinte
  /*Message_rect.w = 100; // controls the width of the rect
  Message_rect.h = 10; // controls the height of the rect*/

  SDL_QueryTexture(Message, NULL, NULL, &Message_rect.w, &Message_rect.h);
  SDL_RenderCopy(gRenderer, Message, NULL, &Message_rect); //
  SDL_DestroyTexture(Message);
}

static void renderTextSmall(const char *text, int x, int y, SDL_Color color) {
  SDL_Rect Message_rect;
  SDL_Surface *surfaceMessage = TTF_RenderText_Blended(gFontSmall, text, color);
  SDL_Texture *Message = SDL_CreateTextureFromSurface(gRenderer, surfaceMessage);
  SDL_FreeSurface(surfaceMessage);

  Message_rect.x = x;
  Message_rect.y = y;

  SDL_QueryTexture(Message, NULL, NULL, &Message_rect.w, &Message_rect.h);
  SDL_RenderCopy(gRenderer, Message, NULL, &Message_rect);
  SDL_DestroyTexture(Message);
}

static void renderTextWithStyle(const char *text, int x, int y, SDL_Color color,
                                int underline) {
  SDL_Rect Message_rect;

  // Set underline style if requested
  if (underline) {
    TTF_SetFontStyle(gFont, TTF_STYLE_UNDERLINE);
  }

  SDL_Surface *surfaceMessage = TTF_RenderText_Blended(gFont, text, color);
  SDL_Texture *Message =
      SDL_CreateTextureFromSurface(gRenderer, surfaceMessage);
  SDL_FreeSurface(surfaceMessage);

  Message_rect.x = x;
  Message_rect.y = y;

  SDL_QueryTexture(Message, NULL, NULL, &Message_rect.w, &Message_rect.h);
  SDL_RenderCopy(gRenderer, Message, NULL, &Message_rect);
  SDL_DestroyTexture(Message);

  // Reset font style back to normal
  if (underline) {
    TTF_SetFontStyle(gFont, TTF_STYLE_NORMAL);
  }
}

static void renderTexture(SDL_Texture *tex, int x, int y) {
  SDL_Rect pos;
  pos.x = x;
  pos.y = y;
  SDL_QueryTexture(tex, NULL, NULL, &pos.w, &pos.h);
  SDL_RenderCopy(gRenderer, tex, NULL, &pos);
}

static void renderTextureRot(SDL_Texture *tex, int x, int y, double degrees) {
  SDL_Rect pos;
  pos.x = x;
  pos.y = y;
  SDL_QueryTexture(tex, NULL, NULL, &pos.w, &pos.h);
  SDL_RenderCopyEx(gRenderer, tex, NULL, &pos, degrees, NULL, SDL_FLIP_NONE);
}

// Helper function to calculate target position for a game space
static void getPositionForSpace(int game_position, int *xPos, int *yPos) {
  const int PROP_WIDTH = 66;
  const int PROP_HEIGHT = 80;
  const int GO_WIDTH = 103;
  const int PLAYER_SIZE = 24;

  if (game_position <= 10) {
    *xPos = BOARD_WIDTH -
           (GO_WIDTH / 2 + PROP_WIDTH * (game_position + 1) - PLAYER_SIZE);
    *yPos = BOARD_HEIGHT - PROP_HEIGHT;
  } else if (game_position >= 11 && game_position <= 19) {
    *xPos = BOARD_WIDTH - (GO_WIDTH / 2 + PROP_WIDTH * 11 - PLAYER_SIZE);
    *yPos = BOARD_HEIGHT -
           (GO_WIDTH / 2 + PROP_WIDTH * (game_position - 9) - PLAYER_SIZE);
  } else if (game_position >= 20 && game_position <= 30) {
    *xPos = BOARD_WIDTH -
           (GO_WIDTH / 2 + PROP_WIDTH * (31 - game_position) - PLAYER_SIZE);
    *yPos = PROP_HEIGHT - PLAYER_SIZE;
  } else if (game_position >= 31 && game_position <= 39) {
    *xPos = BOARD_WIDTH - (GO_WIDTH / 2 + PROP_WIDTH - PLAYER_SIZE);
    *yPos = BOARD_HEIGHT -
           (GO_WIDTH / 2 + PROP_WIDTH * (41 - game_position) - PLAYER_SIZE);
  }
}

// Draw a player token as a colored circle with border
static void drawPlayerToken(int x, int y, int playerid) {
  const int RADIUS = 12;
  const int BORDER = 2;
  
  // Offset players slightly so they don't overlap completely
  if (playerid == 1) {
    x += 8;
    y += 8;
  }
  
  // Draw outer border (black)
  SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
  for (int dy = -RADIUS - BORDER; dy <= RADIUS + BORDER; dy++) {
    for (int dx = -RADIUS - BORDER; dx <= RADIUS + BORDER; dx++) {
      if (dx * dx + dy * dy <= (RADIUS + BORDER) * (RADIUS + BORDER)) {
        SDL_RenderDrawPoint(gRenderer, x + dx, y + dy);
      }
    }
  }
  
  // Draw inner circle (colored)
  if (playerid == 0) {
    SDL_SetRenderDrawColor(gRenderer, 220, 50, 50, 255); // Red
  } else {
    SDL_SetRenderDrawColor(gRenderer, 50, 50, 220, 255); // Blue
  }
  
  for (int dy = -RADIUS; dy <= RADIUS; dy++) {
    for (int dx = -RADIUS; dx <= RADIUS; dx++) {
      if (dx * dx + dy * dy <= RADIUS * RADIUS) {
        SDL_RenderDrawPoint(gRenderer, x + dx, y + dy);
      }
    }
  }
  
  // Add highlight for 3D effect
  SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 180);
  for (int dy = -RADIUS / 2; dy <= 0; dy++) {
    for (int dx = -RADIUS / 2; dx <= 0; dx++) {
      if (dx * dx + dy * dy <= (RADIUS / 2) * (RADIUS / 2)) {
        SDL_RenderDrawPoint(gRenderer, x + dx - 2, y + dy - 2);
      }
    }
  }
}

void renderPlayerTextureAtPos(SDL_Texture *player_texture, int game_position,
                              int playerid) {
  (void)player_texture; // Unused - we draw custom tokens now
  
  int targetX, targetY;
  getPositionForSpace(game_position, &targetX, &targetY);
  
  // Update target position
  playerTargetX[playerid] = targetX;
  playerTargetY[playerid] = targetY;
  
  // Initialize animation position if first frame
  if (playerAnimX[playerid] == 0 && playerAnimY[playerid] == 0) {
    playerAnimX[playerid] = (float)targetX;
    playerAnimY[playerid] = (float)targetY;
  }
  
  // Smooth animation towards target
  float dx = targetX - playerAnimX[playerid];
  float dy = targetY - playerAnimY[playerid];
  float distance = sqrtf(dx * dx + dy * dy);
  
  if (distance > 1.0f) {
    // Move towards target
    float moveX = (dx / distance) * ANIM_SPEED;
    float moveY = (dy / distance) * ANIM_SPEED;
    
    if (fabsf(moveX) > fabsf(dx)) moveX = dx;
    if (fabsf(moveY) > fabsf(dy)) moveY = dy;
    
    playerAnimX[playerid] += moveX;
    playerAnimY[playerid] += moveY;
  } else {
    // Snap to target when close enough
    playerAnimX[playerid] = (float)targetX;
    playerAnimY[playerid] = (float)targetY;
  }
  
  // Draw the player token
  drawPlayerToken((int)playerAnimX[playerid], (int)playerAnimY[playerid], playerid);
}

void renderPropOwnerAtPos(SDL_Texture *banner, int game_position, int playerid,
                          int mortgaged, int level) {
  const int PROP_WIDTH = 66;
  const int PROP_HEIGHT = 80;
  const int PROP_HEIGHT_FULL = 104;
  const int GO_WIDTH = 103;
  const int BANNER_HEIGHT = 10;
  int xPos, yPos;
  double degrees = 0;
  int xPosH = 400, yPosH = 400;

  if (playerid == 0)
    SDL_SetTextureColorMod(ownerBanner, 255, 0, 0);
  if (playerid == 1)
    SDL_SetTextureColorMod(ownerBanner, 0, 0, 255);

  if (game_position % 10 == 0)
    return;

  if (game_position <= 10) {
    xPos = BOARD_WIDTH - (GO_WIDTH + PROP_WIDTH * (game_position));
    yPos = BOARD_HEIGHT - PROP_HEIGHT_FULL - BANNER_HEIGHT / 2;
    xPosH = xPos;
    yPosH = yPos + 90;
  } else if (game_position >= 11 && game_position <= 19) {
    xPos = PROP_HEIGHT - BANNER_HEIGHT;
    yPos = BOARD_HEIGHT - (GO_WIDTH + PROP_WIDTH * (game_position - 10)) + 27;
    degrees = 90;
    xPosH = xPos - 70;
    yPosH = yPos - 27;
  } else if (game_position >= 20 && game_position <= 30) {
    xPos = BOARD_WIDTH - (GO_WIDTH + PROP_WIDTH * (30 - game_position));
    yPos = PROP_HEIGHT_FULL - BANNER_HEIGHT / 2;
    degrees = 180;
    xPosH = xPos;
    yPosH = yPos - 100;
  } else if (game_position >= 31 && game_position <= 39) {
    xPos = BOARD_WIDTH - (PROP_HEIGHT_FULL + BANNER_HEIGHT / 2 + 27);
    yPos = BOARD_HEIGHT - (GO_WIDTH + PROP_WIDTH * (40 - game_position)) + 27;
    degrees = 270;
    xPosH = xPos + 120;
    yPosH = yPos - 27;
  }

  renderTextureRot(banner, xPos, yPos, degrees);

  if (mortgaged) {
    SDL_Rect pos;
    SDL_SetTextureColorMod(ownerBanner, 0, 0, 0);
    pos.x = xPos;
    pos.y = yPos;
    SDL_QueryTexture(banner, NULL, NULL, &pos.w, &pos.h);
    pos.w = 0.5 * pos.w;
    if (degrees == 90 || degrees == 270) {
      pos.x += 16;
      pos.y -= 17;
    }
    SDL_RenderCopyEx(gRenderer, banner, NULL, &pos, degrees, NULL,
                     SDL_FLIP_NONE);
  }

  if (level > 0 && level <= 4) {
    int offset = 16;
    int i;
    for (i = 0; i < level; i++) {
      SDL_Rect r;
      if (degrees == 0 || degrees == 180) {
        r.x = xPosH + offset * i;
        r.y = yPosH;
      } else {
        r.x = xPosH;
        r.y = yPosH + offset * i;
      }
      r.w = 16;
      r.h = 16;
      SDL_SetTextureColorMod(house, 0, 255, 0);

      SDL_RenderCopyEx(gRenderer, house, NULL, &r, degrees, NULL,
                       SDL_FLIP_NONE);
    }
    // renderTextureRot(house, xPos, yPos, degrees);
  }

  if (level == 5) {
    SDL_SetTextureColorMod(house, 255, 0, 0);
    if (degrees == 0) {
      xPosH += 16;
      yPosH -= 16;
    }
    if (degrees == 90) {
      yPosH += 16;
    }
    if (degrees == 180) {
      xPosH += 16;
    }
    if (degrees == 270) {
      xPosH -= 16;
      yPosH += 16;
    }
    renderTextureRot(house, xPosH, yPosH, degrees);
  }
}

void renderDices(SDL_Texture *dice, int face1, int face2) {
  SDL_Rect pos, crop;
  int face[2] = {face1, face2};
  int i;
  for (i = 0; i < 2; i++) {
    pos.x = 300 + i * 80;
    pos.y = 280;
    pos.w = 192 / 3;
    pos.h = 128 / 2;
    crop.w = pos.w;
    crop.h = pos.h;
    crop.x = face[i] % 3 == 0 ? 128 : (face[i] % 3 - 1) * 64;
    crop.y = face[i] > 3 ? 64 : 0;
    // SDL_QueryTexture(tex, NULL, NULL, &pos.w, &pos.h);
    SDL_RenderCopy(gRenderer, dice, &crop, &pos);
  }
}

static int parsePropFromPos(int x, int y) {
  const int PROP_WIDTH = 66;
  const int GO_WIDTH = 103;

  if (y >= BOARD_HEIGHT - GO_WIDTH) {
    return 9 - (x - GO_WIDTH) / PROP_WIDTH;
  }

  if (y <= GO_WIDTH) {
    return (x - GO_WIDTH) / PROP_WIDTH + 21;
  }

  if (x <= GO_WIDTH) {
    return 19 - (y - GO_WIDTH) / PROP_WIDTH;
  }

  if (x >= BOARD_WIDTH - GO_WIDTH) {
    return 31 + (y - GO_WIDTH) / PROP_WIDTH;
  }

  return -1;
}

void Render_processEventsAndRender(void) {
  int quit = 0;
  SDL_Event e;

  while (!quit) {
    Render_renderEverything();
    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
      // User requests quit
      if (e.type == SDL_QUIT) {
        quit = 1;
      }

      if (e.type == SDL_KEYDOWN) {
        Game_receiveinput(e.key.keysym.sym);

        /*switch( e.key.keysym.sym )
        {
        case SDLK_SPACE:
                Game_cycle();
                break;
        }*/
      }

      if (e.type == SDL_MOUSEBUTTONDOWN) {
        int x, y, s;
        SDL_GetMouseState(&x, &y);
        s = parsePropFromPos(x, y);
        printf("%i %i %i\n", x, y, s);
        if (s >= 0) {
          xCross = x;
          yCross = y;
          Game_selectProperty(s);
        } else {
          xCross = -1000;
          yCross = -1000;
        }
      }
    }
  }
}

void Render_renderEverything(void) {
  SDL_Color color = {0, 0, 0, 255}; // r, g, b, alpha
  int i, j;
  char *text;
  SDL_RenderClear(gRenderer);
  renderTexture(background, 0, 0);

  Game_getLastRoll(&i, &j);

  renderDices(diceSheet, i, j);
  i = 0;
  while ((text = Game_getText(i)) != NULL) {
    renderText(text, 130, 440 + 40 * i, color);
    i++;
  }

  renderTextureRot(cross, xCross - 25, yCross - 25, 0);

  // renderTexture(diceSheet, 200, 200);

  for (i = 0; i < 2; i++) {
    SDL_Color playerColor;
    if (i == 0) {
      playerColor.r = 255; // Red player
      playerColor.g = 0;
      playerColor.b = 0;
    } else {
      playerColor.r = 0; // Blue player
      playerColor.g = 0;
      playerColor.b = 255;
    }

    char *reply = Game_getFormattedStatus(i);
    int isJailed = Game_isPlayerJailed(i);
    renderTextWithStyle(reply, 450, 120 + 40 * i, playerColor, isJailed);
    free(reply);
  }

  for (i = 0; i < Game_getTotalPlayers(); i++) {
    renderPlayerTextureAtPos(player, Game_getPlayerPosition(i), i);
  }

  for (i = 0; i < 40; i++) {
    j = Game_getPropOwner(i);
    if (j >= 0)
      renderPropOwnerAtPos(ownerBanner, i, j, Game_getPropMortageStatus(i),
                           Game_getPropLevel(i));
  }

  // Draw sidebar background
  SDL_Rect sidebarRect = {SIDEBAR_X, 0, SIDEBAR_WIDTH, SCREEN_HEIGHT};
  SDL_SetRenderDrawColor(gRenderer, 240, 240, 240, 255); // Light gray
  SDL_RenderFillRect(gRenderer, &sidebarRect);
  SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255); // Black border
  SDL_RenderDrawRect(gRenderer, &sidebarRect);

  // Draw card if active
  if (Game_hasActiveCard()) {
    CardType cardType = Game_getActiveCardType();
    int cardIndex = Game_getActiveCardIndex();
    
    // Card title
    SDL_Color cardColor = {0, 0, 0, 255};
    if (cardType == CARD_CHANCE) {
      cardColor.r = 255;
      cardColor.g = 140;
      cardColor.b = 0; // Orange
      renderTextSmall("CHANCE", SIDEBAR_X + 10, 20, cardColor);
    } else {
      cardColor.r = 255;
      cardColor.g = 215;
      cardColor.b = 0; // Yellow/Gold
      renderTextSmall("COMMUNITY", SIDEBAR_X + 10, 20, cardColor);
      renderTextSmall("CHEST", SIDEBAR_X + 10, 38, cardColor);
    }

    // Card description (wrapped text)
    SDL_Color textColor = {0, 0, 0, 255};
    const char *desc;
    if (cardType == CARD_CHANCE) {
      desc = Cards_getChanceDescription(cardIndex);
    } else {
      desc = Cards_getCommunityChestDescription(cardIndex);
    }
    
    // Simple word wrapping for card text
    char buffer[200];
    strncpy(buffer, desc, 199);
    buffer[199] = '\0';
    
    int yPos = 80;
    char *word = strtok(buffer, " ");
    char line[100] = "";
    
    while (word != NULL) {
      // Check if adding this word would overflow (rough estimate: 8px per char)
      if ((int)((strlen(line) + strlen(word) + 1) * 8) < SIDEBAR_WIDTH - 20) {
        if (strlen(line) > 0) strcat(line, " ");
        strcat(line, word);
      } else {
        renderTextSmall(line, SIDEBAR_X + 10, yPos, textColor);
        yPos += 20;
        strcpy(line, word);
      }
      word = strtok(NULL, " ");
    }
    if (strlen(line) > 0) {
      renderTextSmall(line, SIDEBAR_X + 10, yPos, textColor);
    }

    // Press SPACE to continue
    SDL_Color hintColor = {100, 100, 100, 255};
    renderTextSmall("Press SPACE", SIDEBAR_X + 10, 750, hintColor);
    renderTextSmall("to continue", SIDEBAR_X + 10, 770, hintColor);
  }

  SDL_RenderPresent(gRenderer);
}
