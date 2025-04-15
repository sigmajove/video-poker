#include "handmaster.h"

#include <time.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "game.h"
#include "kept.h"
#include "vpoker.h"

extern double random();

static int rand_int(int N)
// Return a random number in the range 0..N-1
{
  int result = static_cast<int>(random() * static_cast<double>(N));

  if (result >= N) result = N - 1;
  // Needed if N == 0, random() returns 1.0, or I don't
  // really understand float to int conversion rules in C++

  _ASSERT(result >= 0);

  return result;
}

struct deck {
  int size() { return shuffled.size(); }
  std::vector<card> shuffled;

  void initialize(int deck_size);
};

void deck::initialize(int deck_size) {
  shuffled.resize(deck_size);
  for (int j = 0; j < deck_size; j++) shuffled[j] = j;
}

static deck shoe;

void initialize_deck() { shoe.initialize(selected_game->deck_size); }

void shuffle_hand(card *hand) {
  for (int j = 0; j < 5; j++) {
    const int top = shoe.size() - 1 - j;
    const int x = rand_int(shoe.size() - j);

    // Swap top and x
    const card t = shoe.shuffled[x];
    shoe.shuffled[x] = shoe.shuffled[top];
    shoe.shuffled[top] = t;

    hand[j] = t;
  }

  // No need to restore the shoe; one permutation
  // is as good as any.
}

struct triple {
  // The key that is used to denote an equivalence
  // class of hands.

  int deuces;
  int best_priority, next_priority;
};

struct lt_triple {
  bool operator()(const triple t1, const triple t2) const {
    // Lexicographic compare
    if (t1.deuces < t2.deuces) return true;
    if (t1.deuces > t2.deuces) return false;
    if (t1.best_priority < t2.best_priority) return true;
    if (t1.best_priority > t2.best_priority) return false;
    return t1.next_priority < t2.next_priority;
  }
};

struct saved_hand {
  card hand[5];
};

typedef std::map<const triple, saved_hand *, lt_triple> hand_map;

// Maps keys to the one hand of that class we have saved.
// If a key maps to zero, it means we are waiting for a
// new hand with that key to put into the error list.

hand_map cache;

struct iter_list {
  // A bounded list of map iterators.
  // Every nonempty cache value appears exactly once
  // on a list somewhere.

  const int max_length;
  int current_length;
  hand_map::iterator *const list;

  iter_list(int n);
  ~iter_list();

  void add(hand_map::iterator x);
  // Add x to the list, killing one cache entry to make
  // room if necessary.  x should already be in the cache
  // not not on any list.

  hand_map::iterator remove_random();
  // Remove a random item from the list and the cache.

  void clear();
};

iter_list::iter_list(int n)
    : max_length(n), current_length(0), list(new hand_map::iterator[n]) {
  for (int j = 0; j < n; j++) list[j] = hand_map::iterator();
}

void iter_list::clear() {
  for (int j = 0; j < current_length; j++) {
    delete list[j]->second;
    list[j] = hand_map::iterator();
  }
  current_length = 0;
}

iter_list::~iter_list() { delete[] list; }

void iter_list::add(hand_map::iterator x) {
  if (current_length < max_length) {
    list[current_length++] = x;
  } else {
    // Randomly replace one
    const int pick = rand_int(current_length);
    const hand_map::iterator p = list[pick];

    _ASSERT(p->second != 0);
    delete p->second;
    cache.erase(p);

    list[pick] = x;
  }
};

hand_map::iterator iter_list::remove_random() {
  _ASSERT(current_length > 0);
  const int pick = rand_int(current_length);
  const hand_map::iterator result = list[pick];

  list[pick] = list[--current_length];
  // Fill the newly created hole by moving the
  // end element down.

  return result;
};

typedef enum { trivial, easy, simple, hard, max_difficulty } difficulty;

iter_list errors(25);

struct C_lists {
  iter_list *rand_hands[max_difficulty];

  C_lists();
  ~C_lists();
  void clear();
} lists;

const int rand_batch = 150;

C_lists::C_lists() {
  rand_hands[trivial] = new iter_list(30);
  rand_hands[easy] = new iter_list(30);
  rand_hands[simple] = new iter_list(60);
  rand_hands[hard] = new iter_list(60);
}

C_lists::~C_lists() {
  delete rand_hands[trivial];
  delete rand_hands[easy];
  delete rand_hands[simple];
  delete rand_hands[hard];
}

void C_lists::clear() {
  rand_hands[trivial]->clear();
  rand_hands[easy]->clear();
  rand_hands[simple]->clear();
  rand_hands[hard]->clear();
  errors.clear();
}

