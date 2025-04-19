#include "parse_line.h"

#include <string.h>

#include <cstddef>
#include <string>
#include <vector>

#include "vpoker.h"

using patch_list = std::vector<int>;

class LineParser {
 public:
  LineParser(int wild_cards, std::vector<unsigned char> *output)
      : parse_wild_cards(wild_cards), parse_output(output) {}

  void parse_main(const char *line);

 private:
  void parse_error();
  bool match(const char *pat);
  void digit();
  void opt_inside();
  bool peek_n(const char *pat, int &mask);
  void opt_high();
  bool opt_digit();
  int card_value(char p);
  void card_range(int &mask);
  void denom();
  void sequence_op(int op_code, char *start, char *end);
  bool high_cards();
  bool low_cards();
  void or_phrase();
  void high_low(int op_code, const char *pos, const char *final);
  int one_card();
  void with_or_no(int op_code);
  void make_patch(int where);
  void or_finish(patch_list &else_list);
  void or_list(const char *final);
  void paren_clause();
  void paren_modifier();
  void opt_paren_modifiers();
  bool peek(const char *pat);
  bool is_card_name(char p);
  bool high_sequence();
  bool low_sequence();
  void sequence(bool is_no);
  void or_wrapper(int start, patch_list &else_list);
  void bracket_clause();
  void bracket_modifier();
  void opt_bracket_modifiers();
  void top_phrase();

  const int parse_wild_cards;
  std::vector<unsigned char> *const parse_output;

  const char *parse_ptr;
  const char *parse_input;
  int parse_digit;
};

void LineParser::parse_error() {
  int head = parse_ptr - parse_input;
  std::string msg;
  msg.append(parse_input, head);
  msg.append("/-->/");
  msg.append(parse_input + head);

  throw msg;
}

bool LineParser::match(const char *pat) {
  const int n = strlen(pat);
  if (strncmp(pat, parse_ptr, n) == 0) {
    parse_ptr += n;
    return true;
  }

  return false;
}

void LineParser::digit() {
  int w = parse_wild_cards <= 0 ? 0 : parse_wild_cards;

  switch (*parse_ptr) {
    case '2':
    case '3':
    case '4':
      parse_output->push_back(parse_digit = 2 + (parse_ptr[0] - '2') - w);
      if (parse_digit <= 1) {
        parse_error();
      }

      parse_ptr += 1;
      return;
  }

  parse_error();
}

void LineParser::opt_inside() {
  int reach;

  if (match(" i")) {
    reach = parse_digit + 1;
    if (reach > 5) {
      parse_error();
    }
  } else if (match(" di")) {
    reach = parse_digit + 2;
    if (reach > 5) {
      parse_error();
    }
  } else if (match(" ti")) {
    reach = parse_digit + 3;
    if (reach > 5) {
      parse_error();
    }
  } else if (match(" any")) {
    reach = 0;
  } else {
    reach = parse_digit;
  }

  parse_output->push_back(reach);
}

// Look for the pattern followed by the digit 0-5.
bool LineParser::peek_n(const char *pat, int &mask) {
  const int n = strlen(pat);
  if (strncmp(pat, parse_ptr, n) == 0) {
    switch (parse_ptr[n]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':

        mask |= (1 << (parse_ptr[n] - '0'));

        return true;
    }
  }

  return false;
}

void LineParser::opt_high() {
  int mask = 0;

  if (!peek_n(" h", mask)) {
    return;
  }
  parse_ptr += 3;

  if (peek_n(" or h", mask)) {
    parse_ptr += 6;
    goto write_mask;
  }

  if (peek_n(", h", mask)) {
    do {
      parse_ptr += 4;

      if (peek_n(", or h", mask)) {
        parse_ptr += 7;
        goto write_mask;
      }
    } while (peek_n(", h", mask));

    parse_error();
  }

write_mask:
  parse_output->push_back(pc_high_n);
  parse_output->push_back(mask);
}

bool LineParser::opt_digit() {
  int w = parse_wild_cards <= 0 ? 0 : parse_wild_cards;

  if (*parse_ptr == ' ') {
    switch (parse_ptr[1]) {
      case '2':
      case '3':
      case '4':
        parse_output->push_back(parse_digit = 2 + (parse_ptr[1] - '2') - w);
        parse_ptr += 2;

        if (parse_digit <= 1) {
          parse_error();
        }
        return true;
    }
  }

  parse_output->push_back(parse_digit = 5 - w);
  if (parse_digit <= 1) {
    parse_error();
  }

  return false;
}

