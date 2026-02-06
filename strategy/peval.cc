#define _CRT_SECURE_NO_WARNINGS  // For Microsoft Visual Studio
#include <stdio.h>

#define NOMINMAX  // avoid blocking things like std::min
                  // and numeric_limits<int>::max()
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../shared/hand_iter.h"
#include "combin.h"
#include "enum_match.h"
#include "game.h"
#include "kept.h"
#include "pay_dist.h"
#include "vpoker.h"

using std::vector;

struct line_info {
  bool erroneous;
  double total_error;

  card worst_hand[5];
  unsigned char worst_hsize;
  double worst_shortfall;
  unsigned char worst_play;
  unsigned char optimal_play;
  card best_hand[5];
  unsigned char best_hsize;
  double best_shortfall;
  unsigned char best_play;

  line_info() {
    erroneous = false;
    total_error = 0.0;
    worst_shortfall = 0.0;
    best_shortfall = -1.0;
    best_hsize = 0;
  }
};

struct error_info {
  char *image;
  double error;
  card hand[5];
  unsigned char hsize;
  unsigned char worst_play;
  unsigned char optimal_play;
  card best_hand[5];
  unsigned char best_hsize;
  double best_shortfall;
  unsigned char best_play;

  error_info(line_info &l, char *im);
  bool operator<(const error_info &r) { return error > r.error; }
};

error_info::error_info(line_info &l, char *im)
    : image(im),
      error(l.total_error),
      hsize(l.worst_hsize),
      worst_play(l.worst_play),
      optimal_play(l.optimal_play),
      best_hsize(l.best_hsize),
      best_play(l.best_play) {
  for (int j = 0; j < hsize; j++) {
    hand[j] = l.worst_hand[j];
  }
  for (int j = 0; j < best_hsize; j++) {
    best_hand[j] = l.best_hand[j];
  }
}

typedef std::vector<error_info> error_list;

static const int max_trace = 10;

struct estate {
  double multiplier;
  double strategy_return;
  double optimal_return;
  line_info *strategy_info;
  int trace_count;
  FILE *trace_file[max_trace];
  StrategyLine *trace_line[max_trace];
};

static void evaluate(hand_iter &h, int deuces, C_left &left,
                     StrategyLine *lines, estate &e, game_parameters &parms) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);

  left.remove(matcher.hand, matcher.hand_size, deuces);
  // Subtract the hand to be evaluated from the left structure

  unsigned power = 1 << matcher.hand_size;

  unsigned optimal_mask;
  int optimal_deuces;

  double best_value = -1.0, strategy_value = -1.0;

  StrategyLine *best_strategy = lines;
  unsigned strategy_mask = 0;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.

  for (;;) {
    matcher.find(best_strategy->pattern);
    if (matcher.match_count != 0) {
      break;
    }

    best_strategy += 1;
  }

  unsigned mask;

  for (mask = 0; mask < power; mask++) {
    kept_description kept(matcher.hand, matcher.hand_size, mask, parms);
    // Build the description of subset of the hand
    // indicated by mask.

    // In real video poker games offered by casinos you never
    // disard a wild card.  But it's possible to concoct
    // pay tables where that is the right move.
    // (four deuces pays 0; natural royal plays Avagadro's Number)

    for (int keep_deuces = 0; keep_deuces <= deuces; keep_deuces++) {
      pay_dist pays;

      kept.all_draws(keep_deuces, left, pays);

      {
        int total_pays = 0;
        double result = 0.0;

        for (int j = first_pay; j <= last_pay; j++) {
          const int pay = pays[j];
          total_pays += pay;
          result += (double)pay * parms.pay_table[j];
        }

        const double current_total = (double)total_pays;
        const double value = result / current_total;

        if (keep_deuces == deuces && matcher.result_vector[mask]) {
          if (strategy_value < 0.0 || value < strategy_value) {
            strategy_value = value;
            strategy_mask = mask;
          }
        }

        if (value > best_value) {
          best_value = value;
          optimal_mask = mask;
          optimal_deuces = deuces;
        }
      }
    }
  }

  _ASSERT(best_value >= 0);
  _ASSERT(strategy_value >= 0);

  const double mb = e.multiplier * best_value;
  e.optimal_return += mb;

  const double ms = e.multiplier * strategy_value;
  e.strategy_return += ms;

  const double shortfall = mb - ms;
  _ASSERT(shortfall >= 0.0);

  line_info &inf = e.strategy_info[best_strategy - lines];

  if (inf.best_shortfall < 0.0 || shortfall < inf.best_shortfall) {
    inf.best_shortfall = shortfall;
    inf.best_hsize = matcher.hand_size;
    inf.best_play = strategy_mask;
    for (int j = 0; j < matcher.hand_size; j++) {
      inf.best_hand[j] = matcher.hand[j];
    }
  }

  if (shortfall > 0.0) {
    // Allocate the shortfall to the strategy line chosen,
    // and record other information about that line.

    inf.total_error += shortfall;

    if (!inf.erroneous || shortfall > inf.worst_shortfall) {
      inf.erroneous = true;
      inf.worst_shortfall = shortfall;
      inf.worst_hsize = matcher.hand_size;
      inf.optimal_play = optimal_mask;
      inf.worst_play = strategy_mask;

      for (int j = 0; j < matcher.hand_size; j++) {
        inf.worst_hand[j] = matcher.hand[j];
      }
    }

    for (int j = 0; j < e.trace_count; j++) {
      if (e.trace_line[j] == best_strategy) {
        print_move(e.trace_file[j], matcher.hand, matcher.hand_size,
                   optimal_mask);
      }
    }
  }

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

static void print_detail(FILE *file, const card *hand, int hand_size,
                         unsigned mask, C_left &left, game_parameters &parms) {
  const int deuces = 5 - hand_size;
  left.remove(hand, hand_size, deuces);
  kept_description kept(hand, hand_size, mask, parms);
  pay_dist pays;
  kept.all_draws(deuces, left, pays);

  int total_pays = 0;
  for (int j = first_pay; j <= last_pay; j++) {
    total_pays += pays[j];
  }
  const double current_total = (double)total_pays;

  double result = 0.0;
  for (int j = first_pay; j <= last_pay; j++) {
    const int pay = pays[j];
    if (pay > 0 && parms.pay_table[j] != 0) {
      fprintf(file, "%8d %8.6f %s\n", pay,
              pay * parms.pay_table[j] / current_total, payoff_image[j]);
      result += (double)pay * parms.pay_table[j];
    }
  }
  const double value = result / current_total;
  fprintf(file, "Total %8.6f\n", value);
  left.replace(hand, hand_size, deuces);
}

