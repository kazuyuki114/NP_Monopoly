/*
 * ELO Rating System Implementation
 * 
 * Based on the chess ELO system with modifications for Monopoly:
 * - Uses variable K-factor based on player experience and rating
 * - Implements dynamic matchmaking range that expands over time
 * - Ensures minimum rating floor to prevent negative ratings
 */

#include "elo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Get appropriate K-factor based on rating and games played
int elo_get_k_factor(int elo, int games_played) {
    // New players (< 30 games) get higher K for faster adjustment
    if (games_played < 30) {
        return ELO_K_FACTOR_NEW;
    }
    
    // Master level players (> 2000 ELO) get lower K for stability
    if (elo > 2000) {
        return ELO_K_FACTOR_MASTER;
    }
    
    // Standard K factor for most players
    return ELO_K_FACTOR_NORMAL;
}

// Calculate expected score (win probability)
// E = 1 / (1 + 10^((opponent_elo - player_elo) / 400))
double elo_expected_score(int elo1, int elo2) {
    double exponent = (double)(elo2 - elo1) / 400.0;
    return 1.0 / (1.0 + pow(10.0, exponent));
}

// Calculate ELO changes for a match result
void elo_calculate_match(int winner_elo, int loser_elo,
                         int winner_games, int loser_games,
                         EloResult* result) {
    if (!result) return;
    
    // Store old ELOs
    result->winner_old_elo = winner_elo;
    result->loser_old_elo = loser_elo;
    
    // Get K-factors for each player
    int winner_k = elo_get_k_factor(winner_elo, winner_games);
    int loser_k = elo_get_k_factor(loser_elo, loser_games);
    
    // Calculate expected scores
    double winner_expected = elo_expected_score(winner_elo, loser_elo);
    double loser_expected = elo_expected_score(loser_elo, winner_elo);
    
    // Actual scores: 1 for win, 0 for loss
    double winner_actual = 1.0;
    double loser_actual = 0.0;
    
    // Calculate ELO changes: new_elo = old_elo + K * (actual - expected)
    result->winner_change = (int)round(winner_k * (winner_actual - winner_expected));
    result->loser_change = (int)round(loser_k * (loser_actual - loser_expected));
    
    // Ensure minimum change of 1 point for winner (avoid 0 change scenarios)
    if (result->winner_change < 1) {
        result->winner_change = 1;
    }
    // Ensure loser loses at least 1 point
    if (result->loser_change > -1) {
        result->loser_change = -1;
    }
    
    // Calculate new ELOs
    result->winner_new_elo = winner_elo + result->winner_change;
    result->loser_new_elo = loser_elo + result->loser_change;
    
    // Apply minimum floor
    if (result->loser_new_elo < ELO_MIN_RATING) {
        result->loser_new_elo = ELO_MIN_RATING;
        result->loser_change = ELO_MIN_RATING - loser_elo;
    }
    
    printf("[ELO] Match result: Winner %d -> %d (%+d), Loser %d -> %d (%+d)\n",
           winner_elo, result->winner_new_elo, result->winner_change,
           loser_elo, result->loser_new_elo, result->loser_change);
}

// Calculate ELO changes for a draw
int elo_calculate_draw(int player1_elo, int player2_elo) {
    // Expected score for player 1
    double expected = elo_expected_score(player1_elo, player2_elo);
    
    // Actual score for draw is 0.5
    double actual = 0.5;
    
    // Use standard K factor for draws
    int change = (int)round(ELO_K_FACTOR_NORMAL * (actual - expected));
    
    return change;
}

// Get matchmaking range based on search time
// Starts at ELO_MATCHMAKING_RANGE and expands by 25 every 10 seconds
int elo_get_matchmaking_range(int search_time_seconds) {
    int base_range = ELO_MATCHMAKING_RANGE;
    
    // Expand range by 25 ELO every 10 seconds of waiting
    int expansions = search_time_seconds / 10;
    int expansion = expansions * 25;
    
    // Cap at 500 ELO difference max
    int total_range = base_range + expansion;
    if (total_range > 500) {
        total_range = 500;
    }
    
    return total_range;
}

// Check if two players are good match based on ELO difference
int elo_is_good_match(int elo1, int elo2, int search_time_seconds) {
    int elo_diff = abs(elo1 - elo2);
    int allowed_range = elo_get_matchmaking_range(search_time_seconds);
    
    return elo_diff <= allowed_range;
}