void erase_hands() {
  lists.clear();
  cache.clear();
}

void sort_hand(enum_match &matcher, card *hand, int *perm,
               unsigned &wild_mask) {
  // Count and sort the non-wild cards and place them
  // into matcher for analysis.  Also return a permutation
  // that can be used to map the results returned by matcher
  // to the original ordering.

  unsigned packed[5];
  unsigned *sp = packed;
  wild_mask = 0;

  for (int j = 0; j < 5; j++) {
    card c = hand[j];
    if (selected_game->is_wild(c)) {
      wild_mask |= 1 << j;
    } else {
      *sp++ = (c << 3) | j;
    }
  }

  std::sort(packed, sp);
  matcher.hand_size = sp - packed;
  matcher.wild_cards = 5 - matcher.hand_size;
  matcher.parms = selected_game;

  for (int j = 0; j < matcher.hand_size; j++) {
    matcher.hand[j] = packed[j] >> 3;
    perm[j] = packed[j] & 7;
  }
}

void sort_hand(card *hand, card *sorted, int &hand_size) {
  card *sp = sorted;
  for (int j = 0; j < 5; j++) {
    card c = hand[j];
    if (!selected_game->is_wild(c)) {
      *sp++ = c;
    }
  }

  std::sort(sorted, sp);
  hand_size = sp - sorted;
}

unsigned invert_mask(int hand_size, unsigned mask, int *perm) {
  unsigned result = 0;
  for (int j = 0; j < hand_size; j++) {
    if (mask & (1 << j)) {
      result |= (1 << perm[j]);
    }
  }

  return result;
}

bool is_right_move(card *hand, unsigned mask) {
  enum_match matcher;
  int j;
  int perm[5];
  unsigned wild_mask;

  sort_hand(matcher, hand, perm, wild_mask);

  line_list &cs = selected_strategy.at(matcher.wild_cards);

  for (line_list::iterator rover = cs.begin(); rover != cs.end(); ++rover) {
    matcher.find((*rover).pattern);
    if (matcher.match_count) {
      for (j = 0; j < matcher.match_count; j++) {
        if ((invert_mask(matcher.hand_size, matcher.matches[j], perm) |
             wild_mask) == mask)
          return true;
      }
      return false;
    }
  }

  // Should never get here if strategy is anchored by "nothing"
  return mask == 0;
}

void find_right_move(card *hand, unsigned &mask, const char *&name) {
  enum_match matcher;
  int perm[5];
  unsigned wild_mask;

  sort_hand(matcher, hand, perm, wild_mask);
  line_list &cs = selected_strategy.at(matcher.wild_cards);

  for (line_list::iterator rover = cs.begin(); rover != cs.end(); ++rover) {
    matcher.find((*rover).pattern);
    if (matcher.match_count) {
      mask =
          invert_mask(matcher.hand_size, matcher.matches[0], perm) | wild_mask;
      name = (*rover).image;
      return;
    }
  }

  // Should never get here if strategy is anchored by "nothing"
  mask = 0;
  name = "Nothing";
}

void analyze_hand(card *hand, triple &result, difficulty &diff) {
  enum_match matcher;

  int perm[5];
  unsigned wild_mask;

  sort_hand(matcher, hand, perm, wild_mask);

  // Figure out whether there is a paying combination on the hand
  unsigned best_pay_mask = 0;
  unsigned first_pick_mask = 0;
  unsigned second_pick_mask = 0;

  {
    unsigned max_mask = 1 << matcher.hand_size;

    int best_pay = 0;

    for (unsigned m = 1; m < max_mask; m++) {
      int m_pay =
          selected_game
              ->pay_table[kept_description(matcher.hand, matcher.hand_size, m,
                                           *selected_game)
                              .name()];

      if (m_pay > best_pay) {
        best_pay = m_pay;
        best_pay_mask = m;
      }
    }
  }

  result.deuces = matcher.wild_cards;
  result.best_priority = -1;
  result.next_priority = -1;

  int match_number = 0;
  int j = 0;

  line_list &cs = selected_strategy.at(matcher.wild_cards);

  for (line_list::iterator rover = cs.begin(); rover != cs.end();
       ++rover, ++j) {
    matcher.find((*rover).pattern);

    if (matcher.match_count) {
      switch (match_number) {
        case 0:
          result.best_priority = j;
          first_pick_mask = matcher.matches[0];
          match_number = 1;

          // If the strategy simply picks the best-paying
          // hand, classify this hand a no-brainer
          if (best_pay_mask) {
            for (int k = 0; k < matcher.match_count; k++) {
              if (matcher.matches[k] == best_pay_mask) {
                diff = trivial;
                return;
              }
            }
          }

          break;

        case 1:
          if (matcher.matches[0] == first_pick_mask) continue;
          // Ignore a line that makes the same choice for a
          // different reason.

          result.next_priority = j;
          second_pick_mask = matcher.matches[0];
          goto set_diff;
      }
    }
  }

set_diff:
  diff = second_pick_mask == 0                             ? easy
         : result.best_priority + 4 < result.next_priority ? simple
                                                           : hard;
}

