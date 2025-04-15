#pragma once

#include "enum_match.h"

void parse_line (const char *line,
                 int wild_cards,
                 strategy_line &result);
