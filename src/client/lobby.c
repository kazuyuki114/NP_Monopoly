/*
 * Monopoly Network Lobby - SDL GUI
 * 
 * Features:
 * - Connect/Login/Register screens
 * - Main menu with player profile
 * - Online players list with challenge buttons
 * - Matchmaking with animated search
 * - Challenge notifications
 * - Game result display with ELO changes
 */

#include "lobby.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "cJSON.h"

// Window dimensions (same as game)
#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800

// Colors
#define COLOR_BG_R 30
#define COLOR_BG_G 30
#define COLOR_BG_B 50

#define COLOR_PANEL_R 50
#define COLOR_PANEL_G 55
#define COLOR_PANEL_B 80

#define COLOR_ACCENT_R 218
#define COLOR_ACCENT_G 165
#define COLOR_ACCENT_B 32

// Refresh interval for online players (ms)
#define REFRESH_INTERVAL 3000

// Input field
typedef struct {
    char text[128];
    int active;
    SDL_Rect rect;
    int is_password;
} InputField;

// Button
typedef struct {
    char label[64];
    SDL_Rect rect;
    int hovered;
    int enabled;
    int user_data;  // For storing player ID for challenge buttons
} Button;

// Global SDL objects
static SDL_Window* gWindow = NULL;
static SDL_Renderer* gRenderer = NULL;
static TTF_Font* gFontLarge = NULL;
static TTF_Font* gFontMedium = NULL;
static TTF_Font* gFontSmall = NULL;

// UI State
static InputField inputUsername;
static InputField inputPassword;
static InputField inputEmail;
static InputField inputServerIP;
static InputField inputServerPort;
static Button btnLogin;
static Button btnRegister;
static Button btnBack;
static Button btnConnect;
static Button btnFindMatch;
static Button btnViewPlayers;
static Button btnCancelSearch;
static Button btnLogout;
static Button btnExit;
static Button btnAccept;
static Button btnDecline;
static Button btnOK;
static Button btnRematch;

// Challenge buttons for online players
static Button btnChallenge[MAX_ONLINE_PLAYERS];

static char statusMessage[256];
static int statusIsError = 0;

// Online players list
static LobbyPlayerInfo onlinePlayers[MAX_ONLINE_PLAYERS];
static int onlinePlayerCount = 0;
static Uint32 lastPlayersRefresh = 0;
static int showOnlinePlayers = 0;
static int onlinePlayersScrollOffset = 0;

// History state
typedef struct {
    int match_id;
    int opponent_id;
    char opponent_name[50];
    int is_win;
    int elo_change;
    char timestamp[32];
} MatchHistoryEntry;

static MatchHistoryEntry historyList[20];
static int historyCount = 0;
static Button btnHistory;
static Button btnBackHistory;

// Matchmaking state
static Uint32 searchStartTime = 0;

// Challenge state
static ChallengeInfo pendingChallenge;
static int hasPendingChallenge = 0;

// Game result state
static GameResultInfo lastGameResult;
static int hasGameResult = 0;

// Match found state
static MatchFoundInfo matchInfo;
static int matchFound = 0;

// Forward declarations
static void renderText(const char* text, int x, int y, TTF_Font* font, SDL_Color color);
static void renderTextCentered(const char* text, int centerX, int y, TTF_Font* font, SDL_Color color);
static void drawPanel(int x, int y, int w, int h);
static void drawInputField(InputField* field, const char* label, int labelY);
static void drawButton(Button* btn);
static void clearInputField(InputField* field);
static void initButton(Button* btn, const char* label, int x, int y, int w, int h);
static int isMouseOver(SDL_Rect* rect, int mx, int my);
static void setStatus(const char* msg, int isError);
static void refreshOnlinePlayers(ClientState* client);
static void processServerMessages(ClientState* client, LobbyState* state);

int Lobby_init(void) {
    // Initialize all static variables
    memset(&inputUsername, 0, sizeof(InputField));
    memset(&inputPassword, 0, sizeof(InputField));
    memset(&inputEmail, 0, sizeof(InputField));
    memset(&inputServerIP, 0, sizeof(InputField));
    memset(&inputServerPort, 0, sizeof(InputField));
    memset(&btnLogin, 0, sizeof(Button));
    memset(&btnRegister, 0, sizeof(Button));
    memset(&btnBack, 0, sizeof(Button));
    memset(&btnConnect, 0, sizeof(Button));
    memset(&btnFindMatch, 0, sizeof(Button));
    memset(&btnViewPlayers, 0, sizeof(Button));
    memset(&btnCancelSearch, 0, sizeof(Button));
    memset(&btnLogout, 0, sizeof(Button));
    memset(&btnExit, 0, sizeof(Button));
    memset(&btnAccept, 0, sizeof(Button));
    memset(&btnDecline, 0, sizeof(Button));
    memset(&btnOK, 0, sizeof(Button));
    memset(&btnRematch, 0, sizeof(Button));
    memset(&btnHistory, 0, sizeof(Button));
    memset(&btnBackHistory, 0, sizeof(Button));
    memset(btnChallenge, 0, sizeof(btnChallenge));
    memset(statusMessage, 0, sizeof(statusMessage));
    memset(onlinePlayers, 0, sizeof(onlinePlayers));
    memset(&pendingChallenge, 0, sizeof(pendingChallenge));
    memset(&lastGameResult, 0, sizeof(lastGameResult));
    memset(&matchInfo, 0, sizeof(matchInfo));
    statusIsError = 0;
    onlinePlayerCount = 0;
    showOnlinePlayers = 0;
    hasPendingChallenge = 0;
    hasGameResult = 0;
    matchFound = 0;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    gWindow = SDL_CreateWindow(
        "Monopoly Online",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!gWindow) {
        printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) {
        printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return -1;
    }

    gFontLarge = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 52);
    gFontMedium = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 24);
    gFontSmall = TTF_OpenFont("assets/fonts/UbuntuMono-R.ttf", 18);

    if (!gFontLarge || !gFontMedium || !gFontSmall) {
        printf("Failed to load fonts! SDL_ttf Error: %s\n", TTF_GetError());
        return -1;
    }

    SDL_StartTextInput();
    return 0;
}

void Lobby_close(void) {
    SDL_StopTextInput();

    if (gFontLarge) { TTF_CloseFont(gFontLarge); gFontLarge = NULL; }
    if (gFontMedium) { TTF_CloseFont(gFontMedium); gFontMedium = NULL; }
    if (gFontSmall) { TTF_CloseFont(gFontSmall); gFontSmall = NULL; }

    TTF_Quit();

    if (gRenderer) { SDL_DestroyRenderer(gRenderer); gRenderer = NULL; }
    if (gWindow) { SDL_DestroyWindow(gWindow); gWindow = NULL; }

    SDL_Quit();
}

MatchFoundInfo* Lobby_getMatchInfo(void) {
    return matchFound ? &matchInfo : NULL;
}

static void clearInputField(InputField* field) {
    memset(field->text, 0, sizeof(field->text));
    field->active = 0;
}

static void initButton(Button* btn, const char* label, int x, int y, int w, int h) {
    memset(btn, 0, sizeof(Button));
    strncpy(btn->label, label, sizeof(btn->label) - 1);
    btn->rect.x = x;
    btn->rect.y = y;
    btn->rect.w = w;
    btn->rect.h = h;
    btn->enabled = 1;
}

