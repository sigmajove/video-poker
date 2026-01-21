#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

// Pay table for 9-6 Jacks or Better (optimum play)
const std::vector<std::pair<double, int>> pay_table = {
    {0x1.1748f7e5d1781p-1, 0},   {0x1.b7760e67f1a8fp-3, 1},
    {0x1.08c62a2d1af26p-3, 2},   {0x1.30f0f20250250p-4, 3},
    {0x1.7028455de7e5ep-7, 4},   {0x1.6743eb39c271ap-7, 6},
    {0x1.79396fb0bfdffp-7, 9},   {0x1.35a6ec6f3b9f0p-9, 25},
    {0x1.ca7f3edc714fbp-14, 50}, {0x1.a5268d541aaffp-16, 800},
};

int one_play(std::function<double()> rand) {
  const double rand_value = rand();
  double running = 0;
  for (auto iter = pay_table.begin();; ++iter) {
    running += iter->first;
    if (running >= rand_value || &*iter == &pay_table.back()) {
      return iter->second;
    }
  }
}

static constexpr std::size_t kNumGames = 6160;
static constexpr std::size_t kLowerBound = kNumGames - 500;
static constexpr std::size_t kUpperBound = kNumGames + 1000;

void trial() {
  std::mt19937 rng(std::random_device{}());
  // std::mt19937 has an extremely long period. The generator produces a
  // sequence of numbers that only repeats after generating approximately
  // 1e6001 numbers.

  std::uniform_real_distribution<double> dist(0.0, 1.0);
  const auto gen_rand = [&rng, &dist]() { return dist(rng); };

  std::size_t min_win = std::numeric_limits<std::size_t>::max();
  std::size_t max_win = 0;

  auto start = std::chrono::steady_clock::now();

  std::size_t counter = 0;
  std::size_t under = 0;
  std::size_t over = 0;
  std::array<std::size_t, kUpperBound - kLowerBound> histo{/*init to */};
  for (;;) {
    std::size_t sum = 0;
    for (std::size_t i = 0; i < kNumGames; ++i) {
      sum += static_cast<std::size_t>(one_play(gen_rand));
    };
    ++counter;
    if (sum < kLowerBound) {
      ++under;
    } else if (sum >= kUpperBound) {
      ++over;
    } else {
      ++histo[sum - kLowerBound];
    }
    if (sum >= max_win) {
      max_win = sum;
    }
    if (sum <= min_win) {
      min_win = sum;
    }

    std::chrono::duration<double> elapsed_seconds =
        std::chrono::steady_clock::now() - start;

    if (elapsed_seconds.count() >= 30.0) {
      break;  // Exit loop after 30 seconds
    }
  }

  std::cout << "under = " << under << "\n";
  std::cout << "over = " << over << "\n";
  std::cout << "min_win = " << min_win << "\n";
  std::cout << "max_win = " << max_win << "\n";

  const std::size_t interval = std::lround((kUpperBound - kLowerBound) / 100.0);
  std::cout << "interval = " << interval << "\n";

  std::size_t i = kLowerBound;
  for (std::size_t j = 0; j < 100; ++j) {
    std::size_t sum = 0;
    const double avg = 0.5 * static_cast<double>(i + i + interval - 1);
    for (std::size_t k = 0; k < interval; ++k) {
      sum += histo[i++ - kLowerBound];
    }
    // Normalize to probability
    if (sum != 0) {
      std::cout << "(" << avg - kNumGames << ", "
                << static_cast<double>(sum) / counter << "),\n";
    }
  }
}

void points() {
  std::mt19937 rng(std::random_device{}());
  // std::mt19937 has an extremely long period. The generator produces a
  // sequence of numbers that only repeats after generating approximately
  // 1e6001 numbers.

  std::uniform_real_distribution<double> dist(0.0, 1.0);
  const auto gen_rand = [&rng, &dist]() { return dist(rng); };

  std::ofstream out_file("c:/users/sigma/documents/points.txt");
  if (!out_file.is_open()) {
    throw std::runtime_error("Cannot create file");
  }
  for (std::size_t i = 0; i < 10000; ++i) {
    int sum = -static_cast<int>(kNumGames);
    for (std::size_t j = 0; j < kNumGames; ++j) {
      sum += one_play(gen_rand);
    }
    out_file << sum << "\n";
  }
  out_file.close();
}

int main() {
  try {
    points();
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  } catch (...) {
    std::cout << "Unknown exception" << std::endl;
  }

  return 0;
}
