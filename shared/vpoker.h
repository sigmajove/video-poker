#pragma once

#include <map>

typedef unsigned char card;
const int num_denoms = 13;
const int num_suits = 4;
const int deck_size = num_denoms * num_suits;
const int joker = deck_size;

inline int pips(card c) { return c >> 2; }

inline int suit(card c) { return c & 3; }

inline card make_card(int denom, int suit) {
  return (card)((denom << 2) | suit);
}

typedef enum {
  ace,
  deuce,
  three,
  four,
  five,
  six,
  seven,
  eight,
  nine,
  ten,
  jack,
  queen,
  king
} denom_value;

extern const char denom_image[];
extern const char suit_image[];

inline bool is_black(int suit) { return (suit & 1) == 0; }

extern void print_hand(const card *hand, int size);
extern void print_hand(FILE *file, const card *hand, int size);
extern void output_hand(FILE *file, const card *hand, int size);
extern void print_move(FILE *file, const card *hand, int hand_size,
                       unsigned mask);

// Game description

enum payoff_name {
  N_nothing,
  N_high_pair,
  N_two_pair,
  N_trips,
  N_straight,
  N_flush,
  N_full_house,
  N_quads,
  N_quad_aces,
  N_quad_aces_kicker,
  N_quad_low,
  N_quad_low_kicker,
  N_straight_flush,
  N_quints,
  N_wild_royal,
  N_four_deuces,
  N_royal_flush
};

extern const char *payoff_image[];
extern const char *short_payoff_image[];

const payoff_name first_pay = N_nothing;
const payoff_name last_pay = N_royal_flush;

enum game_kind {
  GK_no_wild,
  GK_bonus,
  GK_bonus_with_kicker,
  GK_deuces_wild,
  GK_joker_wild,
  GK_one_eyed_jacks_wild
};

struct vp_game {
  const char *name;
  game_kind kind;
  denom_value min_high_pair;
  const int (*pay_table)[last_pay + 1];

  static vp_game *find(char *named);

  vp_game(const char *n, game_kind k, denom_value mhp,
          const int (*pt)[last_pay + 1]);
};

typedef double pay_prob[last_pay + 1];

namespace games {
extern const vp_game jacks_or_better, kb_joker, all_american, eight_five_bonus,
    double_bonus, double_double_bonus, deuces_wild, nsu_deuces_wild,
    loose_deuces_wild, loose_deuces_wild2;
}
