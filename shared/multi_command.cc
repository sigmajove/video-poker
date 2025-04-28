#include "multi_command.h"

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

// This function parses a command line of the form
//
//   multi [nnn [mmm]]
//
// where nnn and mmm are positive integer defaulting to 1.
//
// I wanted to do this without dragging complicated mechanisms like
// the Unix Lex or Google regular expressions. Perplexity suggested
// using std::istringstream, but I had to do a lot of fiddling to
// get all the edge cases correct.
//
// I probably should have used C++'s std::regex_search instead.

std::optional<std::pair<int, int>> multi_command(const std::string &line) {
  int arg1 = 1;  // default value
  int arg2 = 1;  // default value

  // Strip trailing spaces off the line.
  // std::istringstream doesn't cope with trailing spaces well.
  std::string stripped = line;
  stripped.erase(stripped.find_last_not_of(" ") + 1);
  std::istringstream iss(stripped);

  std::string command;
  iss >> command;
  if (command != "multi") {
    return std::nullopt;
  }
  if (!iss.eof()) {
    iss >> arg1;
    if (arg1 < 0) {
      return std::nullopt;
    }
    if (!iss.eof()) {
      iss >> arg2;
      if (arg2 < 0) {
        return std::nullopt;
      }
    }
  }

  if (!iss.eof() || iss.fail()) {
    return std::nullopt;
  }
  return std::make_pair(arg1, arg2);
}
