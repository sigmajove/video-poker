#include <vector>

#include "enum_match.h"
#include "game.h"

extern game_parameters *selected_game;

typedef std::vector <strategy_line> line_list;
typedef std::vector <line_list> strategy;
extern strategy selected_strategy;

void initialize_deck();
// Should be called every time selected_game changes.

void deal_hand (card *hand, const char* &name);
// Deal out a random hand, and return its name

void erase_hands();

void got_it_wrong();
// Add the last hand given out onto the repeat list

bool is_right_move (card *hand, unsigned mask);
// Determine whether the user made a right move
// for this hand.

void find_right_move (card *hand, unsigned &mask, const char* &name);
// Find the move selected by the strategy
