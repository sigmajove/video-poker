#include <stdio.h>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

#include "combin.h"
#include "enum_match.h"
#include "find_order.h"
#include "game.h"
#include "kept.h"
#include "new_hand_iter.h"
#include "vpoker.h"

using std::vector;

int counter = 0;

struct move : public move_desc {
  char *the_name;
  char *name() { return the_name; }
  move(char *n) : the_name(n) {};
  bool operator<(const move &r) const {
    return strcmp(the_name, r.the_name) < 0;
  }
};

typedef std::set<move> move_set;

struct strategy_move : public move_desc {
  StrategyLine *s;
  char *name() { return s->image; };
};

struct estate {
  int trace_count;
  FILE *trace_file[2];
  StrategyLine *trace_line[2];

  MoveList strategy;

  move_set moves;  // for the string version

  std::vector<strategy_move *> movies;  // for the new version

  // Move more of the parameters here
  // if you have nothing better to do

  move *get_move(char *name);
  strategy_move *get_move(int line, StrategyLine *s);

  double multiplier;

  estate(int hand_size) : strategy(hand_size), trace_count(0) {};
};

move *estate::get_move(char *name) {
  // Create a template move and attempt to add it to the set.
  std::pair<move_set::iterator, bool> x = moves.insert(move(name));

  move *result = const_cast<move *>(&*(x.first));
  // result is either the move that was already there, or a copy
  // of the template.

  if (x.second) {
    // A new move was created.  Initialize it "for real";
    const int name_size = strlen(name) + 1;
    result->the_name = new char[name_size];
    strcpy_s(result->the_name, name_size, name);
    strategy.add_move(result);
  }

  return result;
};

strategy_move *estate::get_move(int line, StrategyLine *s) {
  strategy_move *result = movies[line];
  if (result == 0) {
    result = new strategy_move;
    result->s = s;
    result->line = line;
    movies[line] = result;
    strategy.add_move(result);
  }
  return result;
}

struct xxx {
  StrategyLine *s;
  double value;
  bool last;
  xxx(StrategyLine *ss, bool ll) : s(ss), last(ll), value(0.0) {}
  bool operator<(const xxx &r) const { return value > r.value; }
  // Use > instead of < to get sort in decreasing value
};

typedef std::vector<xxx> yyy;

static void e2d2(estate &global, hand_iter &h, yyy &strat, C_left &left,
                 game_parameters &parms, int wild_cards)

{
  enum_match matcher;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);
  matcher.wild_cards = wild_cards;
  matcher.parms = &parms;

  left.remove(matcher.hand, matcher.hand_size, wild_cards);

  bool found_match = false;

  for (yyy::iterator rover = strat.begin(); rover != strat.end(); ++rover) {
    matcher.find((*rover).s->pattern);
    if (matcher.match_count != 0) {
      found_match = true;

      kept_description kept(matcher.hand, matcher.hand_size, matcher.matches[0],
                            parms);

      pay_dist pays;
      kept.all_draws(wild_cards, left, pays);

      int total_pays = 0;
      double result = 0.0;

      for (int j = first_pay; j <= last_pay; j++) {
        const int pay = pays[j];
        total_pays += pay;
        result += (double)pay * parms.pay_table[j];
      }

      const double current_total = (double)total_pays;
      (*rover).value += global.multiplier * result / (double)total_pays;
    }

    if ((*rover).last && found_match) {
      break;
    }
  }

  left.replace(matcher.hand, matcher.hand_size, wild_cards);
}

