/*
 * Monopoly Network Client - Main Entry Point
 * 
 * This launches the SDL lobby GUI for login/registration,
 * then starts the game when a match is found.
 * 
 * Usage: ./monopoly_client [server_ip] [port]
 *   Default: 127.0.0.1:8888
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "client_network.h"
#include "lobby.h"
#include "game_network.h"

// Game result action after game ends
typedef enum {
    RESULT_NONE,
    RESULT_BACK_TO_LOBBY,
    RESULT_REMATCH,
    RESULT_EXIT
} GameResultAction;

// Forward declarations
static GameResultAction run_network_game(ClientState* client, MatchFoundInfo* match);
static GameResultAction show_game_result_screen(ClientState* client);

int main(int argc, char* argv[]) {
    const char* server_ip = "127.0.0.1";
    int port = 8888;
    
    // Parse command line arguments
    if (argc >= 2) {
        server_ip = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0) port = 8888;
    }
    
    printf("╔═══════════════════════════════════════╗\n");
    printf("║     MONOPOLY ONLINE CLIENT            ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    printf("Default server: %s:%d\n\n", server_ip, port);
    
    // Initialize client state
    ClientState client;
    client_init(&client);
    
    // Main game loop - can return to lobby after game ends
    int keep_running = 1;
    while (keep_running) {
        // Initialize lobby
        if (Lobby_init() != 0) {
            fprintf(stderr, "Failed to initialize lobby!\n");
            return 1;
        }
        
        // Run lobby - this blocks until user starts game or exits
        LobbyState result = Lobby_run(&client, server_ip, port);
        
        // Clean up lobby
        Lobby_close();
        
        if (result == LOBBY_STATE_START_GAME) {
            MatchFoundInfo* match = Lobby_getMatchInfo();
            
            if (match) {
                printf("\n");
                printf("=================================\n");
                printf("  MATCH FOUND!\n");
                printf("  You: %s (ELO: %d)\n", client.username, client.elo_rating);
                printf("  Opponent: %s (ELO: %d)\n", match->opponent_name, match->opponent_elo);
                printf("  You are Player %d\n", match->your_player_num);
                printf("=================================\n\n");
                
                // Store opponent ID before game ends (for rematch)
                int opponent_id = match->opponent_id;
                
                // Run the network game
                GameResultAction action = run_network_game(&client, match);
                
                if (action == RESULT_EXIT) {
                    keep_running = 0;
                } else if (action == RESULT_REMATCH) {
                    // Send rematch request (as a challenge to opponent)
                    printf("[CLIENT] Sending rematch challenge to opponent (ID: %d)...\n", opponent_id);
                    if (client_is_connected(&client) && client.logged_in && opponent_id > 0) {
                        client_send_challenge(&client, opponent_id);
                        printf("[CLIENT] Rematch challenge sent!\n");
                    } else {
                        printf("[CLIENT] Cannot send rematch - not connected or no opponent ID\n");
                    }
                } else {
                    // RESULT_BACK_TO_LOBBY - just continue the loop
                    printf("\nReturning to lobby...\n\n");
                }
                
                // Refresh stats display
                client_refresh_stats(&client);
            } else {
                printf("Error: No match info available\n");
            }
        } else if (result == LOBBY_STATE_EXIT) {
            keep_running = 0;
        } else {
            // Unexpected state
            keep_running = 0;
        }
    }
    
    // Disconnect if still connected
    if (client_is_connected(&client)) {
        client_disconnect(&client);
    }
    
    printf("Goodbye!\n");
    return 0;
}

// ============ Network Game Renderer ============

// Game window dimensions - wider sidebar
#define GAME_WIDTH 1100
#define GAME_HEIGHT 800
#define BOARD_SIZE 800
#define SIDEBAR_WIDTH 300
#define SIDEBAR_X (BOARD_SIZE)

// Colors
static const SDL_Color COLOR_WHITE = {255, 255, 255, 255};
static const SDL_Color COLOR_BLACK = {0, 0, 0, 255};
static const SDL_Color COLOR_RED = {220, 50, 50, 255};
static const SDL_Color COLOR_BLUE = {50, 100, 220, 255};
static const SDL_Color COLOR_GREEN = {50, 200, 50, 255};
static const SDL_Color COLOR_GOLD = {218, 165, 32, 255};
static const SDL_Color COLOR_GRAY = {150, 150, 150, 255};
static const SDL_Color COLOR_DARK_GRAY = {80, 80, 80, 255};
static const SDL_Color COLOR_BG = {35, 40, 50, 255};
static const SDL_Color COLOR_PANEL = {50, 55, 70, 255};
static const SDL_Color COLOR_HIGHLIGHT = {70, 130, 180, 255};

static SDL_Window* gameWindow = NULL;
static SDL_Renderer* gameRenderer = NULL;
static TTF_Font* gameFont = NULL;
static TTF_Font* gameFontSmall = NULL;
static TTF_Font* gameFontLarge = NULL;
static SDL_Texture* boardTexture = NULL;
static SDL_Texture* diceTexture = NULL;

static int init_game_renderer(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    
    gameWindow = SDL_CreateWindow("Monopoly Online - Game",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  GAME_WIDTH, GAME_HEIGHT, SDL_WINDOW_SHOWN);
    if (!gameWindow) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }
    
    gameRenderer = SDL_CreateRenderer(gameWindow, -1, 
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gameRenderer) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }
    
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return -1;
    }
    
    gameFont = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 20);
    gameFontSmall = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 16);
    gameFontLarge = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 28);
    
    if (!gameFont || !gameFontSmall || !gameFontLarge) {
        printf("Failed to load fonts! SDL_ttf Error: %s\n", TTF_GetError());
        return -1;
    }
    
    // Load textures
    SDL_Surface* surface = SDL_LoadBMP("assets/images/monopoly.bmp");
    if (surface) {
        boardTexture = SDL_CreateTextureFromSurface(gameRenderer, surface);
        SDL_FreeSurface(surface);
    }
    
    surface = SDL_LoadBMP("assets/images/dice.bmp");
    if (surface) {
        SDL_SetColorKey(surface, 1, 0x323232);
        diceTexture = SDL_CreateTextureFromSurface(gameRenderer, surface);
        SDL_FreeSurface(surface);
    }
    
    return 0;
}

static void close_game_renderer(void) {
    if (boardTexture) SDL_DestroyTexture(boardTexture);
    if (diceTexture) SDL_DestroyTexture(diceTexture);
    if (gameFont) TTF_CloseFont(gameFont);
    if (gameFontSmall) TTF_CloseFont(gameFontSmall);
    if (gameFontLarge) TTF_CloseFont(gameFontLarge);
    TTF_Quit();
    if (gameRenderer) SDL_DestroyRenderer(gameRenderer);
    if (gameWindow) SDL_DestroyWindow(gameWindow);
    SDL_Quit();
    
    boardTexture = NULL;
    diceTexture = NULL;
    gameFont = NULL;
    gameFontSmall = NULL;
    gameFontLarge = NULL;
    gameRenderer = NULL;
    gameWindow = NULL;
}

static void render_text(const char* text, int x, int y, SDL_Color color, TTF_Font* font) {
    if (!text || !text[0] || !font) return;
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gameRenderer, surface);
    if (texture) {
        SDL_Rect dest = {x, y, surface->w, surface->h};
        SDL_RenderCopy(gameRenderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void render_text_centered(const char* text, int cx, int y, SDL_Color color, TTF_Font* font) {
    if (!text || !text[0] || !font) return;
    int w, h;
    TTF_SizeText(font, text, &w, &h);
    render_text(text, cx - w/2, y, color, font);
}

static void render_button(const char* text, SDL_Rect* rect, int hovered) {
    SDL_Color bg = hovered ? COLOR_HIGHLIGHT : COLOR_DARK_GRAY;
    SDL_SetRenderDrawColor(gameRenderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(gameRenderer, rect);
    
    SDL_SetRenderDrawColor(gameRenderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(gameRenderer, rect);
    
    render_text_centered(text, rect->x + rect->w/2, rect->y + 8, COLOR_WHITE, gameFont);
}

// Get position on board for a space (0-39)
static void get_board_position(int space, int* x, int* y) {
    const int PROP_W = 66;
    const int CORNER = 103;
    const int OFFSET = 12;
    
    if (space >= 0 && space <= 10) {
        // Bottom row (right to left)
        *x = BOARD_SIZE - CORNER - (space * PROP_W) - OFFSET;
        *y = BOARD_SIZE - CORNER/2;
    } else if (space >= 11 && space <= 19) {
        // Left column (bottom to top)
        *x = CORNER/2;
        *y = BOARD_SIZE - CORNER - ((space - 10) * PROP_W) - OFFSET;
    } else if (space >= 20 && space <= 30) {
        // Top row (left to right)
        *x = CORNER + ((space - 20) * PROP_W) + OFFSET;
        *y = CORNER/2;
    } else if (space >= 31 && space <= 39) {
        // Right column (top to bottom)
        *x = BOARD_SIZE - CORNER/2;
        *y = CORNER + ((space - 30) * PROP_W) + OFFSET;
    }
}

static void render_player_token(int player_idx, int position) {
    int x, y;
    get_board_position(position, &x, &y);
    
    // Offset second player slightly
    if (player_idx == 1) {
        x += 16;
        y += 16;
    }
    
    // Draw filled circle
    int radius = 12;
    SDL_Color color = (player_idx == 0) ? COLOR_RED : COLOR_BLUE;
    
    // Draw border
    SDL_SetRenderDrawColor(gameRenderer, 0, 0, 0, 255);
    for (int dy = -radius-2; dy <= radius+2; dy++) {
        for (int dx = -radius-2; dx <= radius+2; dx++) {
            if (dx*dx + dy*dy <= (radius+2)*(radius+2)) {
                SDL_RenderDrawPoint(gameRenderer, x + dx, y + dy);
            }
        }
    }
    
    SDL_SetRenderDrawColor(gameRenderer, color.r, color.g, color.b, 255);
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                SDL_RenderDrawPoint(gameRenderer, x + dx, y + dy);
            }
        }
    }
}

static void render_dice(int d1, int d2) {
    if (!diceTexture || d1 < 1 || d2 < 1) return;
    
    int faces[2] = {d1, d2};
    for (int i = 0; i < 2; i++) {
        SDL_Rect dest = {300 + i * 80, 280, 64, 64};
        SDL_Rect src;
        src.w = 64;
        src.h = 64;
        src.x = ((faces[i] - 1) % 3) * 64;
        src.y = ((faces[i] - 1) / 3) * 64;
        SDL_RenderCopy(gameRenderer, diceTexture, &src, &dest);
    }
}

static void render_property_owners(void) {
    for (int i = 0; i < 40; i++) {
        int owner = g_synced_state.properties[i].owner;
        if (owner < 0) continue;
        
        int x, y;
        get_board_position(i, &x, &y);
        
        // Draw small colored indicator
        SDL_Color color = (owner == 0) ? COLOR_RED : COLOR_BLUE;
        SDL_SetRenderDrawColor(gameRenderer, color.r, color.g, color.b, 255);
        
        SDL_Rect rect = {x - 20, y - 30, 40, 6};
        SDL_RenderFillRect(gameRenderer, &rect);
        
        // Draw houses
        int upgrades = g_synced_state.properties[i].upgrades;
        if (upgrades > 0 && upgrades <= 4) {
            SDL_SetRenderDrawColor(gameRenderer, 0, 200, 0, 255);
            for (int h = 0; h < upgrades; h++) {
                SDL_Rect house = {x - 18 + h*10, y - 40, 8, 8};
                SDL_RenderFillRect(gameRenderer, &house);
            }
        } else if (upgrades == 5) {
            // Hotel
            SDL_SetRenderDrawColor(gameRenderer, 200, 0, 0, 255);
            SDL_Rect hotel = {x - 10, y - 42, 20, 12};
            SDL_RenderFillRect(gameRenderer, &hotel);
        }
    }
}

static void render_sidebar(int my_turn) {
    // Sidebar background
    SDL_Rect sidebar = {SIDEBAR_X, 0, SIDEBAR_WIDTH, GAME_HEIGHT};
    SDL_SetRenderDrawColor(gameRenderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(gameRenderer, &sidebar);
    
    int x = SIDEBAR_X + 15;
    int y = 15;
    
    // Title
    render_text("MONOPOLY ONLINE", x, y, COLOR_GOLD, gameFontLarge);
    y += 40;
    
    // Separator
    SDL_SetRenderDrawColor(gameRenderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(gameRenderer, SIDEBAR_X + 10, y, SIDEBAR_X + SIDEBAR_WIDTH - 10, y);
    y += 15;
    
    // Turn indicator - prominent
    if (NetGame_isPaused()) {
        SDL_Rect pauseBox = {SIDEBAR_X + 10, y, SIDEBAR_WIDTH - 20, 40};
        SDL_SetRenderDrawColor(gameRenderer, 200, 150, 0, 255);
        SDL_RenderFillRect(gameRenderer, &pauseBox);
        if (NetGame_didWePause()) {
            render_text_centered("PAUSED (by you)", SIDEBAR_X + SIDEBAR_WIDTH/2, y + 10, COLOR_BLACK, gameFont);
        } else {
            render_text_centered("PAUSED", SIDEBAR_X + SIDEBAR_WIDTH/2, y + 10, COLOR_BLACK, gameFont);
        }
    } else if (my_turn) {
        SDL_Rect turnBox = {SIDEBAR_X + 10, y, SIDEBAR_WIDTH - 20, 40};
        SDL_SetRenderDrawColor(gameRenderer, 50, 150, 50, 255);
        SDL_RenderFillRect(gameRenderer, &turnBox);
        
        // Show what action is expected
        const char* action = "";
        switch (NetGame_getStateType()) {
            case GSTATE_WAITING_ROLL: action = "YOUR TURN - Roll!"; break;
            case GSTATE_WAITING_BUY: action = "YOUR TURN - Buy?"; break;
            case GSTATE_WAITING_DEBT: action = "YOUR TURN - Pay debt!"; break;
            default: action = "YOUR TURN"; break;
        }
        render_text_centered(action, SIDEBAR_X + SIDEBAR_WIDTH/2, y + 10, COLOR_WHITE, gameFont);
    } else {
        SDL_Rect turnBox = {SIDEBAR_X + 10, y, SIDEBAR_WIDTH - 20, 40};
        SDL_SetRenderDrawColor(gameRenderer, 100, 100, 100, 255);
        SDL_RenderFillRect(gameRenderer, &turnBox);
        render_text_centered("Waiting for opponent...", SIDEBAR_X + SIDEBAR_WIDTH/2, y + 10, COLOR_WHITE, gameFontSmall);
    }
    y += 55;
    
    // Players info
    render_text("PLAYERS", x, y, COLOR_GOLD, gameFont);
    y += 28;
    
    for (int i = 0; i < 2; i++) {
        SDL_Color color = (i == 0) ? COLOR_RED : COLOR_BLUE;
        int is_me = (i == g_synced_state.my_player_index);
        int is_current = (i == g_synced_state.current_player);
        
        // Player panel
        SDL_Rect playerPanel = {SIDEBAR_X + 10, y, SIDEBAR_WIDTH - 20, 70};
        if (is_current && !NetGame_isPaused()) {
            SDL_SetRenderDrawColor(gameRenderer, 60, 70, 90, 255);
        } else {
            SDL_SetRenderDrawColor(gameRenderer, 45, 50, 60, 255);
        }
        SDL_RenderFillRect(gameRenderer, &playerPanel);
        
        // Border
        SDL_SetRenderDrawColor(gameRenderer, color.r, color.g, color.b, 255);
        SDL_RenderDrawRect(gameRenderer, &playerPanel);
        
        // Current player arrow
        if (is_current && !NetGame_isPaused()) {
            render_text(">", SIDEBAR_X + 15, y + 5, COLOR_GREEN, gameFont);
        }
        
        // Name
        char buf[80];
        snprintf(buf, sizeof(buf), "%s%s", 
                 g_synced_state.players[i].username,
                 is_me ? " (You)" : "");
        render_text(buf, SIDEBAR_X + 30, y + 5, color, gameFont);
        
        // Money
        snprintf(buf, sizeof(buf), "$%d", g_synced_state.players[i].money);
        render_text(buf, SIDEBAR_X + 30, y + 28, COLOR_WHITE, gameFont);
        
        // Position
        snprintf(buf, sizeof(buf), "Pos: %d", g_synced_state.players[i].position);
        render_text(buf, SIDEBAR_X + 150, y + 28, COLOR_GRAY, gameFontSmall);
        
        // Jail status
        if (g_synced_state.players[i].jailed) {
            snprintf(buf, sizeof(buf), "[JAIL %d/3]", g_synced_state.players[i].turns_in_jail);
            render_text(buf, SIDEBAR_X + 30, y + 50, COLOR_GRAY, gameFontSmall);
        }
        
        y += 80;
    }
    
    // Separator
    SDL_SetRenderDrawColor(gameRenderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(gameRenderer, SIDEBAR_X + 10, y, SIDEBAR_X + SIDEBAR_WIDTH - 10, y);
    y += 15;
    
    // Dice display
    render_text("LAST ROLL", x, y, COLOR_GOLD, gameFont);
    y += 25;
    if (g_synced_state.dice[0] > 0) {
        char dice_str[32];
        snprintf(dice_str, sizeof(dice_str), "[ %d ] + [ %d ] = %d", 
                 g_synced_state.dice[0], g_synced_state.dice[1],
                 g_synced_state.dice[0] + g_synced_state.dice[1]);
        render_text(dice_str, x, y, COLOR_WHITE, gameFont);
        if (g_synced_state.dice[0] == g_synced_state.dice[1]) {
            render_text("DOUBLES!", x + 180, y, COLOR_GREEN, gameFontSmall);
        }
    } else {
        render_text("--", x, y, COLOR_GRAY, gameFont);
    }
    y += 35;
    
    // Game message
    render_text("STATUS", x, y, COLOR_GOLD, gameFont);
    y += 25;
    
    // Word-wrap message
    if (g_synced_state.message[0]) {
        render_text(g_synced_state.message, x, y, COLOR_WHITE, gameFontSmall);
        y += 20;
    }
    if (g_synced_state.message2[0]) {
        render_text(g_synced_state.message2, x, y, COLOR_GRAY, gameFontSmall);
    }
    y += 35;
    
    // Separator
    SDL_SetRenderDrawColor(gameRenderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(gameRenderer, SIDEBAR_X + 10, y, SIDEBAR_X + SIDEBAR_WIDTH - 10, y);
    y += 15;
    
    // Controls
    render_text("CONTROLS", x, y, COLOR_GOLD, gameFont);
    y += 28;
    
    render_text("SPACE", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Roll / Buy", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("N", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Skip buying", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("P", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Pay jail fine ($50)", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("B", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Build house", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("D", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Sell house", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("M", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Mortgage", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 25;
    
    // Separator
    SDL_SetRenderDrawColor(gameRenderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(gameRenderer, SIDEBAR_X + 10, y, SIDEBAR_X + SIDEBAR_WIDTH - 10, y);
    y += 10;
    
    render_text("F5", x, y, COLOR_HIGHLIGHT, gameFontSmall);
    render_text(": Pause/Resume", x + 55, y, COLOR_GRAY, gameFontSmall);
    y += 20;
    
    render_text("ESC", x, y, COLOR_RED, gameFontSmall);
    render_text(": Surrender", x + 55, y, COLOR_GRAY, gameFontSmall);
}

static void render_game(void) {
    // Clear
    SDL_SetRenderDrawColor(gameRenderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(gameRenderer);
    
    // Board
    if (boardTexture) {
        SDL_Rect dest = {0, 0, BOARD_SIZE, BOARD_SIZE};
        SDL_RenderCopy(gameRenderer, boardTexture, NULL, &dest);
    }
    
    // Property owners and houses
    render_property_owners();
    
    // Players
    for (int i = 0; i < 2; i++) {
        render_player_token(i, g_synced_state.players[i].position);
    }
    
    // Dice in center of board
    render_dice(g_synced_state.dice[0], g_synced_state.dice[1]);
    
    // Pause overlay
    if (NetGame_isPaused()) {
        SDL_SetRenderDrawBlendMode(gameRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(gameRenderer, 0, 0, 0, 150);
        SDL_Rect overlay = {0, 0, BOARD_SIZE, BOARD_SIZE};
        SDL_RenderFillRect(gameRenderer, &overlay);
        
        render_text_centered("GAME PAUSED", BOARD_SIZE/2, BOARD_SIZE/2 - 20, COLOR_WHITE, gameFontLarge);
        if (NetGame_didWePause()) {
            render_text_centered("Press F5 to resume", BOARD_SIZE/2, BOARD_SIZE/2 + 20, COLOR_GRAY, gameFont);
        } else {
            render_text_centered("Waiting for opponent to resume...", BOARD_SIZE/2, BOARD_SIZE/2 + 20, COLOR_GRAY, gameFont);
        }
    }
    
    // Sidebar
    render_sidebar(NetGame_isMyTurn());
    
    SDL_RenderPresent(gameRenderer);
}

/*
 * Show game result screen after game ends
 * Returns the user's choice
 */
