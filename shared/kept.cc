#include <iostream>
#include <sstream>
#include <string>

#include "combin.h"
#include "kept.h"

static const bool trace = true;

static inline int min_int(int x, int y) { return x < y ? x : y; }

static inline int max_int(int x, int y) { return x > y ? x : y; }

C_left::C_left(game_parameters &parms) : parms(parms) {
  // Initialize the counts based on a 48-card deck that
  // doesn't contain any deuces.  The deuces are considered
  // to be four identical jokers that are tracked separately.

  for (int c = 0; c < deck_size; c++) cards[c] = true;

  int num_low_cards = 0;

  for (int j = 0; j < num_denoms; j++) {
    if (j == deuce && parms.kind == GK_deuces_wild) {
      denoms[j] = 0;
    } else if (j == jack && parms.kind == GK_one_eyed_jacks_wild) {
      denoms[j] = 2;
      num_low_cards += 1;  // Jacks are low
    } else {
      if (!parms.is_high(j)) {
        num_low_cards += 1;
      }

      denoms[j] = num_suits;
    }
  }

  int suit_size = num_denoms - (parms.kind == GK_deuces_wild);

  for (int s = 0; s < num_suits; s++) {
    int d = (parms.kind == GK_one_eyed_jacks_wild && s < 2 ? -1 : 0);

    suits[s] = suit_size + d;
    low_suits[s] = num_low_cards + d;
  }

  switch (parms.kind) {
    case GK_no_wild:
    case GK_bonus:
    case GK_bonus_with_kicker:
      jokers = 0;
      break;

    case GK_deuces_wild:
      // Remove the deuces from the deck.
      // They are considered to be four identical jokers that
      // are tracked separately.
     {
        jokers = num_suits;

        for (int s = 0; s < num_suits; s++) {
          cards[make_card(deuce, s)] = false;
          // Remove the deuces from the cards that are left.
        }
      }
      break;

    case GK_joker_wild:
      // There is only one joker in the deck, and it's the real joker.
      jokers = 1;
      break;

    case GK_one_eyed_jacks_wild:
      // Delete the wild jacks
      {
        jokers = 2;
        for (int s = 0; s < 2; s++) cards[make_card(jack, s)] = false;
      }
      break;

    default:
      _RPT0(_CRT_ERROR, "Undefined game kind");
  }
}

void C_left::remove(const card *hand, int hand_size, int jokers_in_hand) {
  for (int j = 0; j < hand_size; j++) {
    const card c = hand[j];
    const int d = pips(c);
    const int s = suit(c);

    cards[c] = false;
    denoms[d] -= 1;
    suits[s] -= 1;

    if (!parms.is_high(d)) {
      low_suits[s] -= 1;
    }
  }

  jokers -= jokers_in_hand;
}

void C_left::replace(const card *hand, int hand_size, int jokers_in_hand) {
  for (int j = 0; j < hand_size; j++) {
    const card c = hand[j];
    const int d = pips(c);
    const int s = suit(c);

    cards[c] = true;
    denoms[d] += 1;
    suits[s] += 1;

    if (!parms.is_high(d)) {
      low_suits[s] += 1;
    }
  }

  jokers += jokers_in_hand;
}

C_kept_description::C_kept_description(const card *hand, int hand_size,
                                       unsigned mask, game_parameters &parms)
    : parms(parms) {
  other_pair = -1;
  other_singleton = -1;
  suited = true;
  the_suit = -1;
  num_discards = 0;
  num_jokers = 5 - hand_size;
  high_denoms = 0;

  int suit_count[num_suits];
  int one_card[num_suits];

  int j;

  for (j = 0; j <= num_suits; j++) {
    multi[j] = 0;
    m_denom[j] = -1;
  }

  for (j = 0; j < num_suits; j++) {
    suit_count[j] = 0;
  }

  for (j = 0; j < num_denoms; j++) {
    have[j] = false;
    have_suit[j] = 0;
  }

  int previous_d = -1;
  int multi_count = 0;

  int endpoint[3];
  int end_index = -1;
  // The lowest, second lowest, and highest card

  for (j = 0; j < hand_size; j++) {
    const card c = hand[j];

    if (mask & 1) {
      const int s = suit(c);
      const int d = pips(c);

      have[d] = true;
      have_suit[d] |= (1 << s);

      suit_count[s] += 1;
      one_card[s] = c;

      if (the_suit < 0) {
        the_suit = s;
      } else if (s != the_suit) {
        suited = false;
      }

      if (d == previous_d) {
        multi_count += 1;
      } else {
        if (parms.is_high(d)) {
          high_denoms += 1;
        }

        if (end_index < 2) end_index += 1;
        endpoint[end_index] = d;

        if (multi_count != 0) {
          // Process one of the multiples
          multi[multi_count] += 1;
          if (multi_count == 1)
            other_singleton = m_denom[1];
          else if (multi_count == 2)
            other_pair = m_denom[2];
          m_denom[multi_count] = previous_d;
        }

        previous_d = d;
        multi_count = 1;
      }
    } else {
      discards[num_discards++] = c;
    }

    mask >>= 1;
  }

  switch (end_index) {
    case -1:
      // No cards
      min_denom = -1;
      have_ace = false;
      min_non_ace = num_denoms + 1;
      max_non_ace = num_denoms + 1;
      reach = 0;
      break;

    case 0:
      // One card
      min_denom = endpoint[0];
      reach = 1;

      if (min_denom == ace) {
        have_ace = true;
        min_non_ace = num_denoms + 1;
        max_non_ace = num_denoms + 1;
      } else {
        have_ace = false;
        min_non_ace = min_denom;
        max_non_ace = min_denom;
      }
      break;

    case 1:
      // Two cards
      endpoint[2] = endpoint[1];
      // fall through

    case 2:
      // Three or more cards
      min_denom = endpoint[0];
      reach = endpoint[2] - endpoint[0] + 1;

      max_non_ace = endpoint[2];

      if (min_denom == ace) {
        have_ace = true;
        min_non_ace = endpoint[1];

        // Try treating the ace as high
        int r2 = king - endpoint[1] + 2;

        if (r2 < reach) {
          reach = r2;
          min_denom = endpoint[1];
        }
      } else {
        have_ace = false;
        min_non_ace = endpoint[0];
      }
      break;

    default:
      _ASSERT(false);
  }

  if (multi_count != 0) {
    // Process one of the multiples
    // (Duplicates previous code.  Ugh)

    multi[multi_count] += 1;
    if (multi_count == 1)
      other_singleton = m_denom[1];
    else if (multi_count == 2)
      other_pair = m_denom[2];
    m_denom[multi_count] = previous_d;
  }

  if (!suited) {
    the_suit = -1;
  }

  int nn = 0;

  for (j = 0; j < num_suits; j++) {
    if (suit_count[j] == 1) {
      singleton = one_card[j];
      nn += 1;
    }
  }

  has_singleton = (nn == 1);
}

const char *C_kept_description::display() {
  static char buffer[255];

  if (multi[4] != 0) return "#quads";
  if (multi[3] == 1 && multi[2] == 1) return "#full house";
  if (multi[3] != 0) return "#trips";
  if (multi[2] > 1) return "#two pair";
  if (multi[2] != 0) return "#pair";

  char *p = buffer;
  *p++ = '#';

  for (int j = 0; j < num_denoms; j++) {
    if (have[j]) {
      switch (j) {
        case ace:
          *p++ = 'A';
          break;

        case king:
          *p++ = 'K';
          break;

        case queen:
          *p++ = 'Q';
          break;

        case jack:
          *p++ = 'J';
          break;

        case ten:
          *p++ = 'T';
          break;

        default:
          *p++ = '3' + (j - three);
      }
    }
  }

  if (suited) *p++ = 's';

  if (num_discards != 0) {
    *p++ = ' ';
    *p++ = '/';
    *p++ = ' ';

    for (int k = 0; k < num_discards; k++) {
      card c = discards[k];

      switch (pips(c)) {
        case ace:
          *p++ = 'A';
          break;

        case king:
          *p++ = 'K';
          break;

        case queen:
          *p++ = 'Q';
          break;

        case jack:
          *p++ = 'J';
          break;

        case ten:
          *p++ = 'T';
          break;

        default:
          *p++ = '3' + (pips(c) - three);
      }

      if (suited && suit(c) == the_suit) {
        *p++ = 's';
      }
    }
  }

  *p++ = 0;
  return buffer;
}

