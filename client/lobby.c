/*
 * Monopoly Network Lobby - SDL GUI
 * 
 * A graphical login/lobby interface matching the game window size (1000x800)
 */

#include "lobby.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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
static Button btnLogout;
static Button btnExit;

static char statusMessage[256];
static int statusIsError = 0;

// Forward declarations
static void renderText(const char* text, int x, int y, TTF_Font* font, SDL_Color color);
static void renderTextCentered(const char* text, int centerX, int y, TTF_Font* font, SDL_Color color);
static void drawPanel(int x, int y, int w, int h);
static void drawInputField(InputField* field, const char* label, int labelY);
static void drawButton(Button* btn);
static void clearInputField(InputField* field);
static void initButton(Button* btn, const char* label, int x, int y, int w, int h);
static int isMouseOver(SDL_Rect* rect, int mx, int my);

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
    memset(&btnLogout, 0, sizeof(Button));
    memset(&btnExit, 0, sizeof(Button));
    memset(statusMessage, 0, sizeof(statusMessage));
    statusIsError = 0;

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
        // Draw thicker border when active
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

// ============ SCREEN RENDERERS ============

static void renderConnectScreen(void) {
    renderTitle();
    
    // Panel
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 200;
    drawPanel(panelX, panelY, 360, 320);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Connect to Server", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    // Input fields
    inputServerIP.rect = (SDL_Rect){panelX + 30, panelY + 100, 300, 44};
    inputServerPort.rect = (SDL_Rect){panelX + 30, panelY + 180, 300, 44};
    
    drawInputField(&inputServerIP, "Server IP Address", panelY + 75);
    drawInputField(&inputServerPort, "Port", panelY + 155);
    
    // Buttons
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
    
    // Panel
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 200;
    drawPanel(panelX, panelY, 360, 380);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Login", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    // Input fields
    inputUsername.rect = (SDL_Rect){panelX + 30, panelY + 100, 300, 44};
    inputPassword.rect = (SDL_Rect){panelX + 30, panelY + 185, 300, 44};
    
    drawInputField(&inputUsername, "Username", panelY + 75);
    drawInputField(&inputPassword, "Password", panelY + 160);
    
    // Buttons
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
    
    // Panel
    int panelX = SCREEN_WIDTH / 2 - 180;
    int panelY = 180;
    drawPanel(panelX, panelY, 360, 450);
    
    SDL_Color white = {255, 255, 255, 255};
    renderTextCentered("Create Account", SCREEN_WIDTH / 2, panelY + 25, gFontMedium, white);
    
    // Input fields
    inputUsername.rect = (SDL_Rect){panelX + 30, panelY + 95, 300, 44};
    inputPassword.rect = (SDL_Rect){panelX + 30, panelY + 180, 300, 44};
    inputEmail.rect = (SDL_Rect){panelX + 30, panelY + 265, 300, 44};
    
    drawInputField(&inputUsername, "Username (3-20 characters)", panelY + 70);
    drawInputField(&inputPassword, "Password (min 4 characters)", panelY + 155);
    drawInputField(&inputEmail, "Email (optional)", panelY + 240);
    
    // Buttons
    initButton(&btnRegister, "Create Account", panelX + 30, panelY + 340, 300, 45);
    initButton(&btnBack, "Back to Login", panelX + 80, panelY + 395, 200, 40);
    
    drawButton(&btnRegister);
    drawButton(&btnBack);
    
    renderStatus();
}

static void renderMainMenu(ClientState* client) {
    renderTitle();
    
    // Profile Panel (left)
    int profileX = 80;
    int profileY = 200;
    drawPanel(profileX, profileY, 340, 280);
    
    SDL_Color gold = {COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 255};
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color gray = {170, 170, 170, 255};
    SDL_Color green = {100, 255, 100, 255};
    SDL_Color red = {255, 100, 100, 255};
    
    renderText("PLAYER PROFILE", profileX + 20, profileY + 20, gFontMedium, gold);
    
    char buf[128];
    
    snprintf(buf, sizeof(buf), "Name: %s", client->username);
    renderText(buf, profileX + 25, profileY + 70, gFontSmall, white);
    
    snprintf(buf, sizeof(buf), "ELO Rating: %d", client->elo_rating);
    renderText(buf, profileX + 25, profileY + 100, gFontSmall, gold);
    
    snprintf(buf, sizeof(buf), "Total Games: %d", client->total_matches);
    renderText(buf, profileX + 25, profileY + 140, gFontSmall, gray);
    
    snprintf(buf, sizeof(buf), "Wins: %d", client->wins);
    renderText(buf, profileX + 25, profileY + 170, gFontSmall, green);
    
    snprintf(buf, sizeof(buf), "Losses: %d", client->losses);
    renderText(buf, profileX + 25, profileY + 200, gFontSmall, red);
    
    if (client->total_matches > 0) {
        float wr = (float)client->wins / client->total_matches * 100.0f;
        snprintf(buf, sizeof(buf), "Win Rate: %.1f%%", wr);
        renderText(buf, profileX + 25, profileY + 235, gFontSmall, gray);
    }
    
    // Action Panel (right)
    int actionX = 580;
    int actionY = 200;
    drawPanel(actionX, actionY, 340, 280);
    
    renderText("PLAY GAME", actionX + 20, actionY + 20, gFontMedium, gold);
    
    renderText("Click 'Find Match' to play", actionX + 25, actionY + 70, gFontSmall, gray);
    renderText("against another player.", actionX + 25, actionY + 95, gFontSmall, gray);
    
    initButton(&btnFindMatch, "Find Match", actionX + 70, actionY + 150, 200, 50);
    initButton(&btnLogout, "Logout", actionX + 70, actionY + 215, 200, 40);
    
    drawButton(&btnFindMatch);
    drawButton(&btnLogout);
    
    renderStatus();
    
    SDL_Color hint = {100, 100, 100, 255};
    renderTextCentered("Press ESC to logout", SCREEN_WIDTH / 2, 750, gFontSmall, hint);
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
        btnLogout.hovered = isMouseOver(&btnLogout.rect, mx, my);
        
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
                    if (key == SDLK_RETURN) {
                        state = LOBBY_STATE_START_GAME;
                        running = 0;
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
                        state = LOBBY_STATE_START_GAME;
                        running = 0;
                    }
                    if (isMouseOver(&btnLogout.rect, cx, cy)) {
                        client_logout(client);
                        state = LOBBY_STATE_LOGIN;
                        clearInputField(&inputUsername);
                        clearInputField(&inputPassword);
                        inputUsername.active = 1;
                        setStatus("Logged out", 0);
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
            default:
                break;
        }
        
        // Present
        SDL_RenderPresent(gRenderer);
    }
    
    return state;
}
