#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

#include "..\shared\eval_game.h"
#include "..\shared\vpoker.h"
#include "combin.h"
#include "gtest/gtest.h"
#include "multi_command.h"
#include "pay_dist.h"

// Number of combinations for n things taken k at a time.
int combination(int n, int k) {
  if (k > n) return 0;
  if (k > n - k) k = n - k;  // Take advantage of symmetry
  int result = 1;

  for (int i = 0; i < k; ++i) {
    result *= (n - i);
    result /= (i + 1);
  }
  return result;
}

TEST(Combinations, AllValues) {
  for (int n = 0; n <= 17; ++n) {
    for (int k = 0; k <= 17; ++k) {
      EXPECT_EQ(combin.choose(n, k), combination(n, k));
    }
  }
}

TEST(Distribution, Normalize1) {
  PayDistribution test(
      {{0.25, 3}, {0.125, 2}, {0.25, 3}, {0.125, 4}, {0.25, 3}});
  test.normalize();
  PayDistribution expected({{0.125, 2}, {0.75, 3}, {0.125, 4}});
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Normalize2) {
  PayDistribution test({{0.25, 3}, {0.25, 3}, {0.25, 3}, {0.25, 3}});
  test.normalize();
  PayDistribution expected({{1.0, 3}});
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Normalize3) {
  PayDistribution test({{0.75, 5}, {0.125, 4}, {0.0625, 3}, {0.625, 2}});
  test.normalize();
  PayDistribution expected({{0.625, 2}, {0.0625, 3}, {0.125, 4}, {0.75, 5}});
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Cutoff) {
  const PayDistribution dist1(50, .0625,
                              {{0.5, 0}, {0.25, 1}, {0.125, 7}, {0.0625, 11}});
  EXPECT_EQ(dist1.total_prob(), 1.0);
  const PayDistribution dist2(
      50, .0625, {{0.5, 0}, {0.25, 20}, {0.125, 30}, {0.0625, 40}});
  EXPECT_EQ(dist2.total_prob(), 1.0);
  const PayDistribution sum = succession(dist1, dist2);
  EXPECT_EQ(sum.total_prob(), 1.0);
  const PayDistribution expected(
      50, .0625 + .0625 * (.0625 + .0625) + (.5 + .25 + .125) * .0625,
      {{0.5 * 0.5, 0},
       {0.5 * 0.25, 1},
       {0.5 * 0.125, 7},
       {0.5 * 0.0625, 11},
       {0.25 * 0.5, 20},
       {0.25 * 0.25, 21},
       {0.25 * 0.125, 27},
       {0.125 * 0.5, 30},
       {0.125 * 0.25 + 0.25 * 0.0625, 31},
       {0.125 * 0.125, 37},
       {0.0625 * 0.5, 40},
       {0.0625 * 0.25 + 0.125 * 0.0625, 41},
       {0.0625 * 0.125, 47}});
  EXPECT_EQ(sum, expected);
}

TEST(Distribution, Times10) {
  // All the probabilities have short binary mantissas to minimize floating
  // point roundoff error.
  const std::vector<ProbPay> test_data{
      {0.5, 0}, {0.25, 1}, {0.125, 7}, {0.0625, 10}, {0.0625, 100}};
  const PayDistribution test_dist(test_data);

  EXPECT_EQ(test_dist.expected(), 8);
  EXPECT_EQ(test_dist.total_prob(), 1);
  const PayDistribution ten_times = repeat(test_dist, 10);
  EXPECT_NEAR(ten_times.expected(), 80.0, 0.00001);
  EXPECT_NEAR(ten_times.total_prob(), 1.0, 0.0000003);

  // Some spot tests
  const std::vector<ProbPay>& dist = ten_times.distribution();

  EXPECT_EQ(dist.size(), 464);
  for (std::size_t i = 1; i < dist.size(); ++i) {
    EXPECT_LT(dist[i - 1].payoff, dist[i].payoff);
  }

  EXPECT_EQ(dist.back().payoff, 1000);
  EXPECT_EQ(dist.back().probability, pow(test_data.back().probability, 10));

  const ProbPay& second = *(dist.rbegin() + 1);
  EXPECT_EQ(second.payoff, 910);  // nine 100s and a 10
  EXPECT_EQ(second.probability, 10 * pow(test_data.back().probability, 9) *
                                    (test_data.rbegin() + 1)->probability);

  EXPECT_EQ(dist.front().payoff, 0);
  EXPECT_EQ(dist.front().probability, pow(test_data.front().probability, 10));

  EXPECT_EQ(dist[1].payoff, 1);
  EXPECT_EQ(dist[1].probability,
            10 * pow(test_data[0].probability, 9) * test_data[1].probability);

  EXPECT_EQ(dist[6].payoff, 6);  // Six 1s
  EXPECT_EQ(dist[6].probability, pow(test_data[1].probability, 6) *
                                     pow(test_data[0].probability, 4) *
                                     combination(10, 6));
}