std::string C_kept_description::move_name() {
  std::ostringstream name;
  if (multi[4] != 0) {
    name << "Quads";
  } else if (multi[3] == 1 && multi[2] == 1) {
    name << "Full House";
  } else if (multi[3] != 0) {
    name << "Trip " << denom_image[m_denom[3]];
  } else if (multi[2] > 1) {
    name << "Two Pair";
  } else if (multi[2] == 1) {
    name << "Pair of " << denom_image[m_denom[2]];
  } else if (multi[1] == 1) {
    name << "Just a " << denom_image[m_denom[1]];
  } else if (reach == 0) {
    name << "Nothing";
  } else if (reach <= 5) {
    if (!suited && min_denom >= jack) {
      if (have[ace]) {
        name << "A";
      }
      if (have[king]) {
        name << "K";
      }
      if (have[queen]) {
        name << "Q";
      }
      if (have[jack]) {
        name << "J";
      }
    } else if (multi[1] == 5) {
      if (suited) {
        if (min_denom == ten) {
          name << "Royal Flush";
        } else {
          name << "Straight Flush";
        }
      } else {
        name << "Straight";
      }
    } else {
      bool is_rf = false;
      if (suited) {
        if (min_denom < ten) {
          name << "SF ";
        } else {
          name << "RF ";
          is_rf = true;
        }
      } else {
        name << "Straight ";
      }
      const int gap = reach - multi[1];
      name << multi[1];
      if (is_rf) {
        if (multi[1] < 4) {
          name << " (";
          if (have[ace]) {
            name << "A";
          }
          if (have[king]) {
            name << "K";
          }
          if (have[queen]) {
            name << "Q";
          }
          if (have[jack]) {
            name << "J";
          }
          if (have[ten]) {
            name << "T";
          }
          name << ")";
        }
      } else {
        if (gap == 0) {
          // don't print
        } else if (gap == 1) {
          name << " i";
        } else if (gap == 2) {
          name << " di";
        } else {
          // Shouldn't happen!
          name << "gap " << gap;
        }
        name << " h" << high_denoms;
      }
    }
  } else if (suited) {
    if (multi[1] == 5) {
      name << "Flush";
    } else {
      name << "Flush " << multi[1] << " h" << high_denoms;
    }
  } else {
    name << "AAA???";
  }

  return name.str();
}

class denom_list {
 private:
  int *left;

 public:
  inline void add(int denom) { avail[left[denom]]++; }
  inline void remove(int denom) { avail[left[denom]]--; }
  // The client must not add the same denomination twice,
  // and must only remove denominations that have been
  // added.  There is no checking of these restrictions.

  int no_pair(int n);
  // returns the number of ways of selecting n cards that do
  // not pair up (that is only one of each denomination).

  int one_pair(int n);
  int two_pair(int n);
  int triple(int n);
  int full_house(int n);
  int quad(int n);

  int multi(int m, int n);
  // m = 2 => one_pair
  // m = 3 => triple, etc.

  denom_list(int *left_table);

 private:
  int avail[num_suits + 1];
  // For each denomination added, we need to know the number of
  // that kind of card is available.  This information is stored
  // inverted-- for each number that might be available 0, 1, 2, 3, 4,
  // we keep the number of denominations that have that availability.
};

denom_list::denom_list(int *left_table) {
  left = left_table;
  for (int j = 0; j <= num_suits; j++) avail[j] = 0;
}

int denom_list::no_pair(int n) {
  _ASSERT(n >= 0);
  // Everything is unrolled here; we know num_suits=4

  int sum4 = avail[4];
  int sum3 = avail[3] + sum4;
  int sum2 = avail[2] + sum3;
  int sum1 = avail[1] + sum2;

  if (n > sum1) return 0;
  // Make sure there are enough different denominations to select

  // The idea here is that we will select
  //
  //    n = j1 + j2 + j3 + j4
  //
  // cards, where j1 is the number of singleton denominations chosen,
  // j2 is the number of doubleton demominations chosen, and so on.
  //
  // For each quadruple (j1, j2, j3, j4), the number of ways to
  // select the cards is
  //
  //  Count (j1, j2, j3, j4) =
  //     choose (avail [1], j1) * 1**j1 *
  //     choose (avail [2], j2) * 2**j2 *
  //     choose (avail [3], j3) * 3**j3 *
  //     choose (avail [4], j4) * 4**j4
  //
  // The first subterm counts the number of subsets of demoninations
  // available, and the second counts the number of suit patterns
  // available for that subset.
  //
  // We iterate over all possible values of (j1, j2, j3, j4) and
  // sum up Count (j1, j2, j3, j4) for each.
  //
  // We find all possible values using four nested loops.
  // Since j1 + j2 + j3 + j4 = n, j1 is bounded below by
  // by n - avail[j2] - avail[j3] - avail[j4].  Likewise,
  // j1 is bounded above by both avail[1] and n.  There are
  // similar constraints at the next two nesting levels.
  // In the innermost level, j4 is determined by the equation
  // j4 = n - j3 - j2 - j1.

  int result = 0;

  int min1 = max_int(n - sum2, 0);
  int max1 = min_int(avail[1], n);
  for (int j1 = min1; j1 <= max1; j1++) {
    int f1 = combin.choose(avail[1], j1);
    n -= j1;

    int min2 = max_int(n - sum3, 0);
    int max2 = min_int(avail[2], n);
    for (int j2 = min2; j2 <= max2; j2++) {
      int f2 = combin.choose(avail[2], j2) * (1 << j2) * f1;
      // (1<<j2) == 2**j2

      n -= j2;

      int min3 = max_int(n - sum4, 0);
      int max3 = min_int(avail[3], n);
      for (int j3 = min3; j3 <= max3; j3++) {
        int power = 1;
        for (int jj = 0; jj < j3; jj++) power *= 3;
        // power == 3**j3

        int f3 = combin.choose(avail[3], j3) * power * f2;

        int j4 = n - j3;
        _ASSERT(0 <= j4 && j4 <= avail[4]);

        int f4 = combin.choose(avail[4], j4) * (1 << (2 * j4)) * f3;
        // (1<<(2*j4)) == 4**j4

        result += f4;
      }

      n += j2;
    }

    n += j1;
  }

  return result;
}

int denom_list::multi(int m, int n) {
  _ASSERT(m >= 1);

  if (m == 1) return no_pair(n);

  int result = 0;

  if (n >= m) {
    for (int j = m; j <= 4; j++) {
      if (avail[j] != 0) {
        const int a = avail[j];
        avail[j]--;
        result += a * combin.choose(j, m) * no_pair(n - m);
        avail[j]++;
      }
    }
  }

  return result;
}

