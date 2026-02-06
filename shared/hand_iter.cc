#include "hand_iter.h"

// The iterator returns all possible N-card poker hands that
// contain no wild cards, ignoring hands that are isomporphic
// with respect to suits.  Each successive value returns one
// member of an equivalence class, and the member multiplier()
// returns the size of that class.

// The algorithm operates using a stack, where each cell of the
// stack represents "count" cards whose value is "denom".  The
// values suit_1 and suit_2 represent the suits of the cards.

// The meaning of suit_1 and suit_2 are dependent on the value
// of count.

//  count==1  suit_1 is the suit of the card.
//  count==2  suit_1 and suit_2 are the suits of the cards
//  count==3  suit_1 is the suit omitted from the triple
//  count==4  all four suits are present

// The really tricky part is getting the suits right.  This trick
// is to maintain in each stack cell an equivalence class on the
// suits.  Initially (when the stack is empty) all four suits are
// equivalent for regulare games and there are two classes of size
// two in one-eyed-jacks game (the suits with jacks and those without).

// When we push a count==1 or count==3 cell onto the stack, we need
// to iterate the possible suits by choosing one member of each equivance
// class.  We choose the smallest numerical value in each class as the
// representative.

// In addition, each class selected must be refined if the class is
// size two or greater.  Basically, assigning a suit to a cell breaks
// the existing symmetry.  The class must be divided to the suit that
// was chosen and all the rest.

// The equivalence classes are represented by bit vector of length 5,
// There is one bit for each suit, and a sentinal bit at the end that
// is always one.  The representatives have the 1 bit set, and all the
// other suits have the bit zero.  We find all the classes by looking
// for 1 bits.  Refining the class for suit N is done by simply setting
// bit N+1 to 1.  If the set is a singleton, this operation is a no-op.

// Pushing a cell with count==2 is a little trickier, because we must
// choose two suits, and those suits can come from the same class, or
// different classes.  If we choose two suits from the same class, we
// refine the class to the suits we chose and the suits we didn't.  If
// the class is size two, no refinement is necessary.  If we choose the
// suits from two different classes, we must refine each of the chosen
// classes, except for ones of size 1.  This sounds pretty complicated,
// but using the bit-vector representation, it pretty much falls out.
//
// Pushing a cell with count==4 has no effect on the classes.
//
// When we push the cell that completes the hand to be output,
// the final value of the equivalence classes determine the multiplier.
// The algorithm is quite different for one-eyed-jacks games and
// regular ones.

hand_iter &hand_iter::operator=(const hand_iter &val) {
  hand_size = val.hand_size;
  is_done = val.is_done;
  missing_denom = val.missing_denom;
  short_denom = val.short_denom;

  for (int j = 0; j < 6; j++) {
    stack[j].count = val.stack[j].count;
    stack[j].denom = val.stack[j].denom;
    stack[j].suit_classes = val.stack[j].suit_classes;
    stack[j].suit_1 = val.stack[j].suit_classes;
    stack[j].suit_2 = val.stack[j].suit_classes;
  }

  top = &(stack[val.top - val.stack]);

  return *this;
}

void hand_iter::current(card &hand) const {
  int x = 0;
  card *h = &hand;

  for (const stack_element *p = stack + 1; p <= top; p++) {
    const int d = p->denom;
    switch (p->count) {
      case 1:
        *h++ = make_card(d, p->suit_1);
        break;

      case 2:
        *h++ = make_card(d, p->suit_1);
        *h++ = make_card(d, p->suit_2);
        break;

      case 3: {
        const int omit = p->suit_1;

        for (int j = 0; j < 4; j++) {
          if (j != omit) {
            *h++ = make_card(d, j);
          }
        }
      } break;

      case 4:
        *h++ = make_card(d, 0);
        *h++ = make_card(d, 1);
        *h++ = make_card(d, 2);
        *h++ = make_card(d, 3);
        break;

      default:
        _ASSERT(0);
    }
  }
}

unsigned hand_iter::multiplier() const {
  unsigned char start = stack[0].suit_classes;
  unsigned char c = top->suit_classes;

  if ((start & (1 << 2)) != 0) {
    // This is a one-eyed jacks game where the suits
    // started out differentiated.
    int result = 1;

    // Test if the nonwild suits had their symmetry broken
    if ((c & (1 << 3)) != 0) {
      result = 2;
    }

    // Test if the wild suits had their symmetry broken,
    // but were not broken initially
    if ((start & (1 << 1)) == 0 && (c & (1 << 1)) != 0) {
      result *= 2;
    }

    return result;
  }

  int class_size = 1;

  int sizes[5];
  for (int k = 0; k < 5; k++) {
    sizes[k] = 0;
  }

  _ASSERT((1 << 0 & c) != 0);
  _ASSERT((1 << 4 & c) != 0);

  for (int j = 1; j < 5; j++) {
    if ((1 << j & c) != 0) {
      sizes[class_size] += 1;
      class_size = 1;
    } else {
      class_size += 1;
    }
  }

  int sum = 0;
  for (int m = 0; m < 5; m++) {
    sum += m * sizes[m];
  }
  _ASSERT(sum == 4);

  // Enumerate the different ways for integers int
  // the range 1..4 can add up to 4.

  if (sizes[4] == 1) {
    return 1;  // 4
  } else if (sizes[3] == 1) {
    return 4;  // 3+1
  } else if (sizes[2] == 2) {
    return 6;  // 2+2
  } else if (sizes[2] == 1) {
    return 12;  // 2+1+1
  } else if (sizes[1] == 4) {
    return 24;  // 1+1+1+1
  }

  _ASSERT(0);
  return 0;
}

