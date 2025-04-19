#include "vpoker.h"

const char denom_image[] = "A23456789TJQK";

const char suit_image[] = "shcd";
// The assignment of the suits is not arbitrary.
// In One-Eyed-Jacks, suits 0 and 1 are the ones
// with the wild jacks.

const char *payoff_image[] = {
    static_cast<const char *>("Nothing"),
    static_cast<const char *>("High Pair"),
    static_cast<const char *>("Two Pair"),
    static_cast<const char *>("Trips"),
    static_cast<const char *>("Straight"),
    static_cast<const char *>("Flush"),
    static_cast<const char *>("Full House"),
    static_cast<const char *>("Quads"),
    static_cast<const char *>("Quad Aces"),
    static_cast<const char *>("Quad Aces w/low kicker"),
    static_cast<const char *>("Quad 2,3 or 4"),
    static_cast<const char *>("Quad 2,3 or 4 w/low kicker"),
    static_cast<const char *>("Straight Flush"),
    static_cast<const char *>("Quints"),
    static_cast<const char *>("Wild Royal"),
    static_cast<const char *>("Four Deuces"),
    static_cast<const char *>("Royal Flush")};

const char *short_payoff_image[] = {
    static_cast<const char *>("N"),  static_cast<const char *>("HP"),
    static_cast<const char *>("TP"), static_cast<const char *>("T"),
    static_cast<const char *>("ST"), static_cast<const char *>("FL"),
    static_cast<const char *>("FH"), static_cast<const char *>("Q"),
    static_cast<const char *>("QA"), static_cast<const char *>("QAK"),
    static_cast<const char *>("QL"), static_cast<const char *>("QLK"),
    static_cast<const char *>("SF"), static_cast<const char *>("QT"),
    static_cast<const char *>("WR"), static_cast<const char *>("FD"),
    static_cast<const char *>("RF")};

std::string move_image(const card *hand, int hand_size, unsigned mask) {
  std::string result;

  for (int j = 0; j < hand_size; j++) {
    const int c = hand[j];
    result.push_back(' ');
    result.push_back(denom_image[pips(c)]);
    result.push_back(suit_image[suit(c)]);
  }
  result.push_back('\n');

  for (int k = 0; k < hand_size; k++) {
    result.push_back(' ');
    if (mask & 1) {
      result.push_back('=');
      result.push_back('=');
    } else {
      result.push_back(' ');
      result.push_back(' ');
    }

    mask >>= 1;
  }

  result.push_back('\n');
  result.push_back('\n');
  return result;
}

void print_move(FILE *file, const card *hand, int hand_size, unsigned mask) {
  fputs(move_image(hand, hand_size, mask).c_str(), file);
}

struct ltstr {
  bool operator()(const char *s1, const char *s2) const {
    return strcmp(s1, s2) < 0;
  }
};

static std::map<const char *, vp_game *, ltstr> name_map;

vp_game::vp_game(const char *n, enum game_kind k, denom_value mhp,
                 const int (*pt)[last_pay + 1]) {
  name = n;
  kind = k;
  min_high_pair = mhp;
  pay_table = pt;

  name_map[n] = this;
}

vp_game *vp_game::find(char *named) { return name_map[named]; }

namespace games {
static const int kb_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    2,    // trips,
    3,    // straight,
    5,    // flush,
    7,    // full_house,
    20,   // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    50,   // straight_flush,
    200,  // quints,
    100,  // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game kb_joker("Kings or Better Joker's Wild", GK_joker_wild, king,
                       &kb_table);

static const int tpjw_table[] = {
    0,    // nothing,
    0,    // high_pair,
    1,    // two_pair,
    2,    // trips,
    5,    // straight,
    6,    // flush,
    10,   // full_house,
    20,   // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    50,   // straight_flush,
    100,  // quints,
    50,   // wild_royal,
    0,    // four_deuces,
    1000  // royal_flush
};

const vp_game tp_joker("Two Pair Joker's Wild", GK_joker_wild, jack,
                       &tpjw_table);

