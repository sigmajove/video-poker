#pragma once

#include "vpoker.h"

typedef class C_hand_iter {
  bool is_done;
  int hand_size;
  unsigned char missing_denom;
  unsigned char short_denom;

  struct stack_element {
    unsigned char denom;
    unsigned char count;
    unsigned char suit_classes;
    int suit_1;
    int suit_2;
  } stack[6];

  struct stack_element *top;
  bool next_suits();
  void start_state();

public:
  C_hand_iter(int size, game_kind kind, int wild_cards);
  void next();
  bool done() {
    return is_done;
  }
  void current (card &hand);
  unsigned multiplier();
  int size() {
    return hand_size;
  }
  C_hand_iter& operator=(const C_hand_iter& val);
} hand_iter;

