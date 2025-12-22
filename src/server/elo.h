#ifndef ELO_H
#define ELO_H

// ELO Rating System Configuration
#define ELO_DEFAULT_RATING 1200
#define ELO_K_FACTOR_NEW 40       // Higher K for new players (< 30 games)
#define ELO_K_FACTOR_NORMAL 32    // Standard K factor
#define ELO_K_FACTOR_MASTER 16    // Lower K for high-rated players (> 2000)
#define ELO_MIN_RATING 100        // Minimum ELO floor
#define ELO_MATCHMAKING_RANGE 150 // Initial ELO range for matchmaking

// ELO change result
typedef struct {
    int winner_old_elo;
    int winner_new_elo;
    int winner_change;
    int loser_old_elo;
    int loser_new_elo;
    int loser_change;
} EloResult;

// Calculate ELO changes for a match result
// winner_elo: Current ELO of winner
// loser_elo: Current ELO of loser
// winner_games: Total games played by winner (for K-factor calculation)
// loser_games: Total games played by loser
// result: Output struct with all ELO changes
void elo_calculate_match(int winner_elo, int loser_elo, 
                         int winner_games, int loser_games,
                         EloResult* result);

// Calculate ELO changes for a draw (if implemented)
// Returns the ELO change for player1 (player2 gets negative of this)
int elo_calculate_draw(int player1_elo, int player2_elo);

// Get appropriate K-factor based on rating and games played
int elo_get_k_factor(int elo, int games_played);

// Calculate expected score (win probability) for player with elo1 against player with elo2
// Returns a value between 0.0 and 1.0
double elo_expected_score(int elo1, int elo2);

// Check if two players are good match based on ELO difference
// Returns 1 if they should be matched, 0 otherwise
int elo_is_good_match(int elo1, int elo2, int search_time_seconds);

// Get matchmaking range based on how long a player has been searching
// Range expands over time to ensure matches happen
int elo_get_matchmaking_range(int search_time_seconds);

#endif // ELO_H