static const int jb_table[] = {
    0,   // nothing,
    1,   // high_pair,
    2,   // two_pair,
    3,   // trips,
    4,   // straight,
    6,   // flush,
    9,   // full_house,
    25,  // quads,
    0,   // quad aces",
    0,   // quad aces w/low kicker",
    0,   // quad 2-4",
    0,   // quad 2-4 w/low kicker",
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game jacks_or_better("9/6 Jacks or Better", GK_no_wild, jack,
                              &jb_table);

static const int jb95_table[] = {
    0,   // nothing,
    1,   // high_pair,
    2,   // two_pair,
    3,   // trips,
    4,   // straight,
    5,   // flush,
    9,   // full_house,
    25,  // quads,
    0,   // quad aces",
    0,   // quad aces w/low kicker",
    0,   // quad 2-4",
    0,   // quad 2-4 w/low kicker",
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game jacks_or_better_95("9/5 Jacks or Better", GK_no_wild, jack,
                                 &jb95_table);

static const int jb75_table[] = {
    0,   // nothing,
    1,   // high_pair,
    2,   // two_pair,
    3,   // trips,
    4,   // straight,
    5,   // flush,
    7,   // full_house,
    25,  // quads,
    0,   // quad aces",
    0,   // quad aces w/low kicker",
    0,   // quad 2-4",
    0,   // quad 2-4 w/low kicker",
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game jacks_or_better_75("7/5 Jacks or Better", GK_no_wild, jack,
                                 &jb75_table);

static const int bdlx_table[] = {
    0,   // nothing,
    1,   // high_pair,
    1,   // two_pair,
    3,   // trips,
    4,   // straight,
    6,   // flush,
    8,   // full_house,
    80,  // quads,
    0,   // quad aces",
    0,   // quad aces w/low kicker",
    0,   // quad 2-4",
    0,   // quad 2-4 w/low kicker",
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game bdlx_game("8/6 Bonus Deluxe", GK_no_wild, jack, &bdlx_table);

static const int bdlx96_table[] = {
    0,   // nothing,
    1,   // high_pair,
    1,   // two_pair,
    3,   // trips,
    4,   // straight,
    6,   // flush,
    9,   // full_house,
    80,  // quads,
    0,   // quad aces",
    0,   // quad aces w/low kicker",
    0,   // quad 2-4",
    0,   // quad 2-4 w/low kicker",
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game bdlx96_game("9/6 Bonus Deluxe", GK_no_wild, jack, &bdlx96_table);

static const int aa_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    8,    // straight,
    8,    // flush,
    8,    // full_house,
    40,   // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    200,  // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game all_american("All American Poker", GK_no_wild, jack, &aa_table);

static const int bp_table[] = {
    0,   // nothing,
    1,   // high_pair,
    2,   // two_pair,
    3,   // trips,
    4,   // straight,
    5,   // flush,
    8,   // full_house,
    25,  // quads,
    80,  // quad aces
    0,   // quad aces w/low kicker
    40,  // quad 2-4
    0,   // quad 2-4 w/low kicker
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game eight_five_bonus("8/5 Bonus Poker", GK_bonus, jack, &bp_table);

static const int sa_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    4,    // straight,
    5,    // flush,
    8,    // full_house,
    50,   // quads,
    400,  // quad aces
    0,    // quad aces w/low kicker
    80,   // quad 2-4
    0,    // quad 2-4 w/low kicker
    60,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game super_aces("Super Aces", GK_bonus, jack, &sa_table);

static const int bp75_table[] = {
    0,   // nothing,
    1,   // high_pair,
    2,   // two_pair,
    3,   // trips,
    4,   // straight,
    5,   // flush,
    7,   // full_house,
    25,  // quads,
    80,  // quad aces
    0,   // quad aces w/low kicker
    40,  // quad 2-4
    0,   // quad 2-4 w/low kicker
    50,  // straight_flush,
    0,   // quints,
    0,   // wild_royal,
    0,   // four_deuces,
    800  // royal_flush
};

const vp_game seven_five_bonus("7/5 Bonus Poker", GK_bonus, jack, &bp75_table);

static const int db_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    5,    // straight,
    7,    // flush,
    10,   // full_house,
    50,   // quads,
    160,  // quad aces
    0,    // quad aces w/low kicker
    80,   // quad 2-4
    0,    // quad 2-4 w/low kicker
    50,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game double_bonus("10/7 Double Bonus Poker", GK_bonus, jack,
                           &db_table);

static const int tbp_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    4,    // straight,
    5,    // flush,
    9,    // full_house,
    50,   // quads,
    240,  // quad aces
    0,    // quad aces w/low kicker
    120,  // quad 2-4
    0,    // quad 2-4 w/low kicker
    100,  // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game triple_bonus_plus("9/5 Triple Bonus Plus", GK_bonus, jack,
                                &tbp_table);

static const int tdb_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    2,    // trips,
    4,    // straight,
    6,    // flush,
    9,    // full_house,
    50,   // quads,
    160,  // quad aces
    800,  // quad aces w/low kicker
    80,   // quad 2-4
    400,  // quad 2-4 w/low kicker
    50,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game triple_double_bonus("9/6 Triple Double Bonus",
                                  GK_bonus_with_kicker, jack, &tdb_table);
static const int db96_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    5,    // straight,
    6,    // flush,
    9,    // full_house,
    50,   // quads,
    160,  // quad aces
    0,    // quad aces w/low kicker
    80,   // quad 2-4
    0,    // quad 2-4 w/low kicker
    50,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game double_bonus96("9/6 Double Bonus Poker", GK_bonus, jack,
                             &db96_table);

static const int db_97_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    5,    // straight,
    7,    // flush,
    9,    // full_house,
    50,   // quads,
    160,  // quad aces
    0,    // quad aces w/low kicker
    80,   // quad 2-4
    0,    // quad 2-4 w/low kicker
    50,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game double_bonus_il("9/7 Double Bonus Poker", GK_bonus, jack,
                              &db_97_table);

static const int ddb_table[] = {
    0,    /// db_96
    1,    // high_pair,
    1,    // two_pair,
    3,    // trips,
    4,    // straight,
    6,    // flush,
    10,   // full_house,
    50,   // quads,
    160,  // quad aces
    400,  // quad aces w/low kicker
    80,   // quad 2-4
    160,  // quad 2-4 w/low kicker
    40,   // straight_flush,
    0,    // quints,
    0,    // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game double_double_bonus("Double Double Bonus Poker",
                                  GK_bonus_with_kicker, jack, &ddb_table);

static const int fpdw_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    2,    // flush,
    3,    // full_house,
    5,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    9,    // straight_flush,
    15,   // quints,
    25,   // wild_royal,
    200,  // four_deuces,
    800   // royal_flush
};

const vp_game deuces_wild("Full Pay Deuces Wild", GK_deuces_wild, ace,
                          &fpdw_table);

static const int vegas_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    2,    // flush,
    3,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    13,   // straight_flush,
    16,   // quints,
    25,   // wild_royal,
    400,  // four_deuces,
    940   // royal_flush
};

const vp_game vegas_deuces("Vegas Club Deuces", GK_deuces_wild, ace,
                           &vegas_table);

static const int stdw_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    3,    // flush,
    4,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    10,   // straight_flush,
    10,   // quints,
    20,   // wild_royal,
    400,  // four_deuces,
    800   // royal_flush
};

const vp_game sams_deuces("Sam's Town Deuces", GK_deuces_wild, ace,
                          &stdw_table);

static const int nsu_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    3,    // flush,
    4,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    10,   // straight_flush,
    16,   // quints,
    25,   // wild_royal,
    200,  // four_deuces,
    800   // royal_flush
};

const vp_game nsu_deuces_wild("NSU Deuces Wild", GK_deuces_wild, ace,
                              &nsu_table);

static const int ldw_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    2,    // flush,
    3,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    8,    // straight_flush,
    15,   // quints,
    25,   // wild_royal,
    500,  // four_deuces,
    800   // royal_flush
};

