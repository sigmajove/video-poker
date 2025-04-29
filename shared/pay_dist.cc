#include "pay_dist.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

// This is a little weird.
// In all real Video Poker machines, the payoffs are integers.
// However, in our tables they are represented by doubles.
// Perform the double-to-int conversion, checking that there
// is no loss of precision.
ProbPay::ProbPay(double prob, double pay) : probability(prob) {
  double intpart;
  if (std::modf(pay, &intpart) != 0.0) {
    throw std::runtime_error("payoff is not an integer");
  }
  if (pay < 0) {
    throw std::runtime_error("payoff is negative");
  }
  if (pay > std::numeric_limits<int>::max()) {
    throw std::runtime_error("payoff would cause integer overflow");
  }
  payoff = static_cast<int>(pay);
}

void normalize(PayDistribution &dist) {
  if (dist.empty()) {
    return;
  }

  int min_pay = dist[0].payoff;
  int max_pay = min_pay;
  for (const auto [prob, pay] : dist) {
    if (pay < min_pay) {
      min_pay = pay;
    }
    if (pay > max_pay) {
      max_pay = pay;
    }
  }
  const int table_size = max_pay - min_pay + 1;
  std::unique_ptr<double[]> table(new double[table_size]());
  for (const auto [prob, pay] : dist) {
    table[pay - min_pay] += prob;
  }

  dist.clear();
  for (int i = 0; i < table_size; ++i) {
    if (table[i] > 0.0) {
      dist.emplace_back(table[i], min_pay + i);
    }
  }
}

PayDistribution succession(const PayDistribution &first,
                           const PayDistribution &second) {
  PayDistribution result;
  result.reserve(first.size() * second.size());
  for (const ProbPay &f : first) {
    for (const ProbPay &s : second) {
      result.emplace_back(f.probability * s.probability, f.payoff + s.payoff);
    }
  }

  normalize(result);
  return result;
}

PayDistribution repeat(const PayDistribution &wager, unsigned int n) {
  if (n == 0) {
    return {{1.0, 0}};
  }
  PayDistribution power = wager;
  PayDistribution result;
  unsigned int bits = n;
  for (;;) {
    if (bits & 1) {
      if (result.empty()) {
        result = power;
      } else {
        result = succession(result, power);
      }
    }
    bits >>= 1;
    if (bits == 0) {
      break;
    }
    power = succession(power, power);
  }
  return result;
}

PayDistribution merge(const PayDistribution &first,
                      const PayDistribution &second) {
  PayDistribution result;
  result.reserve(first.size() + second.size());
  auto it1 = first.begin(), it2 = second.begin();

  while (it1 != first.end() && it2 != second.end()) {
    if (it1->payoff == it2->payoff) {
      result.emplace_back(it1->probability + it2->probability, it1->payoff);
      ++it1;
      ++it2;
    } else if (it1->payoff < it2->payoff) {
      result.push_back(*it1++);
    } else {
      result.push_back(*it2++);
    }
  }

  // Append the remaining elements of the input that was not exhausted.
  result.insert(result.end(), it1, first.end());
  result.insert(result.end(), it2, second.end());

  return result;
}
