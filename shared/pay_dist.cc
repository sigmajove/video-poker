#include "pay_dist.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
  // Sort the results by increasing payoff.
  if (dist.empty()) {
    return;
  }

  std::sort(dist.begin(), dist.end(), [](const ProbPay &a, const ProbPay &b) {
    return a.payoff < b.payoff;
  });

  // Add the probabilities of adjacent payoffs.
  PayDistribution::iterator out = dist.begin();
  bool first = true;
  for (const ProbPay &pp : dist) {
    if (first) {
      first = false;
    } else if (out->payoff == pp.payoff) {
      out->probability += pp.probability;
    } else {
      *++out = pp;
    }
  }
  dist.erase(out + 1, dist.end());
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
