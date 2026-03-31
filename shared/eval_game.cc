#define _CRT_SECURE_NO_WARNINGS  // For Microsoft Visual Studio
#include <stdio.h>

#define NOMINMAX  // avoid blocking things like std::min
                  // and numeric_limits<int>::max()
#include <windows.h>

#include "eval_game.h"

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

#include "combin.h"
#include "enum_match.h"
#include "game.h"
#include "hand_iter.h"
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

  left.replace(matcher.hand, matcher.hand_size, deuces);
}

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

double get_payback(const vp_game &game, pay_prob &prob_pays) {
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

void eval_game(const vp_game &game, pay_prob &prob_pays) {
  const double ev = get_payback(game, prob_pays);
  printf("Return %.5f%%\n", ev * 100.0);
}
