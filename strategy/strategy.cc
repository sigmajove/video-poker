#include <algorithm>
#include <map>
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

using line_list = std::vector<strategy_line>;

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

static strategy_line *get_image(line_list &pat) {
  int n = pat.size();
  strategy_line *result = new strategy_line[n];
  strategy_line *rover = result;

  for (int j = 0; j < n; j++) {
    *rover++ = pat.at(j);
  }

  pat.clear();
  return result;
}

const char *choose_file(const char *f1, const char *f2) { return f1 ? f1 : f2; }

static int parse_line_number;

void parser(const char *name, const char *output_file = 0) {
  FILE *input = NULL;
  const errno_t input_err = fopen_s(&input, name, "rt");

  if (input == NULL) {
    char buffer[81];
    strerror_s(buffer, input_err);
    printf("%s\n", buffer);
    printf("Could not open %s\n", name);
    return;
  }

  enum { ps_game_name, ps_command_line, ps_parsing } state = ps_game_name;

  enum {
    cm_haas,
    cm_order,
    cm_value,
    cm_eval,
    cm_union,
    cm_box_score,
    cm_half_life,
    cm_prune,
    cm_draft,
  } command_name;

  parse_line_number = 0;
  bool read_eof = false;

  int parse_buffer_size = 10;
  char *parse_buffer = new char[parse_buffer_size];
  line_list pat;

  // Parameters
  vp_game *the_game = nullptr;

  strategy_line *wild[5];  // At most 4 wild cards
  int wild_count[5];       // Number of lines in each strategy

  int current_wild = -1;

  {
    for (int j = 0; j <= 4; j++) {
      wild[j] = 0;
      wild_count[j] = 0;
    }
  }

  try {
  read_next_line: {
    int line_size = 0;

    for (;;) {
      char *buffer_tail = parse_buffer + line_size;

      if (read_eof) {
        goto done_reading_file;
      }

      if (fgets(buffer_tail, parse_buffer_size - line_size, input) == NULL) {
        read_eof = true;
        if (line_size == 0) {
          goto done_reading_file;
        }
        goto done_reading_line;
      }

      line_size += strlen(buffer_tail);

      if (parse_buffer[line_size - 1] == '\n') {
        parse_line_number += 1;
        parse_buffer[line_size - 1] = 0;
        goto done_reading_line;
      } else {
        int new_size = 2 * parse_buffer_size;
        char *new_buffer = new char[new_size];
        memcpy(new_buffer, parse_buffer, parse_buffer_size);
        delete parse_buffer;
        parse_buffer = new_buffer;
        parse_buffer_size = new_size;
        continue;
      }

    done_reading_line:
      // I've assembled one complete line
      char *comment = strchr(parse_buffer, '#');
      if (comment) {
        *comment = 0;

        if (comment > parse_buffer && comment[-1] == ';') {
          // This is a continuation line.
          // Keep reading

          line_size = (comment - 1) - parse_buffer;

          if (!read_eof) {
            continue;
          }
        }
      }
      break;
    }
  }

    {
      // Strip trailing blanks
      int n = strlen(parse_buffer);
      while (n > 0 && parse_buffer[n - 1] == ' ') {
        parse_buffer[n-- - 1] = 0;
      }
    }

    if (parse_buffer[0] == 0) {
      goto read_next_line;
    }

    // Now parse_buffer is a complete line, assembled from
    // multi-lines if needed, with any comment and trailing
    // blanks stripped.  Figure out what to do with the
    // shiny new line.

    switch (state) {
      case ps_game_name:

        the_game = vp_game::find(parse_buffer);
        if (!the_game) {
          printf("Unknown game name %s\n", parse_buffer);
          throw 0;
        }

        state = ps_command_line;
        goto read_next_line;

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
          printf("Unknown command name: %s\n", parse_buffer);
          throw 0;
        }
        state = ps_parsing;
        goto read_next_line;

      case ps_parsing:
        break;

      default:
        _ASSERT(0);
    }

    // This is the main loop.  Here is where I
    // parse a line.

    // Look for a % at the end of the line that
    // signals an option string to pass along to
    // the algorithms.

    char *line_options = 0;

    {
      char *options = strchr(parse_buffer, '%');
      if (options) {
        const int options_size = strlen(options + 1) + 1;
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
        pat.push_back(C_strategy_line());

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
      strategy_line temp;
      parse_line(parse_buffer, current_wild, temp);
      temp.options = line_options;
      pat.push_back(temp);
    }

    goto read_next_line;
  } catch (std::string msg) {
    printf("Error on line %d:\n", parse_line_number);
    puts(msg.c_str());
    putchar('\n');
    fclose(input);
    return;
  } catch (...) {
    printf("Processing terminated due to error\n");
    fclose(input);
    return;
  }

done_reading_file:
  fclose(input);

  pat.push_back(C_strategy_line());

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
