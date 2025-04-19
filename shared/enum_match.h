#pragma once
#include "game.h"
#include "vpoker.h"

enum parser_codes {
  pc_eof,
  pc_prefer,

  pc_or,
  pc_else,

  pc_no_x,
  pc_with_x,

  pc_nothing,
  pc_two_pair,
  pc_trips,
  pc_full_house,
  pc_quads,
  pc_quads_with_low_kicker,
  pc_pair_of_x,
  pc_just_a_x,
  pc_trip_x,
  pc_trip_x_with_low_kicker,
  pc_RF_n,
  pc_SF_n,
  pc_Straight_n,
  pc_Flush_n,
  pc_these_n,
  pc_suited_x,

  pc_high_n,

  pc_high_x,
  pc_low_x,

  pc_no_fp,
  pc_dsc_fp,

  pc_no_sp,
  pc_dsc_sp,

  pc_no_gp,
  pc_dsc_gp,

  pc_no_pp,
  pc_dsc_pp,

  pc_no_pair,
  pc_dsc_pair,

  pc_no_le_x,
  pc_dsc_le_x,

  pc_no_ge_x,
  pc_dsc_ge_x,

  pc_no_these_n,
  pc_dsc_these_n,

  pc_no_these_n_and_fp,
  pc_dsc_these_n_and_fp,

  pc_no_these_suited_n,
  pc_dsc_these_suited_n,

  pc_no_these_offsuit_n,
  pc_dsc_these_offsuit_n,

  pc_no_these_onsuit_n,
  pc_dsc_these_onsuit_n,

  pc_discard_suit_count_n,

  pc_least_sp
};

// The output produced by parse_line.
struct StrategyLine {
  StrategyLine() : pattern(0), options(0), image(0) {};

  // The encoded meaning of the parsed input.
  // A pointer to a series of parser codes and small integers.
  // The client is responsible for deleting it.
  unsigned char *pattern;

  // options is the contents of the line after the first #
  // The parser does not know the syntax of the options;
  // it just copies over the characters.
  // The client is responsible for deleting it.
  char *options;

  // The original parsed characters, with the options stripped off.
  // The client is reponsible for deleting it.
  char *image;
};

class enum_match {
 public:
  enum_match() {}

  // Input parameters
  card hand[5];
  int hand_size;
  int wild_cards;

  game_parameters *parms;  // Needed to tell if a card is "high".

  // Output parameters set by find. result_vector and matches
  // are two different ways of representing the same data.

  // One bit for each if the possible 32 combinations of five cards.
  bool result_vector[32];

  // The index in result_vector of each set bit.
  unsigned char matches[32];

  // The number of bits set in result vector, and the number of masks
  // stored in matches.
  int match_count;

  // pattern is an encoded meaning of a strategy line.
  // It might mean, for example, "low pair".
  // The input hand is examined for every combination that matches the pattern.
  // For example, if the pattern is "low pair" and hand is 22337, the
  // matches are the bit masks 11000 and 00110.
  void find(unsigned char *pattern);

 private:
  unsigned char *pat, *pat_eof;
  bool ace_is_low;

  void check(unsigned mask);
  bool matches_tail(unsigned mask, unsigned char *pattern);
};
