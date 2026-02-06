// Program to compute the house edge in video poker variations

#include <cstddef>
#include <iostream>
#include <string>

#include "eval_game.h"
#include "read_file.h"

int main(int argc, const char* argv[]) {
  if (argc != 2) {
    std::cerr << "Missing filename argument\n";
    return 1;
  }
  const std::string filename(argv[1]);
  const auto contents = read_file(filename);
  if (!contents) {
    return 1;
  }

  int pay_table[static_cast<std::size_t>(last_pay) + 1];
  std::copy(contents->pay_table.begin(), contents->pay_table.end(), pay_table);

  for (std::size_t i = 0; i <= last_pay; i++) {
    if (int val = pay_table[i]) {
      std::cout << payoff_image[i] << ": " << val << "\n";
    }
  }

  vp_game the_game(contents->game_name.c_str(), contents->kind, contents->high,
                   &pay_table);
  pay_prob prob_pays;
  eval_game(the_game, prob_pays);

  return 0;
}
