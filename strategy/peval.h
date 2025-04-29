#pragma once
#include <cstddef>

#include "enum_match.h"

void eval_strategy(const vp_game &game, StrategyLine *lines[],
                   const char *filename);

void multi_distribution(const vp_game &game, StrategyLine *lines[],
                        unsigned int num_lines, unsigned int num_games,
                        const char *filename);

void prune_strategy(const vp_game &game, StrategyLine *lines[],
                    std::size_t *strategy_length, const char *filename);

void check_union(const vp_game &game, StrategyLine *lines[],
                 const char *filename);

void box_score(const vp_game &game, StrategyLine *lines[],
               const char *filename);

void half_life(const vp_game &game, StrategyLine *lines[],
               const char *filename);

void optimal_box_score(const vp_game &game, const char *filename);