static void sort_strat(estate &global, game_parameters &parms, C_left &left,
                       int wild_cards, FILE *output) {
  const int hand_size = 5 - wild_cards;

  const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

  const int total_hands = combin.choose(parms.deck_size, 5);

  // Build a structure that defines the partial strategy

  yyy partial;

  {
    for (int_map::iterator rover = global.strategy.move_index.begin();
         rover != global.strategy.move_index.end(); ++rover) {
      move_desc *m = rover->second;
      do {
        partial.push_back(
            xxx(dynamic_cast<strategy_move *>(m)->s, m->pop == 0));
        m = m->pop;

      } while (m);
    }
  }

  hand_iter iter(hand_size, parms.kind, wild_cards);
  while (!iter.done()) {
    const int mult = wmult * iter.multiplier();
    global.multiplier = (double)mult / double(total_hands);

    e2d2(global, iter, partial, left, parms, wild_cards);

    iter.next();
  }

  // Now sort the SCCs by value
  yyy::iterator rover = partial.begin();
  while (rover != partial.end()) {
    yyy::iterator r2 = rover;
    for (;;) {
      bool last = (*r2).last;
      ++r2;
      if (last) {
        break;
      }
    }

    std::sort<yyy::iterator>(rover, r2);
    rover = r2;
  }

  // Finally, print the strategy
  rover = partial.begin();
  bool prev_was_last = true;

  while (rover != partial.end()) {
    yyy::iterator next = rover;
    ++next;

    if (prev_was_last && !(*rover).last) {
      fprintf(output, "#begin conflict\n");
    }

    fprintf(output, "%s\n", (*rover).s->image);

    if ((*rover).last && !prev_was_last) {
      fprintf(output, "#end conflict\n");
    }

    prev_was_last = (*rover).last;
    ++rover;
  }
};

struct move_data {
  move_desc *move;
  unsigned char mask;
  double best, worst;
};

typedef std::vector<move_data> move_data_vector;

int trace_countdown = 500;

struct cache_entry {
  bool valid;
  double value;
  pay_dist pays;

  cache_entry() : valid(false) {};
};

typedef cache_entry eval_cache[1 << 5];

static void print_entry(FILE *f, cache_entry &e, game_parameters &parms) {
  fprintf(f, "%.6e: ", e.value);
  int nothing = 0;
  for (int j = first_pay; j <= last_pay; j++) {
    if (parms.pay_table[j] == 0.0) {
      nothing += e.pays[j];
    } else {
      fprintf(f, " %s=%d", short_payoff_image[j], e.pays[j]);
    }
  }

  if (nothing != 0) {
    fprintf(f, " %s=%d", short_payoff_image[N_nothing], nothing);
  }

  fprintf(f, "\n");
};

inline double get_mask_value(unsigned char mask, enum_match &matcher,
                             eval_cache &cache, int keep_deuces,
                             game_parameters &parms, estate &global,
                             C_left &left) {
  if (cache[mask].valid) {
    return cache[mask].value * global.multiplier;
  }

  pay_dist &pays = cache[mask].pays;

  kept_description(matcher.hand, matcher.hand_size, mask, parms)
      .all_draws(keep_deuces, left, pays);

  int total_pays = 0;
  double result = 0.0;

  for (int j = first_pay; j <= last_pay; j++) {
    const int pay = pays[j];
    total_pays += pay;
    result += (double)pay * parms.pay_table[j];
  }

  const double current_total = (double)total_pays;
  const double value = result / current_total;

  cache[mask].value = value;
  cache[mask].valid = true;

  return value * global.multiplier;
}

