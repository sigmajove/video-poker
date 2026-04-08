#include "denom_list.h"

#include <algorithm>
#include <cassert>

#include "combin.h"

int denom_list::no_pair(int n) {
  assert(n >= 0);
  // Everything is unrolled here; we know num_suits=4

  const int sum4 = avail_[4];
  const int sum3 = avail_[3] + sum4;
  const int sum2 = avail_[2] + sum3;
  const int sum1 = avail_[1] + sum2;

  if (n > sum1) return 0;
  // Make sure there are enough different denominations to select

  // The idea here is that we will select
  //
  //    n = j1 + j2 + j3 + j4
  //
  // cards, where j1 is the number of singleton denominations chosen,
  // j2 is the number of doubleton demominations chosen, and so on.
  //
  // For each quadruple (j1, j2, j3, j4), the number of ways to
  // select the cards is
  //
  //  Count (j1, j2, j3, j4) =
  //     choose (avail_ [1], j1) * 1**j1 *
  //     choose (avail_ [2], j2) * 2**j2 *
  //     choose (avail_ [3], j3) * 3**j3 *
  //     choose (avail_ [4], j4) * 4**j4
  //
  // The first subterm counts the number of subsets of demoninations
  // available, and the second counts the number of suit patterns
  // available for that subset.
  //
  // We iterate over all possible values of (j1, j2, j3, j4) and
  // sum up Count (j1, j2, j3, j4) for each.
  //
  // We find all possible values using four nested loops.
  // Since j1 + j2 + j3 + j4 = n, j1 is bounded below by
  // by n - avail_[j2] - avail_[j3] - avail_[j4].  Likewise,
  // j1 is bounded above by both avail_[1] and n.  There are
  // similar constraints at the next two nesting levels.
  // In the innermost level, j4 is determined by the equation
  // j4 = n - j3 - j2 - j1.

  int result = 0;

  const int min1 = std::max(n - sum2, 0);
  const int max1 = std::min(avail_[1], n);
  for (int j1 = min1; j1 <= max1; j1++) {
    int f1 = combin.choose(avail_[1], j1);
    n -= j1;

    const int min2 = std::max(n - sum3, 0);
    const int max2 = std::min(avail_[2], n);
    for (int j2 = min2; j2 <= max2; j2++) {
      const int f2 = combin.choose(avail_[2], j2) * (1 << j2) * f1;
      // (1<<j2) == 2**j2

      n -= j2;

      const int min3 = std::max(n - sum4, 0);
      const int max3 = std::min(avail_[3], n);
      for (int j3 = min3; j3 <= max3; j3++) {
        int power = 1;
        for (int jj = 0; jj < j3; jj++) power *= 3;
        // power == 3**j3

        const int f3 = combin.choose(avail_[3], j3) * power * f2;

        const int j4 = n - j3;
        assert(0 <= j4 && j4 <= avail_[4]);

        const int f4 = combin.choose(avail_[4], j4) * (1 << (2 * j4)) * f3;
        // (1<<(2*j4)) == 4**j4

        result += f4;
      }

      n += j2;
    }

    n += j1;
  }

  return result;
}

int denom_list::multi(int m, int n) {
  assert(m >= 1);

  if (m == 1) {
    return no_pair(n);
  }

  int result = 0;

  if (n >= m) {
    for (int j = m; j <= 4; j++) {
      if (avail_[j] != 0) {
        const int a = avail_[j];
        avail_[j]--;
        result += a * combin.choose(j, m) * no_pair(n - m);
        avail_[j]++;
      }
    }
  }

  return result;
}

int denom_list::one_pair(int n) {
  if (n < 2) return 0;

  int result = 0;

  if (avail_[2] != 0) {
    int a = avail_[2];
    avail_[2]--;
    result += a * no_pair(n - 2);
    avail_[2]++;
  }

  if (avail_[3] != 0) {
    int a = avail_[3];
    avail_[3]--;
    result += a * 3 * no_pair(n - 2);
    avail_[3]++;
  }

  if (avail_[4] != 0) {
    int a = avail_[4];
    avail_[4]--;
    result += a * 6 * no_pair(n - 2);
    avail_[4]++;
  }

  return result;
}

int denom_list::triple(int n) {
  if (n < 3) return 0;

  int result = 0;

  if (avail_[3] != 0) {
    int a = avail_[3];
    avail_[3]--;
    result += a * no_pair(n - 3);
    avail_[3]++;
  }

  if (avail_[4] != 0) {
    int a = avail_[4];
    avail_[4]--;
    result += a * 4 * no_pair(n - 3);
    avail_[4]++;
  }

  return result;
}

int denom_list::quad(int n) {
  if (n < 4) return 0;

  int result = 0;

  if (avail_[4] != 0) {
    int a = avail_[4];
    avail_[4]--;
    result += a * no_pair(n - 4);
    avail_[4]++;
  }

  return result;
}

int denom_list::two_pair(int n) {
  if (n < 4) return 0;

  int result = 0;

  if (avail_[2] >= 2) {
    int a = avail_[2];
    avail_[2] -= 2;
    result += combin.choose(a, 2) * no_pair(n - 4);
    avail_[2] += 2;
  }

  if (avail_[3] >= 2) {
    int a = avail_[3];
    avail_[3] -= 2;
    result += combin.choose(a, 2) * (3 * 3) * no_pair(n - 4);
    avail_[3] += 2;
  }

  if (avail_[4] >= 2) {
    int a = avail_[4];
    avail_[4] -= 2;
    result += combin.choose(a, 2) * (6 * 6) * no_pair(n - 4);
    avail_[4] += 2;
    ;
  }

  if (avail_[2] > 0 && avail_[3] > 0) {
    const int a = avail_[2];
    const int b = avail_[3];

    avail_[2]--;
    avail_[3]--;
    result += a * b * 3 * no_pair(n - 4);
    avail_[2]++;
    avail_[3]++;
  }

  if (avail_[2] > 0 && avail_[4] > 0) {
    const int a = avail_[2];
    const int b = avail_[4];

    avail_[2]--;
    avail_[4]--;
    result += a * b * 6 * no_pair(n - 4);
    avail_[2]++;
    avail_[4]++;
  }

  if (avail_[3] > 0 && avail_[4] > 0) {
    const int a = avail_[3];
    const int b = avail_[4];

    avail_[3]--;
    avail_[4]--;
    result += a * b * (3 * 6) * no_pair(n - 4);
    avail_[3]++;
    avail_[4]++;
  }

  return result;
}

int denom_list::full_house(int n) {
  if (n < 5) return 0;

  int result = 0;

  if (avail_[3] >= 2) {
    result += avail_[3] * (avail_[3] - 1) * 3;
  }

  if (avail_[4] >= 2) {
    result += avail_[4] * (avail_[4] - 1) * (4 * 6);
  }

  if (avail_[2] > 0 && avail_[3] > 0) {
    result += avail_[2] * avail_[3];
  }

  if (avail_[2] > 0 && avail_[4] > 0) {
    result += avail_[2] * avail_[4] * 4;
  }

  if (avail_[3] > 0 && avail_[4] > 0) {
    result += avail_[3] * avail_[4] * ((3 * 4) + 6);
  }

  return result;
}