void eval_strategy(const vp_game &game, StrategyLine *lines[],
                   const char *filename) {
  int counter = 0;
  int timer = 0;

  game_parameters parms(game);
  C_left left(parms);

  const int total_hands = combin.choose(parms.deck_size, 5);

  estate e;
  e.optimal_return = 0.0;
  e.strategy_return = 0.0;

  printf("Evaluating strategy for %s\n", game.name);

  error_list error_report;

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  fprintf(output, "Evaluating strategy for %s\n", game.name);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    StrategyLine *strategy_w = lines[wild_cards];

    // Scan the strategy looking for trace directives */
    {
      e.trace_count = 0;
      StrategyLine *rover = strategy_w;
      while (rover->pattern) {
        if (rover->options && strncmp(rover->options, " trace ", 7) == 0) {
          if (e.trace_count >= max_trace) {
            printf("Too many trace directives\n");
            throw 0;
          }

          e.trace_line[e.trace_count] = rover;

          char *filename = rover->options + 7;

          e.trace_file[e.trace_count] = fopen(filename, "w");

          if (e.trace_file[e.trace_count] == NULL) {
            printf("Cannot create %s\n", filename);
            throw 0;
          }

          fprintf(e.trace_file[e.trace_count], "%s errors\n", rover->image);

          e.trace_count += 1;
        }

        rover += 1;
      }
    }

    // Allocate structure for strategy eval
    {
      StrategyLine *rover = strategy_w;

      while (rover->pattern) {
        rover += 1;
      }
      std::size_t n = rover - lines[wild_cards];
      e.strategy_info = new line_info[n];
    }

    hand_iter iter(hand_size, parms.kind, wild_cards);
    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      const int mult = wmult * iter.multiplier();
      e.multiplier = (double)mult / double(total_hands);

      evaluate(iter, wild_cards, left, strategy_w, e, parms);
      counter += mult;

      iter.next();
    }

    for (int j = 0; j < e.trace_count; j++) {
      fclose(e.trace_file[j]);
    }

    // Print out errors in the strategy and their cost.

    {
      bool header_printed = false;

      for (int j = 0; strategy_w[j].pattern; j++) {
        line_info &inf = e.strategy_info[j];

        if (inf.erroneous) {
          error_report.push_back(error_info(inf, strategy_w[j].image));
#if 0
          if (!header_printed) {
            header_printed = true;

            if (parms.number_wild_cards) {
              fprintf (output, "Errors with %d wild cards\n\n", wild_cards);
            } else {
              fprintf (output,"Errors in strategy\n\n");
            }
          }

          fprintf (output,"Move: %s\n", strategy_w[j].image);
          fprintf (output,"Error: %0.8f\n", 100.0 * inf.total_error);

          if (inf.best_shortfall >= 0) {
          fprintf (output,"\nBad play is\n");
          print_move(output, inf.worst_hand,
                     inf.worst_hsize, inf.worst_play);
          }

          fprintf (output,"\nBad play is\n");
          print_move(output, inf.worst_hand,
                     inf.worst_hsize, inf.worst_play);

          fprintf (output,"Optimal play is\n");
          print_move(output, inf.worst_hand, inf.worst_hsize,
                     inf.optimal_play);
          fprintf (output,"\n");
#endif
        }
      }
    }
  }
  printf("\n");

  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    throw 0;
  }

  std::sort<error_list::iterator>(error_report.begin(), error_report.end());

  if (error_report.begin() == error_report.end()) {
    fprintf(output, "The strategy contains no errors\n");
  } else {
    fprintf(output, "Errors in strategy\n\n");
  }

  for (error_list::iterator rover = error_report.begin();
       rover != error_report.end(); ++rover) {
    error_info &inf = *rover;

    fprintf(output, "Move: %s\n", inf.image);
    fprintf(output, "Error: %0.8f\n", 100.0 * inf.error);

    fprintf(output, "\nGood play is\n");
    print_move(output, inf.best_hand, inf.best_hsize, inf.best_play);
    print_detail(output, inf.best_hand, inf.best_hsize, inf.best_play, left,
                 parms);

    fprintf(output, "\nBad play is\n");
    print_move(output, inf.hand, inf.hsize, inf.worst_play);
    print_detail(output, inf.hand, inf.hsize, inf.worst_play, left, parms);

    fprintf(output, "Optimal play is\n");
    print_move(output, inf.hand, inf.hsize, inf.optimal_play);
    print_detail(output, inf.hand, inf.hsize, inf.optimal_play, left, parms);
    fprintf(output, "\n");
  };

  printf("The optimal return is %0.8f%%\n", 100.0 * e.optimal_return);

  printf("This strategy returns %0.8f%%\n", 100.0 * e.strategy_return);

  fprintf(output, "The optimal return is %0.8f%%\n", 100.0 * e.optimal_return);

  fprintf(output, "This strategy returns %0.8f%%\n", 100.0 * e.strategy_return);

  fclose(output);
  printf("Report is in %s\n", filename);
}

static double evaluate_play(card *hand, int hand_size, bool *result_vector,
                            int deuces, C_left &left, game_parameters &parms) {
  double strategy_value = -1.0;
  const unsigned power = 1 << hand_size;
  for (unsigned mask = 0; mask < power; mask++) {
    if (!result_vector[mask]) continue;

    // Build the description of subset of the hand indicated by mask.
    kept_description kept(hand, hand_size, mask, parms);

    pay_dist pays;
    kept.all_draws(deuces, left, pays);

    // Compute the expected value of this play.
    int total_pays = 0;
    double result = 0.0;
    for (int j = first_pay; j <= last_pay; j++) {
      const int pay = pays[j];
      total_pays += pay;
      result += (double)pay * parms.pay_table[j];
    }

    const double value = result / (double)total_pays;

    // If the strategy line could select more than one mask, keep the worst one.
    if (strategy_value < 0.0 || value < strategy_value) {
      strategy_value = value;
    }
  }
  return strategy_value;
}

// mult is the number of different ways the starting hand can be dealt.
static PayDistribution evaluate_multi(const hand_iter &h, int deuces,
                                      C_left &left, StrategyLine *lines,
                                      game_parameters &parms) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);

  // Subtract the hand to be evaluated from the left structure
  left.remove(matcher.hand, matcher.hand_size, deuces);

  // Find the first matching strategy line.
  // Since all strategies end with "nothing", there will be one.
  for (StrategyLine *matching = lines;; ++matching) {
    matcher.find(matching->pattern);
    if (matcher.match_count != 0) {
      break;
    }
  }

  // There will typically be only one set of matching cards.
  // In a few cases (one pair in Full Pay Deuces) there may be
  // more then one. In good strategies, it won't matter which
  // combination we pick.
  kept_description kept(matcher.hand, matcher.hand_size, matcher.matches[0],
                        parms);
  pay_dist pays;
  kept.all_draws(deuces, left, pays);
  // pays is now the number of ways of making each of the possible
  // paying combinations (including "nothing", which pays zero).

  // Compute the total number of pays, which we can use to convert
  // counts to probabilities.
  const double num_pays = static_cast<double>(
      std::accumulate(pays + first_pay, pays + last_pay + 1, 0));

  // Convert a pay_dist to a PayDistribution (unfortunate naming!)
  std::vector<ProbPay> pd;
  for (std::size_t j = first_pay; j <= last_pay; j++) {
    const int frequency = pays[j];
    if (frequency > 0) {
      const double probability = frequency / num_pays;
      pd.emplace_back(probability, parms.pay_table[j]);
    }
  }
  left.replace(matcher.hand, matcher.hand_size, deuces);

  // Stop keeping track of details after three royal flushes.
  PayDistribution dist(5 * parms.pay_table[N_royal_flush], 0, pd);
  dist.normalize();
  return dist;
}

