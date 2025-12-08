/*
 * Monopoly Network Client - Test Application
 * 
 * This is a simple command-line test client to demonstrate
 * the login/register functionality.
 * 
 * Usage: ./monopoly_client [server_ip] [port]
 *   Default: 127.0.0.1:8888
 */

#include "client_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 256

static void print_menu(ClientState* state) {
    printf("\n");
    printf("================================\n");
    printf("  MONOPOLY NETWORK CLIENT\n");
    printf("================================\n");
    
    if (!state->connected) {
        printf("Status: Disconnected\n");
    } else if (!state->logged_in) {
        printf("Status: Connected (not logged in)\n");
    } else {
        printf("Status: Logged in as %s\n", state->username);
        printf("ELO: %d | W/L: %d/%d\n", state->elo_rating, state->wins, state->losses);
    }
    
    printf("--------------------------------\n");
    
    if (!state->connected) {
        printf("1. Connect to server\n");
    } else if (!state->logged_in) {
        printf("1. Register new account\n");
        printf("2. Login\n");
        printf("3. Disconnect\n");
    } else {
        printf("1. View profile\n");
        printf("2. Logout\n");
        printf("3. Disconnect\n");
    }
    
    printf("0. Exit\n");
    printf("--------------------------------\n");
    printf("Choice: ");
}

static void get_input(const char* prompt, char* buffer, int max_len) {
    printf("%s", prompt);
    fflush(stdout);
    
    if (fgets(buffer, max_len, stdin)) {
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
    }
}

static void do_connect(ClientState* state) {
    char ip[64];
    char port_str[16];
    
    get_input("Server IP [127.0.0.1]: ", ip, sizeof(ip));
    if (strlen(ip) == 0) strcpy(ip, "127.0.0.1");
    
    get_input("Port [8888]: ", port_str, sizeof(port_str));
    int port = strlen(port_str) > 0 ? atoi(port_str) : 8888;
    
    if (client_connect(state, ip, port) == 0) {
        printf("\n✓ Connected to %s:%d\n", ip, port);
    } else {
        printf("\n✗ Failed to connect!\n");
    }
}

static void do_register(ClientState* state) {
    char username[50];
    char password[50];
    char email[100];
    
    printf("\n--- REGISTER NEW ACCOUNT ---\n");
    get_input("Username (3-20 chars): ", username, sizeof(username));
    get_input("Password (min 4 chars): ", password, sizeof(password));
    get_input("Email (optional): ", email, sizeof(email));
    
    const char* email_ptr = strlen(email) > 0 ? email : NULL;
    
    if (client_register(state, username, password, email_ptr) == 0) {
        printf("\n✓ Registration successful! You can now login.\n");
    } else {
        printf("\n✗ Registration failed!\n");
    }
}

static void do_login(ClientState* state) {
    char username[50];
    char password[50];
    
    printf("\n--- LOGIN ---\n");
    get_input("Username: ", username, sizeof(username));
    get_input("Password: ", password, sizeof(password));
    
    if (client_login(state, username, password) == 0) {
        printf("\n✓ Login successful! Welcome, %s!\n", state->username);
    } else {
        printf("\n✗ Login failed!\n");
    }
}

static void do_view_profile(ClientState* state) {
    printf("\n=== PLAYER PROFILE ===\n");
    printf("Username: %s\n", state->username);
    printf("User ID: %d\n", state->user_id);
    printf("ELO Rating: %d\n", state->elo_rating);
    printf("Total Matches: %d\n", state->total_matches);
    printf("Wins: %d\n", state->wins);
    printf("Losses: %d\n", state->losses);
    if (state->total_matches > 0) {
        float winrate = (float)state->wins / state->total_matches * 100;
        printf("Win Rate: %.1f%%\n", winrate);
    }
    printf("======================\n");
}

int main(int argc, char* argv[]) {
    ClientState state;
    client_init(&state);
    
    char input[MAX_INPUT];
    int running = 1;
    
    printf("\n");
    printf("╔═══════════════════════════════════════╗\n");
    printf("║     MONOPOLY NETWORK CLIENT           ║\n");
    printf("║     Test Login System                 ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    
    // Auto-connect if arguments provided
    if (argc >= 2) {
        const char* ip = argv[1];
        int port = argc >= 3 ? atoi(argv[2]) : 8888;
        
        printf("Connecting to %s:%d...\n", ip, port);
        if (client_connect(&state, ip, port) == 0) {
            printf("✓ Connected!\n");
        }
    }
    
    while (running) {
        print_menu(&state);
        get_input("", input, sizeof(input));
        
        int choice = atoi(input);
        
        if (!state.connected) {
            // Not connected
            switch (choice) {
                case 0: running = 0; break;
                case 1: do_connect(&state); break;
                default: printf("Invalid choice\n"); break;
            }
        } else if (!state.logged_in) {
            // Connected but not logged in
            switch (choice) {
                case 0: running = 0; break;
                case 1: do_register(&state); break;
                case 2: do_login(&state); break;
                case 3: client_disconnect(&state); break;
                default: printf("Invalid choice\n"); break;
            }
        } else {
            // Logged in
            switch (choice) {
                case 0: running = 0; break;
                case 1: do_view_profile(&state); break;
                case 2: client_logout(&state); break;
                case 3: client_disconnect(&state); break;
                default: printf("Invalid choice\n"); break;
            }
        }
    }
    
    // Cleanup
    if (state.connected) {
        client_disconnect(&state);
    }
    
    printf("\nGoodbye!\n");
    return 0;
}