static void renderText(const char* text, int x, int y, TTF_Font* font, SDL_Color color) {
    if (!text || !font || text[0] == '\0') return;
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, surface);
    if (texture) {
        SDL_Rect dest = {x, y, surface->w, surface->h};
        SDL_RenderCopy(gRenderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void renderTextCentered(const char* text, int centerX, int y, TTF_Font* font, SDL_Color color) {
    if (!text || !font || text[0] == '\0') return;
    
    int tw, th;
    TTF_SizeText(font, text, &tw, &th);
    renderText(text, centerX - tw / 2, y, font, color);
}

static void drawPanel(int x, int y, int w, int h) {
    SDL_Rect panel = {x, y, w, h};
    
    // Shadow
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 80);
    SDL_Rect shadow = {x + 4, y + 4, w, h};
    SDL_RenderFillRect(gRenderer, &shadow);
    
    // Main panel
    SDL_SetRenderDrawColor(gRenderer, COLOR_PANEL_R, COLOR_PANEL_G, COLOR_PANEL_B, 255);
    SDL_RenderFillRect(gRenderer, &panel);
    
    // Border
    SDL_SetRenderDrawColor(gRenderer, 80, 85, 110, 255);
    SDL_RenderDrawRect(gRenderer, &panel);
}

static void drawInputField(InputField* field, const char* label, int labelY) {
    SDL_Color labelColor = {180, 180, 180, 255};
    SDL_Color textColor = {30, 30, 30, 255};
    
    // Label above field
    renderText(label, field->rect.x, labelY, gFontSmall, labelColor);
    
    // Input background
    if (field->active) {
        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
    } else {
        SDL_SetRenderDrawColor(gRenderer, 230, 230, 230, 255);
    }
    SDL_RenderFillRect(gRenderer, &field->rect);
    
    // Border
    if (field->active) {
        SDL_SetRenderDrawColor(gRenderer, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255);
        SDL_Rect outer = {field->rect.x - 1, field->rect.y - 1, field->rect.w + 2, field->rect.h + 2};
        SDL_RenderDrawRect(gRenderer, &outer);
    } else {
        SDL_SetRenderDrawColor(gRenderer, 120, 120, 120, 255);
    }
    SDL_RenderDrawRect(gRenderer, &field->rect);
    
    // Text content
    char displayText[140];
    memset(displayText, 0, sizeof(displayText));
    
    if (field->is_password && field->text[0] != '\0') {
        size_t len = strlen(field->text);
        for (size_t i = 0; i < len && i < 126; i++) {
            displayText[i] = '*';
        }
    } else {
        strncpy(displayText, field->text, 126);
    }
    
    // Blinking cursor
    if (field->active) {
        if ((SDL_GetTicks() / 530) % 2 == 0) {
            strcat(displayText, "|");
        }
    }
    
    if (displayText[0] != '\0') {
        renderText(displayText, field->rect.x + 10, field->rect.y + 10, gFontMedium, textColor);
    }
}

static void drawButton(Button* btn) {
    if (!btn->enabled) return;
    
    int r, g, b;
    SDL_Color textColor = {255, 255, 255, 255};
    
    if (btn->hovered) {
        r = COLOR_ACCENT_R;
        g = COLOR_ACCENT_G;
        b = COLOR_ACCENT_B;
        textColor.r = 0;
        textColor.g = 0;
        textColor.b = 0;
    } else {
        r = 70;
        g = 75;
        b = 100;
    }
    
    // Button shadow
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 60);
    SDL_Rect shadow = {btn->rect.x + 2, btn->rect.y + 2, btn->rect.w, btn->rect.h};
    SDL_RenderFillRect(gRenderer, &shadow);
    
    // Button background
    SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
    SDL_RenderFillRect(gRenderer, &btn->rect);
    
    // Button border
    SDL_SetRenderDrawColor(gRenderer, r + 30, g + 30, b + 30, 255);
    SDL_RenderDrawRect(gRenderer, &btn->rect);
    
    // Button text
    int tw, th;
    TTF_SizeText(gFontMedium, btn->label, &tw, &th);
    renderText(btn->label, 
               btn->rect.x + (btn->rect.w - tw) / 2,
               btn->rect.y + (btn->rect.h - th) / 2,
               gFontMedium, textColor);
}

// Draw a small challenge button
static void drawSmallButton(Button* btn, SDL_Color bgColor) {
    if (!btn->enabled) return;
    
    SDL_Color textColor = {255, 255, 255, 255};
    
    if (btn->hovered) {
        bgColor.r = (bgColor.r + 50 > 255) ? 255 : bgColor.r + 50;
        bgColor.g = (bgColor.g + 50 > 255) ? 255 : bgColor.g + 50;
        bgColor.b = (bgColor.b + 50 > 255) ? 255 : bgColor.b + 50;
    }
    
    SDL_SetRenderDrawColor(gRenderer, bgColor.r, bgColor.g, bgColor.b, 255);
    SDL_RenderFillRect(gRenderer, &btn->rect);
    
    SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 100);
    SDL_RenderDrawRect(gRenderer, &btn->rect);
    
    int tw, th;
    TTF_SizeText(gFontSmall, btn->label, &tw, &th);
    renderText(btn->label, 
               btn->rect.x + (btn->rect.w - tw) / 2,
               btn->rect.y + (btn->rect.h - th) / 2,
               gFontSmall, textColor);
}

static int isMouseOver(SDL_Rect* rect, int mx, int my) {
    return mx >= rect->x && mx < rect->x + rect->w &&
           my >= rect->y && my < rect->y + rect->h;
}

static void setStatus(const char* msg, int isError) {
    memset(statusMessage, 0, sizeof(statusMessage));
    if (msg) {
        strncpy(statusMessage, msg, sizeof(statusMessage) - 1);
    }
    statusIsError = isError;
}

static void renderTitle(void) {
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    
    renderTextCentered("MONOPOLY", SCREEN_WIDTH / 2, 60, gFontLarge, gold);
    renderTextCentered("ONLINE", SCREEN_WIDTH / 2, 115, gFontMedium, white);
}

static void renderStatus(void) {
    if (statusMessage[0] == '\0') return;
    
    SDL_Color color;
    if (statusIsError) {
        color = (SDL_Color){255, 120, 120, 255};
    } else {
        color = (SDL_Color){120, 255, 120, 255};
    }
    
    renderTextCentered(statusMessage, SCREEN_WIDTH / 2, 700, gFontSmall, color);
}

// ============ SERVER MESSAGE PROCESSING ============