int LineParser::card_value(char p) {
  if (p == 0) {
    return -1;
  }
  const char *rover = denom_image;
  for (;;) {
    if (*rover == p) {
      return rover - denom_image;
    }
    if (*rover == 0) {
      return -1;
    }
    rover += 1;
  }
}

void LineParser::card_range(int &mask) {
  if (match("Aces") || match("Ace")) {
    mask |= (1 << ace);
    return;
  }

  if (match("Kings") || match("King")) {
    mask |= (1 << king);
    return;
  }

  if (match("Queens") || match("Queen")) {
    mask |= (1 << queen);
    return;
  }

  if (match("Jacks") || match("Jack")) {
    mask |= (1 << jack);
    return;
  }

  int left_end = card_value(parse_ptr[0]);

  if (left_end < 0) {
    parse_error();
  }

  switch (parse_ptr[1]) {
    case 0:
    case ' ':
    case ',':
      parse_ptr += 1;
      mask |= (1 << left_end);
      return;

    case '-': {
      int right_end = card_value(parse_ptr[2]);
      if (right_end < 0) {
        parse_error();
      }

      switch (parse_ptr[3]) {
        case ' ':
        case 0:
        case ',':
          parse_ptr += 3;

          if (right_end == left_end) {
            parse_error();
          }

          if (left_end == ace) {
            mask |= (1 << ace);
            left_end = deuce;
          }

          if (right_end == ace) {
            mask |= (1 << ace);
            right_end = king;
          }

          if (right_end < left_end) {
            parse_error();
          }

          for (int j = left_end; j <= right_end; j++) {
            mask |= (1 << j);
          }

          return;
      }
    }
  }

  parse_error();
}

void LineParser::denom() {
  int mask = 0;

  card_range(mask);

  if (match(" or ")) {
    card_range(mask);
    goto write_mask;
  }

  if (match(", ")) {
    do {
      card_range(mask);

      if (match(", or ")) {
        card_range(mask);
        goto write_mask;
      }

    } while (match(", "));

    parse_error();
  }

write_mask:
  parse_output->push_back(mask & 0xff);
  parse_output->push_back((mask >> 8) & 0xff);
}

void LineParser::sequence_op(int op_code, char *start, char *end) {
  parse_output->push_back(op_code);
  parse_output->push_back(end - start);
  for (char *rover = start; rover < end; rover++) {
    parse_output->push_back(card_value(*rover));
  }
}

static char card_name[] = "AKQJT98765432A";

// One or more names in a row, in decending order,
// starting with a high card.
bool LineParser::high_cards() {
  char buffer[num_denoms];
  char *b = buffer;

  char *p = strchr(card_name, *parse_ptr);
  if (!p) {
    return false;
  }
  if (p > card_name + 3) {
    return false;
  }

  const char *q = parse_ptr;

  for (;;) {
    if (*p == *q) {
      *b++ = *p;
      p += 1;
      q += 1;

      switch (*q) {
        case 0:
        case ' ':
        case ',':
        case ')':
        case ']':
          parse_ptr = q;
          sequence_op(pc_these_n, buffer, b);
          return true;
      }
    } else {
      p += 1;
      if (!*p) {
        return false;
      }
    }
  }
}

// One or more card names that are in ascending order
// and contain no high cards.
bool LineParser::low_cards() {
  char buffer[num_denoms];
  char *b = buffer;

  char *p = strchr(card_name + 1, *parse_ptr);
  if (!p) {
    return false;
  }

  const char *q = parse_ptr;

  for (;;) {
    if (p <= card_name + 3) {
      return false;
    }

    if (*p == *q) {
      *b++ = *p;
      p -= 1;
      q += 1;

      switch (*q) {
        case 0:
        case ' ':
        case ',':
        case ')':
        case ']':
          parse_ptr = q;
          sequence_op(pc_these_n, buffer, b);
          return true;
      }
    } else {
      p -= 1;
    }
  }
}

void LineParser::or_phrase() {
  if (high_cards()) {
    return;
  }
  if (low_cards()) {
    return;
  }
  parse_error();
}

void LineParser::high_low(int op_code, const char *pos, const char *final) {
  int mask = 0;
  parse_output->push_back(op_code);
  denom();
  if (parse_ptr != pos) {
    parse_error();
  }
  parse_ptr = final;
}

