#define _CRT_SECURE_NO_WARNINGS  // For Microsoft Visual Studio
#include <stdio.h>

#define NOMINMAX  // avoid blocking things like std::min
                  // and numeric_limits<int>::max()
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../shared/combin.h"
#include "../shared/enum_match.h"
#include "../shared/game.h"
#include "../shared/hand_iter.h"
#include "../shared/kept.h"
#include "../shared/pay_dist.h"
#include "../shared/vpoker.h"

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

#if 0
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

#endif

static const int max_trace = 10;
struct estate {
  double multiplier;
  double optimal_return;
  int trace_count;
  FILE *trace_file[max_trace];
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

#if 0
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
#endif

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

#if 0

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
    std::cout << "prob " << std::hexfloat << prob << " pay " << pay << "\n";
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
#endif

static void evaluate(hand_iter &h, int deuces, C_left &left,
                     game_parameters &parms, double multiplier,
                     pay_prob &prob_pays) {
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

  _ASSERT(total_pays ==
          combin.choose(parms.deck_size - 5, kept.number_of_discards()));

  left.replace(hand, hand_size, deuces);
}

double find_payback(const vp_game &game, pay_prob &prob_pays) {
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

  double ev = 0.0;
  for (std::size_t i = first_pay; i <= last_pay; ++i) {
    ev += prob_pays[i] * (*game.pay_table)[i];
  }

  return ev;
}

double eval_game(const vp_game &game, pay_prob &prob_pays) {
  printf("Return %.5f%%\n", find_payback(game, prob_pays) * 100.0);
}