static void parseOnlinePlayersList(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    cJSON* success = cJSON_GetObjectItem(json, "success");
    if (!success || !cJSON_IsTrue(success)) {
        cJSON_Delete(json);
        return;
    }
    
    cJSON* players = cJSON_GetObjectItem(json, "players");
    if (!players || !cJSON_IsArray(players)) {
        cJSON_Delete(json);
        return;
    }
    
    onlinePlayerCount = 0;
    cJSON* player;
    cJSON_ArrayForEach(player, players) {
        if (onlinePlayerCount >= MAX_ONLINE_PLAYERS) break;
        
        cJSON* id = cJSON_GetObjectItem(player, "user_id");
        cJSON* name = cJSON_GetObjectItem(player, "username");
        cJSON* elo = cJSON_GetObjectItem(player, "elo_rating");
        cJSON* status = cJSON_GetObjectItem(player, "status");
        
        if (id && name) {
            onlinePlayers[onlinePlayerCount].user_id = id->valueint;
            strncpy(onlinePlayers[onlinePlayerCount].username, 
                    name->valuestring, sizeof(onlinePlayers[0].username) - 1);
            onlinePlayers[onlinePlayerCount].elo_rating = elo ? elo->valueint : 1200;
            if (status && cJSON_IsString(status)) {
                strncpy(onlinePlayers[onlinePlayerCount].status,
                        status->valuestring, sizeof(onlinePlayers[0].status) - 1);
            }
            onlinePlayerCount++;
        }
    }
    
    cJSON_Delete(json);
    printf("[LOBBY] Updated online players: %d\n", onlinePlayerCount);
}

static void parseChallengeRequest(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    cJSON* id = cJSON_GetObjectItem(json, "challenge_id");
    cJSON* challenger_id = cJSON_GetObjectItem(json, "challenger_id");
    cJSON* challenger_name = cJSON_GetObjectItem(json, "challenger_name");
    cJSON* challenger_elo = cJSON_GetObjectItem(json, "challenger_elo");
    
    if (id && challenger_id && challenger_name) {
        pendingChallenge.challenge_id = id->valueint;
        pendingChallenge.challenger_id = challenger_id->valueint;
        strncpy(pendingChallenge.challenger_name, challenger_name->valuestring,
                sizeof(pendingChallenge.challenger_name) - 1);
        pendingChallenge.challenger_elo = challenger_elo ? challenger_elo->valueint : 1200;
        hasPendingChallenge = 1;
        printf("[LOBBY] Challenge received from %s\n", pendingChallenge.challenger_name);
    }
    
    cJSON_Delete(json);
}

static void parseMatchFound(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    cJSON* match_id = cJSON_GetObjectItem(json, "match_id");
    cJSON* opp_id = cJSON_GetObjectItem(json, "opponent_id");
    cJSON* opp_name = cJSON_GetObjectItem(json, "opponent_name");
    cJSON* opp_elo = cJSON_GetObjectItem(json, "opponent_elo");
    cJSON* player_num = cJSON_GetObjectItem(json, "your_player_num");
    
    if (match_id && opp_name) {
        matchInfo.match_id = match_id->valueint;
        matchInfo.opponent_id = opp_id ? opp_id->valueint : 0;
        strncpy(matchInfo.opponent_name, opp_name->valuestring,
                sizeof(matchInfo.opponent_name) - 1);
        matchInfo.opponent_elo = opp_elo ? opp_elo->valueint : 1200;
        matchInfo.your_player_num = player_num ? player_num->valueint : 1;
        matchFound = 1;
        printf("[LOBBY] Match found! vs %s (ELO %d)\n", 
               matchInfo.opponent_name, matchInfo.opponent_elo);
    }
    
    cJSON_Delete(json);
}

static void parseGameResult(ClientState* client, const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    cJSON* match_id = cJSON_GetObjectItem(json, "match_id");
    cJSON* is_draw = cJSON_GetObjectItem(json, "is_draw");
    cJSON* winner_id = cJSON_GetObjectItem(json, "winner_id");
    cJSON* winner_name = cJSON_GetObjectItem(json, "winner_name");
    cJSON* loser_name = cJSON_GetObjectItem(json, "loser_name");
    cJSON* winner_elo_before = cJSON_GetObjectItem(json, "winner_elo_before");
    cJSON* winner_elo_after = cJSON_GetObjectItem(json, "winner_elo_after");
    cJSON* winner_elo_change = cJSON_GetObjectItem(json, "winner_elo_change");
    cJSON* loser_elo_before = cJSON_GetObjectItem(json, "loser_elo_before");
    cJSON* loser_elo_after = cJSON_GetObjectItem(json, "loser_elo_after");
    cJSON* loser_elo_change = cJSON_GetObjectItem(json, "loser_elo_change");
    cJSON* reason = cJSON_GetObjectItem(json, "reason");
    
    lastGameResult.match_id = match_id ? match_id->valueint : 0;
    lastGameResult.is_draw = is_draw && cJSON_IsTrue(is_draw);
    
    int you_won = (winner_id && winner_id->valueint == client->user_id);
    lastGameResult.you_won = you_won;
    
    if (you_won) {
        strncpy(lastGameResult.opponent_name, 
                loser_name ? loser_name->valuestring : "Opponent",
                sizeof(lastGameResult.opponent_name) - 1);
        lastGameResult.your_elo_before = winner_elo_before ? winner_elo_before->valueint : 0;
        lastGameResult.your_elo_after = winner_elo_after ? winner_elo_after->valueint : 0;
        lastGameResult.your_elo_change = winner_elo_change ? winner_elo_change->valueint : 0;
        lastGameResult.opponent_elo_before = loser_elo_before ? loser_elo_before->valueint : 0;
        lastGameResult.opponent_elo_after = loser_elo_after ? loser_elo_after->valueint : 0;
    } else {
        strncpy(lastGameResult.opponent_name,
                winner_name ? winner_name->valuestring : "Opponent",
                sizeof(lastGameResult.opponent_name) - 1);
        lastGameResult.your_elo_before = loser_elo_before ? loser_elo_before->valueint : 0;
        lastGameResult.your_elo_after = loser_elo_after ? loser_elo_after->valueint : 0;
        lastGameResult.your_elo_change = loser_elo_change ? loser_elo_change->valueint : 0;
        lastGameResult.opponent_elo_before = winner_elo_before ? winner_elo_before->valueint : 0;
        lastGameResult.opponent_elo_after = winner_elo_after ? winner_elo_after->valueint : 0;
    }
    
    if (reason && cJSON_IsString(reason)) {
        strncpy(lastGameResult.reason, reason->valuestring, sizeof(lastGameResult.reason) - 1);
    }
    
    // Update client ELO
    client->elo_rating = lastGameResult.your_elo_after;
    if (you_won) client->wins++;
    else client->losses++;
    client->total_matches++;
    
    hasGameResult = 1;
    printf("[LOBBY] Game result: %s (ELO %+d)\n", 
           you_won ? "WIN" : "LOSS", lastGameResult.your_elo_change);
    
    cJSON_Delete(json);
}

static void parseHistoryList(const char* payload) {
    cJSON* json = cJSON_Parse(payload);
    if (!json) return;
    
    if (!cJSON_IsArray(json)) {
        cJSON_Delete(json);
        return;
    }
    
    historyCount = 0;
    cJSON* item;
    cJSON_ArrayForEach(item, json) {
        if (historyCount >= 20) break;
        
        cJSON* mid = cJSON_GetObjectItem(item, "match_id");
        cJSON* oid = cJSON_GetObjectItem(item, "opponent_id");
        cJSON* oname = cJSON_GetObjectItem(item, "opponent_name");
        cJSON* win = cJSON_GetObjectItem(item, "is_win");
        cJSON* elo = cJSON_GetObjectItem(item, "elo_change");
        cJSON* time = cJSON_GetObjectItem(item, "timestamp");
        
        if (mid) {
            historyList[historyCount].match_id = mid->valueint;
            historyList[historyCount].opponent_id = oid ? oid->valueint : 0;
            strncpy(historyList[historyCount].opponent_name, 
                    oname ? oname->valuestring : "Unknown", 
                    sizeof(historyList[0].opponent_name) - 1);
            historyList[historyCount].is_win = win ? win->valueint : 0;
            historyList[historyCount].elo_change = elo ? elo->valueint : 0;
            strncpy(historyList[historyCount].timestamp, 
                    time ? time->valuestring : "", 
                    sizeof(historyList[0].timestamp) - 1);
            historyCount++;
        }
    }
    
    cJSON_Delete(json);
    printf("[LOBBY] Parsed %d history entries\n", historyCount);
}

