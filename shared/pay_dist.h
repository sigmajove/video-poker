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
using PayDistribution = std::vector<ProbPay>;

// Converts a distribution to one with strictly increasing payoffs with
// no duplications.
void normalize(PayDistribution &dist);

// The aggregate pay distribution of making the first wager and then
// (independently) making the second one.
PayDistribution succession(const PayDistribution &first,
                           const PayDistribution &second);

// The pay distribution of repeating a wager n independent times.
PayDistribution repeat(const PayDistribution &wager, unsigned int n);

// Assumes the inputs are normalized.
PayDistribution merge(const PayDistribution &first,
                      const PayDistribution &second);