int denom_list::one_pair(int n) {
  if (n < 2) return 0;

  int result = 0;

  if (avail[2] != 0) {
    int a = avail[2];
    avail[2]--;
    result += a * no_pair(n - 2);
    avail[2]++;
  }

  if (avail[3] != 0) {
    int a = avail[3];
    avail[3]--;
    result += a * 3 * no_pair(n - 2);
    avail[3]++;
  }

  if (avail[4] != 0) {
    int a = avail[4];
    avail[4]--;
    result += a * 6 * no_pair(n - 2);
    avail[4]++;
  }

  return result;
}

int denom_list::triple(int n) {
  if (n < 3) return 0;

  int result = 0;

  if (avail[3] != 0) {
    int a = avail[3];
    avail[3]--;
    result += a * no_pair(n - 3);
    avail[3]++;
  }

  if (avail[4] != 0) {
    int a = avail[4];
    avail[4]--;
    result += a * 4 * no_pair(n - 3);
    avail[4]++;
  }

  return result;
}

int denom_list::quad(int n) {
  if (n < 4) return 0;

  int result = 0;

  if (avail[4] != 0) {
    int a = avail[4];
    avail[4]--;
    result += a * no_pair(n - 4);
    avail[4]++;
  }

  return result;
}

int denom_list::two_pair(int n) {
  if (n < 4) return 0;

  int result = 0;

  if (avail[2] >= 2) {
    int a = avail[2];
    avail[2] -= 2;
    result += combin.choose(a, 2) * no_pair(n - 4);
    avail[2] += 2;
  }

  if (avail[3] >= 2) {
    int a = avail[3];
    avail[3] -= 2;
    result += combin.choose(a, 2) * (3 * 3) * no_pair(n - 4);
    avail[3] += 2;
  }

  if (avail[4] >= 2) {
    int a = avail[4];
    avail[4] -= 2;
    result += combin.choose(a, 2) * (6 * 6) * no_pair(n - 4);
    avail[4] += 2;
    ;
  }

  if (avail[2] > 0 && avail[3] > 0) {
    int a = avail[2];
    int b = avail[3];

    avail[2]--;
    avail[3]--;
    result += a * b * 3 * no_pair(n - 4);
    avail[2]++;
    avail[3]++;
  }

  if (avail[2] > 0 && avail[4] > 0) {
    int a = avail[2];
    int b = avail[4];

    avail[2]--;
    avail[4]--;
    result += a * b * 6 * no_pair(n - 4);
    avail[2]++;
    avail[4]++;
  }

  if (avail[3] > 0 && avail[4] > 0) {
    int a = avail[3];
    int b = avail[4];

    avail[3]--;
    avail[4]--;
    result += a * b * (3 * 6) * no_pair(n - 4);
    avail[3]++;
    avail[4]++;
  }

  return result;
}

int denom_list::full_house(int n) {
  if (n < 5) return 0;

  int result = 0;

  if (avail[3] >= 2) {
    result += avail[3] * (avail[3] - 1) * 3;
  }

  if (avail[4] >= 2) {
    result += avail[4] * (avail[4] - 1) * (4 * 6);
  }

  if (avail[2] > 0 && avail[3] > 0) {
    result += avail[2] * avail[3];
  }

  if (avail[2] > 0 && avail[4] > 0) {
    result += avail[2] * avail[4] * 4;
  }

  if (avail[3] > 0 && avail[4] > 0) {
    result += avail[3] * avail[4] * ((3 * 4) + 6);
  }

  return result;
}