static void processServerMessages(ClientState* client, LobbyState* state) {
    if (!client_is_connected(client)) return;
    
    // Check for incoming messages (non-blocking)
    while (client_data_available(client) > 0) {
        NetworkMessage msg;
        if (client_receive(client, &msg) == 0) {
            printf("[LOBBY] Received message type: %d\n", msg.type);
            
            switch (msg.type) {
                case MSG_ONLINE_PLAYERS_LIST:
                    parseOnlinePlayersList(msg.payload);
                    break;
                    
                case MSG_CHALLENGE_REQUEST:
                    parseChallengeRequest(msg.payload);
                    if (hasPendingChallenge) {
                        *state = LOBBY_STATE_CHALLENGE_RECEIVED;
                    }
                    break;
                    
                case MSG_MATCH_FOUND:
                    parseMatchFound(msg.payload);
                    if (matchFound) {
                        *state = LOBBY_STATE_START_GAME;
                    }
                    break;
                    
                case MSG_GAME_RESULT:
                    parseGameResult(client, msg.payload);
                    if (hasGameResult) {
                        *state = LOBBY_STATE_GAME_RESULT;
                    }
                    break;

                case MSG_HISTORY_LIST:
                    parseHistoryList(msg.payload);
                    *state = LOBBY_STATE_VIEW_HISTORY;
                    break;
                    
                case MSG_DECLINE_CHALLENGE:
                    setStatus("Your challenge was declined", 0);
                    break;
                    
                case MSG_ERROR:
                    {
                        cJSON* json = cJSON_Parse(msg.payload);
                        if (json) {
                            cJSON* error = cJSON_GetObjectItem(json, "error");
                            if (error && cJSON_IsString(error)) {
                                setStatus(error->valuestring, 1);
                            }
                            cJSON_Delete(json);
                        }
                    }
                    break;
                    
                case MSG_SUCCESS:
                    {
                        cJSON* json = cJSON_Parse(msg.payload);
                        if (json) {
                            cJSON* message = cJSON_GetObjectItem(json, "message");
                            if (message && cJSON_IsString(message)) {
                                setStatus(message->valuestring, 0);
                            }
                            cJSON_Delete(json);
                        }
                    }
                    break;
                    
                default:
                    break;
            }
        }
    }
}

static void refreshOnlinePlayers(ClientState* client) {
    Uint32 now = SDL_GetTicks();
    if (now - lastPlayersRefresh >= REFRESH_INTERVAL) {
        client_get_online_players(client);
        lastPlayersRefresh = now;
    }
}

// ============ SCREEN RENDERERS ============

static void renderConnectScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 200;
    drawPanel(panelX, panelY, 360, 320);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Connect to Server", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    inputServerIP.rect = (SDL_Rect){panelX + 30, panelY + 100, 300, 44};
    inputServerPort.rect = (SDL_Rect){panelX + 30, panelY + 180, 300, 44};
    
    drawInputField(&inputServerIP, "Server IP Address", panelY + 75);
    drawInputField(&inputServerPort, "Port", panelY + 155);
    
    initButton(&btnConnect, "Connect", panelX + 30, panelY + 250, 145, 45);
    initButton(&btnExit, "Exit", panelX + 185, panelY + 250, 145, 45);
    
    drawButton(&btnConnect);
    drawButton(&btnExit);
    
    renderStatus();
    
    SDL_Color gray = {130, 130, 130, 255};
    renderTextCentered("Press ENTER to connect", SCREEN_WIDTH / 2, 750, gFontSmall, gray);
}

static void renderLoginScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 200;
    drawPanel(panelX, panelY, 360, 380);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Login", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    inputUsername.rect = (SDL_Rect){panelX + 30, panelY + 100, 300, 44};
    inputPassword.rect = (SDL_Rect){panelX + 30, panelY + 185, 300, 44};
    
    drawInputField(&inputUsername, "Username", panelY + 75);
    drawInputField(&inputPassword, "Password", panelY + 160);
    
    initButton(&btnLogin, "Login", panelX + 30, panelY + 260, 145, 45);
    initButton(&btnRegister, "Register", panelX + 185, panelY + 260, 145, 45);
    initButton(&btnBack, "Disconnect", panelX + 80, panelY + 320, 200, 40);
    
    drawButton(&btnLogin);
    drawButton(&btnRegister);
    drawButton(&btnBack);
    
    renderStatus();
    
    SDL_Color gray = {130, 130, 130, 255};
    renderTextCentered("Press TAB to switch fields, ENTER to login", SCREEN_WIDTH / 2, 750, gFontSmall, gray);
}

static void renderRegisterScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 180;
    drawPanel(panelX, panelY, 360, 450);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Create Account", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    inputUsername.rect = (SDL_Rect){panelX + 30, panelY + 95, 300, 44};
    inputPassword.rect = (SDL_Rect){panelX + 30, panelY + 180, 300, 44};
    inputEmail.rect = (SDL_Rect){panelX + 30, panelY + 265, 300, 44};
    
    drawInputField(&inputUsername, "Username (3-20 characters)", panelY + 70);
    drawInputField(&inputPassword, "Password (min 4 characters)", panelY + 155);
    drawInputField(&inputEmail, "Email (optional)", panelY + 240);
    
    initButton(&btnRegister, "Create Account", panelX + 30, panelY + 340, 300, 45);
    initButton(&btnBack, "Back to Login", panelX + 80, panelY + 395, 200, 40);
    
    drawButton(&btnRegister);
    drawButton(&btnBack);
    
    renderStatus();
}

static void renderHistoryScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 350;
    int panelY = 180;
    drawPanel(panelX, panelY, 700, 500);
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    SDL_Color green = {100, 255, 100, 255};
    SDL_Color red = {255, 100, 100, 255};
    
    renderTextCentered("MATCH HISTORY", SCREEN_WIDTH / 2, panelY + 20, gFontMedium, gold);
    
    // Headers
    int y = panelY + 60;
    renderText("Result", panelX + 30, y, gFontSmall, gray);
    renderText("Opponent", panelX + 130, y, gFontSmall, gray);
    renderText("ELO Change", panelX + 350, y, gFontSmall, gray);
    renderText("Date", panelX + 500, y, gFontSmall, gray);
    
    y += 30;
    
    if (historyCount == 0) {
        renderTextCentered("No matches played yet.", SCREEN_WIDTH / 2, panelY + 200, gFontMedium, gray);
    }
    
    for (int i = 0; i < historyCount; i++) {
        MatchHistoryEntry* e = &historyList[i];
        
        // Result
        SDL_Color resColor = gray;
        const char* resText = "Draw";
        if (e->is_win == 1) { resText = "WIN"; resColor = green; }
        else if (e->is_win == 0) { resText = "LOSS"; resColor = red; }
        
        renderText(resText, panelX + 30, y, gFontSmall, resColor);
        
        // Opponent
        renderText(e->opponent_name, panelX + 130, y, gFontSmall, white);
        
        // ELO
        char eloBuf[32];
        snprintf(eloBuf, sizeof(eloBuf), "%+d", e->elo_change);
        SDL_Color eloColor = e->elo_change >= 0 ? green : red;
        renderText(eloBuf, panelX + 350, y, gFontSmall, eloColor);
        
        // Date
        renderText(e->timestamp, panelX + 500, y, gFontSmall, gray);
        
        y += 25;
    }
    
    initButton(&btnBackHistory, "Back", SCREEN_WIDTH / 2 - 50, panelY + 440, 100, 40);
    drawButton(&btnBackHistory);
}