static void evaluate(estate &global, hand_iter &h, int deuces, C_left &left,
                     StrategyLine *lines, game_parameters &parms, FILE *file) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  const bool trace = false;

  left.remove(matcher.hand, matcher.hand_size, deuces);
  // Subtract the hand to be evaluated from the left structure

  {
    StrategyLine *rover = lines;
    for (;;) {
      unsigned char *pat = rover->pattern;
      char *img = rover->image;

      if ((img == 0) ^ (pat == 0)) {
        printf("image/pat mismatch\n");
        exit(0);
      }

      if (img == 0) {
        break;
      }

      rover += 1;
    }

    global.movies.resize(rover - lines);
  }

  // pay_dist strategy_pays;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.

  eval_cache cache;

  StrategyLine *rover = lines;
  // move_desc *good_move = 0;

  bool trace_good[2];
  trace_good[0] = false;
  trace_good[1] = false;

  bool trace_match[2];
  trace_match[0] = false;
  trace_match[1] = false;

  bool earlier_match = false;

  unsigned trace_mask[2];

  counter += 1;

  move_data_vector good_move;
  move_data_vector bad_move;
  double best_value = 0.0;

  bool simple_trace = false;

  while (rover->pattern) {
    matcher.find(rover->pattern);

    if (matcher.match_count != 0) {
      if (global.trace_count == 2) {
        if (earlier_match) {
          // Stop the trace
        } else if (global.trace_line[0] == rover) {
          trace_match[0] = true;
          trace_mask[0] = matcher.matches[0];
        } else if (global.trace_line[1] == rover) {
          trace_match[1] = true;
          trace_mask[1] = matcher.matches[0];
        } else {
          earlier_match = true;
        }
      }

      if (global.trace_count == 1 && global.trace_line[0] == rover) {
        simple_trace = true;
      }

      // move_desc *this_move = global.get_move (matcher.name (*rover));
      move_desc *this_move = global.get_move(rover - lines, rover);

      move_data md;
      md.move = global.get_move(rover - lines, rover);
      md.mask = matcher.matches[0];
      md.best = get_mask_value(matcher.matches[0], matcher, cache, deuces,
                               parms, global, left);
      // md.best = mask_value[matcher.matches[0]];
      md.worst = md.best;

      for (int j = 1; j < matcher.match_count; j++) {
        double d = get_mask_value(matcher.matches[j], matcher, cache, deuces,
                                  parms, global, left);

        if (d > md.best) {
          md.best = d;
        }
        if (d < md.worst) {
          md.worst = d;
          md.mask = matcher.matches[j];
        }
      }

      if (md.worst == best_value) {
        good_move.push_back(md);
      } else if (md.worst > best_value) {
        best_value = md.worst;

        for (move_data_vector::iterator rover = good_move.begin();
             rover != good_move.end(); rover++) {
          bad_move.push_back(*rover);
        }

        good_move.resize(0);
        good_move.push_back(md);

        trace_good[0] = false;
        trace_good[1] = false;
      } else {
        bad_move.push_back(md);
      }

      if (md.worst == best_value && global.trace_count == 2) {
        if (global.trace_line[0] == rover) {
          trace_good[0] = true;
        }
        if (global.trace_line[1] == rover) {
          trace_good[1] = true;
        }
      }
    }

    rover += 1;
  }

  if (trace_match[0] && trace_match[1] && trace_good[0] != trace_good[1]) {
    // These are the droid we are looking for.
    // Both strategy lines match, but the produce different
    // answers.  Note this situation in the appropiate trace
    // file.

    int me = trace_good[0] ? 0 : 1;
    FILE *tf = global.trace_file[me];

    print_move(tf, matcher.hand, matcher.hand_size,
               trace_mask[0] | trace_mask[1]);

    print_entry(tf, cache[trace_mask[me]], parms);
    print_entry(tf, cache[trace_mask[1 - me]], parms);
    fprintf(tf, "\n");
  }

  if (good_move.size() == 0) {
    throw 0;
  }

  if (simple_trace) {
    print_hand(global.trace_file[0], matcher.hand, matcher.hand_size);
    fprintf(global.trace_file[0], "%s\n\n", good_move[0].move->name());
  }

  for (move_data_vector::iterator b = bad_move.begin(); b != bad_move.end();
       b++) {
    move_data &g = good_move[0];
    global.strategy.add_conflict(g.move, (*b).move, g.worst - (*b).worst,
                                 matcher.hand, g.mask);
  }

  if (0) {
    print_hand(file, matcher.hand, matcher.hand_size);

    move_data_vector::iterator g = good_move.begin();
    { fprintf(file, "good: %s\n", (*g).move->name()); }
    for (move_data_vector::iterator b = bad_move.begin(); b != bad_move.end();
         b++) {
      fprintf(file, "bad: %s\n", (*b).move->name());
    }
    fprintf(file, "\n");
  }

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

