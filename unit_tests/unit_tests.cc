#include <iomanip>
#include <iostream>
#include <numeric>

#include "pay_dist.h"

const PayDistribution test_dist = {
    {0.5, 0}, {0.25, 1}, {0.125, 7}, {0.0625, 10}, {0.0625, 100}};

// Returns the expected value of a distribution.
double expected(const PayDistribution& dist) {
  return std::accumulate(dist.begin(), dist.end(), 0.0,
                         [](float sum, const ProbPay& pp) {
                           return sum + pp.probability * pp.payoff;
                         });
}

// Returns the sum of the probabilities in the distribuition.
// It should be 1.0
double total_prob(const PayDistribution& dist) {
  return std::accumulate(
      dist.begin(), dist.end(), 0.0,
      [](float sum, const ProbPay& pp) { return sum + pp.probability; });
}

int main() {
  std::cout << "EV = " << expected(test_dist) << "\n";
  std::cout << "prob sum = " << total_prob(test_dist) << "\n";
  const PayDistribution ten_times = repeat(test_dist, 10);
  std::cout << "EV = " << expected(ten_times) << "\n";
  std::cout << "prob sum = " << total_prob(ten_times) << "\n";

  std::cout << "10x distribution\n";
  for (const auto& [prob, pays] : ten_times) {
    std::cout << std::scientific
              << std::setprecision(std::numeric_limits<double>::max_digits10)
              << prob << std::defaultfloat << "   " << std::setw(5) << pays
              << "\n";
  }
  return 0;
}