int LineParser::one_card() {
  if (match("Aces") || match("Ace")) {
    return ace;
  }

  if (match("Kings") || match("King")) {
    return king;
  }

  if (match("Queens") || match("Queen")) {
    return queen;
  }

  if (match("Jacks") || match("Jack")) {
    return jack;
  }

  int result = card_value(parse_ptr[0]);

  if (result < 0) {
    parse_error();
  }

  parse_ptr += 1;

  return result;
}

void LineParser::with_or_no(int op_code) {
  parse_output->push_back(op_code);
  parse_output->push_back(one_card());
}

void LineParser::make_patch(int where) {
  int delta = parse_output->size() - (where + 1);
  if (delta < 0 || delta > 255) {
    printf("Patch offset error %d\n", delta);
    throw 0;
  }

  parse_output->at(where) = delta;
}

void LineParser::or_finish(patch_list &else_list) {
  for (patch_list::iterator p = else_list.begin(); p != else_list.end(); ++p) {
    make_patch(*p);
  }
}

void LineParser::or_list(const char *final) {
  patch_list else_list;
  bool first = true;

  for (;;) {
    parse_output->push_back(pc_or);
    int or_patch = parse_output->size();
    parse_output->push_back(0);

    or_phrase();

    parse_output->push_back(pc_else);
    else_list.push_back(parse_output->size());
    parse_output->push_back(0);

    make_patch(or_patch);

    if (first) {
      first = false;
      if (match(" or ")) {
        break;
      }
    } else {
      if (match(", or ")) {
        break;
      }
    }

    if (match(", ")) {
      continue;
    }
    parse_error();
  }

  or_phrase();

  or_finish(else_list);

  if (parse_ptr != final) {
    parse_error();
  }
}

void LineParser::paren_clause() {
  const char *final = strchr(parse_ptr, ')');
  if (!final) {
    parse_error();
  }

  const const char *cno = strstr(parse_ptr, ", no ");
  if (cno && cno < final) {
    final = cno;
  }

  const char *cwith = strstr(parse_ptr, ", with ");
  if (cwith && cwith < final) {
    final = cwith;
  }

  const char *hpos = final - 5;
  if (hpos > parse_ptr && strncmp(hpos, " high", 5) == 0) {
    high_low(pc_high_x, hpos, final);
  } else {
    const char *lpos = final - 4;
    if (lpos > parse_ptr && strncmp(lpos, " low", 4) == 0) {
      high_low(pc_low_x, lpos, final);
    } else {
      const char *opos = strstr(parse_ptr, " or ");
      if (opos && opos > parse_ptr && opos + 4 < final) {
        or_list(final);
      } else if (match("no ")) {
        with_or_no(pc_no_x);
      } else if (match("with ")) {
        with_or_no(pc_with_x);
      } else if (high_cards()) {
      } else if (low_cards()) {
      } else {
        parse_error();
      }
    }
  }

  if (parse_ptr != final) {
    parse_error();
  }
}

void LineParser::paren_modifier() {
  if (match("others)")) {
    return;  // (others) has no semantics
  }

  if (match("spades or hearts)")) {
    parse_output->push_back(pc_suited_x);
    parse_output->push_back(3);  // ouch. There should be a constant
    return;
  }

  if (match("clubs or diamonds)")) {
    parse_output->push_back(pc_suited_x);
    parse_output->push_back(3 * 4);  // ouch. There should be a constant
    return;
  }

  if (match("suited with jack)")) {
    if (parse_wild_cards != 1) {
      parse_error();
    }
    parse_output->push_back(pc_suited_x);
    parse_output->push_back(1);  // ouch. There should be a constant
    return;
  }

  if (match("other wild suit)")) {
    if (parse_wild_cards != 1) {
      parse_error();
    }
    parse_output->push_back(pc_suited_x);
    parse_output->push_back(2);  // ouch. There should be a constant
    return;
  }

  for (;;) {
    paren_clause();

    if (match(", ")) {
      continue;
    }
    if (match(")")) {
      break;
    }
    parse_error();
  }
}

void LineParser::opt_paren_modifiers() {
  while (match(" (")) {
    paren_modifier();
  }
}