static void renderMainMenu(ClientState* client) {
    renderTitle();
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    SDL_Color green = {100, 255, 100, 255};
    SDL_Color red = {255, 100, 100, 255};
    SDL_Color cyan = {100, 200, 255, 255};
    
    // Profile Panel (left)
    int profileX = 50;
    int profileY = 170;
    drawPanel(profileX, profileY, 280, 280);
    
    renderText("PLAYER PROFILE", profileX + 20, profileY + 15, gFontMedium, gold);
    
    char buf[128];
    
    snprintf(buf, sizeof(buf), "Name: %s", client->username);
    renderText(buf, profileX + 20, profileY + 60, gFontSmall, white);
    
    snprintf(buf, sizeof(buf), "ELO: %d", client->elo_rating);
    renderText(buf, profileX + 20, profileY + 90, gFontSmall, gold);
    
    snprintf(buf, sizeof(buf), "Games: %d", client->total_matches);
    renderText(buf, profileX + 20, profileY + 125, gFontSmall, gray);
    
    snprintf(buf, sizeof(buf), "Wins: %d", client->wins);
    renderText(buf, profileX + 20, profileY + 150, gFontSmall, green);
    
    snprintf(buf, sizeof(buf), "Losses: %d", client->losses);
    renderText(buf, profileX + 20, profileY + 175, gFontSmall, red);
    
    if (client->total_matches > 0) {
        float wr = (float)client->wins / client->total_matches * 100.0f;
        snprintf(buf, sizeof(buf), "Win Rate: %.1f%%", wr);
        renderText(buf, profileX + 20, profileY + 205, gFontSmall, gray);
    }
    
    initButton(&btnHistory, "History", profileX + 40, profileY + 235, 95, 35);
    initButton(&btnLogout, "Logout", profileX + 145, profileY + 235, 95, 35);
    drawButton(&btnHistory);
    drawButton(&btnLogout);
    
    // Actions Panel (center)
    int actionX = 350;
    int actionY = 170;
    drawPanel(actionX, actionY, 280, 280);
    
    renderText("PLAY GAME", actionX + 20, actionY + 15, gFontMedium, gold);
    
    renderText("Find an opponent", actionX + 20, actionY + 60, gFontSmall, gray);
    renderText("based on your ELO", actionX + 20, actionY + 82, gFontSmall, gray);
    
    initButton(&btnFindMatch, "Find Match", actionX + 40, actionY + 120, 200, 50);
    drawButton(&btnFindMatch);
    
    renderText("Or challenge a player", actionX + 20, actionY + 190, gFontSmall, gray);
    renderText("from the online list", actionX + 20, actionY + 212, gFontSmall, gray);
    
    initButton(&btnViewPlayers, showOnlinePlayers ? "Hide Players" : "View Players", 
               actionX + 40, actionY + 235, 200, 35);
    drawButton(&btnViewPlayers);
    
    // Online Players Panel (right)
    int playersX = 650;
    int playersY = 170;
    drawPanel(playersX, playersY, 300, 450);
    
    snprintf(buf, sizeof(buf), "ONLINE PLAYERS (%d)", onlinePlayerCount);
    renderText(buf, playersX + 15, playersY + 15, gFontMedium, gold);
    
    if (showOnlinePlayers) {
        int y = playersY + 55;
        int maxDisplay = 8;
        
        for (int i = 0; i < onlinePlayerCount && i < maxDisplay; i++) {
            LobbyPlayerInfo* p = &onlinePlayers[i];
            
            // Skip self
            if (p->user_id == client->user_id) {
                snprintf(buf, sizeof(buf), "%s (You)", p->username);
                renderText(buf, playersX + 15, y, gFontSmall, cyan);
            } else {
                // Player name and ELO
                snprintf(buf, sizeof(buf), "%s", p->username);
                renderText(buf, playersX + 15, y, gFontSmall, white);
                
                snprintf(buf, sizeof(buf), "ELO: %d", p->elo_rating);
                renderText(buf, playersX + 15, y + 18, gFontSmall, gray);
                
                // Status indicator
                SDL_Color statusColor;
                if (strcmp(p->status, "idle") == 0) {
                    statusColor = green;
                } else if (strcmp(p->status, "searching") == 0) {
                    statusColor = (SDL_Color){255, 200, 100, 255};
                } else {
                    statusColor = red;
                }
                
                // Challenge button (only for idle players)
                if (strcmp(p->status, "idle") == 0) {
                    initButton(&btnChallenge[i], "Challenge", playersX + 200, y + 5, 85, 28);
                    btnChallenge[i].user_data = p->user_id;
                    btnChallenge[i].enabled = 1;
                    drawSmallButton(&btnChallenge[i], (SDL_Color){80, 120, 80, 255});
                } else {
                    renderText(p->status, playersX + 200, y + 8, gFontSmall, statusColor);
                }
            }
            
            y += 48;
        }
        
        if (onlinePlayerCount == 0) {
            renderText("No players online", playersX + 15, playersY + 60, gFontSmall, gray);
        }
    } else {
        renderText("Click 'View Players'", playersX + 15, playersY + 60, gFontSmall, gray);
        renderText("to see online players", playersX + 15, playersY + 82, gFontSmall, gray);
    }
    
    renderStatus();
    
    SDL_Color hint = {100, 100, 100, 255};
    renderTextCentered("Press ESC to logout", SCREEN_WIDTH / 2, 760, gFontSmall, hint);
}

