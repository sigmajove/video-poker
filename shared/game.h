#pragma once

#include "vpoker.h"

typedef double pay_values[last_pay + 1];

struct game_parameters {
  game_kind kind;
  denom_value min_high_pair;
  pay_values pay_table;
  int deck_size;          // Including all jokers
  int number_wild_cards;  // deuces or jokers
  bool bonus_quads;
  bool bonus_quads_kicker;

  bool is_high(int d) { return d == ace || d >= min_high_pair; };
  bool is_wild(card c);
  game_parameters(const vp_game &g);
};