bool LineParser::peek(const char *pat) {
  const int n = strlen(pat);
  if (strncmp(pat, parse_ptr, n) == 0) {
    return true;
  }

  return false;
}

bool LineParser::is_card_name(char p) {
  switch (p) {
    case 'A':
    case 'K':
    case 'Q':
    case 'J':
    case 'T':
    case '9':
    case '8':
    case '7':
    case '6':
    case '5':
    case '4':
    case '3':
    case '2':
      return true;
  }
  return false;
}

bool LineParser::high_sequence() {
  char buffer[num_denoms];
  char *b = buffer;

  const char *p = parse_ptr;
  if (!is_card_name(*p)) {
    return false;
  }
  *b++ = *p;

  char *q = card_name;
  while (*q != *p) {
    q += 1;
  }

  q += 1;
  p += 1;

  for (;;) {
    switch (*p) {
      case ' ':
      case ',':
      case ')':
      case ']':
      case '0':
        parse_ptr = p;
        sequence_op(0, buffer, b);
        return true;

      default:
        for (;;) {
          if (*q == *p) {
            *b++ = *p;
            p += 1;
            q += 1;
            break;
          }

          if (*q == 'A') {
            return false;
          }

          q += 1;
        }
    }
  }
}

bool LineParser::low_sequence() {
  char buffer[num_denoms];
  char *b = buffer;

  const char *p = parse_ptr;
  if (!is_card_name(*p)) {
    return false;
  }
  *b++ = *p;

  char *q = card_name + 1;
  while (*q != *p) {
    q += 1;
  }

  q -= 1;
  p += 1;

  for (;;) {
    switch (*p) {
      case ' ':
      case ',':
      case '-':
      case ')':
      case ']':
      case '+':
        parse_ptr = p;
        sequence_op(0, buffer, b);
        return true;

      case 0:
        return false;

      default:
        for (;;) {
          if (*q == *p) {
            *b++ = *p;
            p += 1;
            q -= 1;
            break;
          }

          if (*q == 'A') {
            return false;
          }

          q -= 1;
        }
    }
  }
}

void LineParser::sequence(bool is_no) {
  int patch_loc = parse_output->size();
  int op_code;

  if (high_sequence() || low_sequence()) {
    if (match(" suited")) {
      op_code = is_no ? pc_no_these_suited_n : pc_dsc_these_suited_n;
    } else if (match(" offsuit")) {
      op_code = is_no ? pc_no_these_offsuit_n : pc_dsc_these_offsuit_n;
    } else if (match(" onsuit")) {
      op_code = is_no ? pc_no_these_onsuit_n : pc_dsc_these_onsuit_n;
    } else if (match("+fp")) {
      op_code = is_no ? pc_no_these_n_and_fp : pc_dsc_these_n_and_fp;
    } else {
      op_code = is_no ? pc_no_these_n : pc_dsc_these_n;
    }

    parse_output->at(patch_loc) = op_code;

    return;
  }

  parse_error();
}

void LineParser::or_wrapper(int start, patch_list &else_list) {
  // Insert two dummy bytes
  parse_output->push_back(0);
  parse_output->push_back(0);

  // Now shift everything over two bytes
  for (int j = parse_output->size();;) {
    j -= 1;
    parse_output->at(j) = parse_output->at(j - 2);
    if (j - 2 == start) {
      break;
    }
  }

  parse_output->push_back(pc_else);
  else_list.push_back(parse_output->size());
  parse_output->push_back(0);

  parse_output->at(start) = pc_or;
  make_patch(start + 1);
}

