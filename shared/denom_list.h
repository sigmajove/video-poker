#include <array>

#include "vpoker.h"

class denom_list {
 public:
  explicit denom_list(int *const left) : left_(left), avail_{} {};

  inline void add(int denom) { avail_[left_[denom]]++; }
  inline void remove(int denom) { avail_[left_[denom]]--; }
  // The client must not add the same denomination twice,
  // and must only remove denominations that have been
  // added.  There is no checking of these restrictions.

  int no_pair(int n);
  // returns the number of ways of selecting n cards that do
  // not pair up (that is only one of each denomination).

  int one_pair(int n);
  int two_pair(int n);
  int triple(int n);
  int full_house(int n);
  int quad(int n);

  int multi(int m, int n);
  // m = 2 => one_pair
  // m = 3 => triple, etc.

 private:
  const int *left_;

  std::array<int, num_suits + 1> avail_;
  // For each denomination added, we need to know the number of
  // that kind of card is available.  This information is stored
  // inverted-- for each number that might be available 0, 1, 2, 3, 4,
  // we keep the number of denominations that have that availability.
};
