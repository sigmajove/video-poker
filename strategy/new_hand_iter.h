#pragma once

#include "vpoker.h"

class hand_iter {
 public:
  hand_iter(int size, game_kind kind, int wild_cards);
  hand_iter& operator=(const hand_iter& val);

  void next();
  bool done() const { return is_done; }
  void current(card& hand) const;
  unsigned multiplier() const;
  int size() const { return hand_size; }

 private:
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

  struct stack_element* top;
  bool next_suits();
  void start_state();
};
