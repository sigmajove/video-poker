#include <stdio.h>

#include "strategy.h"

int main (int argc, char *argv[]) {
  if (argc == 3) {
    parser (argv[1], argv[2]);
  } else if (argc == 2) {
    parser (argv[1]);
  } else {
    printf ("Usage: %s filename\n", argv[0]);
  }

  return 0;
}