void multi_distribution(const vp_game &game, StrategyLine *lines[],
                        unsigned int num_lines, unsigned int num_games,
                        const char *filename) {
  char buffer[256];
  GetCurrentDirectory(256, buffer);
  printf("Current dir %s\n", buffer);

  game_parameters parms(game);
  C_left left(parms);

  const double total_hands = combin.choose(parms.deck_size, 5);

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == 0) {
    throw std::runtime_error(std::format("Could not open {}", filename));
  }

  fprintf(output, "Multi %s with %u lines and %u games\n", game.name, num_lines,
          num_games);

  printf("Computing");

  int counter = 0;
  int timer = 0;

  PayDistribution total_pays;

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;

    // wide_mult is the number of different ways of being dealt "wild_cards"
    // wild cards.
    const int wide_mult = combin.choose(parms.number_wild_cards, wild_cards);

    for (hand_iter iter(hand_size, parms.kind, wild_cards); !iter.done();
         iter.next()) {
      if (++timer > 2558) {
        printf(".");
        timer = 0;
      }
      PayDistribution dist = repeat(
          evaluate_multi(iter, wild_cards, left, lines[wild_cards], parms),
          num_lines);

      // Compute the probability of the starting hand.
      const int mult = wide_mult * iter.multiplier();
      const double start_prob = mult / total_hands;

      // Adjust the pay distribution by this probability.
      dist.scale(start_prob);
      total_pays = merge(total_pays, dist);

      counter += mult;
    }
  }
  printf("\n");
  if (counter != total_hands) {
    throw std::runtime_error("Iteration counter wrong\n");
  }

  for (const auto &[prob, pay] : total_pays.distribution()) {
#if 0
    printf("prob %.6f, pay %d\n", prob, pay);
#else
    std::cout << "prob " << std::hexfloat << prob << " pay " << pay << "\n";
#endif
  }

  const double payback = total_pays.expected();
  const double percent = 100 * payback / num_lines;
  fprintf(output, "Payback %.6f%%\n", percent);
  printf("Payback %.6f%%\n", percent);

  double variance = 0.0;
  for (const auto &[prob, pay] : total_pays.distribution()) {
    const double delta = payback - pay;
    variance += prob * delta * delta;
  }

  // Not sure why we maintain a cutoff for the multi distribution.
  printf("cutoff prob %.6e\n", total_pays.cutoff_prob());
  const double delta = payback - total_pays.cutoff();
  variance += total_pays.cutoff_prob() * delta * delta;

  fprintf(output, "Variance = %.4f\n", variance);
  printf("Variance = %.4f\n", variance);

  // The actual cutoff we will use is based on the number of games.
  total_pays.set_cutoff(num_games + 2001);
  total_pays = repeat(total_pays, num_games);

  // Create cumulative distribution./
  std::vector<ProbPay> cumulative;
  cumulative.reserve(100);

  const unsigned int num_bets = num_lines * num_games;
  auto [worst_prob, worst_outcome] = total_pays.distribution().front();
  printf("prob %.10e worst %d\n", worst_prob, worst_outcome - num_bets);

  auto [best_prob, best_outcome] = total_pays.distribution().back();
  printf("prob %.10e best %d\n", best_prob, best_outcome - num_bets);

  printf("prob %.10e cutoff %d\n", total_pays.cutoff_prob(),
         total_pays.cutoff() - num_bets);

  double total_prob = 0;
  for (const auto &[prob, pay] : total_pays.distribution()) {
    total_prob += prob;
    if (total_prob >= 0.001) {
      printf("lower %.2f %d\n", total_prob * 100, pay - num_bets);
      break;
    }
  }

  total_prob = 0.0;
  int bracket = 1;
  double limit = static_cast<double>(bracket) / 100;
  for (const auto &[prob, pay] : total_pays.distribution()) {
    total_prob += prob;
    if (total_prob >= limit) {
      cumulative.emplace_back(total_prob, static_cast<int>(pay - num_bets));
      limit = static_cast<double>(++bracket) / 100;
    }
  }
  total_prob += total_pays.cutoff_prob();
  if (total_prob >= limit) {
    cumulative.emplace_back(total_prob,
                            static_cast<int>(total_pays.cutoff() - num_bets));
    limit = static_cast<double>(++bracket) / 100;
  }
#if 0
  cumulative.emplace_back(
      1.0, static_cast<int>(total_pays.back().payoff - num_bets));
#endif

  fprintf(output, "distribution = [\n");
  for (const auto &[prob, pay] : cumulative) {
    fprintf(output, "(%.8e,%5d),\n", prob, pay);
  }
  fprintf(output, "]\n");

  fclose(output);
  printf("Output is in %s\n", filename);
}

typedef std::map<std::pair<int, int>, double> prune_data;

static void evaluate_for_prune(hand_iter &h, int deuces, C_left &left,
                               StrategyLine *lines, std::size_t strategy_length,
                               game_parameters &parms, double multiplier,
                               prune_data &accum) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);

  // Subtract the hand to be evaluated from the left structure
  left.remove(matcher.hand, matcher.hand_size, deuces);

  struct {
    int index;
    bool result_vector[32];
  } plays[2];

  int i = 0;
  for (int k = 0; lines[k].pattern; ++k) {
    matcher.find(lines[k].pattern);
    if (matcher.match_count != 0) {
      if (i == 1) {
        bool same = true;
        for (int j = 0; j < 32; ++j) {
          same = same && matcher.result_vector[j] == plays[0].result_vector[j];
        }
        if (same) continue;
      }
      plays[i].index = k;
      for (int j = 0; j < 32; ++j) {
        plays[i].result_vector[j] = matcher.result_vector[j];
      }
      i += 1;
      if (i == 2) break;
    }
  }

  if (i == 2) {
    double values[2];
    card xhand[5];
    h.current(xhand[0]);
    for (int j = 0; j < 2; ++j) {
      values[j] = evaluate_play(xhand, h.size(), plays[j].result_vector, deuces,
                                left, parms);
    }
    const double delta = (values[0] - values[1]) * multiplier;
    accum[std::make_pair(plays[0].index, plays[1].index)] += delta;
  }

  // Undo the remove at the beginning of the routine.
  left.replace(matcher.hand, matcher.hand_size, deuces);
}

struct sort_compare {
  bool operator()(prune_data::const_iterator lhs,
                  prune_data::const_iterator rhs) const {
    return lhs->second < rhs->second;
  }
};