void find_strategy(const vp_game &game, const char *filename,
                   StrategyLine *lines[], bool print_haas, bool print_value) {
  int counter = 0;
  int timer = 0;

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == NULL) {
    printf("fopen failed\n");
    throw 0;
  }

  game_parameters parms(game);
  C_left left(parms);

  fprintf(output, "%s\n", game.name);
  if (!print_haas) {
    fprintf(output, "eval\n");
  }
  fprintf(output, "\n");

  const int total_hands = combin.choose(parms.deck_size, 5);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    estate global(hand_size);
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    // Scan the strategy looking for trace directives */
    {
      StrategyLine *rover = lines[wild_cards];
      while (rover->pattern) {
        if (rover->options && strncmp(rover->options, " trace ", 7) == 0) {
          if (global.trace_count >= 2) {
            printf("Too many trace directives\n");
            throw 0;
          }

          global.trace_line[global.trace_count] = rover;

          char *filename = rover->options + 7;

          FILE **global_trace_file = &(global.trace_file[global.trace_count]);
          *global_trace_file = NULL;
          fopen_s(global_trace_file, filename, "w");

          if (*global_trace_file == NULL) {
            printf("Cannot create %s\n", filename);
            throw 0;
          }

          fprintf(*global_trace_file, "%s is correct\n", rover->image);

          global.trace_count += 1;
        }

        rover += 1;
      }
    }

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      int m = wmult * iter.multiplier();
      counter += m;

      global.multiplier =
          static_cast<double>(m) / static_cast<double>(total_hands);

      evaluate(global, iter, wild_cards, left, lines[wild_cards], parms,
               output);

      iter.next();
    }

    for (int j = 0; j < global.trace_count; j++) {
      fclose(global.trace_file[j]);
    }

    global.strategy.display(output, parms.number_wild_cards != 0, print_haas,
                            print_value);

    // global.strategy.sort_moves();
    // sort_strat(global, parms, left, wild_cards, output);
  }

  fclose(output);

  printf("\nstrategy written in %s\n", filename);

  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    exit(1);
  }
}

#if 0
class HandKind {
 public:
  double Match(const Card *card, int hand_size, C_left &left,
               game_parameters &parms);

 private:
  enum class Keep {
    kRoyalFlush,
    kStraightFlush,
    kQuads,
    kFullHouse,
    kFlush,
    kStraight,
    kTrips,
    kTwoPair,
    kPair,
    kRF4,
    kRF3,
    kRF2,
    kSF3,
    kSF3i,
    kSF3di,
    kSF4,
    kSF4i,
    kSF4di,
    kFlush4,
    kStraight4,
    kStraight4i,
    kNothing,
  };

  Keep keep_;
};
using HandKindSet = std::set<HandKind>;

double HandKind::Match(const Card *card, int hand_size, C_left &left,
                       game_parameters &parms) {
  const int num_jokers = 5 - hand_size;

  // multi[0] = always zero
  // multi[1] = the number of singletons
  // multi[2] = the number of pairs
  // multi[3] = the number of trips
  // multi[4] = the number of quads

  // multi[1] + 2*multi[2] + 3*multi[3] + 4*multi[4] + num_jokers == 5
  std::array<int, 5> multi;
  multi.fill(0);

  // If multi[n] != 0, m_denom[n] is the denomination
  // of one of the multies.
  std::array<int, 5> m_denom;
  m_denom.fill(-1);

  int high_denoms = 0;

  int previous_d = -1;
  int multi_count = 0;

  for (int i = 0; i < hand_size; ++i) {
    const card c = hand[j];
    const int s = suit(c);
    const int d = pips(c);

    if (d == previous_d) {
      multi_count += 1;
    } else {
      if (parms.is_high(d)) {
        high_denoms += 1;
      }

      if (multi_count != 0) {
        // Process one of the multiples
        multi[multi_count] += 1;
        m_denom[multi_count] = previous_d;
      }

      previous_d = d;
      multi_count = 1;
    }
  }
  if (multi_count != 0) {
    // Process one of the multiples
    multi[multi_count] += 1;
    m_denom[multi_count] = previous_d;
  }

  return 0.0;
}

