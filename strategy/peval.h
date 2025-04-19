#pragma once

#include "enum_match.h"

void eval_strategy(const vp_game &game, StrategyLine *lines[],
                   const char *filename);

void prune_strategy(const vp_game &game, StrategyLine *lines[],
                    int *strategy_length, const char *filename);

void check_union(const vp_game &game, StrategyLine *lines[],
                 const char *filename);

void box_score(const vp_game &game, StrategyLine *lines[],
               const char *filename);

void half_life(const vp_game &game, StrategyLine *lines[],
               const char *filename);

void optimal_box_score(const vp_game &game, const char *filename);