static void renderSearchingScreen(ClientState* client) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 200;
    int panelY = 250;
    drawPanel(panelX, panelY, 400, 250);
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    
    renderTextCentered("SEARCHING FOR MATCH", SCREEN_WIDTH / 2, panelY + 30, gFontMedium, gold);
    
    // Animated dots
    Uint32 elapsed = SDL_GetTicks() - searchStartTime;
    int dots = (elapsed / 500) % 4;
    char dotStr[8] = "";
    for (int i = 0; i < dots; i++) strcat(dotStr, ".");
    
    char buf[64];
    snprintf(buf, sizeof(buf), "Searching%s", dotStr);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 80, gFontMedium, white);
    
    // Spinning animation
    int centerX = SCREEN_WIDTH / 2;
    int centerY = panelY + 130;
    int radius = 20;
    float angle = (elapsed / 100.0f);
    
    for (int i = 0; i < 8; i++) {
        float a = angle + (i * 3.14159f / 4);
        int x = centerX + (int)(cos(a) * radius);
        int y = centerY + (int)(sin(a) * radius);
        int alpha = 255 - (i * 30);
        if (alpha < 50) alpha = 50;
        
        SDL_SetRenderDrawColor(gRenderer, COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, alpha);
        SDL_Rect dot = {x - 4, y - 4, 8, 8};
        SDL_RenderFillRect(gRenderer, &dot);
    }
    
    snprintf(buf, sizeof(buf), "Your ELO: %d", client->elo_rating);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 170, gFontSmall, gray);
    
    int searchSecs = elapsed / 1000;
    snprintf(buf, sizeof(buf), "Time: %d:%02d", searchSecs / 60, searchSecs % 60);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 195, gFontSmall, gray);
    
    initButton(&btnCancelSearch, "Cancel", SCREEN_WIDTH / 2 - 75, panelY + 220, 150, 40);
    btnCancelSearch.rect.y = panelY + 200;
    drawButton(&btnCancelSearch);
}

static void renderChallengeScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 200;
    int panelY = 250;
    drawPanel(panelX, panelY, 400, 280);
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    
    renderTextCentered("CHALLENGE RECEIVED!", SCREEN_WIDTH / 2, panelY + 30, gFontMedium, gold);
    
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", pendingChallenge.challenger_name);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 80, gFontMedium, white);
    
    snprintf(buf, sizeof(buf), "ELO: %d", pendingChallenge.challenger_elo);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 115, gFontSmall, gray);
    
    renderTextCentered("wants to play against you!", SCREEN_WIDTH / 2, panelY + 150, gFontSmall, white);
    
    initButton(&btnAccept, "Accept", panelX + 40, panelY + 200, 140, 50);
    initButton(&btnDecline, "Decline", panelX + 220, panelY + 200, 140, 50);
    
    drawButton(&btnAccept);
    drawButton(&btnDecline);
}

static void renderGameResultScreen(void) {
    renderTitle();
    
    int panelX = SCREEN_WIDTH / 2 - 220;
    int panelY = 200;
    drawPanel(panelX, panelY, 440, 380);
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    SDL_Color green = {100, 255, 100, 255};
    SDL_Color red = {255, 100, 100, 255};
    
    // Result title
    if (lastGameResult.is_draw) {
        renderTextCentered("DRAW!", SCREEN_WIDTH / 2, panelY + 30, gFontLarge, gold);
    } else if (lastGameResult.you_won) {
        renderTextCentered("VICTORY!", SCREEN_WIDTH / 2, panelY + 30, gFontLarge, green);
    } else {
        renderTextCentered("DEFEAT", SCREEN_WIDTH / 2, panelY + 30, gFontLarge, red);
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "vs %s", lastGameResult.opponent_name);
    renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 90, gFontMedium, white);
    
    if (lastGameResult.reason[0] != '\0') {
        snprintf(buf, sizeof(buf), "(%s)", lastGameResult.reason);
        renderTextCentered(buf, SCREEN_WIDTH / 2, panelY + 120, gFontSmall, gray);
    }
    
    // ELO changes
    renderText("ELO Rating:", panelX + 40, panelY + 160, gFontMedium, gold);
    
    snprintf(buf, sizeof(buf), "Before: %d", lastGameResult.your_elo_before);
    renderText(buf, panelX + 60, panelY + 195, gFontSmall, gray);
    
    snprintf(buf, sizeof(buf), "After:  %d", lastGameResult.your_elo_after);
    renderText(buf, panelX + 60, panelY + 220, gFontSmall, white);
    
    SDL_Color changeColor = lastGameResult.your_elo_change >= 0 ? green : red;
    snprintf(buf, sizeof(buf), "Change: %+d", lastGameResult.your_elo_change);
    renderText(buf, panelX + 60, panelY + 250, gFontMedium, changeColor);
    
    // Opponent info
    snprintf(buf, sizeof(buf), "Opponent ELO: %d -> %d", 
             lastGameResult.opponent_elo_before, lastGameResult.opponent_elo_after);
    renderText(buf, panelX + 60, panelY + 290, gFontSmall, gray);
    
    // Buttons
    initButton(&btnOK, "Back to Lobby", panelX + 80, panelY + 330, 140, 40);
    initButton(&btnRematch, "Rematch", panelX + 240, panelY + 330, 120, 40);
    
    drawButton(&btnOK);
    drawButton(&btnRematch);
}

// ============ MAIN LOOP ============

