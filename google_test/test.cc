#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>

#include "combin.h"
#include "gtest/gtest.h"
#include "multi_command.h"
#include "pay_dist.h"

// Returns the expected value of a distribution.
double expected(const PayDistribution& dist) {
  return std::accumulate(dist.begin(), dist.end(), 0.0,
                         [](float sum, const ProbPay& pp) {
                           return sum + pp.probability * pp.payoff;
                         });
}

// Returns the sum of the probabilities in the distribution.
// It should be 1.0
double total_prob(const PayDistribution& dist) {
  return std::accumulate(
      dist.begin(), dist.end(), 0.0,
      [](float sum, const ProbPay& pp) { return sum + pp.probability; });
}

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
  PayDistribution test{{0.25, 3}, {0.125, 2}, {0.25, 3}, {0.125, 4}, {0.25, 3}};
  normalize(test);
  PayDistribution expected{{0.125, 2}, {0.75, 3}, {0.125, 4}};
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Normalize2) {
  PayDistribution test{{0.25, 3}, {0.25, 3}, {0.25, 3}, {0.25, 3}};
  normalize(test);
  PayDistribution expected{{1.0, 3}};
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Normalize3) {
  PayDistribution test{{0.75, 5}, {0.125, 4}, {0.0625, 3}, {0.625, 2}};
  normalize(test);
  PayDistribution expected{{0.625, 2}, {0.0625, 3}, {0.125, 4}, {0.75, 5}};
  EXPECT_EQ(test, expected);
}

TEST(Distribution, Times10) {
  // All the probabilities have short binary mantissas to minimize floating
  // point roundoff error.
  static const PayDistribution test_dist = {
      {0.5, 0}, {0.25, 1}, {0.125, 7}, {0.0625, 10}, {0.0625, 100}};

  EXPECT_EQ(expected(test_dist), 8);
  EXPECT_EQ(total_prob(test_dist), 1);
  const PayDistribution ten_times = repeat(test_dist, 10);
  EXPECT_FLOAT_EQ(expected(ten_times), 80.0);
  EXPECT_NEAR(total_prob(ten_times), 1.0, 0.0000003);

  // Some spot tests
  EXPECT_EQ(ten_times.size(), 464);
  for (std::size_t i = 1; i < ten_times.size(); ++i) {
    EXPECT_LT(ten_times[i - 1].payoff, ten_times[i].payoff);
  }

  EXPECT_EQ(ten_times.back().payoff, 1000);
  EXPECT_EQ(ten_times.back().probability,
            pow(test_dist.back().probability, 10));

  const ProbPay& second = *(ten_times.rbegin() + 1);
  EXPECT_EQ(second.payoff, 910);  // nine 100s and a 10
  EXPECT_EQ(second.probability, 10 * pow(test_dist.back().probability, 9) *
                                    (test_dist.rbegin() + 1)->probability);

  EXPECT_EQ(ten_times.front().payoff, 0);
  EXPECT_EQ(ten_times.front().probability,
            pow(test_dist.front().probability, 10));

  EXPECT_EQ(ten_times[1].payoff, 1);
  EXPECT_EQ(ten_times[1].probability,
            10 * pow(test_dist[0].probability, 9) * test_dist[1].probability);

  EXPECT_EQ(ten_times[6].payoff, 6);  // Six 1s
  EXPECT_EQ(ten_times[6].probability, pow(test_dist[1].probability, 6) *
                                          pow(test_dist[0].probability, 4) *
                                          combination(10, 6));
}

using RandomEngine = std::mt19937_64;
std::uniform_int_distribution<int> dist1_3(1, 3);
std::uniform_int_distribution<int> dist0_9(0, 9);

PayDistribution increasing(RandomEngine& engine) {
  const int len = dist0_9(engine);
  PayDistribution result;
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
  return result;
}

TEST(Distribution, Merge) {
  RandomEngine engine;
  engine.seed(12345);
  for (int i = 0; i < 1000; ++i) {
    const PayDistribution dist1 = increasing(engine);
    const PayDistribution dist2 = increasing(engine);
    const PayDistribution sum = merge(dist1, dist2);

    // Compute the sum another way
    PayDistribution result = dist1;
    result.insert(result.end(), dist2.begin(), dist2.end());
    normalize(result);

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
}