void LineParser::bracket_clause() {
  if (match("least sp")) {
    parse_output->push_back(pc_least_sp);
    return;
  }
  bool is_no;

  if (match("no ")) {
    is_no = true;
  } else if (match("dsc ")) {
    is_no = false;
  } else {
    parse_error();
  }

  if (match("sp")) {
    parse_output->push_back(is_no ? pc_no_sp : pc_dsc_sp);
  } else if (match("gp")) {
    parse_output->push_back(is_no ? pc_no_gp : pc_dsc_gp);
    parse_output->push_back(1);
  } else if (match("paired gp")) {
    parse_output->push_back(is_no ? pc_no_gp : pc_dsc_gp);
    parse_output->push_back(2);
  } else if (match("fp")) {
    parse_output->push_back(is_no ? pc_no_fp : pc_dsc_fp);
  } else if (match("high pair")) {
    parse_output->push_back(is_no ? pc_no_pp : pc_dsc_pp);
  } else if (match("pair")) {
    parse_output->push_back(is_no ? pc_no_pair : pc_dsc_pair);
  } else if (match("<= ")) {
    parse_output->push_back(is_no ? pc_no_le_x : pc_dsc_le_x);
    parse_output->push_back(one_card());
  } else if (match(">= ")) {
    parse_output->push_back(is_no ? pc_no_ge_x : pc_dsc_ge_x);
    parse_output->push_back(one_card());
  } else {
    patch_list else_list;
    int start = parse_output->size();

    sequence(is_no);

    if (match(" or ")) {
      if (!is_no) {
        or_wrapper(start, else_list);
      }
      sequence(is_no);
    } else if (!peek(", no ") && !peek(", dsc ") && match(", ")) {
      if (!is_no) {
        or_wrapper(start, else_list);
      }
      start = parse_output->size();

      sequence(is_no);

      for (;;) {
        if (!is_no) {
          or_wrapper(start, else_list);
        }

        if (match(", or ")) {
          sequence(is_no);
          break;
        } else if (match(", ")) {
          start = parse_output->size();
          sequence(is_no);
        } else {
          parse_error();
        }
      }
    }

    or_finish(else_list);
  }
}

void LineParser::bracket_modifier() {
  if (match("others]")) {
    return;  // (others) has no semantics
  }

  if (match("suited]")) {
    parse_output->push_back(pc_discard_suit_count_n);
    parse_output->push_back(1);
    return;
  }

  switch (*parse_ptr) {
    case '1':
    case '2':
    case '3':
    case '4':
      parse_ptr += 1;
      if (peek(" suits]")) {
        // n suits
        parse_output->push_back(pc_discard_suit_count_n);
        parse_output->push_back(parse_ptr[-1] - '0');

        parse_ptr += 7;
        return;
      } else {
        parse_ptr -= 1;
      }
  }

  for (;;) {
    bracket_clause();

    if (match(", ")) {
      continue;
    }
    if (match("]")) {
      break;
    }
    parse_error();
  }
}

void LineParser::opt_bracket_modifiers() {
  while (match(" [")) {
    bracket_modifier();
  }
}