LobbyState Lobby_run(ClientState* client, const char* server_ip, int port) {
    LobbyState state = LOBBY_STATE_CONNECTING;
    SDL_Event event;
    int running = 1;
    int mx, my;
    
    // Setup initial connect screen
    strncpy(inputServerIP.text, server_ip ? server_ip : "127.0.0.1", sizeof(inputServerIP.text) - 1);
    snprintf(inputServerPort.text, sizeof(inputServerPort.text), "%d", port > 0 ? port : 8888);
    inputServerIP.active = 1;
    
    // Check if already connected and logged in - skip to main menu
    if (client_is_connected(client) && client->user_id > 0) {
        printf("[LOBBY] Already logged in as %s, going to main menu\n", client->username);
        state = LOBBY_STATE_MAIN_MENU;
        setStatus("", 0);  // Clear any old status
    } else if (client_is_connected(client)) {
        printf("[LOBBY] Connected but not logged in, going to login\n");
        state = LOBBY_STATE_LOGIN;
        setStatus("", 0);
    }
    
    while (running) {
        // Get mouse position
        SDL_GetMouseState(&mx, &my);
        
        // Update button hover states
        btnConnect.hovered = isMouseOver(&btnConnect.rect, mx, my);
        btnExit.hovered = isMouseOver(&btnExit.rect, mx, my);
        btnLogin.hovered = isMouseOver(&btnLogin.rect, mx, my);
        btnRegister.hovered = isMouseOver(&btnRegister.rect, mx, my);
        btnBack.hovered = isMouseOver(&btnBack.rect, mx, my);
        btnFindMatch.hovered = isMouseOver(&btnFindMatch.rect, mx, my);
        btnViewPlayers.hovered = isMouseOver(&btnViewPlayers.rect, mx, my);
        btnCancelSearch.hovered = isMouseOver(&btnCancelSearch.rect, mx, my);
        btnLogout.hovered = isMouseOver(&btnLogout.rect, mx, my);
        btnAccept.hovered = isMouseOver(&btnAccept.rect, mx, my);
        btnDecline.hovered = isMouseOver(&btnDecline.rect, mx, my);
        btnOK.hovered = isMouseOver(&btnOK.rect, mx, my);
        btnRematch.hovered = isMouseOver(&btnRematch.rect, mx, my);
        btnHistory.hovered = isMouseOver(&btnHistory.rect, mx, my);
        btnBackHistory.hovered = isMouseOver(&btnBackHistory.rect, mx, my);
        
        for (int i = 0; i < MAX_ONLINE_PLAYERS; i++) {
            btnChallenge[i].hovered = isMouseOver(&btnChallenge[i].rect, mx, my);
        }
        
        // Process server messages
        if (state != LOBBY_STATE_CONNECTING && client_is_connected(client)) {
            processServerMessages(client, &state);
        }
        
        // Auto-refresh online players
        if ((state == LOBBY_STATE_MAIN_MENU || state == LOBBY_STATE_SEARCHING) && 
            showOnlinePlayers && client_is_connected(client)) {
            refreshOnlinePlayers(client);
        }
        
        // Check for match found during search
        if (state == LOBBY_STATE_SEARCHING && matchFound) {
            state = LOBBY_STATE_START_GAME;
            running = 0;
        }
        
        // Process events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                state = LOBBY_STATE_EXIT;
            }
            
            // Text input
            if (event.type == SDL_TEXTINPUT) {
                InputField* activeField = NULL;
                
                if (state == LOBBY_STATE_CONNECTING) {
                    if (inputServerIP.active) activeField = &inputServerIP;
                    else if (inputServerPort.active) activeField = &inputServerPort;
                } else if (state == LOBBY_STATE_LOGIN) {
                    if (inputUsername.active) activeField = &inputUsername;
                    else if (inputPassword.active) activeField = &inputPassword;
                } else if (state == LOBBY_STATE_REGISTER) {
                    if (inputUsername.active) activeField = &inputUsername;
                    else if (inputPassword.active) activeField = &inputPassword;
                    else if (inputEmail.active) activeField = &inputEmail;
                }
                
                if (activeField && strlen(activeField->text) < sizeof(activeField->text) - 2) {
                    strcat(activeField->text, event.text.text);
                }
            }
            
            // Key input
            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                
                // Handle backspace for active field
                InputField* activeField = NULL;
                if (state == LOBBY_STATE_CONNECTING) {
                    if (inputServerIP.active) activeField = &inputServerIP;
                    else if (inputServerPort.active) activeField = &inputServerPort;
                } else if (state == LOBBY_STATE_LOGIN || state == LOBBY_STATE_REGISTER) {
                    if (inputUsername.active) activeField = &inputUsername;
                    else if (inputPassword.active) activeField = &inputPassword;
                    else if (inputEmail.active) activeField = &inputEmail;
                }
                
                if (key == SDLK_BACKSPACE && activeField && activeField->text[0] != '\0') {
                    activeField->text[strlen(activeField->text) - 1] = '\0';
                }
                
                // State-specific key handling
                if (state == LOBBY_STATE_CONNECTING) {
                    if (key == SDLK_TAB) {
                        inputServerIP.active = !inputServerIP.active;
                        inputServerPort.active = !inputServerPort.active;
                    }
                    if (key == SDLK_RETURN) {
                        int p = atoi(inputServerPort.text);
                        if (p <= 0) p = 8888;
                        
                        setStatus("Connecting...", 0);
                        if (client_connect(client, inputServerIP.text, p) == 0) {
                            setStatus("Connected!", 0);
                            state = LOBBY_STATE_LOGIN;
                            clearInputField(&inputUsername);
                            clearInputField(&inputPassword);
                            inputUsername.active = 1;
                        } else {
                            setStatus("Connection failed!", 1);
                        }
                    }
                    if (key == SDLK_ESCAPE) {
                        running = 0;
                        state = LOBBY_STATE_EXIT;
                    }
                }
                else if (state == LOBBY_STATE_LOGIN) {
                    if (key == SDLK_TAB) {
                        inputUsername.active = !inputUsername.active;
                        inputPassword.active = !inputPassword.active;
                    }
                    if (key == SDLK_RETURN) {
                        if (inputUsername.text[0] != '\0' && inputPassword.text[0] != '\0') {
                            setStatus("Logging in...", 0);
                            if (client_login(client, inputUsername.text, inputPassword.text) == 0) {
                                setStatus("Welcome!", 0);
                                state = LOBBY_STATE_MAIN_MENU;
                                showOnlinePlayers = 0;
                                lastPlayersRefresh = 0;
                            } else {
                                setStatus("Invalid username or password!", 1);
                            }
                        } else {
                            setStatus("Please enter username and password", 1);
                        }
                    }
                    if (key == SDLK_ESCAPE) {
                        client_disconnect(client);
                        state = LOBBY_STATE_CONNECTING;
                        setStatus("Disconnected", 0);
                    }
                }
                else if (state == LOBBY_STATE_REGISTER) {
                    if (key == SDLK_TAB) {
                        if (inputUsername.active) {
                            inputUsername.active = 0;
                            inputPassword.active = 1;
                        } else if (inputPassword.active) {
                            inputPassword.active = 0;
                            inputEmail.active = 1;
                        } else {
                            inputEmail.active = 0;
                            inputUsername.active = 1;
                        }
                    }
                    if (key == SDLK_RETURN) {
                        const char* email = inputEmail.text[0] != '\0' ? inputEmail.text : NULL;
                        setStatus("Creating account...", 0);
                        if (client_register(client, inputUsername.text, inputPassword.text, email) == 0) {
                            setStatus("Account created! Please login.", 0);
                            state = LOBBY_STATE_LOGIN;
                            clearInputField(&inputPassword);
                            clearInputField(&inputEmail);
                            inputUsername.active = 1;
                        } else {
                            setStatus("Registration failed!", 1);
                        }
                    }
                    if (key == SDLK_ESCAPE) {
                        state = LOBBY_STATE_LOGIN;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        clearInputField(&inputEmail);
                        inputUsername.active = 1;
                        setStatus("", 0);
                    }
                }
                else if (state == LOBBY_STATE_MAIN_MENU) {
                    if (key == SDLK_ESCAPE) {
                        client_logout(client);
                        state = LOBBY_STATE_LOGIN;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        inputUsername.active = 1;
                        setStatus("Logged out", 0);
                    }
                    if (key == SDLK_r) {
                        // Refresh players
                        client_get_online_players(client);
                        lastPlayersRefresh = SDL_GetTicks();
                    }
                }
                else if (state == LOBBY_STATE_SEARCHING) {
                    if (key == SDLK_ESCAPE) {
                        client_cancel_search(client);
                        state = LOBBY_STATE_MAIN_MENU;
                        setStatus("Search cancelled", 0);
                    }
                }
                else if (state == LOBBY_STATE_GAME_RESULT) {
                    if (key == SDLK_ESCAPE || key == SDLK_RETURN) {
                        hasGameResult = 0;
                        state = LOBBY_STATE_MAIN_MENU;
                    }
                }
            }
            
            // Mouse click
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int cx = event.button.x;
                int cy = event.button.y;
                
                if (state == LOBBY_STATE_CONNECTING) {
                    inputServerIP.active = isMouseOver(&inputServerIP.rect, cx, cy);
                    inputServerPort.active = isMouseOver(&inputServerPort.rect, cx, cy);
                    
                    if (isMouseOver(&btnConnect.rect, cx, cy)) {
                        int p = atoi(inputServerPort.text);
                        if (p <= 0) p = 8888;
                        setStatus("Connecting...", 0);
                        if (client_connect(client, inputServerIP.text, p) == 0) {
                            setStatus("Connected!", 0);
                            state = LOBBY_STATE_LOGIN;
                            clearInputField(&inputUsername);
                            clearInputField(&inputPassword);
                            inputUsername.active = 1;
                        } else {
                            setStatus("Connection failed!", 1);
                        }
                    }
                    if (isMouseOver(&btnExit.rect, cx, cy)) {
                        running = 0;
                        state = LOBBY_STATE_EXIT;
                    }
                }
                else if (state == LOBBY_STATE_LOGIN) {
                    inputUsername.active = isMouseOver(&inputUsername.rect, cx, cy);
                    inputPassword.active = isMouseOver(&inputPassword.rect, cx, cy);
                    
                    if (isMouseOver(&btnLogin.rect, cx, cy)) {
                        if (inputUsername.text[0] != '\0' && inputPassword.text[0] != '\0') {
                            setStatus("Logging in...", 0);
                            if (client_login(client, inputUsername.text, inputPassword.text) == 0) {
                                setStatus("Welcome!", 0);
                                state = LOBBY_STATE_MAIN_MENU;
                                showOnlinePlayers = 0;
                                lastPlayersRefresh = 0;
                            } else {
                                setStatus("Invalid username or password!", 1);
                            }
                        }
                    }
                    if (isMouseOver(&btnRegister.rect, cx, cy)) {
                        state = LOBBY_STATE_REGISTER;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        clearInputField(&inputEmail);
                        inputUsername.active = 1;
                        setStatus("", 0);
                    }
                    if (isMouseOver(&btnBack.rect, cx, cy)) {
                        client_disconnect(client);
                        state = LOBBY_STATE_CONNECTING;
                        inputServerIP.active = 1;
                        setStatus("Disconnected", 0);
                    }
                }
                else if (state == LOBBY_STATE_REGISTER) {
                    inputUsername.active = isMouseOver(&inputUsername.rect, cx, cy);
                    inputPassword.active = isMouseOver(&inputPassword.rect, cx, cy);
                    inputEmail.active = isMouseOver(&inputEmail.rect, cx, cy);
                    
                    if (isMouseOver(&btnRegister.rect, cx, cy)) {
                        const char* email = inputEmail.text[0] != '\0' ? inputEmail.text : NULL;
                        setStatus("Creating account...", 0);
                        if (client_register(client, inputUsername.text, inputPassword.text, email) == 0) {
                            setStatus("Account created! Please login.", 0);
                            state = LOBBY_STATE_LOGIN;
                            clearInputField(&inputPassword);
                            clearInputField(&inputEmail);
                            inputUsername.active = 1;
                        } else {
                            setStatus("Registration failed!", 1);
                        }
                    }
                    if (isMouseOver(&btnBack.rect, cx, cy)) {
                        state = LOBBY_STATE_LOGIN;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        inputUsername.active = 1;
                        setStatus("", 0);
                    }
                }
                else if (state == LOBBY_STATE_MAIN_MENU) {
                    if (isMouseOver(&btnFindMatch.rect, cx, cy)) {
                        client_search_match(client);
                        searchStartTime = SDL_GetTicks();
                        state = LOBBY_STATE_SEARCHING;
                        setStatus("", 0);
                    }
                    if (isMouseOver(&btnViewPlayers.rect, cx, cy)) {
                        showOnlinePlayers = !showOnlinePlayers;
                        if (showOnlinePlayers) {
                            client_get_online_players(client);
                            lastPlayersRefresh = SDL_GetTicks();
                        }
                    }
                    if (isMouseOver(&btnHistory.rect, cx, cy)) {
                        client_send(client, MSG_GET_HISTORY, "{}");
                        setStatus("Loading history...", 0);
                    }
                    if (isMouseOver(&btnLogout.rect, cx, cy)) {
                        client_logout(client);
                        state = LOBBY_STATE_LOGIN;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        inputUsername.active = 1;
                        setStatus("Logged out", 0);
                    }
                    
                    // Check challenge buttons
                    for (int i = 0; i < MAX_ONLINE_PLAYERS; i++) {
                        if (btnChallenge[i].enabled && isMouseOver(&btnChallenge[i].rect, cx, cy)) {
                            client_send_challenge(client, btnChallenge[i].user_data);
                            setStatus("Challenge sent!", 0);
                            break;
                        }
                    }
                }
                else if (state == LOBBY_STATE_SEARCHING) {
                    if (isMouseOver(&btnCancelSearch.rect, cx, cy)) {
                        client_cancel_search(client);
                        state = LOBBY_STATE_MAIN_MENU;
                        setStatus("Search cancelled", 0);
                    }
                }
                else if (state == LOBBY_STATE_VIEW_HISTORY) {
                    if (isMouseOver(&btnBackHistory.rect, cx, cy)) {
                        state = LOBBY_STATE_MAIN_MENU;
                    }
                }
                else if (state == LOBBY_STATE_CHALLENGE_RECEIVED) {
                    if (isMouseOver(&btnAccept.rect, cx, cy)) {
                        client_accept_challenge(client, pendingChallenge.challenge_id);
                        hasPendingChallenge = 0;
                        setStatus("Challenge accepted! Starting game...", 0);
                        // Wait for MSG_MATCH_FOUND
                        state = LOBBY_STATE_MAIN_MENU;
                    }
                    if (isMouseOver(&btnDecline.rect, cx, cy)) {
                        client_decline_challenge(client, pendingChallenge.challenge_id);
                        hasPendingChallenge = 0;
                        state = LOBBY_STATE_MAIN_MENU;
                        setStatus("Challenge declined", 0);
                    }
                }
                else if (state == LOBBY_STATE_GAME_RESULT) {
                    if (isMouseOver(&btnOK.rect, cx, cy)) {
                        hasGameResult = 0;
                        state = LOBBY_STATE_MAIN_MENU;
                    }
                    if (isMouseOver(&btnRematch.rect, cx, cy)) {
                        // Send rematch request
                        cJSON* json = cJSON_CreateObject();
                        cJSON_AddNumberToObject(json, "opponent_id", matchInfo.opponent_id);
                        char* payload = cJSON_PrintUnformatted(json);
                        client_send(client, MSG_REMATCH_REQUEST, payload);
                        free(payload);
                        cJSON_Delete(json);
                        setStatus("Rematch request sent!", 0);
                        hasGameResult = 0;
                        state = LOBBY_STATE_MAIN_MENU;
                    }
                }
            }
        }
        
        // === RENDER ===
        
        // Clear screen
        SDL_SetRenderDrawColor(gRenderer, COLOR_BG_R, COLOR_BG_G, COLOR_BG_B, 255);
        SDL_RenderClear(gRenderer);
        
        // Render current screen
        switch (state) {
            case LOBBY_STATE_CONNECTING:
                renderConnectScreen();
                break;
            case LOBBY_STATE_LOGIN:
                renderLoginScreen();
                break;
            case LOBBY_STATE_REGISTER:
                renderRegisterScreen();
                break;
            case LOBBY_STATE_MAIN_MENU:
                renderMainMenu(client);
                break;
            case LOBBY_STATE_SEARCHING:
                renderSearchingScreen(client);
                break;
            case LOBBY_STATE_CHALLENGE_RECEIVED:
                renderChallengeScreen();
                break;
            case LOBBY_STATE_GAME_RESULT:
                renderGameResultScreen();
                break;
            case LOBBY_STATE_VIEW_HISTORY:
                renderHistoryScreen();
                break;
            case LOBBY_STATE_START_GAME:
                running = 0;
                break;
            default:
                break;
        }
        
        // Present
        SDL_RenderPresent(gRenderer);
    }
    
    return state;
}