void hand_iter::start_state() {
  top->suit_1 = top->denom == short_denom ? 1 : -1;
  top->suit_2 = 3;
  const bool b = next_suits();
  _ASSERT(b);
}

bool hand_iter::next_suits() {
  switch (top->count) {
    case 2:
      do {
        top->suit_2 += 1;
      } while (((1 << top->suit_2) & top[-1].suit_classes) == 0);

      if (top->suit_2 < 4) {
        top->suit_classes = top[-1].suit_classes | (1 << (top->suit_1 + 1)) |
                            (1 << (top->suit_2 + 1));

        return true;
      }

      do {
        top->suit_1 += 1;
      } while (((1 << top->suit_1) & top[-1].suit_classes) == 0);

      if (top->suit_1 >= 3) {
        return false;
      }

      top->suit_2 = top->suit_1 + 1;
      top->suit_classes = top[-1].suit_classes | 1 << (top->suit_2 + 1);
      return true;

    case 1:
    case 3:
      do {
        top->suit_1 += 1;
      } while (((1 << top->suit_1) & top[-1].suit_classes) == 0);

      top->suit_classes = top[-1].suit_classes | (1 << (top->suit_1 + 1));
      return top->suit_1 < 4;

    case 4:
      top->suit_classes = top[-1].suit_classes;
      top->suit_1 += 1;
      return top->suit_1 == 0;

    default:
      _ASSERT(0);
      return false;
  }
}

void hand_iter::next() {
  int need_cards = 0;

  for (;;) {
    if (next_suits()) {
      // Work done in next_suits
    } else if (need_cards > 0 &&
               top->count < (top->denom == short_denom ? 2 : 4)) {
      need_cards -= 1;
      top->count += 1;
      this->start_state();
    } else if (top->denom == queen) {
      need_cards += top->count;

      if (need_cards > (queen == short_denom ? 2 : 4)) {
        is_done = true;
        return;
      }

      top->denom = king;
      top->count = need_cards;
      this->start_state();
      need_cards = 0;
    } else if (top->denom < king) {
      need_cards += top->count - 1;
      top->denom += 1;
      if (top->denom == missing_denom) {
        top->denom += 1;
      }
      top->count = 1;
      this->start_state();
    } else {
      if (top == stack + 1) {
        is_done = true;
        return;
      }
      need_cards += top->count;
      top -= 1;
      continue;
    }
    break;
  }

  if (need_cards != 0) {
    for (;;) {
      if (top->denom == queen) {
        top += 1;
        top->denom = king;
        top->count = need_cards;
        this->start_state();
        break;
      } else {
        top[1].denom = top->denom + 1;
        if (top[1].denom == missing_denom) {
          top[1].denom += 1;
        }
        top += 1;
        top->count = 1;
        this->start_state();

        if (--need_cards == 0) {
          break;
        }
      }
    }
  }
}

hand_iter::hand_iter(int size, game_kind kind, int wild_cards)
    : is_done(false),
      hand_size(size),
      missing_denom(kind == GK_deuces_wild ? deuce : 0xff),
      short_denom(kind == GK_one_eyed_jacks_wild ? jack : 0xff) {
  stack[0].suit_classes = (1 << 0) + (1 << 4) +
                          (kind == GK_one_eyed_jacks_wild
                               ? (1 << 2) + (wild_cards == 1 ? (1 << 1) : 0)
                               : 0);

  // Create a dummy bottom entry for the stack
  // that contains the intial value for the
  // suit equivalence classes.

  // This way we can always compute top->suit_classes by
  // adding bits to top[-1].suit_classes.

  // For vanilla games, all four suits begin in the same
  // equivalence class.  For one_eyed_jacks games, the suits
  // with wild jacks are in their own class.  If only one wild
  // card is being held, the wild class is split in two.  The
  // distinction is necessary because a wild royal flush in that
  // suit, completed by that card, will be payed off as a natural
  // royal.

  top = stack + 1;
  int d = ace;

  for (int j = 0;; j++) {
    top->denom = d++;
    if (top->denom == missing_denom) {
      top->denom = d++;
    }
    top->count = 1;
    this->start_state();
    if (j == size - 1) {
      break;
    }
    top += 1;
  }
}