void prune_strategy(const vp_game &game, StrategyLine *lines[],
                    std::size_t *strategy_length, const char *filename) {
  int counter = 0;
  int timer = 0;

  game_parameters parms(game);
  C_left left(parms);

  const int total_hands = combin.choose(parms.deck_size, 5);

  printf("Pruning strategy for %s\n", game.name);

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  fprintf(output, "Pruning strategy for %s\n", game.name);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    StrategyLine *strategy_w = lines[wild_cards];
    std::size_t strategy_l = strategy_length[wild_cards];

    prune_data accum;

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      const int mult = wmult * iter.multiplier();
      double multiplier = (double)mult / double(total_hands);

      evaluate_for_prune(iter, wild_cards, left, strategy_w, strategy_l, parms,
                         multiplier, accum);
      counter += mult;

      iter.next();
    }

    fprintf(output, "Least useful rules for %d wild\n", wild_cards);
    std::vector<prune_data::const_iterator> result;
    for (prune_data::const_iterator iter = accum.begin(); iter != accum.end();
         ++iter) {
      result.push_back(iter);
    }
    std::sort(result.begin(), result.end(), sort_compare());
    int limit = std::min(25, static_cast<int>(result.size()));
    for (int i = 0; i < limit; ++i) {
      fprintf(output, "%s vs %s: %8.5e\n",
              strategy_w[result[i]->first.first].image,
              strategy_w[result[i]->first.second].image, result[i]->second);
    }
  }
  printf("\n");

  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    throw 0;
  }

  fclose(output);
  printf("Report is in %s\n", filename);
}

typedef double prob_vector[last_pay + 1];

static void evaluate(hand_iter &h, int deuces, C_left &left,
                     game_parameters &parms, double multiplier,
                     prob_vector &prob_pays) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  card hand[5];

  h.current(hand[0]);
  const int hand_size = 5 - deuces;

  left.remove(hand, hand_size, deuces);
  // Subtract the hand to be evaluated from the left structure

  unsigned power = 1 << hand_size;

  double best_value = -1.0;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.

  unsigned optimal_mask;
  unsigned optimal_deuces;

  for (unsigned mask = 0; mask < power; mask++) {
    kept_description kept(hand, hand_size, mask, parms);
    // Build the description of subset of the hand
    // indicated by mask.

    // In real video poker games offered by casinos you never
    // disard a wild card.  But it's possible to concoct
    // pay tables where that is the right move.
    // (four deuces pays 0; natural royal pays Avogadro's Number)

    for (int keep_deuces = 0; keep_deuces <= deuces; keep_deuces++) {
      pay_dist pays;

      kept.all_draws(keep_deuces, left, pays);
      {
        int total_pays = 0;
        double value = 0.0;

        for (int j = first_pay; j <= last_pay; j++) {
          const int pay = pays[j];
          total_pays += pay;
          value += (double)pay * parms.pay_table[j];
        }

        value /= (double)total_pays;

        if (value > best_value) {
          best_value = value;
          optimal_mask = mask;
          optimal_deuces = deuces;
        }
      }
    }
  }

  static int counter = 0;
  counter += 1;

  kept_description kept(hand, hand_size, optimal_mask, parms);
  pay_dist pays;

  kept.all_draws(optimal_deuces, left, pays);

  double scale_factor =
      multiplier / static_cast<double>(combin.choose(
                       parms.deck_size - 5, kept.number_of_discards()));

  int total_pays = 0;

  for (int j = first_pay; j <= last_pay; j++) {
    total_pays += pays[j];
    if (pays[j]) {
      prob_pays[j] += scale_factor * static_cast<double>(pays[j]);
    }
  }

#if 0
  static int limit = 2000;
  if (limit > 0) {
    limit -= 1;
    fprintf (trace_file, "counter = %d\n", counter);
    print_move(trace_file, hand, hand_size, optimal_mask);
    for (int j = first_pay; j <= last_pay; j++) if (parms.pay_table[j] != 0.0) {
        fprintf (trace_file, "%d ", pays[j]);
      }
    fprintf (trace_file, "\n");

  }
#endif

  _ASSERT(total_pays ==
          combin.choose(parms.deck_size - 5, kept.number_of_discards()));

  left.replace(hand, hand_size, deuces);
}

void eval_game(const vp_game &game, prob_vector &prob_pays) {
  int counter = 0;
  int timer = 0;

  game_parameters parms(game);
  C_left left(parms);

  printf("Evaluating optimal return for %s\n", game.name);

  for (int j = first_pay; j <= last_pay; j++) {
    prob_pays[j] = 0.0;
  }

  const int total_hands = combin.choose(parms.deck_size, 5);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      const int mult = wmult * iter.multiplier();

      evaluate(iter, wild_cards, left, parms,
               static_cast<double>(mult) / static_cast<double>(total_hands),
               prob_pays);

      counter += mult;

      iter.next();
    }
  }

  printf("\n");
  if (counter != combin.choose(parms.deck_size, 5)) {
    printf("Iteration counter wrong\n");
    throw 0;
  }
}

typedef struct {
  double abs, rel;
} ppair;

// outcome is a file-scope linked list. Allocating a new outcome object
// pushes it onto the front of the list.
struct outcome {
  const char *name;
  double abs, rel;
  ppair *const prob;  // pointer to a dynamic array of ppairs

  outcome *next;
  static outcome *first;

  outcome(const char *n, int goal)
      : name(n), prob(new ppair[goal]), next(first) {
    first = this;
  }
  ~outcome() { delete[] prob; }
};

// Initialize the list of outcomes to empty.
outcome *outcome::first = nullptr;

// Free all the outcomes that have been allocated.
static void free_outcomes() {
  while (outcome::first) {
    outcome *const next = outcome::first->next;
    delete outcome::first;
    outcome::first = next;
  }
}

// The pay tables were defined as doubles, since I guess one could imagine a
// game that pays a nonintegral fraction of a bet.  However, a lot of the
// algorithms in this file assume a payout is integral. This function converts a
// double to an int checking that the double really is an int.

static int double_to_int(double x) {
  _ASSERT(floor(x) == x);
  return static_cast<int>(x);
}

void median_length(FILE *output, game_parameters &game, prob_vector &prob_pays,
                   int starting_bankroll, int goal) {
  double *current = new double[goal];
  double *next = new double[goal];

  for (int j = 0; j < goal; j++) {
    current[j] = 0.0;
  }
  current[starting_bankroll] = 1.0;

  int game_number;

  for (game_number = 1;; game_number++) {
    for (int j = 1; j < goal; j++) {
      next[j] = 0.0;
    }
    next[0] = current[0];

    for (int bankroll = 1; bankroll < goal; bankroll++) {
      for (int p = first_pay; p <= last_pay; p++) {
        double pp = prob_pays[p];
        if (pp != 0.0) {
          pp *= current[bankroll];

          int new_roll = bankroll - 1 + double_to_int(game.pay_table[p]);
          _ASSERT(new_roll >= 0);
          if (new_roll >= goal) {
            new_roll = 0;
          }
          // next[0] is the probability that we stop,
          // either because we ran out of money or because
          // we doubled it.

          next[new_roll] += pp;
        }
      }
    }

    if (next[0] >= 0.5) {
      break;
    }

    {
      double *temp = next;
      next = current, current = temp;
    }
  }

  // next[0] is the probability that we will stop after game_number games.
  // current[0] is the probability that we will stop after game_number-1 games.
  // Which is closer to .5?

  _ASSERT(current[0] < 0.5);

  if (0.5 - current[0] > next[0] - 0.5) {
    game_number -= 1;
  }

  fprintf(output, "Median number of games: %d\n", game_number);

  delete current;
  delete next;
}

