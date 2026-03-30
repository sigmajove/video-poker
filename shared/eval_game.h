#pragma once

#include "vpoker.h"

double get_payback(const vp_game &game, pay_prob &prob_pays);
void eval_game(const vp_game &game, pay_prob &prob_pays);