using RandomEngine = std::mt19937_64;
std::uniform_int_distribution<int> dist1_3(1, 3);
std::uniform_int_distribution<int> dist0_9(0, 9);

PayDistribution increasing(RandomEngine& engine) {
  const int len = dist0_9(engine);
  std::vector<ProbPay> result;
  result.reserve(len);
  int counter = -1;
  for (int i = 0; i < len; ++i) {
    const double prob = 0.0625 * dist1_3(engine);
    const int payoff = counter + dist1_3(engine);
    result.emplace_back(prob, payoff);
    counter = payoff;
  }
  for (std::size_t i = 1; i < len; ++i) {
    EXPECT_LT(result[i - 1].payoff, result[i].payoff);
  }
  return PayDistribution(result);
}

TEST(Distribution, Merge) {
  RandomEngine engine;
  engine.seed(12345);
  for (int i = 0; i < 1000; ++i) {
    const PayDistribution dist1 = increasing(engine);
    const PayDistribution dist2 = increasing(engine);
    const PayDistribution sum = merge(dist1, dist2);

    // Compute the sum another way
    std::vector<ProbPay> concatenated = dist1.distribution();
    concatenated.insert(concatenated.end(), dist2.distribution().begin(),
                        dist2.distribution().end());
    PayDistribution result(concatenated);
    result.normalize();

    EXPECT_EQ(sum, result);
  }
}

void test_multi(const std::string& line, int arg1, int arg2) {
  const auto parsed = multi_command(line);
  ASSERT_TRUE(parsed);
  const int f = parsed->first;
  const int s = parsed->second;
  EXPECT_EQ(f, arg1);
  EXPECT_EQ(s, arg2);
}

void bad_multi(const std::string& line) {
  const auto parsed = multi_command(line);
  EXPECT_FALSE(parsed);
}

TEST(MultiCommand, Only) {
  test_multi("multi 5 6", 5, 6);
  test_multi("multi    5    6    ", 5, 6);
  test_multi("multi    5   ", 5, 1);
  test_multi("multi     ", 1, 1);
  test_multi("multi", 1, 1);
  bad_multi("foo");
  bad_multi("multi bar");
  bad_multi("multi 5 bar");
  bad_multi("multi 5 6 bar");
  bad_multi("multi -1");
  bad_multi("multi 5 -1");
  bad_multi("multi 0");
  bad_multi("multi 5 0");
  bad_multi("multi 5555555555555555555555555555");
}

enum Deck { cards52, cards53 };
std::string PrintCombinations(const pay_prob& prob_pays,
                              const int (&pay_table)[], Deck deck) {
  double total_combinations;
  switch (deck) {
    case cards52:
      // The number 1,661,102,543,100 represents the total number of distinct
      // final 5-card hands possible in video poker games like Jacks or Better,
      // after accounting for all possible player decisions on holding cards and
      // drawing replacements from a standard 52-card deck.
      total_combinations = 1661102543100.0;
      break;
    case cards53:
      // Joker's Wild games use a different value because it has a 53-card deck.
      total_combinations = 2047405460100.0;
      break;
  }

  std::stringstream ss;
  for (int j = first_pay; j <= last_pay; j++) {
    const double prob = prob_pays[j];
    if (pay_table[j] != 0) {
      ss << std::llround(total_combinations * prob_pays[j]) << " "
         << payoff_image[j] << "\n";
    }
  }
  return ss.str();
}