const vp_game loose_deuces_wild("8/15 Loose Deuces Wild", GK_deuces_wild, ace,
                                &ldw_table);

static const int ldw_table2[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    2,    // flush,
    3,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    10,   // straight_flush,
    17,   // quints,
    25,   // wild_royal,
    500,  // four_deuces,
    800   // royal_flush
};

const vp_game loose_deuces_wild2("10/17 Loose Deuces Wild", GK_deuces_wild, ace,
                                 &ldw_table2);

static const int ap_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    3,    // flush,
    4,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    11,   // straight_flush,
    15,   // quints,
    25,   // wild_royal,
    200,  // four_deuces,
    800   // royal_flush
};

const vp_game ap_deuces_wild("11/15 AP Deuces Wild", GK_deuces_wild, ace,
                             &ap_table);

static const int airport_deuces_table[] = {
    0,    // nothing,
    0,    // high_pair,
    0,    // two_pair,
    1,    // trips,
    2,    // straight,
    3,    // flush,
    4,    // full_house,
    4,    // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    9,    // straight_flush,
    15,   // quints,
    25,   // wild_royal,
    200,  // four_deuces,
    800   // royal_flush
};

const vp_game airport_deuces_wild("Airport Deuces Wild", GK_deuces_wild, ace,
                                  &airport_deuces_table);

static const int oej_table[] = {
    0,    // nothing,
    1,    // high_pair,
    1,    // two_pair,
    1,    // trips,
    2,    // straight,
    4,    // flush,
    5,    // full_house,
    10,   // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    50,   // straight_flush,
    100,  // quints,
    100,  // wild_royal,
    0,    // four_deuces,
    1600  // royal_flush
};

const vp_game one_eyed_jacks(
    "One Eyed Jacks", GK_one_eyed_jacks_wild,
    static_cast<denom_value>(king + 1),  // not ace, since that is low

    &oej_table);

static const int oej_table2[] = {
    0,    // nothing,
    0,    // high_pair,
    1,    // two_pair,
    1,    // trips,
    2,    // straight,
    3,    // flush,
    5,    // full_house,
    15,   // quads,
    0,    // quad aces",
    0,    // quad aces w/low kicker",
    0,    // quad 2-4",
    0,    // quad 2-4 w/low kicker",
    50,   // straight_flush,
    80,   // quints,
    180,  // wild_royal,
    0,    // four_deuces,
    800   // royal_flush
};

const vp_game one_eyed_jacks2("180/80/50 One Eyed Jacks",
                              GK_one_eyed_jacks_wild, king,

                              &oej_table2);
}  // namespace games