static void need_name(estate &global, hand_iter &h, int deuces, C_left &left,
                      const HandKindSet &hand_kinds, game_parameters &parms,
                      FILE *file) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  const bool trace = false;

  left.remove(matcher.hand, matcher.hand_size, deuces);
  // Subtract the hand to be evaluated from the left structure

  // pay_dist strategy_pays;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.

  eval_cache cache;

  // move_desc *good_move = 0;

  bool trace_good[2];
  trace_good[0] = false;
  trace_good[1] = false;

  bool trace_match[2];
  trace_match[0] = false;
  trace_match[1] = false;

  bool earlier_match = false;

  unsigned trace_mask[2];

  counter += 1;

  move_data_vector good_move;
  move_data_vector bad_move;
  double best_value = 0.0;

  bool simple_trace = false;

  for (const HandKind kind : hand_kinds) {
    matcher.find(rover->pattern);

    if (matcher.match_count != 0) {
      // move_desc *this_move = global.get_move (matcher.name (*rover));
      move_desc *this_move = global.get_move(rover - lines, rover);

      move_data md;
      md.move = global.get_move(rover - lines, rover);
      md.mask = matcher.matches[0];
      md.best = get_mask_value(matcher.matches[0], matcher, cache, deuces,
                               parms, global, left);
      // md.best = mask_value[matcher.matches[0]];
      md.worst = md.best;

      for (int j = 1; j < matcher.match_count; j++) {
        double d = get_mask_value(matcher.matches[j], matcher, cache, deuces,
                                  parms, global, left);

        if (d > md.best) {
          md.best = d;
        }
        if (d < md.worst) {
          md.worst = d;
          md.mask = matcher.matches[j];
        }
      }

      if (md.worst == best_value) {
        good_move.push_back(md);
      } else if (md.worst > best_value) {
        best_value = md.worst;

        for (move_data_vector::iterator rover = good_move.begin();
             rover != good_move.end(); rover++) {
          bad_move.push_back(*rover);
        }

        good_move.resize(0);
        good_move.push_back(md);

        trace_good[0] = false;
        trace_good[1] = false;
      } else {
        bad_move.push_back(md);
      }

      if (md.worst == best_value && global.trace_count == 2) {
        if (global.trace_line[0] == rover) {
          trace_good[0] = true;
        }
        if (global.trace_line[1] == rover) {
          trace_good[1] = true;
        }
      }
    }
  }

  if (trace_match[0] && trace_match[1] && trace_good[0] != trace_good[1]) {
    // These are the droid we are looking for.
    // Both strategy lines match, but the produce different
    // answers.  Note this situation in the appropiate trace
    // file.

    int me = trace_good[0] ? 0 : 1;
    FILE *tf = global.trace_file[me];

    print_move(tf, matcher.hand, matcher.hand_size,
               trace_mask[0] | trace_mask[1]);

    print_entry(tf, cache[trace_mask[me]], parms);
    print_entry(tf, cache[trace_mask[1 - me]], parms);
    fprintf(tf, "\n");
  }

  if (good_move.size() == 0) {
    throw 0;
  }

  if (simple_trace) {
    print_hand(global.trace_file[0], matcher.hand, matcher.hand_size);
    fprintf(global.trace_file[0], "%s\n\n", good_move[0].move->name());
  }

  for (move_data_vector::iterator b = bad_move.begin(); b != bad_move.end();
       b++) {
    move_data &g = good_move[0];
    global.strategy.add_conflict(g.move, (*b).move, g.worst - (*b).worst,
                                 matcher.hand, g.mask);
  }

  if (0) {
    print_hand(file, matcher.hand, matcher.hand_size);

    move_data_vector::iterator g = good_move.begin();
    { fprintf(file, "good: %s\n", (*g).move->name()); }
    for (move_data_vector::iterator b = bad_move.begin(); b != bad_move.end();
         b++) {
      fprintf(file, "bad: %s\n", (*b).move->name());
    }
    fprintf(file, "\n");
  }

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

