#include "pay_dist.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
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

double PayDistribution::expected() const {
  return std::accumulate(dist_.begin(), dist_.end(), cutoff_prob_ * cutoff_,
                         [](float sum, const ProbPay &pp) {
                           return sum + pp.probability * pp.payoff;
                         });
}

double PayDistribution::total_prob() const {
  return std::accumulate(
      dist_.begin(), dist_.end(), cutoff_prob_,
      [](float sum, const ProbPay &pp) { return sum + pp.probability; });
}

void PayDistribution::scale(double prob) {
  for (ProbPay &pp : dist_) {
    pp.probability *= prob;
  }
  cutoff_prob_ *= prob;
}

void PayDistribution::normalize() {
  if (dist_.empty()) {
    return;
  }

  int min_pay = dist_[0].payoff;
  int max_pay = min_pay;
  for (const auto [prob, pay] : dist_) {
    if (pay < min_pay) {
      min_pay = pay;
    }
    if (pay > max_pay) {
      max_pay = pay;
    }
  }
  const int table_size = max_pay - min_pay + 1;
  std::unique_ptr<double[]> table(new double[table_size]());
  for (const auto [prob, pay] : dist_) {
    table[pay - min_pay] += prob;
  }

  dist_.clear();
  for (int i = 0; i < table_size; ++i) {
    if (table[i] > 0.0) {
      dist_.emplace_back(table[i], min_pay + i);
    }
  }
}

PayDistribution succession(const PayDistribution &first,
                           const PayDistribution &second) {
  PayDistribution result;
  result.cutoff_ = std::min(first.cutoff_, second.cutoff_);

  // The probability that that either first or second is past
  // the cutoff_. Then the result is past the cutoff_.
  result.cutoff_prob_ = first.cutoff_prob_ + second.cutoff_prob_ -
                        first.cutoff_prob_ * second.cutoff_prob_;

  // Now examine all the cases where both distributions are less
  // than their respective cutoffs.
  result.dist_.reserve(first.dist_.size() * second.dist_.size());
  for (const ProbPay &f : first.dist_) {
    for (const ProbPay &s : second.dist_) {
      const double prob = f.probability * s.probability;
      const int pay = f.payoff + s.payoff;
      if (pay >= result.cutoff_) {
        result.cutoff_prob_ += prob;
      } else {
        result.dist_.emplace_back(prob, pay);
      }
    }
  }

  result.normalize();
  return result;
}

PayDistribution repeat(const PayDistribution &wager, unsigned int n) {
  PayDistribution result;
  if (n == 0) {
    result.cutoff_ = wager.cutoff_;
    result.cutoff_prob_ = 0.0;
    result.dist_ = {ProbPay(1.0, 0)};
  } else {
    PayDistribution power = wager;
    unsigned int bits = n;
    bool first = true;
    for (;;) {
      if (bits & 1) {
        if (first) {
          result = power;
          first = false;
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
  }
  return result;
}

PayDistribution merge(const PayDistribution &first,
                      const PayDistribution &second) {
  PayDistribution result;
  result.cutoff_ = std::min(first.cutoff_, second.cutoff_);
  result.cutoff_prob_ = first.cutoff_prob_ + second.cutoff_prob_;

  result.dist_.reserve(first.dist_.size() + second.dist_.size());
  auto it1 = first.dist_.begin(), it2 = second.dist_.begin();

  while (it1 != first.dist_.end() && it2 != second.dist_.end()) {
    if (it1->payoff == it2->payoff) {
      result.dist_.emplace_back(it1->probability + it2->probability,
                                it1->payoff);
      ++it1;
      ++it2;
    } else if (it1->payoff < it2->payoff) {
      result.dist_.push_back(*it1++);
    } else {
      result.dist_.push_back(*it2++);
    }
  }

  // Append the remaining elements of the input that was not exhausted.
  result.dist_.insert(result.dist_.end(), it1, first.dist_.end());
  result.dist_.insert(result.dist_.end(), it2, second.dist_.end());

  // Handle the unexpected case where first and second have different cutoffs.
  while (not result.dist_.empty() &&
         result.dist_.back().payoff >= result.cutoff_) {
    result.cutoff_prob_ += result.dist_.back().probability;
    result.dist_.pop_back();
  }

  return result;
}
