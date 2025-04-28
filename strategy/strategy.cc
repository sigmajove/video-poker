#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "combin.h"
#include "enum_match.h"
#include "kept.h"
#include "new_hand_iter.h"
#include "parse_line.h"
#include "peval.h"
#include "pstrat.h"
#include "vpoker.h"

struct domain {
  card hand[5];
  const int hand_size;
  domain(int size) : hand_size(size) { _ASSERT(size <= 5); }
  domain &operator=(const domain &d);
  bool operator<(const domain &y) const;
  bool operator==(const domain &y) const;
};

domain &domain::operator=(const domain &d) {
  _ASSERT(hand_size == d.hand_size);
  for (int j = 0; j < hand_size; j++) {
    hand[j] = d.hand[j];
  }
  return *this;
}

bool domain::operator<(const domain &y) const {
  if (hand_size < y.hand_size) {
    return true;
  }
  if (hand_size > y.hand_size) {
    return false;
  }

  for (int j = 0; j < hand_size; j++) {
    if (hand[j] < y.hand[j]) {
      return true;
    }
    if (hand[j] > y.hand[j]) {
      return false;
    }
  }

  return false;
}

bool domain::operator==(const domain &y) const {
  if (hand_size != y.hand_size) {
    return false;
  }
  if (hand_size > y.hand_size) {
    return false;
  }

  for (int j = 0; j < hand_size; j++) {
    if (hand[j] != y.hand[j]) {
      return false;
    }
  }

  return true;
}

void canonicalize(domain &d, domain &best_name) {
  bool first = 1;

  int suit_map[4];
  for (int s0 = 0; s0 < 4; s0++) {
    suit_map[0] = s0;

    for (int s1 = 0; s1 < 4; s1++)
      if (s1 != s0) {
        suit_map[1] = s1;

        for (int s2 = 0; s2 < 4; s2++)
          if (s2 != s1 && s2 != s0) {
            suit_map[2] = s2;
            suit_map[3] = 0 + 1 + 2 + 3 - s0 - s1 - s2;

            domain rename(d.hand_size);
            for (int j = 0; j < rename.hand_size; j++) {
              card c = d.hand[j];
              rename.hand[j] = make_card(pips(c), suit_map[suit(c)]);
            }
            std::sort<card *>(rename.hand, rename.hand + rename.hand_size);

            if (first || rename < best_name) {
              best_name = rename;
              first = false;
            }
          }
      }
  }
}

void oej_canonicalize(domain &d, domain &best_name, int wild_cards) {
  bool first = 1;

  int suit_map[4];

  for (int s0 = 0; s0 < (wild_cards == 1 ? 1 : 2); s0++) {
    suit_map[0] = s0;
    suit_map[1] = 0 + 1 - s0;

    for (int s1 = 2; s1 < 4; s1++) {
      suit_map[2] = s1;
      suit_map[3] = 2 + 3 - s1;

      domain rename(d.hand_size);
      for (int j = 0; j < d.hand_size; j++) {
        card c = d.hand[j];
        rename.hand[j] = make_card(pips(c), suit_map[suit(c)]);
      }
      std::sort<card *>(rename.hand, rename.hand + rename.hand_size);

      if (first || rename < best_name) {
        best_name = rename;
        first = false;
      }
    }
  }
}

static StrategyLine *get_image(std::vector<StrategyLine> &pat) {
  int n = pat.size();
  StrategyLine *result = new StrategyLine[n];
  StrategyLine *rover = result;

  for (int j = 0; j < n; j++) {
    *rover++ = pat.at(j);
  }

  pat.clear();
  return result;
}

// It seemed like a good idea to pull this out as a helper function.
// But it has a lot of parameters. Maybe introduce a class?
void parse_strategy_line(char *parse_buffer, std::vector<StrategyLine> &pat,
                         int &current_wild, StrategyLine *(&wild)[5],
                         int (&wild_count)[5]) {
  // Look for a % at the end of the line that
  // signals an option string to pass along to
  // the algorithms.

  char *line_options = 0;

  {
    char *options = strchr(parse_buffer, '%');
    if (options) {
      const int options_size = strlen(options + 1) + 1;

      // TODO: fix this storage leak.
      line_options = new char[options_size];

      strcpy_s(line_options, options_size, options + 1);

      do {
        *options-- = 0;
      } while (options >= parse_buffer && *options == ' ');
    }
  }

  if (parse_buffer[0] == 0) {
    // ignore it, it's a blank line
  } else if ('0' <= parse_buffer[0] && parse_buffer[0] <= '4' &&
             (strcmp(parse_buffer + 1, " Deuces") == 0 ||
              strcmp(parse_buffer, "1 Deuce") == 0)) {
    // Deuce Divider
    if (pat.size() != 0) {
      pat.push_back(StrategyLine());

      if (current_wild == -1) {
        printf("Inconsistent deuce headers\n");
        pat.clear();
      }
      if (wild[current_wild]) {
        printf("Duplicate deuce header for %d\n", current_wild);
        pat.clear();
      } else {
        wild_count[current_wild] = pat.size();
        wild[current_wild] = get_image(pat);
      }
    }

    current_wild = parse_buffer[0] - '0';
  } else {
    StrategyLine temp = parse_line(parse_buffer, current_wild);
    temp.options = line_options;
    pat.push_back(temp);
  }
}

const char *choose_file(const char *f1, const char *f2) { return f1 ? f1 : f2; }