static double ror_func(double r, prob_vector &prob_pays,
                       game_parameters &game) {
  double result = -r;
  for (int j = first_pay; j <= last_pay; j++) {
    result += prob_pays[j] * pow(r, game.pay_table[j]);
  }

  return result;
}

void risk_of_ruin(FILE *output, game_parameters &game, prob_vector &prob_pays) {
  double delta = 0.45;

  double lo = 1.0 - 2 * delta;
  double hi = 1.0 - delta;

  double f_lo = ror_func(lo, prob_pays, game);
  double f_hi = ror_func(hi, prob_pays, game);

  for (;;) {
    if ((f_lo < 0.0) != (f_hi < 0.0)) {
      break;
    }

    delta = 0.5 * delta;

    if (delta == 0.0) {
      fprintf(output, "couldn't find initial interval\n");
      return;
    }

    lo = hi;
    f_lo = f_hi;

    hi = 1.0 - delta;
    f_hi = ror_func(hi, prob_pays, game);
  }

  for (int j = 0; j < 500; j++) {
    double mid = 0.5 * (lo + hi);

    if (mid == lo || mid == hi) {
      if (fabs(f_hi) < fabs(f_lo)) {
        lo = hi;
      }
      break;
    }

    double f_mid = ror_func(mid, prob_pays, game);

    if ((f_lo < 0.0) == (f_mid < 0.0)) {
      lo = mid;
      f_lo = f_mid;
    } else {
      hi = mid;
      f_hi = f_mid;
    }
  }

  double log_r = log(lo);

  fprintf(output, " 1%% RoR = $%.2f\n", 1.25 * log(.01) / log_r);
  fprintf(output, "10%% RoR = $%.2f\n", 1.25 * log(.10) / log_r);
  fprintf(output, "50%% RoR = $%.2f\n", 1.25 * log(.50) / log_r);
}

double bust_prob(game_parameters &game, prob_vector &prob_pays,
                 int initial_bankroll, int goal) {
  if (goal <= initial_bankroll) {
    return 0.0;
  }
  _ASSERT(goal >= 2);
  vector<double> rel_vec(goal, 0.0);

  for (int j = goal - 1; j > 0; j--) {
    double push = 0.0;
    double smaller = 0.0;

    for (int k = 0; k <= last_pay; k++) {
      const double prob = prob_pays[k];
      const int pay = double_to_int(game.pay_table[k]);

      if (pay == 0) {
        // We drop down to a bankroll of j-1
        smaller += prob;
      } else if (pay == 1) {
        push += prob;
      } else {
        const int bigger = j + pay - 1;
        if (bigger < goal) {
          push += prob * rel_vec[bigger];
        }
      }
    }

    const double not_push = 1.0 - push;
    smaller /= not_push;

    rel_vec[j] = smaller;

    for (int m = goal - 1; m > j; m--) {
      rel_vec[m] *= smaller;
    }
  }

  const double result = rel_vec[initial_bankroll];

  return result;
}

int break_even_goal(game_parameters &game, prob_vector &prob_pays,
                    int initial_bankroll) {
  // Find a goal for which the bust probability is > 0.5
  int winnings = 1;
  while (bust_prob(game, prob_pays, initial_bankroll,
                   initial_bankroll + winnings) <= 0.5) {
    winnings *= 2;
  }

  // Use binary search to find the largest goal
  // whose bust probability is <= 0.5
  int left = initial_bankroll;              // bust prob == 0
  int right = initial_bankroll + winnings;  // bust_prob > 0.5
  // note: left < right
  for (;;) {
    // Invariant: bust probability of left is <= 0.5
    //            bust probablity of right is > 0.5
    const int mid = (left + right) / 2;
    if (mid == left) break;
    if (bust_prob(game, prob_pays, initial_bankroll, mid) <= 0.5) {
      left = mid;
    } else {
      right = mid;
    }
  }
  return left;
}

void eval_bankroll(FILE *output, game_parameters &game, prob_vector &prob_pays,
                   int initial_bankroll, int goal) {
  _ASSERT(goal >= initial_bankroll);
  _ASSERT(outcome::first == NULL);

  char goal_name[20];
  if (goal % 4 == 0) {
    sprintf(goal_name, "Reach $%d", (goal / 4 * 5));
  } else {
    sprintf(goal_name, "Reach $%.2f", static_cast<double>(goal) / 4.0 * 5.0);
  }

  // Find the max payout in the pay table.
  int max_pay = 0;
  for (int j = first_pay; j <= last_pay; j++) {
    if (game.pay_table[j] >= max_pay) {
      max_pay = double_to_int(game.pay_table[j]);
    }
  }

  _ASSERT(max_pay >= 2);

  // Create a separate outcome for each of the values
  // by which we can hit when we reach the goal.
  //
  vector<outcome *> reach_goal;
  reach_goal.reserve(max_pay - 1);
  for (int j = 0; j < max_pay - 1; j++) {
    reach_goal.push_back(new outcome(0, goal));
  }

  outcome *dvect[last_pay + 1];
  outcome *bust = new outcome("Bust", goal);
  outcome *hard_double = new outcome(goal_name, goal);
  outcome *rover;

  // Count the number of pays that push you over the top immediately
  for (int j = first_pay; j <= last_pay; j++) {
    dvect[j] =
        game.pay_table[j] >= goal ? new outcome(payoff_image[j], goal) : 0;
  }

  for (int j = goal - 1; j > 0; j--) {
    // Invariant:
    // Let P be the chance of going bust with j bets.
    // Then for x > j, the chance of going bust with x
    // bets is bust[x].abs + P * bust[x].rel.

    // Compute P

    for (rover = outcome::first; rover; rover = rover->next) {
      rover->abs = 0.0;
      rover->rel = 0.0;
    }

    double smaller = 0.0;

    for (int k = 0; k <= last_pay; k++) {
      const double prob = prob_pays[k];
      const int pay = double_to_int(game.pay_table[k]);

      if (pay == 0) {
        // We drop down to a bankroll of j-1
        smaller += prob;
      } else if (pay == 1) {
        for (rover = outcome::first; rover; rover = rover->next) {
          rover->rel += prob;
        }
      } else {
        const int bigger = j + pay - 1;

        if (bigger >= goal) {
          (dvect[k] ? dvect[k] : hard_double)->abs += prob;

          _ASSERT(bigger - goal < max_pay - 1);
          reach_goal[bigger - goal]->abs += prob;
        } else {
          for (rover = outcome::first; rover; rover = rover->next) {
            // Add in the chances of getting the corresponding
            // outcome with the bigger bankroll
            rover->abs += prob * rover->prob[bigger].abs;
            rover->rel += prob * rover->prob[bigger].rel;
          }
        }
      }
    }

    // Let P2 = prob of going bust with bankroll j-1.
    // We have P = abs + rel * P + smaller * P2
    // Solving for P we have
    //
    // P - rel * P = abs + smaller * P2
    // P = abs / (1.0 - rel) + smaller / (1.0 - rel) * P2

    for (rover = outcome::first; rover; rover = rover->next) {
      const double not_rel = 1.0 - rover->rel;
      const double abs = rover->abs / not_rel;
      const double rel = smaller / not_rel;

      rover->prob[j].abs = abs;
      rover->prob[j].rel = rel;

      // Now to reestablish the invariant, substitute abs + rel * P2
      // for P1 in the equation
      //  bust[m].abs + bust[m].rel * P1, for m in the range j+1..goal-1

      for (int m = goal - 1; m > j; m--) {
        rover->prob[m].abs += rover->prob[m].rel * abs;
        rover->prob[m].rel *= rel;
      }
    }
  }

  // The probability of going bust with a bankroll of zero is 1.0.
  // Therefore the probability of going bust with bankroll of x > 0
  // is bust[x].rel + bust[x].abs

  if (initial_bankroll == 0) {
    // Compute the initial bankroll that gives a 50=50 chance
    // of going bust

    double prev = 0.0;
    double sum;

    for (int j = 1;; j++) {
      sum = bust->prob[j].abs + bust->prob[j].rel;

      if (sum <= 0.5 || j == goal - 1) {
        if (j > 1 && prev - 0.5 > 0.5 - sum) {
          j -= 1;
        }
        initial_bankroll = j;
        break;
      }
      prev = sum;
    }
  }

  {
    const int k = initial_bankroll;

    fprintf(output, "\nWith a bankroll of ");

    if (k % 4 == 0) {
      fprintf(output, "$%d\n", (k / 4) * 5);
    } else {
      fprintf(output, "$%.2f\n", 1.25 * (double)k);
    }

    for (rover = outcome::first; rover; rover = rover->next)
      if (rover->name) {
        double p = rover == bust ? rover->prob[k].abs + rover->prob[k].rel
                                 : rover->prob[k].abs;

        fprintf(output, "%5.2f%% %s\n", 100.0 * p, rover->name);
      }

    // Compute the expected value of the whole thing
    double ev = 0.0;
    for (int j = 0; j < max_pay - 1; j++) {
      ev += reach_goal[j]->prob[k].abs * (double)(goal + j);
    }
    fprintf(output, "Average cashout $%.2f\n", ev * 1.25);

    // Free all the storage we used
    free_outcomes();
  }

  median_length(output, game, prob_pays, initial_bankroll, goal);
}

