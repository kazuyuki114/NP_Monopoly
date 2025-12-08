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

#include "client_network.h"
#include "lobby.h"

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
        printf("\n");
        printf("=================================\n");
        printf("  GAME STARTING!\n");
        printf("  Player: %s (ELO: %d)\n", client.username, client.elo_rating);
        printf("=================================\n");
        printf("\n");
        printf("NOTE: Full game integration coming soon!\n");
        printf("      For now, returning to lobby is not implemented.\n");
        printf("\n");
        
        // TODO: Here you would integrate with the actual game
        // Game_init_network(&client);
        // Render_init();
        // Render_processEventsAndRender();
        // Render_close();
    }
    
    // Disconnect if still connected
    if (client_is_connected(&client)) {
        client_disconnect(&client);
    }
    
    printf("Goodbye!\n");
    return 0;
}