TEST(GetPayback, Jacks) {
  const int jb_table[] = {
      0,   // nothing,
      1,   // high_pair,
      2,   // two_pair,
      3,   // trips,
      4,   // straight,
      6,   // flush,
      9,   // full_house,
      25,  // quads,
      0,   // quad aces",
      0,   // quad aces w/low kicker",
      0,   // quad 2-4",
      0,   // quad 2-4 w/low kicker",
      50,  // straight_flush,
      0,   // quints,
      0,   // wild_royal,
      0,   // four_deuces,
      800  // royal_flush
  };

  const vp_game jacks_or_better("9/6 Jacks or Better", GK_no_wild, jack,
                                &jb_table);
  pay_prob prob_pays;
  const double ev = get_payback(jacks_or_better, prob_pays);
  ASSERT_DOUBLE_EQ(ev, 0.99543904369518432);
  const std::string combos = PrintCombinations(prob_pays, jb_table, cards52);
  EXPECT_EQ(combos,
            R"(356447740914 High Pair
214745513679 Two Pair
123666922527 Trips
18653130482 Straight
18296232180 Flush
19122956883 Full House
3924430647 Quads
181573608 Straight Flush
41126022 Royal Flush
)");
}

TEST(GetPayback, DoubleBonus) {
  const int db_table[] = {
      0,    // nothing,
      1,    // high_pair,
      1,    // two_pair,
      3,    // trips,
      5,    // straight,
      7,    // flush,
      10,   // full_house,
      50,   // quads,
      160,  // quad aces
      0,    // quad aces w/low kicker
      80,   // quad 2-4
      0,    // quad 2-4 w/low kicker
      50,   // straight_flush,
      0,    // quints,
      0,    // wild_royal,
      0,    // four_deuces,
      800   // royal_flush
  };
  const vp_game double_bonus("10/7 Double Bonus Poker", GK_no_wild, jack,
                             &db_table);
  pay_prob prob_pays;
  const double ev = get_payback(double_bonus, prob_pays);
  ASSERT_DOUBLE_EQ(ev, 1.0017252235510166);
  const std::string combos = PrintCombinations(prob_pays, db_table, cards52);
  EXPECT_EQ(combos,
            R"(319561323444 High Pair
207070336215 Two Pair
119930687197 Trips
24948778786 Straight
24839043284 Flush
18587554884 Full House
2670499348 Quads
330211491 Quad Aces
870522057 Quad 2,3 or 4
187878280 Straight Flush
34571706 Royal Flush
)");
}

TEST(GetPayback, DoubleDoubleBonus) {
  const int ddb_table[] = {
      0,    // nothing
      1,    // high_pair,
      1,    // two_pair,
      3,    // trips,
      4,    // straight,
      6,    // flush,
      10,   // full_house,
      50,   // quads,
      160,  // quad aces
      400,  // quad aces w/low kicker
      80,   // quad 2-4
      160,  // quad 2-4 w/low kicker
      40,   // straight_flush,
      0,    // quints,
      0,    // wild_royal,
      0,    // four_deuces,
      800   // royal_flush
  };
  const vp_game double_double_bonus("Double Double Bonus Poker", GK_no_wild,
                                    jack, &ddb_table);
  pay_prob prob_pays;
  const double ev = get_payback(double_double_bonus, prob_pays);
  ASSERT_DOUBLE_EQ(ev, 0.999576698739713);
  const std::string combos = PrintCombinations(prob_pays, ddb_table, cards52);
  EXPECT_EQ(combos,
            R"(351476355342 High Pair
204537482307 Two Pair
125098748131 Trips
21203534298 Straight
18754389656 Flush
18047434611 Full House
2709448318 Quads
288413337 Quad Aces
102334500 Quad Aces w/low kicker
638537018 Quad 2,3 or 4
237869374 Quad 2,3 or 4 w/low kicker
180935528 Straight Flush
40428326 Royal Flush
)");
}