#if 0

void cost_of_royal (FILE *output, game_parameters &game, prob_vector &prob_pays) {

  int goal = game.pay_table[N_royal_flush];
  ppair *royal = new ppair[goal];
  ppair *lucky = new ppair[goal];
  ppair *bust  = new ppair[goal];

  for (int j=goal-1; j > 0; j--) {
    int k;

    // Invariant:
    // Let P be the chance of going royal with j bests.
    // The for x > j, the chance of going royal with x
    // bets is royal[x].abs + P * royal[x].rel.

    // Compute P
    double abs_royal  = 0.0;
    double rel_royal  = 0.0;
    double abs_lucky  = 0.0;
    double rel_lucky  = 0.0;
    double abs_bust   = 0.0;
    double rel_bust   = 0.0;


    double smaller = 0.0;

    for (k=0; k<=last_pay; k++) {
      const double prob = prob_pays[k];
      const int    pay  = game.pay_table[k];

      if (pay == 0) {
        // We drop down to a bankroll of j-1
        smaller += prob;
      } else if (pay == 1) {
        // We just get our bet back.
        rel_royal += prob;
        rel_lucky += prob;
        rel_bust  += prob;
      } else {
        const int bigger = j+pay-1;

        if (bigger >= goal) {
          if (k == N_royal_flush) {
            abs_royal += prob;
          } else {
            abs_lucky += prob;
          }

        } else {
          // Add in the chances hitting the goal
          // with the bigger bankroll
          abs_royal += prob * royal[bigger].abs;
          rel_royal += prob * royal[bigger].rel;

          abs_lucky += prob * lucky[bigger].abs;
          rel_lucky += prob * lucky[bigger].rel;

          abs_bust += prob * bust[bigger].abs;
          rel_bust += prob * bust[bigger].rel;


        }
      }
    }

    // Let P2 = prob of going royal with bankroll j-1.
    // We have P = abs + rel * P + smaller * P2
    // Solving for P we have
    //
    // P - rel * P = abs + smaller * P2
    // P = abs / (1.0 - rel) + smaller / (1.0 - rel) * P2

    const double not_rel_royal = 1.0 - rel_royal;
    royal[j].abs = abs_royal = abs_royal / not_rel_royal;
    royal[j].rel = rel_royal = smaller / not_rel_royal;

    const double not_rel_lucky = 1.0 - rel_lucky;
    lucky[j].abs = abs_lucky = abs_lucky / not_rel_lucky;
    lucky[j].rel = rel_lucky = smaller / not_rel_lucky;

    const double not_rel_bust = 1.0 - rel_bust;
    bust[j].abs = abs_bust = abs_bust / not_rel_bust;
    bust[j].rel = rel_bust = smaller / not_rel_bust;

    // Now to reestablish the invariant, substitute abs + rel * P2
    // for P1 in the equation
    //  royal[m].abs + royal[m].rel * P1, for m in the range j+1..doubled-1

    for (int m=goal-1; m>j; m--) {
      royal[m].abs += royal[m].rel * abs_royal;
      royal[m].rel *= rel_royal;

      lucky[m].abs += lucky[m].rel * abs_lucky;
      lucky[m].rel *= rel_lucky;

      bust[m].abs += bust[m].rel * abs_bust;
      bust[m].rel *= rel_bust;
    }

  }

  // The probability of going royal with a bankroll of zero is 0.0.
  // Therefore the probability of going royal with bankroll of x > 0
  // royal[x].abs
  // Find the number of bets required to give you a 50-50 shot

  double prev = 0.0;
  double sum;

  for (int j=1;; j++) {
    sum = royal[j].abs + lucky[j].abs;

    if (sum >= 0.5 || j == goal-1) {
      break;
    }
    prev = sum;
  }

  if (j > 1 && 0.5-prev > sum-0.5) {
    j -= 1;
  }

  fprintf (output,
           "\nWith a bankroll of $%.2f\n%.2f%% Royal Flush\n%.2f%% Reach $1000\n%.2f%% Bust\n",
           1.25 * (double) j,
           100.0 * royal[j].abs,
           100.0 * lucky[j].abs,
           100.0 * (bust[j].abs + bust[j].rel));

  delete royal;
  delete lucky;
  delete bust;

  median_length(output, game, prob_pays, j, goal);
}

#endif

struct vstate {
  double multiplier;
  prob_vector prob_pays;
};