void create_strategy(const vp_game &game, char *filename,
                     const std::vector<std::set<HandKind>> &kinds,
                     bool print_haas, bool print_value) {
  int counter = 0;
  int timer = 0;

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == NULL) {
    printf("fopen failed\n");
    throw 0;
  }

  game_parameters parms(game);
  C_left left(parms);

  fprintf(output, "%s\n", game.name);
  if (!print_haas) {
    fprintf(output, "eval\n");
  }
  fprintf(output, "\n");

  const int total_hands = combin.choose(parms.deck_size, 5);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    estate global(hand_size);
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      int m = wmult * iter.multiplier();
      counter += m;

      global.multiplier =
          static_cast<double>(m) / static_cast<double>(total_hands);

      need_name(global, iter, wild_cards, left, kinds[wild_cards], parms,
                output);

      iter.next();
    }

    global.strategy.display(output, parms.number_wild_cards != 0, print_haas,
                            print_value);

    // global.strategy.sort_moves();
    // sort_strat(global, parms, left, wild_cards, output);
  }

  fclose(output);

  printf("\nstrategy written in %s\n", filename);

  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    exit(1);
  }
}

#endif
static void identify(hand_iter &h, int deuces, C_left &left,
                     game_parameters &parms, std::set<std::string> &moves,
                     FILE *file) {
  card hand[5];
  h.current(hand[0]);

  // Subtract the hand to be evaluated from the left structure.
  left.remove(hand, h.size(), deuces);

  // A list of all the best masks. Usually this vector is of size 1
  // ties are quite rare.
  vector<unsigned> best;
  best.reserve(32);
  double best_value = 0.0;

  const unsigned combos = 1 << h.size();
  for (unsigned mask = 0; mask < combos; ++mask) {
    kept_description kept(hand, h.size(), mask, parms);

    pay_dist pays;
    kept.all_draws(deuces, left, pays);

    int total_pays = 0;
    double result = 0.0;
    for (int j = first_pay; j <= last_pay; j++) {
      const int pay = pays[j];
      total_pays += pay;
      result += static_cast<double>(pay) * parms.pay_table[j];
    }
    const double value = total_pays ? result / total_pays : 0.0;
    if (best.empty()) {
      best.push_back(mask);
      best_value = value;
    } else if (value == best_value) {
      best.push_back(mask);
    } else if (value > best_value) {
      best.clear();
      best.push_back(mask);
      best_value = value;
    }
  }

  for (unsigned mask : best) {
    kept_description kept(hand, h.size(), mask, parms);
    moves.insert(kept.move_name());
  }

  // Restore left to its initial value.
  left.replace(hand, h.size(), deuces);
}

void draft(const vp_game &game, const char *filename) {
  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == NULL) {
    printf("fopen failed\n");
    throw 0;
  }
  fprintf(output, "%s\n", game.name);
  printf("Computing");

  int timer = 0;
  game_parameters parms(game);
  C_left left(parms);

  std::set<std::string> moves;
  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    estate global(hand_size);
    hand_iter iter(hand_size, parms.kind, wild_cards);

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      identify(iter, wild_cards, left, parms, moves, output);
      iter.next();
    }
  }

  for (const std::string &m : moves) {
    fprintf(output, "%s\n", m.c_str());
  }

  fclose(output);
  printf("\nstrategy written in %s\n", filename);
}
