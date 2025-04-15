#pragma once

#include <string>
#include "game.h"
#include "vpoker.h"

class C_left {
 public:
  // These variables describe what is
  // left in the deck to be drawn.

  int denoms[num_denoms];
  int suits[num_suits];
  bool cards[deck_size];

  int low_suits[num_suits];
  // The number of low cards left of each suit

  int jokers;

  C_left(game_parameters &parms);

  void remove(const card *hand, int hand_size, int jokers_in_hand);
  // Make fewer cards left to be drawn.

  void replace(const card *hand, int hand_size, int jokers_in_hand);
  // Undoes the changes made by remove

  bool available(int denom, int suit) { return cards[make_card(denom, suit)]; }

 private:
  game_parameters &parms;
};

typedef int pay_dist[last_pay + 1];

typedef class C_kept_description {
 private:
  int num_jokers;

  int multi[num_suits + 1];
  // multi[1] = the number of singletons
  // multi[2] = the number of pairs
  // multi[3] = the number of trips
  // multi[4] = the number of quads

  // multi[1] + 2*multi[2] + 3*multi[3] + 4*multi[4] + num_jokers == 5

  int m_denom[num_suits + 1];
  // If multi[n] != 0, m_denom[n] is the denomination
  // of one of the multies.

  int other_singleton;
  // if multi[1] == 2, this is the denomination
  // of the other singleton

  int other_pair;
  // if multi[2] == 2, this is the
  // denomination of the other pair.

  bool have[num_denoms];
  // For each denomination, whether at least one was kept.

  unsigned char have_suit[num_denoms];
  // A bit vector of suits for each denomination

  int high_denoms;
  // The number of distinct high denominations (i.e AAKK counts as 2)

  int reach;
  // The width of the smallest window that holds all the denomination.
  // A straight is possible only if reach <= 5
  // If nothing is held, reach == 0

  int min_denom;
  // Only meaningful if reach != 0.
  // min_denom + reach - 1 == max_denom
  // For this purpose, and ace is treated as high or low as
  // necessary to make a straight possible

  bool have_ace;
  int min_non_ace;
  int max_non_ace;
  // Smallest and largest card that is not an ace,
  // max_denom+1 if the hand consists of only an ace

  bool suited;
  // Whether all cards of the hand are the same suit.

  int the_suit;
  // If so, what the suit is.

  int num_discards;
  card discards[5];
  // The first num_discard slots contains the discards

  bool has_singleton;
  // Whether there one suit containing a single card

  card singleton;
  // if so, the card

  game_parameters &parms;

 public:
  C_kept_description(const card *hand, int hand_size, unsigned mask,
                     game_parameters &parms);
  // Builds the descriptor.
  //
  // The low order bits of mask determine whether a card is kept.
  // The 2**j bit corresponds to hand[j]

  const char *display();

  void all_draws(int deuces_kept, C_left &left, pay_dist &pays);

  // Count up all the possible hands that can be made, and
  // return the counts in pay_dist.

  // The parameter left describes the deck from which we may draw
  // cards to make the different hands.

  // The discards consist of the cards that are not described
  // by kept, deuces_kept, or left.

  payoff_name name();

  #if 0
  bool is_draw(int code, int wild_cards);
  bool matches_pattern(unsigned char *pattern, int wild_cards);
  #endif

  int number_of_discards() { return num_discards; }

  std::string move_name();

} kept_description;