static GameResultAction show_game_result_screen(ClientState* client) {
    if (!NetGame_hasResult()) {
        printf("[RESULT] No game result available\n");
        return RESULT_BACK_TO_LOBBY;
    }
    
    const GameResult* result = NetGame_getResult();
    int am_winner = (result->winner_id == client->user_id);
    
    printf("[RESULT] Showing result screen: %s won by %s\n", 
           result->winner_name, result->reason);
    
    // Button positions
    SDL_Rect btnRematch = {GAME_WIDTH/2 - 200, 550, 180, 45};
    SDL_Rect btnLobby = {GAME_WIDTH/2 + 20, 550, 180, 45};
    SDL_Rect btnExit = {GAME_WIDTH/2 - 90, 610, 180, 45};
    
    int quit = 0;
    GameResultAction action = RESULT_NONE;
    
    while (!quit) {
        SDL_Event e;
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        
        int hoverRematch = (mx >= btnRematch.x && mx < btnRematch.x + btnRematch.w &&
                           my >= btnRematch.y && my < btnRematch.y + btnRematch.h);
        int hoverLobby = (mx >= btnLobby.x && mx < btnLobby.x + btnLobby.w &&
                         my >= btnLobby.y && my < btnLobby.y + btnLobby.h);
        int hoverExit = (mx >= btnExit.x && mx < btnExit.x + btnExit.w &&
                        my >= btnExit.y && my < btnExit.y + btnExit.h);
        
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                action = RESULT_EXIT;
                quit = 1;
            }
            
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (hoverRematch) {
                    action = RESULT_REMATCH;
                    quit = 1;
                } else if (hoverLobby) {
                    action = RESULT_BACK_TO_LOBBY;
                    quit = 1;
                } else if (hoverExit) {
                    action = RESULT_EXIT;
                    quit = 1;
                }
            }
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    action = RESULT_BACK_TO_LOBBY;
                    quit = 1;
                }
            }
        }
        
        // Render
        SDL_SetRenderDrawColor(gameRenderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderClear(gameRenderer);
        
        // Title
        render_text_centered("GAME OVER", GAME_WIDTH/2, 80, COLOR_GOLD, gameFontLarge);
        
        // Result box
        SDL_Rect resultBox = {GAME_WIDTH/2 - 250, 140, 500, 350};
        SDL_SetRenderDrawColor(gameRenderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(gameRenderer, &resultBox);
        SDL_SetRenderDrawColor(gameRenderer, am_winner ? COLOR_GREEN.r : COLOR_RED.r,
                              am_winner ? COLOR_GREEN.g : COLOR_RED.g,
                              am_winner ? COLOR_GREEN.b : COLOR_RED.b, 255);
        SDL_RenderDrawRect(gameRenderer, &resultBox);
        
        int y = 170;
        
        // Win/Lose message
        if (result->is_draw) {
            render_text_centered("DRAW!", GAME_WIDTH/2, y, COLOR_GOLD, gameFontLarge);
        } else if (am_winner) {
            render_text_centered("YOU WIN!", GAME_WIDTH/2, y, COLOR_GREEN, gameFontLarge);
        } else {
            render_text_centered("YOU LOSE", GAME_WIDTH/2, y, COLOR_RED, gameFontLarge);
        }
        y += 50;
        
        // Reason
        char buf[128];
        snprintf(buf, sizeof(buf), "Reason: %s", result->reason);
        render_text_centered(buf, GAME_WIDTH/2, y, COLOR_GRAY, gameFont);
        y += 45;
        
        // Winner info
        SDL_SetRenderDrawColor(gameRenderer, 60, 70, 85, 255);
        SDL_Rect winnerBox = {GAME_WIDTH/2 - 220, y, 200, 90};
        SDL_RenderFillRect(gameRenderer, &winnerBox);
        SDL_SetRenderDrawColor(gameRenderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255);
        SDL_RenderDrawRect(gameRenderer, &winnerBox);
        
        render_text_centered("WINNER", GAME_WIDTH/2 - 120, y + 8, COLOR_GREEN, gameFontSmall);
        render_text_centered(result->winner_name, GAME_WIDTH/2 - 120, y + 30, COLOR_WHITE, gameFont);
        snprintf(buf, sizeof(buf), "ELO: %d (+%d)", result->winner_new_elo, result->winner_elo_change);
        render_text_centered(buf, GAME_WIDTH/2 - 120, y + 55, COLOR_GREEN, gameFontSmall);
        
        // Loser info
        SDL_Rect loserBox = {GAME_WIDTH/2 + 20, y, 200, 90};
        SDL_SetRenderDrawColor(gameRenderer, 60, 70, 85, 255);
        SDL_RenderFillRect(gameRenderer, &loserBox);
        SDL_SetRenderDrawColor(gameRenderer, COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255);
        SDL_RenderDrawRect(gameRenderer, &loserBox);
        
        render_text_centered("LOSER", GAME_WIDTH/2 + 120, y + 8, COLOR_RED, gameFontSmall);
        render_text_centered(result->loser_name, GAME_WIDTH/2 + 120, y + 30, COLOR_WHITE, gameFont);
        snprintf(buf, sizeof(buf), "ELO: %d (%d)", result->loser_new_elo, result->loser_elo_change);
        render_text_centered(buf, GAME_WIDTH/2 + 120, y + 55, COLOR_RED, gameFontSmall);
        
        // Buttons
        render_button("Rematch", &btnRematch, hoverRematch);
        render_button("Back to Lobby", &btnLobby, hoverLobby);
        render_button("Exit", &btnExit, hoverExit);
        
        SDL_RenderPresent(gameRenderer);
        SDL_Delay(16);
    }
    
    return action;
}

