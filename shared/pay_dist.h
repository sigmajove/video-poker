#include <limits>
#include <vector>

struct ProbPay {
  ProbPay(double prob, int pay) : probability(prob), payoff(pay) {}
  ProbPay(double prob, double pay);
  friend bool operator==(const ProbPay &lhs, const ProbPay &rhs) {
    return lhs.payoff == rhs.payoff && lhs.probability == rhs.probability;
  }

  double probability;
  int payoff;
};

// The probabilites of a pay distribution must add to one.
// Hence, they always have at least one element.
class PayDistribution {
 public:
  PayDistribution()
      : cutoff_(std::numeric_limits<int>::max()), cutoff_prob_(0) {}

  PayDistribution(int cutoff, double cutoff_prob,
                  const std::vector<ProbPay> &dist)
      : cutoff_(cutoff), cutoff_prob_(cutoff_prob), dist_(dist) {}

  PayDistribution(const std::vector<ProbPay> &dist)
      : cutoff_(std::numeric_limits<int>::max()),
        cutoff_prob_(0),
        dist_(dist) {}

  friend bool operator==(const PayDistribution &lhs,
                         const PayDistribution &rhs) {
    return lhs.cutoff_ == rhs.cutoff_ && lhs.cutoff_prob_ == rhs.cutoff_prob_ &&
           lhs.dist_ == rhs.dist_;
  }

  // A constant view of the distribution.
  const std::vector<ProbPay> &distribution() const { return dist_; }
  int cutoff() const { return cutoff_; }
  double cutoff_prob() const { return cutoff_prob_; }

  // Returns the expected value of the distribution.
  // If there are cutoff values, the result is only an approximation.
  double expected() const;

  // Returns the sum of the probabilities in the distribution.
  // It should be 1.0 modulo floating point roundoff errors.
  double total_prob() const;

  // Converts a distribution to one with strictly increasing payoffs with
  // no duplications.
  void normalize();

  // Multiply all the probabilities by prob.
  void scale(double prob);

  // The aggregate pay distribution of making the first wager and then
  // (independently) making the second one.
  friend PayDistribution succession(const PayDistribution &first,
                                    const PayDistribution &second);

  // The pay distribution of repeating a wager n independent times.
  friend PayDistribution repeat(const PayDistribution &wager, unsigned int n);

  // Assumes the inputs are normalized.
  // I don't think this is called any more.
  friend PayDistribution merge(const PayDistribution &first,
                               const PayDistribution &second);

 private:
  // Don't keep details past cutoff.
  int cutoff_;

  // The probability that the payoff is cutoff or more.
  double cutoff_prob_;

  // The probability of payoffs less than cutoff.
  std::vector<ProbPay> dist_;
};