static void new_hand() {
  saved_hand s;
  std::pair<triple, saved_hand *> key;

  shuffle_hand(s.hand);
  difficulty diff;

  analyze_hand(s.hand, key.first, diff);
  key.second = 0;

  const std::pair<hand_map::iterator, bool> inserted = cache.insert(key);

  if (inserted.first->second == 0) {
    inserted.first->second = new saved_hand(s);

    if (inserted.second) {
      // This is a brand new element.
      // Put it on the normal list.
      lists.rand_hands[diff]->add(inserted.first);
    } else {
      // This element was added to the cache
      // by got_it_wrong.
      errors.add(inserted.first);
    }
  }
}

class init_rand {
 public:
  init_rand();
};

init_rand::init_rand() {
  extern void ranf_start(long seed);
  time_t seed;
  time(&seed);
  ranf_start(seed);
}

init_rand set_random_seed;

static triple last_hand;
int error_countdown = -1;

void got_it_wrong() {
  std::pair<triple, saved_hand *> key(last_hand,
                                      reinterpret_cast<saved_hand *>(0));
  cache.insert(key);
  // Put the last hand back on the list with a
  // null hand.  Eventually a real hand will be
  // placed there and it will go onto the errors list.
}

// The proportion of hands to be dealt */
static double diff_weight[max_difficulty] = {
    0.03,  // trivial
    0.15,  // easy
    0.20,  // simple
    0.62   // hard
};

void deal_hand(card *hand, const char *&name) {
  iter_list *source = 0;
  int j;

  // Generate some fresh hands to choose from
  for (j = 0; j < rand_batch; j++) {
    new_hand();
  }

  // Select a source of hands with the given distribution.
  // Handle the stupid, probability zero corner case
  // that some of the sources are empty.  They can't all
  // be empty, because we just generated rand_batch of them.
  {
    double total = 0.0;

    for (j = 0; j < max_difficulty; j++) {
      if (lists.rand_hands[j]->current_length) {
        total += diff_weight[j];
        source = lists.rand_hands[j];
        // Make sure it gets initialized to something
        // in case of floating-point oddities
      }
    }

    double rand = random() * total;

    double t2 = 0.0;
    for (j = 0; j < max_difficulty; j++) {
      if (lists.rand_hands[j]->current_length) {
        t2 += diff_weight[j];
        if (t2 >= rand) {
          source = lists.rand_hands[j];
          break;
        }
      }
    }
  }

  if (errors.current_length != 0) {
    switch (error_countdown) {
      case 0:
        source = &errors;
        error_countdown = -1;
        break;

      case -1:
        error_countdown = 10 + rand_int(5);
        break;

      default:
        error_countdown -= 1;
    }
  }

  _ASSERT(source);

  {
    const hand_map::iterator result = source->remove_random();
    last_hand = result->first;
    saved_hand *h = result->second;

    for (j = 0; j < 5; j++) {
      hand[j] = h->hand[j];
    }

    delete h;
    cache.erase(result);
  }

#if 0
	hand[0] = make_card (ace, 0);
	hand[1] = joker;
	hand[2] = make_card (three, 1);
	hand[3] = make_card (five, 1);
	hand[4] = make_card (seven, 2);
#endif

  // Figure out if we have dealt a paying hand,
  // and if so, which one.  This turns out to be
  // harder than one might think!

#if 1
  card sorted[5];
  int hand_size;

  sort_hand(hand, sorted, hand_size);
  const int power = 1 << hand_size;
  payoff_name best = N_nothing;

  for (unsigned m = 0; m < power; m++) {
    payoff_name p =
        kept_description(sorted, hand_size, m, *selected_game).name();

    if (p > best) best = p;
  }

  name = selected_game->pay_table[best] ? payoff_image[best] : 0;
#else
  {
    triple result;
    difficulty diff;
    analyze_hand(hand, result, diff);
    switch (diff) {
      case trivial:
        name = "no-brainer";
        break;
      case easy:
        name = "easy";
        break;
      case simple:
        name = "simple";
        break;
      case hard:
        name = "hard";
        break;
    }
  }
#endif
}