static void variance(hand_iter &h, int deuces, C_left &left,
                     StrategyLine *lines, vstate &e, game_parameters &parms) {
  // Compute the probability distribution of an initial five-card
  // hand consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);

  const bool trace = false;

  left.remove(matcher.hand, matcher.hand_size, deuces);
  // Subtract the hand to be evaluated from the left structure

  StrategyLine *best_strategy = lines;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.

  for (;;) {
    matcher.find(best_strategy->pattern);
    if (matcher.match_count != 0) {
      break;
    }

    best_strategy += 1;
  }

  unsigned mask = matcher.matches[0];
  // If the strategy can suggest more than one play, the distribution
  // can differ depending on which play we take.  Someday write code to
  // test for this corner case and object to the strategy.

  kept_description kept(matcher.hand, matcher.hand_size, mask, parms);
  // Build the description of subset of the hand
  // indicated by mask.

  int m1 = combin.choose(parms.deck_size - 5, kept.number_of_discards());

  double scale_factor =
      e.multiplier / static_cast<double>(combin.choose(
                         parms.deck_size - 5, kept.number_of_discards()));

  if (trace) {
    printf("mask %x: %s\n", mask, kept.display());
  }

  pay_dist pays;
  kept.all_draws(deuces, left, pays);
  int total_pays = 0;

  for (int j = first_pay; j <= last_pay; j++) {
    total_pays += pays[j];
    if (pays[j]) {
      e.prob_pays[j] += scale_factor * static_cast<double>(pays[j]);
    }
  }

  _ASSERT(total_pays == m1);

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

void box_score(const vp_game &game, StrategyLine *lines[],
               const char *filename) {
  int counter = 0;
  int timer = 0;

  game_parameters parms(game);
  C_left left(parms);

  const int total_hands = combin.choose(parms.deck_size, 5);

  vstate v;
  {
    for (int j = first_pay; j <= last_pay; j++) {
      v.prob_pays[j] = 0.0;
    }
  }

  printf("Evaluating strategy for %s\n", game.name);

  FILE *output = fopen(filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  fprintf(output, "Box Score for %s\n\n", game.name);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    StrategyLine *strategy_w = lines[wild_cards];

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      const int mult = wmult * iter.multiplier();
      v.multiplier = (double)mult / (double)total_hands;

      variance(iter, wild_cards, left, strategy_w, v, parms);
      counter += mult;

      iter.next();
    }
  }

  printf("\n");
  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    throw 0;
  }

  {
    double ev = 0.0;
#if 0
    // Temporary code for exact output
    {
      fprintf(output, "Begin hex payoffs\n");
      double prob_nothing = 0;
      for (int j = first_pay; j <= last_pay; j++)
        if (parms.pay_table[j] == 0) {
          prob_nothing += v.prob_pays[j];
        }
      fprintf(output, "%a%7.1f\n", prob_nothing, 0.0);
      for (int j = first_pay; j <= last_pay; j++)
        if (parms.pay_table[j] != 0) {
          double p = v.prob_pays[j];

          fprintf(output, "%a%7.1f\n", p, parms.pay_table[j]);

          ev += p * static_cast<double>(parms.pay_table[j]);
        }

      fprintf(output, "Expected value = %.4f%%\n", 100.0 * ev);
      fprintf(output, "End hex payoffs\n");
    }
#endif

    // Hack for one-eyed jacks.  A "high pair" has a nonzero probability,
    // but pays nothing.
    static const char *const format = "%6.2f%% %8.2f %s\n";
    double prob_nothing = 0;
    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] == 0) {
        prob_nothing += v.prob_pays[j];
      }
    fprintf(output, format, 100.0 * prob_nothing, 1.0 / prob_nothing,
            payoff_image[N_nothing]);
    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] != 0) {
        double p = v.prob_pays[j];

        fprintf(output, format, 100.0 * p, 1.0 / p, payoff_image[j]);

        ev += p * static_cast<double>(parms.pay_table[j]);
      }

    fprintf(output, "Expected value = %.4f%%\n", 100.0 * ev);

    double var = 0.0;
    for (int j = first_pay; j <= last_pay; j++) {
      double delta = ev - double(parms.pay_table[j]);
      double p = v.prob_pays[j];

      var += p * delta * delta;
    }

    fprintf(output, "Variance = %.4f\n", var);
  }

  risk_of_ruin(output, parms, v.prob_pays);

  eval_bankroll(output, parms, v.prob_pays, 16, 32);
  eval_bankroll(output, parms, v.prob_pays, 16,
                break_even_goal(parms, v.prob_pays, 16));
  eval_bankroll(output, parms, v.prob_pays, 80, 160);
  eval_bankroll(output, parms, v.prob_pays, 80,
                break_even_goal(parms, v.prob_pays, 80));

  eval_bankroll(output, parms, v.prob_pays, 0, 800);
  if ((*game.pay_table)[N_royal_flush] > 800) {
    eval_bankroll(output, parms, v.prob_pays, 0,
                  (*game.pay_table)[N_royal_flush]);
  }

  fclose(output);
  printf("Report is in %s\n", filename);
}

void half_life(const vp_game &game, StrategyLine *lines[],
               const char *filename) {
  int counter = 0;
  int timer = 0;

  game_parameters parms(game);
  C_left left(parms);

  const int total_hands = combin.choose(parms.deck_size, 5);

  vstate v;
  {
    for (int j = first_pay; j <= last_pay; j++) {
      v.prob_pays[j] = 0.0;
    }
  }

  printf("Selecting a bankroll for %s\n", game.name);

  FILE *output = fopen(filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  fprintf(output, "Box Score for %s\n\n", game.name);

  printf("Computing");

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;
    hand_iter iter(hand_size, parms.kind, wild_cards);

    const int wmult = combin.choose(parms.number_wild_cards, wild_cards);

    StrategyLine *strategy_w = lines[wild_cards];

    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }

      const int mult = wmult * iter.multiplier();
      v.multiplier = (double)mult / (double)total_hands;

      variance(iter, wild_cards, left, strategy_w, v, parms);
      counter += mult;

      iter.next();
    }
  }

  printf("\n");
  if (counter != total_hands) {
    printf("Iteration counter wrong\n");
    throw 0;
  }

  {
    double ev = 0.0;

    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] || j == N_nothing) {
        double p = v.prob_pays[j];

        fprintf(output, "%6.2f%% %8.2f %s\n", 100.0 * p, 1.0 / p,
                payoff_image[j]);

        ev += p * static_cast<double>(parms.pay_table[j]);
      }

    fprintf(output, "Expected value = %.4f%%\n", 100.0 * ev);

    double var = 0.0;
    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] || j == N_nothing) {
        double delta = ev - double(parms.pay_table[j]);
        double p = v.prob_pays[j];

        var += p * delta * delta;
      }

    fprintf(output, "Variance = %.4f\n", var);
  }

  risk_of_ruin(output, parms, v.prob_pays);

  // Use binary search to find the smallest bankroll that has a better
  // than 50-50 chance of doubling itself.
  int upper = 1;
  for (;;) {
    const double bp = bust_prob(parms, v.prob_pays, upper, 2 * upper);
    if (bp < 0.5) {
      break;
    }
    upper *= 2;
  }
  int lower = 1;
  for (;;) {
    const int mid = (lower + upper) / 2;
    if (mid == lower) {
      break;
    }
    const double bp = bust_prob(parms, v.prob_pays, mid, 2 * mid);
    if (bp < 0.5) {
      upper = mid;
    } else {
      lower = mid;
    }
  }

  eval_bankroll(output, parms, v.prob_pays, 16, 32);
  eval_bankroll(output, parms, v.prob_pays, 80, 160);
  eval_bankroll(output, parms, v.prob_pays, 0,
                (*game.pay_table)[N_royal_flush]);
  eval_bankroll(output, parms, v.prob_pays, upper, 2 * upper);

  fclose(output);
  printf("Report is in %s\n", filename);
}

