#pragma once
#include <cstddef>
#include <stdexcept>

class Chooser {
 public:
  Chooser();

  int choose(int x, int y) {
    if (x < 0 || y < 0) {
      throw std::runtime_error("choose not defined for negatives");
    }

    return (x < x_size && y < y_size) ? table[x][y] : do_choose(x, y);
  }

 private:
  static constexpr std::size_t x_size = 16;
  static constexpr std::size_t y_size = 16;

  // For all the values we expect to see, cache them in a table.
  // This optimization probably isn't necessary in 2025.
  int table[x_size][y_size];

  int do_choose(int x, int y);
};

extern Chooser combin;
