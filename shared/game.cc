#include "game.h"

#include "vpoker.h"

game_parameters::game_parameters(const vp_game &g)
    : kind(g.kind), min_high_pair(g.min_high_pair), deck_size(52) {
  for (int j = first_pay; j <= last_pay; j++) {
    pay_table[j] = static_cast<double>((*g.pay_table)[j]);
  }

  bonus_quads = (g.kind == GK_bonus || g.kind == GK_bonus_with_kicker);
  bonus_quads_kicker = g.kind == GK_bonus_with_kicker;

  switch (g.kind) {
    case GK_no_wild:
    case GK_bonus:
    case GK_bonus_with_kicker:
      number_wild_cards = 0;
      break;

    case GK_deuces_wild:
      number_wild_cards = num_suits;
      break;

    case GK_joker_wild:
      number_wild_cards = 1;
      deck_size += 1;
      break;

    case GK_one_eyed_jacks_wild:
      number_wild_cards = 2;
      break;

    default:
      _RPT0(_CRT_ERROR, "Undefined game kind");
  }
}

bool game_parameters::is_wild(card c) {
  switch (kind) {
    case GK_deuces_wild:
      return pips(c) == deuce;

    case GK_one_eyed_jacks_wild:
      return pips(c) == jack && suit(c) <= 1;

    case GK_joker_wild:
      return c == joker;

    default:
      return false;
  }
}
