#include <exception>
#include <iostream>

#include "strategy.h"

int main(int argc, char* argv[]) {
  try {
    if (argc == 3) {
      parser(argv[1], argv[2]);
    } else if (argc == 2) {
      parser(argv[1]);
    } else {
      std::cerr << "Wrong number of args\n";
      return 1;
    }
  } catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << '\n';
    return 1;
  } catch (...) {  // Catch-all handler (MUST be last)
    std::cerr << "Unknown exception occurred\n";
    return 1;
  }

  return 0;
}
