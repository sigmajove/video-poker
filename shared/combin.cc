#include "combin.h"

#include <stdexcept>

int Chooser::do_choose(int x, int y) {
  // Given x things, how many ways are there of choosing y of them?
  if (x < 0 || y < 0) {
    throw std::runtime_error("choose not defined for negatives");
  }

  if (y > x) {
    return 0;
  }

  {
    int other = x - y;
    if (other < y) {
      y = other;
    }
  }

  if (y == 0) {
    return 1;
  }
  if (y == 1) {
    return x;
  }

  int result = x;

  result *= --x;
  result >>= 1;

  for (int j = 3; j <= y; j++) {
    result *= --x;
    result /= j;
  }

  return result;
}

Chooser::Chooser() {
  for (int x = 0; x < x_size; x++)
    for (int y = 0; y < y_size; y++) {
      table[x][y] = do_choose(x, y);
    }
}

Chooser combin;
