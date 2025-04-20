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

      move_desc *this_move = global.get_move(rover - lines, rover);

      move_data md;
      md.move = global.get_move(rover - lines, rover);
      md.mask = matcher.matches[0];
      md.best = get_mask_value(matcher.matches[0], matcher, cache, deuces,
                               parms, global, left);
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
  }

  fclose(output);

  printf("\nstrategy written in %s\n", filename);

  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    exit(1);
  }
}

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