TEST(GetPayback, DoubleDoubleBonusNoAces) {
  const int pay_table[] = {
      0,    // nothing
      1,    // high_pair,
      1,    // two_pair,
      3,    // trips,
      4,    // straight,
      6,    // flush,
      10,   // full_house,
      50,   // quads,
      0,    // quad aces
      0,    // quad aces w/low kicker
      80,   // quad 2-4
      160,  // quad 2-4 w/low kicker
      50,   // straight_flush,
      0,    // quints,
      0,    // wild_royal,
      0,    // four_deuces,
      800   // royal_flush
  };
  const vp_game game_desc("Double Double Bonus No Ace Bonus", GK_no_wild, jack,
                          &pay_table);
  pay_prob prob_pays;
  const double ev = get_payback(game_desc, prob_pays);
  ASSERT_DOUBLE_EQ(ev, 0.95490742757086533);
  const std::string combos = PrintCombinations(prob_pays, pay_table, cards52);
  EXPECT_EQ(combos,
            R"(351476355342 High Pair
204537482307 Two Pair
125098748131 Trips
21203534298 Straight
18754389656 Flush
18047434611 Full House
2709448318 Quads
288413337 Quad Aces
102334500 Quad Aces w/low kicker
638537018 Quad 2,3 or 4
237869374 Quad 2,3 or 4 w/low kicker
180935528 Straight Flush
40428326 Royal Flush
)");
}
TEST(GetPayback, KingsOrBetterJokersWild) {
  const int kb_table[] = {
      0,    // nothing,
      1,    // high_pair,
      1,    // two_pair,
      2,    // trips,
      3,    // straight,
      5,    // flush,
      7,    // full_house,
      20,   // quads,
      0,    // quad aces",
      0,    // quad aces w/low kicker",
      0,    // quad 2-4",
      0,    // quad 2-4 w/low kicker",
      50,   // straight_flush,
      200,  // quints,
      100,  // wild_royal,
      0,    // four_deuces,
      800   // royal_flush
  };
  const vp_game kb_joker("Kings or Better Joker's Wild", GK_joker_wild, king,
                         &kb_table);
  pay_prob prob_pays;
  const double ev = get_payback(kb_joker, prob_pays);

  std::ios_base::fmtflags original_flags = std::cout.flags();
  std::streamsize original_precision = std::cout.precision();
  char original_fill = std::cout.fill();
  std::cout.flags(original_flags);
  std::cout.precision(original_precision);
  std::cout.fill(original_fill);

  ASSERT_DOUBLE_EQ(ev, 1.0064629663459064);
  const std::string combos = PrintCombinations(prob_pays, kb_table, cards53);

  // This test result is slightly different from what the Wizard of Odds
  // reports. The reason is rare hands like 9s Js 6h Ah joker.
  // There is no single best play for this hand. Keeping 9J+joker has
  // an expected value identical to that of keeping A+joker. This choice
  // does not affect the final house edge but does affect the totals for
  // the combinations. For example, keeping A+joker has Quints as a possible
  // outcome, but 9J+joker does not. Apparently my code and the Wizard make
  // different choices in situtations like this.
  EXPECT_EQ(combos,
            R"(290649393121 High Pair
227002401450 Two Pair
274223843817 Trips
33975984497 Straight
31891371778 Flush
32101477947 Full House
17516371686 Quads
1176962909 Straight Flush
191113461 Quints
213027379 Wild Royal
49677654 Royal Flush
)");
}

TEST(GetPayback, NsuDeuces) {
  const int nsu_table[] = {
      0,    // nothing,
      0,    // high_pair,
      0,    // two_pair,
      1,    // trips,
      2,    // straight,
      3,    // flush,
      4,    // full_house,
      4,    // quads,
      0,    // quad aces",
      0,    // quad aces w/low kicker",
      0,    // quad 2-4",
      0,    // quad 2-4 w/low kicker",
      10,   // straight_flush,
      16,   // quints,
      25,   // wild_royal,
      200,  // four_deuces,
      800   // royal_flush
  };

  const vp_game nsu_deuces_wild("NSU Deuces Wild", GK_deuces_wild, ace,
                                &nsu_table);
  pay_prob prob_pays;
  const double ev = get_payback(nsu_deuces_wild, prob_pays);
  ASSERT_DOUBLE_EQ(ev, 0.99728294651662486);
  const std::string combos = PrintCombinations(prob_pays, nsu_table, cards52);
  EXPECT_EQ(combos,
            R"(443825967643 Trips
95240456400 Straight
34489242338 Flush
43380578592 Full House
101390107459 Quads
8532702998 Straight Flush
5163436138 Quints
3167246872 Wild Royal
310144767 Four Deuces
38224692 Royal Flush
)");
}