void optimal_box_score(const vp_game &game, const char *filename) {
  game_parameters parms(game);
  prob_vector prob_pays;

  FILE *output = fopen(filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  eval_game(game, prob_pays);

  fprintf(output, "Optimal box score for %s\n\n", game.name);

  {
    double ev = 0.0;
    double prob_nothing = 0.0;

    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] == 0.0) {
        prob_nothing += prob_pays[j];
      }

    for (int j = first_pay; j <= last_pay; j++)
      if (parms.pay_table[j] != 0.0 || j == N_nothing) {
        double p = j == N_nothing ? prob_nothing : prob_pays[j];

        fprintf(output, "%6.2f%% %8.2f %s\n", 100.0 * p, 1.0 / p,
                payoff_image[j]);

        ev += p * static_cast<double>(parms.pay_table[j]);
      }

    fprintf(output, "Expected value = %.4f%%\n", 100.0 * ev);

    double var = 0.0;
    for (int j = first_pay; j <= last_pay; j++) {
      double delta = ev - double(parms.pay_table[j]);
      double p = prob_pays[j];

      var += p * delta * delta;
    }

    fprintf(output, "Variance = %.4f\n", var);
  }

  risk_of_ruin(output, parms, prob_pays);

  eval_bankroll(output, parms, prob_pays, 16, 32);
  eval_bankroll(output, parms, prob_pays, 80, 160);
  eval_bankroll(output, parms, prob_pays, 0, 800);
  if ((*game.pay_table)[N_royal_flush] > 800) {
    eval_bankroll(output, parms, prob_pays, 0,
                  (*game.pay_table)[N_royal_flush]);
  }

  // parms.pay_table[N_royal_flush]);
  // cost_of_royal(output, parms, prob_pays);

  fclose(output);
  printf("Report is in %s\n", filename);
}

static void union_evaluate(hand_iter &h, int deuces, C_left &left,
                           const StrategyLine *lines, vector<bool> *used_lines,
                           game_parameters &parms, FILE *output) {
  // Compute the expected value of an initial five-card hand
  // consisting of the cards returned by the iterator plus
  // the indicated number of deuces.

  enum_match matcher;
  matcher.wild_cards = deuces;
  matcher.parms = &parms;

  matcher.hand_size = h.size();
  h.current(matcher.hand[0]);

  // Subtract the hand to be evaluated from the left structure
  left.remove(matcher.hand, matcher.hand_size, deuces);

  struct Play {
    Play(unsigned mask, int deuces) : mask(mask), deuces(deuces) {}

    unsigned mask;
    int deuces;
  };
  vector<Play> best_plays;
  double best_value = -1.0;

  // Incrementing the binary mask iterates over all
  // 2^hand_size combinations of cards to be kept.
  const unsigned power = 1 << matcher.hand_size;
  unsigned mask;
  for (mask = 0; mask < power; mask++) {
    kept_description kept(matcher.hand, matcher.hand_size, mask, parms);
    // Build the description of subset of the hand
    // indicated by mask.

    // In real video poker games offered by casinos you never
    // disard a wild card.  But it's possible to concoct
    // pay tables where that is the right move.
    // (four deuces pays 0; natural royal plays Avagadro's Number)

    for (int keep_deuces = 0; keep_deuces <= deuces; keep_deuces++) {
      pay_dist pays;
      kept.all_draws(keep_deuces, left, pays);
      {
        int total_pays = 0;
        double result = 0.0;

        for (int j = first_pay; j <= last_pay; j++) {
          const int pay = pays[j];
          total_pays += pay;
          result += (double)pay * parms.pay_table[j];
        }

        const double current_total = (double)total_pays;
        const double value = result / current_total;

        if (value > best_value) {
          best_plays.clear();
        }
        if (value >= best_value) {
          best_plays.push_back(Play(mask, keep_deuces));
          best_value = value;
        }
      }
    }
  }

  // Filter out funky deuces.
  vector<Play>::iterator iter = best_plays.begin();
  while (iter != best_plays.end()) {
    if (iter->deuces == deuces) {
      ++iter;
    } else {
      // We should never actually see this message.
      fprintf(output, "Discard a deuce?\n");
      iter = best_plays.erase(iter);
    }
  }

  _ASSERT(best_value >= 0);
  _ASSERT(!best_plays.empty());

  for (const StrategyLine *line = lines;
       !best_plays.empty() && line->pattern != 0; ++line) {
    matcher.find(line->pattern);

    vector<Play>::iterator iter = best_plays.begin();
    while (iter != best_plays.end()) {
      // If the play matches the strategy line, mark the line as used,
      // and erase the play so only the first line will get marked, and
      // duplicate lines will be flagged.
      if (matcher.result_vector[iter->mask]) {
        (*used_lines)[line - lines] = true;
        iter = best_plays.erase(iter);
      } else {
        ++iter;
      }
    }
  }

  // If any best_plays are left, we are missing a strategy line.
  // Report it in the output.
  for (vector<Play>::const_iterator iter = best_plays.begin();
       iter != best_plays.end(); ++iter) {
    print_move(output, matcher.hand, matcher.hand_size, iter->mask);
  }

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

// Returns the number of entries in a strategy, not counting the sentinel
// at the end.
static std::size_t strategy_length(const StrategyLine *lines) {
  for (const StrategyLine *rover = lines;; ++rover) {
    if (rover->pattern == 0) {
      return rover - lines;
    }
  }
}

void check_union(const vp_game &game, StrategyLine *lines[],
                 const char *filename) {
  int timer = 0;
  game_parameters parms(game);
  C_left left(parms);

  FILE *output = NULL;
  fopen_s(&output, filename, "w");
  if (output == 0) {
    printf("fopen failed\n");
    throw 0;
  }

  for (int wild_cards = 0; wild_cards <= parms.number_wild_cards;
       wild_cards++) {
    const int hand_size = 5 - wild_cards;

    const StrategyLine *const wild_strategy = lines[wild_cards];
    vector<bool> used_lines(strategy_length(wild_strategy));

    hand_iter iter(hand_size, parms.kind, wild_cards);
    while (!iter.done()) {
      if (++timer > 102359 / 40) {
        printf(".");
        timer = 0;
      }
      union_evaluate(iter, wild_cards, left, wild_strategy, &used_lines, parms,
                     output);
      iter.next();
    }

    // Check if there are any unused lines, and if so, report them.
    bool printed = false;
    for (size_t i = 0; i < used_lines.size(); ++i) {
      if (!used_lines[i]) {
        if (!printed) {
          fprintf(output, "Unused strategy lines:\n");
          printed = true;
        }
        fprintf(output, "%s\n", wild_strategy[i].image);
      }
    }
    if (!printed) {
      fprintf(output, "All strategy lines used\n");
    }
  }

  fclose(output);
  printf("\nUnion report in %s\n", filename);
}