void parser(const char *name, const char *output_file = 0) {
  std::ifstream infile(name);
  if (!infile.is_open()) {
    printf("Could not open %s\n", name);
  }

  enum { ps_game_name, ps_command_line, ps_parsing } state = ps_game_name;

  enum {
    cm_haas,
    cm_order,
    cm_value,
    cm_eval,
    cm_multi,
    cm_union,
    cm_box_score,
    cm_half_life,
    cm_prune,
    cm_draft,
  } command_name;
  int command_arg = 0;

  int parse_line_number = 0;

  std::vector<StrategyLine> pat;

  // Parameters
  vp_game *the_game = nullptr;

  StrategyLine *wild[5];  // At most 4 wild cards
  int wild_count[5];      // Number of lines in each strategy

  int current_wild = -1;

  {
    for (int j = 0; j <= 4; j++) {
      wild[j] = 0;
      wild_count[j] = 0;
    }
  }

  for (;;) {
    std::string line;
    if (!std::getline(infile, line)) {
      if (state != ps_parsing) {
        throw std::runtime_error("Incomplete strategy file");
      }
      break;
    }
    ++parse_line_number;

    // Set pos to the length of the line, ignoring any comment.
    std::size_t pos = line.find('#');
    if (pos == std::string::npos) {
      // There is no comment.
      pos = line.size();
    } else {
      line[pos] = '\0';
    }

    // Replace trailing spaces with nulls.
    for (;;) {
      if (pos == 0) {
        // Empty line
        break;
      }
      --pos;
      if (line[pos] != ' ') {
        break;
      }
      line[pos] = '\0';
    }
    if (pos == 0) {
      // Read the next line.
      continue;
    }

    // Now line has any comment and trailing blanks stripped.
    // We treat parse_buffer as a C-style string, stopping at
    // the first null character, which we might have inserted.
    char *const parse_buffer = line.data();

    switch (state) {
      case ps_game_name:
        the_game = vp_game::find(parse_buffer);
        if (!the_game) {
          throw std::runtime_error(
              std::format("Unknown game name {}", parse_buffer));
        }

        state = ps_command_line;
        break;

      case ps_command_line:
        // Process game command
        // Okay this is admittedly pretty klunky!

        if (strcmp(parse_buffer, "haas") == 0) {
          command_name = cm_haas;
        } else if (strcmp(parse_buffer, "order") == 0) {
          command_name = cm_order;
        } else if (strcmp(parse_buffer, "value") == 0) {
          command_name = cm_value;
        } else if (strcmp(parse_buffer, "eval") == 0) {
          command_name = cm_eval;
        } else if (strncmp(parse_buffer, "multi ", 6) == 0 &&
                   (command_arg = atoi(parse_buffer + 6)) > 0) {
          command_name = cm_multi;
        } else if (strcmp(parse_buffer, "union") == 0) {
          command_name = cm_union;
        } else if (strcmp(parse_buffer, "box score") == 0) {
          command_name = cm_box_score;
        } else if (strcmp(parse_buffer, "half life") == 0) {
          command_name = cm_half_life;
        } else if (strcmp(parse_buffer, "prune") == 0) {
          command_name = cm_prune;
        } else if (strcmp(parse_buffer, "game box") == 0) {
          optimal_box_score(*the_game,
                            choose_file(output_file, "game_box.txt"));
          return;
        } else if (strcmp(parse_buffer, "draft") == 0) {
          command_name = cm_draft;
        } else {
          throw std::runtime_error(std::format("Bad command {}", parse_buffer));
        }
        state = ps_parsing;
        break;

      case ps_parsing:
        parse_strategy_line(parse_buffer, pat, current_wild, wild, wild_count);
        break;

      default:
        throw std::runtime_error("Enum not handled");
    }
  }

  infile.close();

  pat.push_back(StrategyLine());

  // The strategy file is divided into sections separated by
  // "n Deuces" lines. These are read and handled by parse_strategy_line
  // But the last section does not have a delimiter. So we finalize
  // everything here.
  if (current_wild == -1) {
    wild_count[0] = pat.size();
    wild[0] = get_image(pat);
  } else if (wild[current_wild]) {
    printf("Duplicate wild header for %d\n", current_wild);
    pat.clear();
  } else {
    wild_count[current_wild] = pat.size();
    wild[current_wild] = get_image(pat);
    int N = game_parameters(*the_game).number_wild_cards;

    for (int j = 0; j <= N; j++) {
      if (wild[j] == 0) {
        printf("Missing wild header for %d\n", j);
        return;
      }
    }
  }

  switch (command_name) {
    case cm_haas:
      find_strategy(*the_game, choose_file(output_file, "haas.txt"), wild, true,
                    false);
      break;

    case cm_order:
      find_strategy(*the_game, choose_file(output_file, "order.txt"), wild,
                    false, false);
      break;

    case cm_value:
      find_strategy(*the_game, choose_file(output_file, "value.txt"), wild,
                    false, true);
      break;

    case cm_eval:
      eval_strategy(*the_game, wild, choose_file(output_file, "report.txt"));
      break;

    case cm_multi:
      multi_strategy(*the_game, wild, static_cast<unsigned int>(command_arg),
                     choose_file(output_file, "multi.txt"));
      break;

    case cm_prune:
      prune_strategy(*the_game, wild, wild_count,
                     choose_file(output_file, "prune.txt"));
      break;

    case cm_union:
      check_union(*the_game, wild, choose_file(output_file, "union.txt"));
      break;

    case cm_box_score:
      box_score(*the_game, wild, choose_file(output_file, "box_score.txt"));
      break;

    case cm_half_life:
      half_life(*the_game, wild, choose_file(output_file, "half_life.txt"));
      break;

    case cm_draft:
      draft(*the_game, choose_file(output_file, "draft.txt"));
      break;

    default:
      _ASSERT(0);
  }
}