void LineParser::top_phrase() {
  if (match("Nothing")) {
    if (parse_wild_cards > 0) {
      parse_error();
    }
    parse_output->push_back(pc_nothing);
  } else if (match("Just the deuces") || match("Just the jacks")) {
    if (parse_wild_cards < 2) {
      parse_error();
    }
    parse_output->push_back(pc_nothing);
  } else if (match("Just the deuce") || match("Just the jack") ||
             match("Just the joker")) {
    if (parse_wild_cards != 1) {
      parse_error();
    }
    parse_output->push_back(pc_nothing);
  } else if (match("Two Pair")) {
    if (parse_wild_cards > 0) {
      parse_error();
    }
    parse_output->push_back(pc_two_pair);
  } else if (match("Trips")) {
    switch (parse_wild_cards) {
      case 0:
      case -1:
        parse_output->push_back(pc_trip_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      case 1:
        parse_output->push_back(pc_pair_of_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      default:
        parse_error();
    }
  } else if (match("Full House")) {
    switch (parse_wild_cards) {
      case 0:
      case -1:
        parse_output->push_back(pc_full_house);
        break;

      case 1:
        parse_output->push_back(pc_two_pair);
        break;

      default:
        parse_error();
    }
  } else if (match("Quads")) {
    switch (parse_wild_cards) {
      case 0:
      case -1:
        if (match(" with low kicker")) {
          parse_output->push_back(pc_quads_with_low_kicker);
        } else {
          parse_output->push_back(pc_quads);
        }
        break;

      case 1:
        parse_output->push_back(pc_trip_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      case 2:
        parse_output->push_back(pc_pair_of_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      default:
        parse_error();
    }
  }

  else if (match("Quints")) {
    switch (parse_wild_cards) {
      case 1:
        parse_output->push_back(pc_quads);
        break;

      case 2:
        parse_output->push_back(pc_trip_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      case 3:
        parse_output->push_back(pc_pair_of_x);
        parse_output->push_back(0xff);
        parse_output->push_back(0xff);
        break;

      default:
        parse_error();
    }
  } else if (match("Royal Flush")) {
    if (parse_wild_cards != -1) {
      parse_error();
    }
    parse_output->push_back(pc_RF_n);
    parse_output->push_back(5);
  } else if (match("Natural Royal Flush")) {
    if (parse_wild_cards != 0) {
      parse_error();
    }
    parse_output->push_back(pc_RF_n);
    parse_output->push_back(5);
  } else if (match("Wild Royal Flush")) {
    if (parse_wild_cards <= 0) {
      parse_error();
    }
    parse_output->push_back(pc_RF_n);
    parse_output->push_back(5 - parse_wild_cards);
  } else if (match("Pair of ")) {
    switch (parse_wild_cards) {
      case 0:
      case -1:
        parse_output->push_back(pc_pair_of_x);
        denom();
        break;

      case 1:
        parse_output->push_back(pc_just_a_x);
        denom();
        break;

      default:
        parse_error();
    }
  } else if (match("One Pair")) {
    if (parse_wild_cards > 0) {
      parse_error();
    }
    parse_output->push_back(pc_pair_of_x);
    parse_output->push_back(0xff);
    parse_output->push_back(0xff);
  } else if (match("Just a ") || match("Just an ")) {
    if (parse_wild_cards > 0) {
      parse_error();
    }
    parse_output->push_back(pc_just_a_x);
    denom();
  } else if (match("Trip ")) {
    switch (parse_wild_cards) {
      case 0:
      case -1: {
        const size_t patch = parse_output->size();
        parse_output->push_back(pc_trip_x);
        denom();
        if (match(" with a low kicker")) {
          (*parse_output)[patch] = pc_trip_x_with_low_kicker;
        }
        break;
      }

      case 1:
        parse_output->push_back(pc_pair_of_x);
        denom();
        break;

      case 2:
        parse_output->push_back(pc_just_a_x);
        denom();
        break;

      default:
        parse_error();
    }
  } else if (match("RF ")) {
    parse_output->push_back(pc_RF_n);
    digit();
  } else if (match("SF ")) {
    parse_output->push_back(pc_SF_n);
    digit();
    opt_inside();
    opt_high();
  } else if (match("Straight")) {
    int w = parse_wild_cards <= 0 ? 0 : parse_wild_cards;
    if (w >= 4) {
      parse_error();
    }

    if (match(" Flush")) {
      parse_output->push_back(pc_SF_n);
      parse_output->push_back(5 - w);  // length
      parse_output->push_back(0);      // any reach will do
    } else {
      parse_output->push_back(pc_Straight_n);
      if (opt_digit()) {
        opt_inside();
        opt_high();
      } else {
        parse_output->push_back(0);
        // 0 is the wild card for reach
      }
    }
  } else if (match("Flush")) {
    parse_output->push_back(pc_Flush_n);
    if (opt_digit()) {
      opt_high();
    }
  } else if (parse_wild_cards > 0) {
    parse_error();
  } else if (high_cards()) {
  } else if (low_cards()) {
  } else {
    parse_error();
  }

  opt_paren_modifiers();
}

void LineParser::parse_main(const char *line) {
  parse_ptr = line;
  parse_input = line;

  for (;;) {
    patch_list else_list;
    bool first = true;

    for (;;) {
      int start = parse_output->size();
      top_phrase();

      if (first) {
        first = false;
        if (match(", ")) {
          goto more;
        }
        if (match(" or ")) {
          goto last_two;
        }
        break;
      } else {
        if (match(", or ")) {
          goto last_two;
        }
        if (match(", ")) {
          goto more;
        }
        parse_error();
      }

    more:
      or_wrapper(start, else_list);
      continue;

    last_two:
      or_wrapper(start, else_list);
      top_phrase();
      break;
    }

    or_finish(else_list);

    opt_bracket_modifiers();

    if (match(" << ")) {
      parse_output->push_back(pc_prefer);
    } else {
      break;
    }
  }

  if (*parse_ptr != '\0') {
    parse_error();
  }
}

StrategyLine parse_line(const char *line, int wild_cards) {
  std::vector<unsigned char> output;
  LineParser(wild_cards, &output).parse_main(line);
  output.push_back(pc_eof);

  const std::size_t n = output.size();
  unsigned char *result_pattern = new unsigned char[n];
  const std::size_t result_size = strlen(line) + 1;
  char *result_image = new char[result_size];
  strcpy_s(result_image, result_size, line);

  return StrategyLine(output, result_image);
}
