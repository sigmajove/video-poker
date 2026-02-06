#include "read_file.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <string>
#include <unordered_set>
#include <utility>

#include "..\shared\vpoker.h"

const std::map<std::string, payoff_name> hand_name = {
    {"jacks or better", N_high_pair},
    {"kings or better", N_high_pair},
    {"two pair", N_two_pair},
    {"three of a kind", N_trips},
    {"straight", N_straight},
    {"flush", N_flush},
    {"full house", N_full_house},
    {"four of a kind", N_quads},
    {"four aces", N_quad_aces},
    {"four aces with 2-4", N_quad_aces_kicker},
    {"four 2-4", N_quad_low},
    {"four 2-4 with a-4", N_quad_low_kicker},
    {"straight flush", N_straight_flush},
    {"five of a kind", N_quints},
    {"wild royal flush", N_wild_royal},
    {"four deuces", N_four_deuces},
    {"royal flush", N_royal_flush}};

// These are the ones that need parens in the regular expression
const std::unordered_set<std::string> special = {
    "jacks or better", "kings or better", "four deuces"};

std::string make_pattern() {
  std::string keys;
  for (auto [name, _] : hand_name) {
    if (keys.size() > 0) {
      keys.append("|");
    }
    if (special.contains(name)) {
      keys.append(std::format("({})", name));
    } else {
      keys.append(name);
    }
  }
  return std::format(R"( *({}) +(\d{{1,4}}) *)", keys);
}

const std::regex name_line = std::regex(R"_( *"([^"]+)" *)_");
const std::regex file_line = std::regex(make_pattern());

std::optional<std::pair<payoff_name, std::uint16_t>> parse(
    const std::string& text, denom_value& high, game_kind& kind) {
  std::smatch match;
  if (std::regex_match(text, match, file_line)) {
    if (match[2].matched) {
      kind = GK_deuces_wild;
    } else if (match[3].matched) {
      high = jack;
    } else if (match[4].matched) {
      high = king;
    }
    return std::make_pair(
        hand_name.at(match[1]),
        static_cast<std::uint16_t>(std::stoul(match[5], nullptr, 10)));
  }
  return std::nullopt;
}

std::optional<FileContents> read_file(const std::string& filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    std::cerr << "Error: Could not open " << filename << "\n";
    return std::nullopt;
  }

  FileContents contents;
  std::fill(contents.pay_table.begin(), contents.pay_table.end(), 0);
  contents.kind = GK_no_wild;
  contents.high = jack;

  std::string line;
  int line_number = 0;
  std::array<bool, static_cast<std::size_t>(last_pay) + 1> used = {};

  while (std::getline(file, line)) {
    ++line_number;
    // Skip lines that are all blank.
    if (std::all_of(line.begin(), line.end(),
                    [](unsigned char ch) { return ch == ' '; })) {
      continue;
    }

    // Skip comment lines beginning with a pound sign.
    if (line.front() == '#') {
      continue;
    }
    if (contents.game_name.empty()) {
      std::smatch match;
      if (std::regex_match(line, match, name_line)) {
        contents.game_name = match[1];
      } else {
        std::cout << "Error on line " << line_number << "\n";
        return std::nullopt;
      }
    } else {
      const auto val = parse(line, contents.high, contents.kind);
      if (val) {
        const size_t index = static_cast<size_t>(val->first);
        if (used[index]) {
          std::cout << "Duplicate on line " << line_number << "\n";
          return std::nullopt;
        }
        used[index] = true;
        contents.pay_table[index] = val->second;
      } else {
        std::cout << "Error on line " << line_number << "\n";
        return std::nullopt;
      }
    }
  }
  return contents;
}
