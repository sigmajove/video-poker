// Program to compute the house edge in video poker variations

#include <cstddef>
#include <iostream>

#include "read_file.h"

int main() {
  const auto contents = read_file(R"(C:\users\sigma\documents\edge_test.txt)");
  if (!contents) {
    return 1;
  }

  int pay_table[static_cast<std::size_t>(last_pay) + 1];
  std::copy(contents->pay_table.begin(), contents->pay_table.end(), pay_table);

  std::cout << "Game name " << contents->game_name << "\n";

  for (std::size_t i = 0; i <= last_pay; i++) {
    if (int val = pay_table[i]) {
      std::cout << payoff_image[i] << ": " << val << "\n";
    }
  }

  // Enough information to get started.
  vp_game the_game(contents->game_name.c_str(), contents->kind, contents->high,
                   &pay_table);

  return 0;
}