/*
 * Run the network game loop
 */
static GameResultAction run_network_game(ClientState* client, MatchFoundInfo* match) {
    // Initialize network game state
    NetGame_init(client, match);
    
    // Initialize renderer
    if (init_game_renderer() != 0) {
        fprintf(stderr, "Failed to initialize game renderer!\n");
        NetGame_close();
        return RESULT_BACK_TO_LOBBY;
    }
    
    // Game loop
    int quit = 0;
    int waiting_for_result = 0;  // Flag to wait for game result after surrender
    Uint32 wait_start_time = 0;  // When we started waiting
    const Uint32 RESULT_TIMEOUT = 5000;  // 5 second timeout
    SDL_Event e;
    int selectedProperty = -1;
    
    printf("[GAME] Starting network game loop...\n");
    
    while (!quit) {
        // Process network messages
        if (!NetGame_processMessages(client)) {
            printf("[GAME] Game ended via network\n");
            break;
        }
        
        // If waiting for result and we got it, exit
        if (waiting_for_result) {
            if (NetGame_hasResult()) {
                printf("[GAME] Received game result\n");
                break;
            }
            // Timeout after RESULT_TIMEOUT ms
            if (SDL_GetTicks() - wait_start_time > RESULT_TIMEOUT) {
                printf("[GAME] Timeout waiting for game result\n");
                break;
            }
        }
        
        // Handle SDL events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                printf("[GAME] Player quit - surrendering\n");
                NetGame_surrender(client);
                waiting_for_result = 1;
                wait_start_time = SDL_GetTicks();
            }
            
            // Skip other input if waiting for result
            if (waiting_for_result) continue;
            
            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode key = e.key.keysym.sym;
                
                // Pause/Resume (F5) - works anytime
                if (key == SDLK_F5) {
                    if (NetGame_isPaused()) {
                        if (NetGame_didWePause()) {
                            NetGame_resume(client);
                        }
                    } else {
                        NetGame_pause(client);
                    }
                    continue;
                }
                
                // Surrender (ESC) - works anytime
                if (key == SDLK_ESCAPE) {
                    printf("[GAME] Player surrendering...\n");
                    NetGame_surrender(client);
                    waiting_for_result = 1;
                    wait_start_time = SDL_GetTicks();
                    continue;  // Don't break - wait for server response
                }
                
                // Don't process game inputs if paused
                if (NetGame_isPaused()) {
                    continue;
                }
                
                // Only process game inputs if it's our turn
                if (NetGame_isMyTurn()) {
                    switch (key) {
                        case SDLK_SPACE:
                            // SPACE does different things based on game state
                            if (NetGame_getStateType() == GSTATE_WAITING_ROLL) {
                                NetGame_rollDice(client);
                            } else if (NetGame_getStateType() == GSTATE_WAITING_BUY) {
                                NetGame_buyProperty(client);
                            }
                            break;
                            
                        case SDLK_n:
                            if (NetGame_getStateType() == GSTATE_WAITING_BUY) {
                                NetGame_skipProperty(client);
                            }
                            break;
                            
                        case SDLK_p:
                            NetGame_payJailFine(client);
                            break;
                            
                        case SDLK_b:
                            if (selectedProperty >= 0) {
                                NetGame_upgradeProperty(client, selectedProperty);
                            }
                            break;
                            
                        case SDLK_d:
                            if (selectedProperty >= 0) {
                                NetGame_downgradeProperty(client, selectedProperty);
                            }
                            break;
                            
                        case SDLK_m:
                            if (selectedProperty >= 0) {
                                NetGame_mortgageProperty(client, selectedProperty);
                            }
                            break;
                            
                        case SDLK_x:
                            NetGame_declareBankrupt(client);
                            waiting_for_result = 1;
                            wait_start_time = SDL_GetTicks();
                            break;
                            
                        default:
                            break;
                    }
                }
            }
            
            // Mouse click for property selection
            if (e.type == SDL_MOUSEBUTTONDOWN && !NetGame_isPaused()) {
                int mx = e.button.x;
                int my = e.button.y;
                
                // Simple property selection from board position
                if (mx < BOARD_SIZE && my < BOARD_SIZE) {
                    const int PROP_W = 66;
                    const int CORNER = 103;
                    
                    if (my >= BOARD_SIZE - CORNER) {
                        // Bottom row
                        selectedProperty = (BOARD_SIZE - CORNER - mx) / PROP_W;
                        if (selectedProperty < 0) selectedProperty = 0;
                        if (selectedProperty > 10) selectedProperty = 10;
                    } else if (mx <= CORNER) {
                        // Left column
                        selectedProperty = 10 + (BOARD_SIZE - CORNER - my) / PROP_W;
                    } else if (my <= CORNER) {
                        // Top row
                        selectedProperty = 20 + (mx - CORNER) / PROP_W;
                    } else if (mx >= BOARD_SIZE - CORNER) {
                        // Right column
                        selectedProperty = 30 + (my - CORNER) / PROP_W;
                    }
                    
                    if (selectedProperty >= 0 && selectedProperty < 40) {
                        printf("[GAME] Selected property: %d\n", selectedProperty);
                    }
                }
            }
        }
        
        // Render
        render_game();
        
        // Show "waiting for result" overlay if needed
        if (waiting_for_result) {
            SDL_SetRenderDrawBlendMode(gameRenderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(gameRenderer, 0, 0, 0, 180);
            SDL_Rect overlay = {0, 0, GAME_WIDTH, GAME_HEIGHT};
            SDL_RenderFillRect(gameRenderer, &overlay);
            
            render_text_centered("GAME ENDING...", GAME_WIDTH/2, GAME_HEIGHT/2 - 20, COLOR_WHITE, gameFontLarge);
            render_text_centered("Waiting for result from server", GAME_WIDTH/2, GAME_HEIGHT/2 + 20, COLOR_GRAY, gameFont);
            SDL_RenderPresent(gameRenderer);
        }
        
        // Small delay
        SDL_Delay(16);
    }
    
    // Show result screen if we have results
    GameResultAction action = RESULT_BACK_TO_LOBBY;
    if (NetGame_hasResult()) {
        action = show_game_result_screen(client);
    }
    
    // Clean up
    close_game_renderer();
    NetGame_close();
    
    printf("[GAME] Network game loop ended\n");
    
    return action;
}
