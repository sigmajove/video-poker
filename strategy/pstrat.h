#pragma once

#include "vpoker.h"
#include "enum_match.h"

extern void find_strategy (const vp_game& game,
                           const char *filename,
                           StrategyLine *lines[],
                           bool print_haas,
                           bool print_value);

void draft(const vp_game& game, const char *filename);