void C_kept_description::all_draws(int deuces_kept, C_left &left,
                                   pay_dist &pays) {
  {
    for (int j = first_pay; j <= last_pay; j++) {
      pays[j] = 0;
    }
  }

  const int all_multiples = multi[2] + multi[3] + multi[4];

  const int cards_to_draw = num_discards + (num_jokers - deuces_kept);

  denom_list any_kept(left.denoms);
  // This is the list of cards available to be drawn
  // that will make a pair (or trip or quad) with a
  // demomination being kept.

  denom_list not_kept(left.denoms);

  // This is the list of cards available to be drawn
  // that will NOT pair up with a denomination being kept.

  denom_list low_not_kept(left.denoms);
  // The not_kept list, divided between high and low cards

  // Build these lists
  for (int k = 0; k < num_denoms; k++) {
    if (have[k]) {
      any_kept.add(k);
    } else {
      not_kept.add(k);

      if (!parms.is_high(k)) {
        low_not_kept.add(k);
      }
    }
  }

  const int max_jokers_drawn = min_int(cards_to_draw, left.jokers);

  // We first iterate over the number of wild cards it is possible
  // to draw.  Knowing how many wild cards we will end up with makes
  // it simpler to compute the number of payoff combinations.

  for (int jokers_drawn = 0; jokers_drawn <= max_jokers_drawn; jokers_drawn++) {
    const int must_draw = cards_to_draw - jokers_drawn;
    const int jokers = deuces_kept + jokers_drawn;

    pay_dist combos;
    // The sub-answer goes here.
    // It is the number of ways of making each payoff combination
    // given we have drawn jokers_drawn wild cards.

    {
      for (int k = first_pay; k <= last_pay; k++) {
        combos[k] = 0;
      }
    }

    int royals[4];
    {
      for (int k = 0; k < 4; k++) royals[k] = 0;
    }
    // Keep track of royal flushes separately by suit.
    // This is only needed for one_eyed_jacks

    // We also need to keep track how many of these
    // four payoffs consist only of low cards,
    // but only when jokers == 1

    int low_straight_combos = 0;
    int low_flush_combos = 0;
    int low_straight_flush_combos = 0;

    // Evaluate the payoff possibilities one by one.

    // Compute the number of ways of getting no pairs

    if (jokers + all_multiples == 0) {
      combos[N_nothing] += not_kept.no_pair(must_draw);
    }

    // Count the number of ways of getting one pair.
    // High pairs are added into combos[N_high_pair]
    // and low pairs are added int combos[N_nada]

    if (jokers == 1 && all_multiples == 0) {
      // Start with a joker and no natural pair

      const int no_pair = not_kept.no_pair(must_draw);
      // Number of draws that don't make a natural pair

      if (high_denoms > 0) {
        // If we began with a high card we have a wild high pair
        combos[N_high_pair] += no_pair;
      } else {  // Begin with no natural pairs or high cards

        const int no_pair_no_high = low_not_kept.no_pair(must_draw);
        // The number of draws that do not improve

        // Start with a wild low pair and don't improve
        combos[N_nothing] += no_pair_no_high;

        // Subtract to get the number of ways to
        // get a high card but still no natural pair
        combos[N_high_pair] += (no_pair - no_pair_no_high);
      }
    }

    if (jokers == 0 && multi[2] == 1 && multi[3] + multi[4] == 0) {
      // Start with a natural pair and don't improve
      if (parms.is_high(m_denom[2])) {
        combos[N_high_pair] += not_kept.no_pair(must_draw);
      } else {
        combos[N_nothing] += not_kept.no_pair(must_draw);
      }
    }

    if (jokers + all_multiples == 0) {  // Start with nothing

      // Draw a card that pairs one you kept
      if (must_draw >= 1) {
        int high_pairing_cards = 0;
        int low_pairing_cards = 0;

        for (int k = 0; k < num_denoms; k++) {
          if (have[k]) {
            if (parms.is_high(k)) {
              high_pairing_cards += left.denoms[k];
            } else {
              low_pairing_cards += left.denoms[k];
            }
          }
        }

        int non_pairing_cards = not_kept.no_pair(must_draw - 1);

        combos[N_high_pair] += high_pairing_cards * non_pairing_cards;
        combos[N_nothing] += low_pairing_cards * non_pairing_cards;
      }

      // or draw a pair that doesn't match a card you kept.
      if (must_draw >= 2) {
        for (int k = 0; k < num_denoms; k++) {
          if (!have[k]) {
            int pair_count = combin.choose(left.denoms[k], 2);
            if (must_draw > 2) {
              not_kept.remove(k);
              pair_count *= not_kept.no_pair(must_draw - 2);
              not_kept.add(k);
            }

            combos[parms.is_high(k) ? N_high_pair : N_nothing] += pair_count;
          }
        }
      }
    }

    // Count the number of ways of getting two pair.
    // Two pair cannot involve any jokers-- otherwise
    // it would be trips or a full house.

    // A pair is either an old pair, a new pair
    // (draw two cards that pair), a a split pair
    // (draw a card that pairs one kept).  Consider
    // all combintions.

    if (jokers + multi[3] + multi[4] == 0) {
      switch (multi[2]) {
        case 0:  // Start with no pairs

          // Two split pairs
          if (must_draw >= 2) {
            combos[N_two_pair] +=
                any_kept.no_pair(2) * not_kept.no_pair(must_draw - 2);
          }

          // One split pair and one new pair
          if (must_draw >= 1) {
            combos[N_two_pair] +=
                any_kept.no_pair(1) * not_kept.one_pair(must_draw - 1);
          }

          // Two new pair.
          combos[N_two_pair] += not_kept.two_pair(must_draw);

          break;

        case 1:  // Start with one pair
          // One new pair
          combos[N_two_pair] += not_kept.one_pair(must_draw);

          // One split pair

          if (must_draw >= 1) {
            // Disallow the paired item;
            // drawing that would get us trips.

            any_kept.remove(m_denom[2]);
            combos[N_two_pair] +=
                any_kept.no_pair(1) * not_kept.no_pair(must_draw - 1);
            any_kept.add(m_denom[2]);
          }
          break;

        case 2:  // Start with two pair
          combos[N_two_pair] += not_kept.no_pair(must_draw);
          break;
      }
    }

    // Count the number of ways of getting a full house

    if (multi[4] == 0) switch (jokers) {
        case 0:  // No jokers
          if (multi[3] == 0) {
            switch (multi[2]) {
              case 0:
                // Start with nothing.
                // Each of the pair/trips can be new or split.
                // That leads four combinations.

                // Both new
                combos[N_full_house] += not_kept.full_house(must_draw);

                // Split trips, new pair
                if (must_draw >= 2) {
                  combos[N_full_house] +=
                      any_kept.one_pair(2) * not_kept.one_pair(must_draw - 2);
                }

                // Split pair, new trips
                if (must_draw >= 1) {
                  combos[N_full_house] +=
                      any_kept.no_pair(1) * not_kept.triple(must_draw - 1);
                }

                // Both split
                if (must_draw == 3) {
                  combos[N_full_house] += any_kept.one_pair(3);
                }

                break;

              case 1:
                // Start with a pair.

                // Draw a card that matches the pair plus a new pair
                if (must_draw >= 1) {
                  combos[N_full_house] += left.denoms[m_denom[2]] *
                                          not_kept.one_pair(must_draw - 1);
                }

                // Draw a card that matches the pair plus a split pair
                if (must_draw >= 2) {
                  any_kept.remove(m_denom[2]);
                  combos[N_full_house] += left.denoms[m_denom[2]] *
                                          any_kept.no_pair(1) *
                                          not_kept.no_pair(must_draw - 2);
                  any_kept.add(m_denom[2]);
                }

                // Draw a pair that matches an existing non-pair
                if (must_draw >= 2) {
                  any_kept.remove(m_denom[2]);
                  combos[N_full_house] +=
                      any_kept.one_pair(2) * not_kept.no_pair(must_draw - 2);
                  any_kept.add(m_denom[2]);
                }

                // Draw a new triple
                combos[N_full_house] += not_kept.triple(must_draw);
                break;

              case 2:
                // Start with two pair
                // Draw a card that matches one of the pairs

                if (must_draw >= 1) {
                  combos[N_full_house] +=
                      (left.denoms[m_denom[2]] + left.denoms[other_pair]) *
                      not_kept.no_pair(must_draw - 1);
                }
                break;
            }
          } else  // Start with trips
          {
            switch (multi[2]) {
              case 0:
                // Start with just trips and draw a pair
                combos[N_full_house] += not_kept.one_pair(must_draw);

                // get a split pair
                if (must_draw >= 1) {
                  any_kept.remove(m_denom[3]);
                  combos[N_full_house] +=
                      any_kept.no_pair(1) * not_kept.no_pair(must_draw - 1);
                  any_kept.add(m_denom[3]);
                }
                break;

              case 1:
                // Start with a full house.  Get no help.
                _ASSERTE(must_draw == 0);
                combos[N_full_house] += 1;
            }
          }

          break;

        case 1:  // One deuce
          if (multi[3] == 0) {
            // The only way to get a full house with a deuce
            // is to get two pair plus the deuce.

            switch (multi[2]) {
              case 0:  // Start a deuce and no pairs

                // Two split pairs
                if (must_draw >= 2) {
                  combos[N_full_house] +=
                      any_kept.no_pair(2) * not_kept.no_pair(must_draw - 2);
                }

                // One split pair and one new pair
                if (must_draw >= 1) {
                  combos[N_full_house] +=
                      any_kept.no_pair(1) * not_kept.one_pair(must_draw - 1);
                }

                // Two new pair.
                combos[N_full_house] += not_kept.two_pair(must_draw);

                break;

              case 1:  // Start with a deuce and one pair
                // One new pair
                combos[N_full_house] += not_kept.one_pair(must_draw);

                // One split pair

                if (must_draw >= 1) {
                  // Disallow the paired item;
                  // drawing that would get us trips.

                  any_kept.remove(m_denom[2]);
                  combos[N_full_house] +=
                      any_kept.no_pair(1) * not_kept.no_pair(must_draw - 1);
                  any_kept.add(m_denom[2]);
                }
                break;

              case 2:
                // Start with two pair and a deuce.
                _ASSERT(must_draw == 0);
                combos[N_full_house] += 1;
                break;
            }
          }
          break;

          // Two jokers can lead to trips or four of a kind,
          // but not a full house.
      }

    // Count the number of ways of getting three, four or five of a kind

    if (jokers < 4) switch (all_multiples) {
        case 0:
          // The hand contains only singletons
          // Draw cards to match one and make the hand.

          // Draw two to make trips
          {
            int match = 2 - jokers;

            // if (match == 0)
            //{
            //	combos [trips] += not_kept.no_pair (must_draw);
            //}
            // else
            if (must_draw >= match && match > 0) {
              combos[N_trips] += any_kept.multi(match, match) *
                                 not_kept.no_pair(must_draw - match);
            }
          }

          // Draw three to make quads
          if (parms.bonus_quads) {
            _ASSERT(jokers == 0);

            switch (multi[1]) {
              case 1:
                // Holding one card.
                // Draw the other three plus a kicker
                {
                  const int d = m_denom[1];
                  if (left.denoms[d] == 3) {
                    const int kickers = parms.deck_size - 5 - 3;

                    int low_kickers = 0;
                    if (d != ace) low_kickers += left.denoms[ace];
                    if (d != deuce) low_kickers += left.denoms[deuce];
                    if (d != three) low_kickers += left.denoms[three];
                    if (d != four) low_kickers += left.denoms[four];

                    const int high_kickers = kickers - low_kickers;
                    switch (d) {
                      case ace:
                        if (parms.bonus_quads_kicker) {
                          combos[N_quad_aces] += high_kickers;
                          combos[N_quad_aces_kicker] += low_kickers;

                        } else {
                          combos[N_quad_aces] += kickers;
                        }
                        break;

                      case deuce:
                      case three:
                      case four:
                        if (parms.bonus_quads_kicker) {
                          combos[N_quad_low] += high_kickers;
                          combos[N_quad_low_kicker] += low_kickers;

                        } else {
                          combos[N_quad_low] += kickers;
                        }
                        break;

                      default:
                        combos[N_quads] += kickers;
                        break;
                    }
                  }
                }
                break;

              case 2:
                // Holding two cards.
                // Draw three to match one,
                // the other is the kicker
                for (int j = 0; j < 2; j++) {
                  int match, kicker;
                  if (j) {
                    match = m_denom[1];
                    kicker = other_singleton;
                  } else {
                    kicker = m_denom[1];
                    match = other_singleton;
                  }

                  if (left.denoms[match] == 3) {
                    int low_kicker;

                    switch (kicker) {
                      case ace:
                      case deuce:
                      case three:
                      case four:
                        low_kicker = 1;
                        break;
                      default:
                        low_kicker = 0;
                        break;
                    }

                    switch (match) {
                      case ace:
                        if (parms.bonus_quads_kicker) {
                          combos[low_kicker ? N_quad_aces_kicker
                                            : N_quad_aces] += 1;
                        } else {
                          combos[N_quad_aces] += 1;
                        }
                        break;

                      case deuce:
                      case three:
                      case four:
                        if (parms.bonus_quads_kicker) {
                          combos[low_kicker ? N_quad_low_kicker : N_quad_low] +=
                              1;
                        } else {
                          combos[N_quad_low] += 1;
                        }
                        break;

                      default:
                        combos[N_quads] += 1;
                        break;
                    }
                  }
                }
                break;
            }
          } else {
            int match = 3 - jokers;

            if (must_draw >= match && match > 0) {
              combos[N_quads] += any_kept.multi(match, match) *
                                 not_kept.no_pair(must_draw - match);
            }
          }

          // Draw four to make five of a kind
          {
            int match = 4 - jokers;

            if (must_draw >= match) {
              combos[N_quints] += any_kept.multi(match, match) *
                                  not_kept.no_pair(must_draw - match);
            }
          }

          // Or, make the hand by drawing all new cards.

          if (jokers < 3) {
            combos[N_trips] += not_kept.multi(3 - jokers, must_draw);
          }
          combos[N_quints] += not_kept.multi(5 - jokers, must_draw);

          if (parms.bonus_quads) {
            _ASSERT(jokers == 0);

            switch (multi[1]) {
              case 0:
                // Holding no cards.
                // Draw draw four plus a kicker
                {
                  for (int d = 0; d < num_denoms; d++) {
                    if (left.denoms[d] == 4) {
                      const int kickers = parms.deck_size - 5 - 4;

                      int low_kickers = 0;
                      if (d != ace) low_kickers += left.denoms[ace];
                      if (d != deuce) low_kickers += left.denoms[deuce];
                      if (d != three) low_kickers += left.denoms[three];
                      if (d != four) low_kickers += left.denoms[four];

                      const int high_kickers = kickers - low_kickers;
                      switch (d) {
                        case ace:
                          if (parms.bonus_quads_kicker) {
                            combos[N_quad_aces] += high_kickers;
                            combos[N_quad_aces_kicker] += low_kickers;

                          } else {
                            combos[N_quad_aces] += kickers;
                          }
                          break;

                        case deuce:
                        case three:
                        case four:
                          if (parms.bonus_quads_kicker) {
                            combos[N_quad_low] += high_kickers;
                            combos[N_quad_low_kicker] += low_kickers;
                          } else {
                            combos[N_quad_low] += kickers;
                          }
                          break;

                        default:
                          combos[N_quads] += kickers;
                          break;
                      }
                    }
                  }
                }
                break;

              case 1:
                // Holding one card, which will be the kicker.
                // Draw four other cards.
                {
                  for (int match = 0; match < num_denoms; match++) {
                    if (left.denoms[match] == 4) {
                      int low_kicker;

                      switch (m_denom[1]) {
                        case ace:
                        case deuce:
                        case three:
                        case four:
                          low_kicker = 1;
                          break;
                        default:
                          low_kicker = 0;
                          break;
                      }

                      switch (match) {
                        case ace:
                          if (parms.bonus_quads_kicker) {
                            combos[low_kicker ? N_quad_aces_kicker
                                              : N_quad_aces] += 1;
                          } else {
                            combos[N_quad_aces] += 1;
                          }
                          break;

                        case deuce:
                        case three:
                        case four:
                          if (parms.bonus_quads_kicker) {
                            combos[low_kicker ? N_quad_low_kicker
                                              : N_quad_low] += 1;
                          } else {
                            combos[N_quad_low] += 1;
                          }
                          break;

                        default:
                          combos[N_quads] += 1;
                          break;
                      }
                    }
                  }
                }
                break;
            }
          } else {
            combos[N_quads] += not_kept.multi(4 - jokers, must_draw);
          }
          break;

        case 1:
          // The hand contains a single multiple denomination.
          // It must be extended make the hand.
          {
            int c, d;

            if (multi[2] == 1) {
              c = 2;
              d = m_denom[2];
            } else if (multi[3] == 1) {
              c = 3;
              d = m_denom[3];
            } else {
              _ASSERT(multi[4] == 1);
              c = 4;
              d = m_denom[4];
            }

            {
              int match = 3 - c - jokers;
              if (must_draw >= match && match >= 0) {
                combos[N_trips] += combin.choose(left.denoms[d], match) *
                                   not_kept.no_pair(must_draw - match);
              }
            }

            {
              int match = 4 - c - jokers;
              if (must_draw >= match && match >= 0) {
                const int n = combin.choose(left.denoms[d], match);
                // The number of ways of completing the
                // four of a kind

                int kickers, low_kickers = 0;

                switch (multi[1]) {
                  case 0:
                    kickers =
                        parms.deck_size - 5 - left.denoms[d] - left.jokers;

                    if (parms.bonus_quads_kicker) {
                      if (d != ace) low_kickers += left.denoms[ace];
                      if (d != deuce) low_kickers += left.denoms[deuce];
                      if (d != three) low_kickers += left.denoms[three];
                      if (d != four) low_kickers += left.denoms[four];
                    }
                    break;

                  case 1:
                    kickers = 1;

                    if (parms.bonus_quads_kicker) {
                      switch (m_denom[1]) {
                        case ace:
                        case deuce:
                        case three:
                        case four:
                          low_kickers = 1;
                          break;
                      }
                    }
                    break;

                  default:
                    _ASSERT(false);
                }

                int high_kickers = kickers - low_kickers;

                if (parms.bonus_quads) {
                  switch (d) {
                    case ace:
                      if (parms.bonus_quads_kicker) {
                        combos[N_quad_aces_kicker] += n * low_kickers;
                        combos[N_quad_aces] += n * high_kickers;
                      } else {
                        combos[N_quad_aces] += n * kickers;
                      }
                      break;

                    case deuce:
                    case three:
                    case four:
                      if (parms.bonus_quads_kicker) {
                        combos[N_quad_low_kicker] += n * low_kickers;
                        combos[N_quad_low] += n * high_kickers;
                      } else {
                        combos[N_quad_low] += n * kickers;
                      }
                      break;

                    default:
                      combos[N_quads] += n * kickers;
                      break;
                  }
                } else {
                  combos[N_quads] += n * kickers;
                }
              }
            }

            {
              int match = 5 - c - jokers;
              if (must_draw >= match && match >= 0) {
                combos[N_quints] += combin.choose(left.denoms[d], match) *
                                    not_kept.no_pair(must_draw - match);
              }
            }
          }

          break;
      }

    if (jokers == 4) {
      combos[N_four_deuces] +=
          combin.choose(parms.deck_size - 5 - jokers_drawn, must_draw);
    }

    // Count all the ways of making a straight
    if (all_multiples == 0 && jokers <= 2 && reach <= 5) {
      switch (jokers) {
        case 0:
          // Consider all consecutive intervals of length 5
          if (reach == 1 && min_denom == ace) {
            // Only possible straights are A-high and A-low
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              prod *= left.denoms[three];
              prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[ten];
              prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = ten;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 5);
              hi = min_int(ten, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              int prod = 1;

              if (!have[j]) prod *= left.denoms[j];
              if (!have[j + 1]) prod *= left.denoms[j + 1];
              if (!have[j + 2]) prod *= left.denoms[j + 2];
              if (!have[j + 3]) prod *= left.denoms[j + 3];
              int k = (j == ten ? ace : j + 4);
              if (!have[k]) prod *= left.denoms[k];

              combos[N_straight] += prod;
            }
          }
          break;

        case 1:
          // Consider all consecutive intervals of length 5
          // with one internal draw (in poker slang a gutshot)
          // omitted due the wild card

          if (reach == 1 && min_denom == ace) {
            // A-low straights with one gutshow
            {
              int prod = 1;

              // prod *= left.denoms[two];
              prod *= left.denoms[three];
              prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              // prod *= left.denoms[three];
              prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              prod *= left.denoms[three];
              // prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }

            // A-high straights with one gutshot
            {
              int prod = 1;

              prod *= left.denoms[ten];
              // prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[ten];
              prod *= left.denoms[jack];
              // prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[ten];
              prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              // prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = ten;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 5);
              hi = min_int(ten, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              if (!have[j + 1]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                // if (!have[j+1]) prod *= left.denoms[j+1];
                if (!have[j + 2]) prod *= left.denoms[j + 2];
                if (!have[j + 3]) prod *= left.denoms[j + 3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;

                if (j != ace && !parms.is_high(k)) {
                  low_straight_combos += prod;
                }
              }
              if (!have[j + 2]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                if (!have[j + 1]) prod *= left.denoms[j + 1];
                // if (!have[j+2]) prod *= left.denoms[j+2];
                if (!have[j + 3]) prod *= left.denoms[j + 3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
                if (j != ace && !parms.is_high(k)) {
                  low_straight_combos += prod;
                }
              }
              if (!have[j + 3]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                if (!have[j + 1]) prod *= left.denoms[j + 1];
                if (!have[j + 2]) prod *= left.denoms[j + 2];
                // if (!have[j+3]) prod *= left.denoms[j+3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;

                if (j != ace && !parms.is_high(k)) {
                  low_straight_combos += prod;
                }
              }
            }
          }

          // Consider all consecutive intervals of length 4
          if (reach == 1 && min_denom == ace) {
            // Only possible straights are A-high and A-low
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              prod *= left.denoms[three];
              prod *= left.denoms[four];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else if (reach <= 4) {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = jack;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 4);
              hi = min_int(jack, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              int prod = 1;

              if (!have[j]) prod *= left.denoms[j];
              if (!have[j + 1]) prod *= left.denoms[j + 1];
              if (!have[j + 2]) prod *= left.denoms[j + 2];
              int k = (j == jack ? ace : j + 3);
              if (!have[k]) prod *= left.denoms[k];

              combos[N_straight] += prod;

              if (j != ace && !parms.is_high(k)) {
                low_straight_combos += prod;
              }
            }
          }
          break;

        case 2:
          // Consider all consecutive intervals of length 5
          // with two gutshots.

          if (reach == 1 && min_denom == ace) {
            // A-low straights with two gutshots
            {
              int prod = 1;

              // prod *= left.denoms[two];
              // prod *= left.denoms[three];
              prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              // prod *= left.denoms[deuce];
              prod *= left.denoms[three];
              // prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              // prod *= left.denoms[three];
              // prod *= left.denoms[four];
              prod *= left.denoms[five];

              combos[N_straight] += prod;
            }

            // A-high straghts with two gutshots
            {
              int prod = 1;

              prod *= left.denoms[ten];
              // prod *= left.denoms[jack];
              // prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[ten];
              // prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              // prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[ten];
              prod *= left.denoms[jack];
              // prod *= left.denoms[queen];
              // prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = ten;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 5);
              hi = min_int(ten, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              if (!have[j + 1] && !have[j + 2]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                // if (!have[j+1]) prod *= left.denoms[j+1];
                // if (!have[j+2]) prod *= left.denoms[j+2];
                if (!have[j + 3]) prod *= left.denoms[j + 3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
              }
              if (!have[j + 1] && !have[j + 3]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                // if (!have[j+1]) prod *= left.denoms[j+1];
                if (!have[j + 2]) prod *= left.denoms[j + 2];
                // if (!have[j+3]) prod *= left.denoms[j+3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
              }
              if (!have[j + 2] && !have[j + 3]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                if (!have[j + 1]) prod *= left.denoms[j + 1];
                // if (!have[j+2]) prod *= left.denoms[j+2];
                // if (!have[j+3]) prod *= left.denoms[j+3];
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
              }
            }
          }

          // Consider all consecutive intervals of length 4
          // with one gutshot

          if (reach == 1 && min_denom == ace) {
            // Only possible straights are A-high and A-low
            {
              int prod = 1;

              // prod *= left.denoms[deuce];
              prod *= left.denoms[three];
              prod *= left.denoms[four];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              // prod *= left.denoms[three];
              prod *= left.denoms[four];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[jack];
              // prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[jack];
              prod *= left.denoms[queen];
              // prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else if (reach <= 4) {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = jack;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 4);
              hi = min_int(jack, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              if (!have[j + 1]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                // if (!have[j+1]) prod *= left.denoms[j+1];
                if (!have[j + 2]) prod *= left.denoms[j + 2];
                int k = (j == jack ? ace : j + 3);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
              }
              if (!have[j + 2]) {
                int prod = 1;

                if (!have[j]) prod *= left.denoms[j];
                if (!have[j + 1]) prod *= left.denoms[j + 1];
                // if (!have[j+2]) prod *= left.denoms[j+2];
                int k = (j == jack ? ace : j + 3);
                if (!have[k]) prod *= left.denoms[k];

                combos[N_straight] += prod;
              }
            }
          }

          // Consider all consecutive intervals of length 3
          if (reach == 1 && min_denom == ace) {
            // Only possible straights are A-high and A-low
            {
              int prod = 1;

              prod *= left.denoms[deuce];
              prod *= left.denoms[three];

              combos[N_straight] += prod;
            }
            {
              int prod = 1;

              prod *= left.denoms[queen];
              prod *= left.denoms[king];

              combos[N_straight] += prod;
            }
          } else if (reach <= 3) {
            int lo, hi;

            if (reach == 0) {
              // Any straight is possible
              lo = ace;
              hi = queen;
            } else {
              // Only straights in the vicinity of the reach
              // interval are possible
              lo = max_int(ace, min_denom + reach - 3);
              hi = min_int(queen, min_denom);
            }

            for (int j = lo; j <= hi; j++) {
              int prod = 1;

              if (!have[j]) prod *= left.denoms[j];
              if (!have[j + 1]) prod *= left.denoms[j + 1];
              int k = (j == queen ? ace : j + 2);
              if (!have[k]) prod *= left.denoms[k];

              combos[N_straight] += prod;
            }
          }
          break;

        default:
          _ASSERT(false);
      }
    }

    // Count the number of ways of getting a flush

    if (all_multiples == 0 && jokers <= 2) {
      if (multi[1] == 0) {
        // The only way to go for flushes of different suits
        for (int s = 0; s < num_suits; s++) {
          combos[N_flush] += combin.choose(left.suits[s], must_draw);

          if (jokers == 1 && high_denoms == 0) {
            low_flush_combos += combin.choose(left.low_suits[s], must_draw);
          }
        }
      } else if (suited) {
        combos[N_flush] = combin.choose(left.suits[the_suit], must_draw);

        if (jokers == 1 && high_denoms == 0) {
          low_flush_combos +=
              combin.choose(left.low_suits[the_suit], must_draw);
        }
      }
    }

    // Count the number of ways of getting a straight flush

    if (suited && all_multiples == 0 && jokers <= 3 && reach <= 5) {
      int min_suit, max_suit;

      if (reach == 0) {
        min_suit = 0;
        max_suit = num_suits - 1;
      } else {
        min_suit = the_suit;
        max_suit = the_suit;
      }

      for (int s = min_suit; s <= max_suit; s++) {
        switch (jokers) {
          case 0:
            // Consider all consecutive intervals of length 5
            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                prod &= left.available(three, s);
                prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(ten, s);
                prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = ten;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 5);
                hi = min_int(ten, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                bool prod = true;

                if (!have[j]) prod &= left.available(j, s);
                if (!have[j + 1]) prod &= left.available(j + 1, s);
                if (!have[j + 2]) prod &= left.available(j + 2, s);
                if (!have[j + 3]) prod &= left.available(j + 3, s);
                int k = (j == ten ? ace : j + 4);
                if (!have[k]) prod &= left.available(k, s);

                combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                if (j >= ten) royals[s] += prod;
              }
            }
            break;

          case 1:
            // Consider all consecutive intervals of length 5
            // with one internal draw (in poker slang a gutshot)
            // omitted due the wild card

            if (reach == 1 && min_denom == ace) {
              // A-low straights with one gutshow
              {
                bool prod = true;

                // prod &= left.available(two, s);
                prod &= left.available(three, s);
                prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                // prod &= left.available(three, s);
                prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                prod &= left.available(three, s);
                // prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }

              // A-high straights with one gutshot
              {
                bool prod = true;

                prod &= left.available(ten, s);
                // prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(ten, s);
                prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(ten, s);
                prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = ten;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 5);
                hi = min_int(ten, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  if (!have[j + 2]) prod &= left.available(j + 2, s);
                  if (!have[j + 3]) prod &= left.available(j + 3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;

                  if (j != ace && !parms.is_high(k)) {
                    low_straight_flush_combos += prod;
                  }
                }
                if (!have[j + 2]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  if (!have[j + 1]) prod &= left.available(j + 1, s);
                  // if (!have[j+2]) prod &= left.available(j+2, s);
                  if (!have[j + 3]) prod &= left.available(j + 3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;

                  if (j != ace && !parms.is_high(k)) {
                    low_straight_flush_combos += prod;
                  }
                }
                if (!have[j + 3]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  if (!have[j + 1]) prod &= left.available(j + 1, s);
                  if (!have[j + 2]) prod &= left.available(j + 2, s);
                  // if (!have[j+3]) prod &= left.available(j+3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                  if (j != ace && !parms.is_high(k)) {
                    low_straight_flush_combos += prod;
                  }
                }
              }
            }

            // Consider all consecutive intervals of length 4
            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                prod &= left.available(three, s);
                prod &= left.available(four, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 4) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = jack;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 4);
                hi = min_int(jack, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                bool prod = true;

                if (!have[j]) prod &= left.available(j, s);
                if (!have[j + 1]) prod &= left.available(j + 1, s);
                if (!have[j + 2]) prod &= left.available(j + 2, s);
                int k = (j == jack ? ace : j + 3);
                if (!have[k]) prod &= left.available(k, s);

                combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                if (j >= ten) royals[s] += prod;
                if (j != ace && !parms.is_high(k)) {
                  low_straight_flush_combos += prod;
                }
              }
            }
            break;

          case 2:
            // Consider all consecutive intervals of length 5
            // with two gutshots.

            if (reach == 1 && min_denom == ace) {
              // A-low straights with two gutshots
              {
                bool prod = true;

                // prod &= left.available(two, s);
                // prod &= left.available(three, s);
                prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                // prod &= left.available(deuce, s);
                prod &= left.available(three, s);
                // prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                // prod &= left.available(three, s);
                // prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }

              // A-high straghts with two gutshots
              {
                bool prod = true;

                prod &= left.available(ten, s);
                // prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(ten, s);
                // prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(ten, s);
                prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = ten;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 5);
                hi = min_int(ten, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1] && !have[j + 2]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  // if (!have[j+2]) prod &= left.available(j+2, s);
                  if (!have[j + 3]) prod &= left.available(j + 3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
                if (!have[j + 1] && !have[j + 3]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  if (!have[j + 2]) prod &= left.available(j + 2, s);
                  // if (!have[j+3]) prod &= left.available(j+3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
                if (!have[j + 2] && !have[j + 3]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  if (!have[j + 1]) prod &= left.available(j + 1, s);
                  // if (!have[j+2]) prod &= left.available(j+2, s);
                  // if (!have[j+3]) prod &= left.available(j+3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
              }
            }

            // Consider all consecutive intervals of length 4
            // with one gutshot

            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                // prod &= left.available(deuce, s);
                prod &= left.available(three, s);
                prod &= left.available(four, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                // prod &= left.available(three, s);
                prod &= left.available(four, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(jack, s);
                prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 4) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = jack;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 4);
                hi = min_int(jack, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  if (!have[j + 2]) prod &= left.available(j + 2, s);
                  int k = (j == jack ? ace : j + 3);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
                if (!have[j + 2]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  if (!have[j + 1]) prod &= left.available(j + 1, s);
                  // if (!have[j+2]) prod &= left.available(j+2, s);
                  int k = (j == jack ? ace : j + 3);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
              }
            }

            // Consider all consecutive intervals of length 3
            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                prod &= left.available(deuce, s);
                prod &= left.available(three, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(queen, s);
                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 3) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = queen;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 3);
                hi = min_int(queen, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                bool prod = true;

                if (!have[j]) prod &= left.available(j, s);
                if (!have[j + 1]) prod &= left.available(j + 1, s);
                int k = (j == queen ? ace : j + 2);
                if (!have[k]) prod &= left.available(k, s);

                combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                if (j >= ten) royals[s] += prod;
              }
            }
            break;

          case 3:
            // Consider all consecutive intervals of length 5
            // with three gutshots.

            if (reach == 1 && min_denom == ace) {
              // A-low straight with three gutshots
              {
                bool prod = true;

                // prod &= left.available(two, s);
                // prod &= left.available(three, s);
                // prod &= left.available(four, s);
                prod &= left.available(five, s);

                combos[N_straight_flush] += prod;
              }

              // A-high straight with three gutshots
              {
                bool prod = true;

                prod &= left.available(ten, s);
                // prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = ten;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 5);
                hi = min_int(ten, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1] && !have[j + 2] && !have[j + 3]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  // if (!have[j+2]) prod &= left.available(j+2, s);
                  // if (!have[j+3]) prod &= left.available(j+3, s);
                  int k = (j == ten ? ace : j + 4);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
              }
            }

            // Consider all consecutive intervals of length 4
            // with two gutshots

            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                // prod &= left.available(deuce, s);
                // prod &= left.available(three, s);
                prod &= left.available(four, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(jack, s);
                // prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 4) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = jack;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 4);
                hi = min_int(jack, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1] && !have[j + 2]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  // if (!kept.have[j+2]) prod &= left.available(j+2, s);
                  int k = (j == jack ? ace : j + 3);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
              }
            }

            // Consider all consecutive intervals of length 3
            // with one gutshot
            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                // prod &= left.available(deuce, s);
                prod &= left.available(three, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(queen, s);
                // prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 3) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = queen;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 3);
                hi = min_int(queen, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                if (!have[j + 1]) {
                  bool prod = true;

                  if (!have[j]) prod &= left.available(j, s);
                  // if (!have[j+1]) prod &= left.available(j+1, s);
                  int k = (j == queen ? ace : j + 2);
                  if (!have[k]) prod &= left.available(k, s);

                  combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                  if (j >= ten) royals[s] += prod;
                }
              }
            }

            // Consider all consecutive intervals of length 2
            if (reach == 1 && min_denom == ace) {
              // Only possible straights are A-high and A-low
              {
                bool prod = true;

                prod &= left.available(deuce, s);

                combos[N_straight_flush] += prod;
              }
              {
                bool prod = true;

                prod &= left.available(king, s);

                combos[N_royal_flush] += prod;
                royals[s] += prod;
              }
            } else if (reach <= 2) {
              int lo, hi;

              if (reach == 0) {
                // Any straight is possible
                lo = ace;
                hi = king;
              } else {
                // Only straights in the vicinity of the reach
                // interval are possible
                lo = max_int(ace, min_denom + reach - 2);
                hi = min_int(king, min_denom);
              }

              for (int j = lo; j <= hi; j++) {
                bool prod = true;

                if (!have[j]) prod &= left.available(j, s);
                int k = (j == king ? ace : j + 1);
                if (!have[k]) prod &= left.available(k, s);

                combos[j >= ten ? N_royal_flush : N_straight_flush] += prod;
                if (j >= ten) royals[s] += prod;
              }
            }
            break;

          default:
            _ASSERT(false);
        }
      }
    }

    // Now make adjustments for counting things twice.

    // A straight or royal flush is also counted as a straight and a flush.
    // But we only counted it as a straight or flush in the
    // first place if jokers <= 2.

    if (jokers <= 2) {
      combos[N_straight] -= combos[N_straight_flush];
      combos[N_straight] -= combos[N_royal_flush];
      combos[N_flush] -= combos[N_straight_flush];
      combos[N_flush] -= combos[N_royal_flush];
    }

    const int straight_or_flush = combos[N_straight] + combos[N_flush] +
                                  combos[N_straight_flush] +
                                  combos[N_royal_flush];

    // straights and flushes were also counted for whatever
    // pair structure they have.

    switch (jokers) {
      case 0:
        combos[N_nothing] -= straight_or_flush;
        break;

      case 1: {
        // Here is the case that causes a huge wrinkle in
        // the code that computes combinations.  If there
        // is one joker, a straight or a flush will be counted
        // as a pair.  But we need to know whether it is a
        // high pair or a low pair, so we know which count
        // to subtract it from.

        low_straight_combos -= low_straight_flush_combos;
        low_flush_combos -= low_straight_flush_combos;

        const int low_straight_or_flush =
            low_straight_combos + low_flush_combos + low_straight_flush_combos;

        const int high_straight_or_flush =
            straight_or_flush - low_straight_or_flush;

        combos[N_nothing] -= low_straight_or_flush;
        combos[N_high_pair] -= high_straight_or_flush;
        break;
      }

      case 2:
        combos[N_trips] -= straight_or_flush;
        break;

      case 3:
        combos[N_quads] -= straight_or_flush;
        break;
    }

    const int joker_factor = combin.choose(left.jokers, jokers_drawn);

    for (int p = first_pay; p <= last_pay; p++) {
      pays[(p == N_royal_flush && jokers != 0) ? N_wild_royal : p] +=
          joker_factor * combos[p];
    }

    if (parms.kind == GK_one_eyed_jacks_wild && jokers == 1) {
      // Since suits 0 and 1 are missing jacks, any wild royal
      // flushes will be paid as natural royals if the suit of the
      // jack matches the suit of the other cards.

      // Correct for the fact that we have countes these as wild royals.
      int promote_wild_royals = royals[0];

      switch (deuces_kept) {
        case 0:
          // We kept none but drew one.
          _ASSERT(joker_factor == 2);

          // We have added into pays all four combinations of
          // the suited royals and the wild jacks.  On each
          // of the royal combinations must be promoted.
          promote_wild_royals += royals[1];
          break;

        case 1:
          _ASSERT(joker_factor == 1);
          // If we've saved the jack, by convention we can deem it
          // to be suit zero.  We only promote royals of that suit.
          // In this case, the iterator never considers suits 0 and
          // 1 to be equivalent, just so this code works.
          break;

        default:
          _ASSERT(0);
      }
      // We must convert all the wild royals of the suit that
      // matches the joker to natural royal flushes.

      _ASSERT(pays[N_wild_royal] >= promote_wild_royals);
      pays[N_wild_royal] -= promote_wild_royals;
      pays[N_royal_flush] += promote_wild_royals;
    }
  }

  {
    int total_pays = 0;

    for (int j = first_pay; j <= last_pay; j++) {
      const int pay = pays[j];
      total_pays += pay;
    }

    // Make sure the books balance.
    // Every possible draw should be represented exactly once
    // in pays.

    {
      const int correct = combin.choose(parms.deck_size - 5, cards_to_draw);

      if (total_pays != correct) {
        _ASSERT(0);
      }
    }
  }
}

payoff_name C_kept_description::name() {
  payoff_name straightlike = N_nothing;
  payoff_name pairlike = N_nothing;

  if (multi[2] + multi[3] + multi[4] == 0 &&
      multi[1] + num_jokers ==
          5) {  // A five-card hand containing wild cards and singletons.
    // Check for straight, flush, straight_flush, etc.

    int min_denom = -1;
    int min_no_ace = ace;
    int max_denom;
    bool all_over_ten = true;

    for (int j = 0; j < num_denoms; j++) {
      if (have[j]) {
        if (min_denom < 0) min_denom = j;
        if (min_no_ace == ace) min_no_ace = j;
        if (ace < j && j < ten) all_over_ten = false;
        max_denom = j;
      }
    }

    _ASSERT(min_denom >= 0);

    if (have[ace] && all_over_ten) {
      // If we have a straight, it will be Ace high
      max_denom = king + 1;
      min_denom = min_no_ace;
    }

    const int reach = max_denom - min_denom + 1;

    if (reach <= 5) {
      // We have some flavor of straight

      if (suited) {
        if (all_over_ten) {
          if (num_jokers == 0) {
            straightlike = N_royal_flush;
          } else {
            straightlike = N_wild_royal;
          }
        } else {
          straightlike = N_straight_flush;
        }
      } else {
        straightlike = N_straight;
      }
    } else if (suited) {
      straightlike = N_flush;
    }
  }

  if (multi[1] + multi[2] + multi[3] + multi[4] == 1) {
    int m = 0;

    // N-of-a-kind
    if (multi[1] == 1) m = 1;
    if (multi[2] == 1) m = 2;
    if (multi[3] == 1) m = 3;
    if (multi[4] == 1) m = 4;

    switch (m + num_jokers) {
      case 2:
        if (parms.is_high(m_denom[m])) pairlike = N_high_pair;
        break;

      case 3:
        pairlike = N_trips;
        break;

      case 4:
        pairlike = N_quads;
        break;

      case 5:
        pairlike = N_quints;
        break;
    }
  } else if (multi[2] == 2 && num_jokers == 1 ||
             multi[3] == 1 && multi[2] == 1 && num_jokers == 0) {
    pairlike = N_full_house;
  } else if (multi[2] == 2 && num_jokers == 0) {
    pairlike = N_two_pair;
  } else if (num_jokers == 4 && multi[1] == 0) {
    pairlike = N_four_deuces;
  }

  return pairlike > straightlike ? pairlike : straightlike;
}
