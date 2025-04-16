#include "enum_match.h"

#include <vector>

static int count_suits(unsigned x) {
  switch (x) {
    default:
      _ASSERT(0);

    case 0:
      return 0;

    case 1:
    case 2:
    case 4:
    case 8:
      return 1;

    case 1 + 2:
    case 1 + 4:
    case 1 + 8:
    case 2 + 4:
    case 2 + 8:
    case 4 + 8:
      return 2;

    case 1 + 2 + 4:
    case 1 + 2 + 8:
    case 1 + 4 + 8:
    case 2 + 4 + 8:
      return 3;

    case 1 + 2 + 4 + 8:
      return 4;
  }
}

bool enum_match::matches_tail(unsigned mask, unsigned char *pattern) {
  // Analyze the discards for penalty cards

  bool suited = true;
  int the_suit = -1;
  unsigned have_suits = 0;

  bool have[num_denoms];
  unsigned char have_discard[num_denoms];
  // a mask to indicate the suit of each discard

  unsigned char discard_suits = 0;
  // The union of all those masks

  int discard_suit_count = 0;
  // The number of distinct suits represented in the discards

  int high_denoms = 0;

  int max_non_ace = ace - 5;
  int min_non_ace = king + 5;

  int suited_discard = 0;

  int min_discard = king + 1;
  int max_discard = -1;

  int j;

  int num_kept = 0;
  int num_discards = 0;
  card discards[5];

  for (j = 0; j < num_denoms; j++) {
    have[j] = false;
    have_discard[j] = 0;
  }

  for (j = 0; j < hand_size; j++) {
    const card c = hand[j];

    if (mask & 1) {
      num_kept += 1;

      const int s = suit(c);
      const int d = pips(c);

      if (!have[d] && parms->is_high(d)) {
        high_denoms += 1;
      }

      have[d] = true;
      have_suits |= (1 << s);

      if (d != ace) {
        if (d < min_non_ace) {
          min_non_ace = d;
        }
        if (d > max_non_ace) {
          max_non_ace = d;
        }
      }

      if (the_suit < 0) {
        the_suit = s;
      } else if (s != the_suit) {
        suited = false;
      }
    } else {
      discards[num_discards++] = c;
    }

    mask >>= 1;
  }

  for (j = 0; j < num_discards; j++) {
    const card c = discards[j];
    const int s = suit(c);
    const unsigned suit_mask = (1 << s);

    if (suited && s == the_suit) {
      suited_discard += 1;
    }

    const int p = pips(c);
    have_discard[p] |= suit_mask;

    if ((discard_suits & suit_mask) == 0) {
      discard_suit_count += 1;
      discard_suits |= suit_mask;
    }

    // For the purposes of min_discard and max_discard,
    // ace is counted low.
    if (p > max_discard) {
      max_discard = p;
    }
    if (p < min_discard) {
      min_discard = p;
    }
  }

  unsigned char *rover = pattern;
  unsigned char *or_operand = 0;

next_code: {
  unsigned char code = *rover++;
  switch (code) {
    case pc_eof:
    case pc_prefer:
      pat_eof = rover - 1;
      return true;

    case pc_or: {
      unsigned char offset = *rover++;
      _ASSERT(!or_operand);
      or_operand = rover + offset;
    } break;

    case pc_else: {
      unsigned char offset = *rover++;
      rover += offset;
      or_operand = 0;
    } break;

    case pc_no_x:
      if (have[*rover++]) {
        goto fail;
      }
      break;

    case pc_with_x:
      if (!have[*rover++]) {
        goto fail;
      }
      break;

    case pc_high_n:
      if ((*rover++ & (1 << high_denoms)) == 0) {
        goto fail;
      }
      break;

    case pc_these_n: {
      const int n = *rover++;
      if (num_kept != n) {
        goto fail;
      }

      bool answer = true;

      for (int j = 0; j < n; j++) {
        if (!have[*rover++]) {
          answer = false;
        }
      }
      if (!answer) {
        goto fail;
      }
    } break;

    case pc_suited_x: {
      int s = *rover++;
      if (!suited) {
        goto fail;
      }
      if (((1 << the_suit) & s) == 0) {
        goto fail;
      }
    } break;

    case pc_high_x:
    case pc_low_x: {
      int mask = *rover++;
      mask |= (*rover++) << 8;
      int d;

      if (min_non_ace > max_non_ace) {
        if (have[ace]) {
          d = ace;
        } else {
          goto fail;
        }
      } else if (code == pc_low_x) {
        d = ace_is_low && have[ace] ? ace : min_non_ace;
      } else {
        d = !ace_is_low && have[ace] ? ace : max_non_ace;
      }

      if ((mask & (1 << d)) == 0) {
        goto fail;
      }
    } break;

    case pc_no_fp:
      if (suited_discard) {
        goto fail;
      }
      break;

    case pc_dsc_fp:
      if (!suited_discard) {
        goto fail;
      }
      break;

    case pc_dsc_gp:
    case pc_no_gp: {
      const int count = *rover++;

      if (num_kept <= 1) {
        break;
      }

      int min_denom, max_denom, reach;

      if (have[ace]) {
        // Choose ace to be high or low to minimize the reach
        int r_lo = max_non_ace - ace + 1;
        int r_hi = king + 1 - min_non_ace + 1;

        if (r_lo < r_hi) {
          reach = r_lo;
          min_denom = ace;
          max_denom = max_non_ace;
        } else {
          reach = r_hi;
          min_denom = min_non_ace;
          max_denom = king + 1;
        }
      } else {
        min_denom = min_non_ace;
        max_denom = max_non_ace;
        reach = max_non_ace - min_non_ace + 1;
      }

      if (reach > 5) {
        break;
      }

      bool has_gp = false;
      for (int j = min_denom + 1; j <= max_denom - 1; j++) {
        if (!have[j] && count_suits(have_discard[j]) == count) {
          has_gp = true;
        }
      }

      if (code == pc_dsc_gp && !has_gp) {
        goto fail;
      }
      if (code == pc_no_gp && has_gp) {
        goto fail;
      }
    } break;

    case pc_dsc_sp:
    case pc_no_sp: {
      bool has_sp = false;

      int min_denom, max_denom, reach;

      if (have[ace]) {
        if (min_non_ace > max_non_ace) {
          // Just the ace
          // It's both lo and hi!
          reach = 1;

          int j;

          for (j = deuce; j <= 5; j++) {
            if (have_discard[j]) {
              has_sp = true;
            }
          }

          for (j = ten; j <= king; j++) {
            if (have_discard[j]) {
              has_sp = true;
            }
          }

          goto check_has_sp;

        } else {
          int r_lo = max_non_ace - ace + 1;
          int r_hi = king + 1 - min_non_ace + 1;

          if (r_lo < r_hi) {
            reach = r_lo;
            min_denom = ace;
            max_denom = max_non_ace;
          } else {
            reach = r_hi;
            min_denom = min_non_ace;
            max_denom = king + 1;
          }
        }
      } else {
        min_denom = min_non_ace;
        max_denom = max_non_ace;
        reach = max_non_ace - min_non_ace + 1;
      }

      if (reach > 5) {
        break;
      }

      {
        int max_denom = min_denom + reach - 1;
        int lo = max_denom - 4;
        int hi = min_denom + 4;
        bool check_ace = false;

        if (lo < ace) {
          lo = ace;
        }
        if (hi > king) {
          hi = king;
          check_ace = true;
        }

        for (int j = lo; j <= hi; j++) {
          if (!have[j] && have_discard[j]) {
            has_sp = true;
          }
        }

        if (check_ace) {
          if (!have[ace] && have_discard[ace]) {
            has_sp = true;
          }
        }
      }

    check_has_sp:
      if (code == pc_dsc_sp && !has_sp) {
        goto fail;
      }
      if (code == pc_no_sp && has_sp) {
        goto fail;
      }
    } break;

    case pc_dsc_pp:
    case pc_no_pp: {
      bool discard_pair = false;
      for (j = 0; j < num_discards; j++) {
        const card c = discards[j];
        const int p = pips(c);
        if (parms->is_high(p) && have[p]) {
          discard_pair = true;
        }
      }

      if (code == pc_dsc_pp && !discard_pair) {
        goto fail;
      }
      if (code == pc_no_pp && discard_pair) {
        goto fail;
      }
    } break;

    case pc_dsc_pair:
    case pc_no_pair: {
      bool discard_pair = false;
      for (j = 0; j < num_discards; j++) {
        const card c = discards[j];
        const int p = pips(c);
        if (have[p]) {
          discard_pair = true;
        }
      }

      if (code == pc_dsc_pair && !discard_pair) {
        goto fail;
      }
      if (code == pc_no_pair && discard_pair) {
        goto fail;
      }
    } break;
    case pc_no_le_x:
      if (min_discard <= *rover++) {
        goto fail;
      }
      break;

    case pc_dsc_le_x:
      if (min_discard > *rover++) {
        goto fail;
      }
      break;

    case pc_no_ge_x:
      if (max_discard >= *rover++) {
        goto fail;
      }
      if (have_discard[ace]) {
        goto fail;
      }
      break;

    case pc_dsc_ge_x: {
      int x = *rover++;
      if (!have_discard[ace] && max_discard < x) {
        goto fail;
      }
    } break;

    case pc_no_these_n_and_fp:
    case pc_dsc_these_n_and_fp:
    case pc_no_these_n:
    case pc_dsc_these_n: {
      int n = *rover++;
      int these_suited = 0;
      bool answer = true;

      for (int j = 0; j < n; j++) {
        int p = *rover++;

        if (suited) {
          if (have_discard[p] & (1 << the_suit)) {
            these_suited += 1;
          }
        }

        if (!have_discard[p]) {
          answer = false;
        }
      }

      if (code == pc_no_these_n_and_fp || code == pc_dsc_these_n_and_fp) {
        if (these_suited == suited_discard) {
          answer = false;
        }
      }

      if ((!answer) ^ (code == pc_no_these_n || code == pc_no_these_n_and_fp)) {
        goto fail;
      }
    } break;

    case pc_discard_suit_count_n: {
      if (*rover++ != discard_suit_count) {
        goto fail;
      }
      break;
    }

    case pc_no_these_suited_n:
    case pc_dsc_these_suited_n: {
      // "suited" means the cards match each other
      int n = *rover++;
      int answer = ~0;
      for (int j = 0; j < n; j++) {
        answer &= have_discard[*rover++];
      }

      if ((answer == 0) ^ (code == pc_no_these_suited_n)) {
        goto fail;
      }
    } break;

    case pc_no_these_offsuit_n:
    case pc_dsc_these_offsuit_n: {
      // "offsuit" means is different from
      // any of the suits being held

      int n = *rover++;
      bool answer = true;

      int mask = ~have_suits;

      for (int j = 0; j < n; j++) {
        if ((have_discard[*rover++] & mask) == 0) {
          answer = false;
        }
      }

      if ((!answer) ^ (code == pc_no_these_offsuit_n)) {
        goto fail;
      }
    } break;

    case pc_no_these_onsuit_n:
    case pc_dsc_these_onsuit_n: {
      // "onsuit" means each card matches
      // one of the cards held

      int n = *rover++;
      bool answer = true;

      int mask = have_suits;

      for (int j = 0; j < n; j++) {
        if ((have_discard[*rover++] & mask) == 0) {
          answer = false;
        }
      }

      if ((!answer) ^ (code == pc_no_these_onsuit_n)) {
        goto fail;
      }
    } break;
    case pc_least_sp: {
      if (num_kept != 1) {
        goto fail;
      }
      const int denom_kept = have[ace] ? ace : min_non_ace;

      int inner_lo, inner_hi;
      if (denom_kept == five || denom_kept == ten) {
        inner_lo = five;
        inner_hi = ten;
      } else {
        inner_lo = six;
        inner_hi = nine;
        if (denom_kept < inner_lo || denom_kept > inner_hi) {
          goto fail;
        }
      }

      int min_inner = denom_kept;
      int max_inner = denom_kept;
      for (int d = inner_lo; d <= inner_hi; ++d) {
        if (have_discard[d]) {
          if (d < min_inner) {
            min_inner = d;
          }
          if (d > max_inner) {
            max_inner = d;
          }
        }
      }
      if (denom_kept == min_inner && denom_kept == max_inner) {
        break;
      }
      if (denom_kept != min_inner && denom_kept != max_inner) {
        goto fail;
      }

      if (have_discard[ace]) {
        goto fail;
      }

      int low_pen = num_denoms;  // really big
      for (int d = deuce; d < inner_lo; d++) {
        if (have_discard[d]) {
          low_pen = min_inner - d;
        }
      }
      _ASSERT(low_pen > 0);

      int high_pen = num_denoms;  // really big
      for (int d = king; d > inner_hi; d--) {
        if (have_discard[d]) {
          high_pen = d - max_inner;
        }
      }
      _ASSERT(high_pen > 0);

      if (denom_kept == min_inner && low_pen >= high_pen) {
        break;
      }
      if (denom_kept == max_inner && high_pen >= low_pen) {
        break;
      }
      goto fail;
    } break;
    default:
      _ASSERT(0);
  }
  goto next_code;

fail:
  if (!or_operand) {
    return false;
  }
  rover = or_operand;
  or_operand = 0;
  goto next_code;
}
}

void enum_match::check(unsigned mask) {
  unsigned char *save_pat_eof = pat_eof;

  if (*pat == pc_eof || *pat == pc_prefer) {
    // There is no tail to match.
    // Keep this one.
    pat_eof = pat;
  } else if (matches_tail(mask, pat)) {
    // matches_tail sets pat_eof if is succeeds
  } else {
    return;
  }

  if (save_pat_eof == 0) {
    // First match of a new preference.
    // Only keep the matches from the new preference
    memset((void *)result_vector, 0, sizeof result_vector);
    match_count = 0;
  }

  if (!result_vector[mask]) {
    result_vector[mask] = true;
    matches[match_count++] = mask;
  }
}

void enum_match::find(unsigned char *pattern) {
  const unsigned end_marker = 0xff;
  pat = pattern;

  unsigned char denom[6];
  denom[hand_size];

  unsigned char *or_operand = 0;

  {
    for (int j = 0; j < hand_size; j++) {
      denom[j] = pips(hand[j]);
    }
    denom[hand_size] = end_marker;
  }

  // Initialize the results to empty.
  // This will get done again if the first time check
  // is called, but if check doesn't get called, we want
  // the results to be empty.

  memset((void *)result_vector, 0, sizeof result_vector);
  match_count = 0;

restart:
  pat_eof = 0;

loop:
  ace_is_low = false;

  {
    unsigned op_code = *pat++;

    switch (op_code) {
      case pc_or: {
        unsigned char offset = *pat++;
        _ASSERT(!or_operand);
        or_operand = pat + offset;
        goto loop;
      } break;

      case pc_else: {
        unsigned char offset = *pat++;
        pat += offset;
        or_operand = 0;
      } break;

      case pc_nothing:
        check(0);
        break;

      case pc_two_pair: {
        int j = 0;
        struct {
          int first, after;
        } plist[2];
        int pcount = 0;

        for (;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          if (j - left >= 2) {
            _ASSERT(pcount < 2);
            plist[pcount].first = left;
            plist[pcount].after = j;
            pcount += 1;
          }
        }

        if (pcount == 2) {
          _ASSERT(plist[0].after - plist[0].first <= 3);
          _ASSERT(plist[1].after - plist[1].first <= 3);

          unsigned masks[6];
          int mcount = 0;

          struct {
            int first, after;
          } mask_index[2];

          for (int z = 0; z < 2; z++) {
            switch (plist[z].after - plist[z].first) {
              case 2:
                mask_index[z].first = mcount;
                masks[mcount++] = 3 << plist[z].first;
                mask_index[z].after = mcount;
                break;

              case 3:
                mask_index[z].first = mcount;
                masks[mcount++] = 3 << plist[z].first;
                masks[mcount++] = 5 << plist[z].first;
                masks[mcount++] = 6 << plist[z].first;
                mask_index[z].after = mcount;
                break;

              default:
                _ASSERT(0);
            }
          }

          for (int j0 = mask_index[0].first; j0 < mask_index[0].after; j0++)
            for (int j1 = mask_index[1].first; j1 < mask_index[1].after; j1++) {
              check(masks[j0] | masks[j1]);
            }
        }
      } break;

      case pc_trips:
      case pc_trip_x:
      case pc_trip_x_with_low_kicker: {
        unsigned trip_denoms;

        if (op_code == pc_trip_x || op_code == pc_trip_x_with_low_kicker) {
          trip_denoms = *pat++;
          trip_denoms |= (*pat++) << 8;
        } else {
          trip_denoms = 0xffff;
        }

        int j = 0;
        for (;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          // Find all the low kickers that are not d
          std::vector<unsigned char> kickers;
          if (op_code == pc_trip_x_with_low_kicker) {
            size_t k = 0;
            for (;;) {
              int dd = denom[k];
              if (dd == end_marker) break;
              if (dd != d && dd <= four) {
                kickers.push_back(1 << k);
              }
              ++k;
            }
            if (kickers.empty()) {
              kickers.push_back(0);
            }
          } else {
            kickers.push_back(0);
          }

          // j - left is the number of d's
          if (j - left >= 3 && ((1 << d) & trip_denoms) != 0) {
            switch (j - left) {
              case 3:
                for (size_t z = 0; z < kickers.size(); ++z) {
                  check((7 << left) | kickers[z]);
                }
                break;

              case 4:
                // all the four-bit patterns with
                // with exactly three bits on
                for (size_t z = 0; z < kickers.size(); ++z) {
                  check((7 << left) | kickers[z]);
                  check((11 << left) | kickers[z]);
                  check((13 << left) | kickers[z]);
                  check((14 << left) | kickers[z]);
                }
                break;

              default:
                _ASSERT(0);
            }
          }
        }
      } break;

      case pc_full_house: {
        int j = 0;
        struct {
          int first, after;
        } plist[2];
        int pcount = 0;

        for (;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          if (j - left >= 2) {
            _ASSERT(pcount < 2);
            plist[pcount].first = left;
            plist[pcount].after = j;
            pcount += 1;
          }
        }

        if (pcount == 2) {
          if ((plist[0].after - plist[0].first == 3 &&
               plist[1].after - plist[1].first == 2) ||
              (plist[0].after - plist[0].first == 2 &&
               plist[1].after - plist[1].first == 3)) {
            // Full house uses all five cards
            check(0x1f);
          }
        }
      } break;

      case pc_quads:
      case pc_quads_with_low_kicker: {
        int j = 0;
        int kicker = -1;
        int quad_pos = -1;
        for (;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          // j - left is the number of d's
          switch (j - left) {
            case 1:
              kicker = d;
              break;
            case 4:
              quad_pos = left;
              break;
          }
        }
        if (quad_pos >= 0) {
          if (op_code == pc_quads_with_low_kicker && kicker >= 0 &&
              kicker <= four) {
            check(0x1f);  // Keep all five
          } else {
            check(0xf << quad_pos);  // Keep just the quads
          }
        }
      } break;

      case pc_pair_of_x: {
        int pair_denoms = *pat++;
        pair_denoms |= (*pat++) << 8;

        int j = 0;
        for (;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          // j - left is the number of d's
          if (j - left >= 2 && ((1 << d) & pair_denoms) != 0) {
            // Iterate all pairs
            for (int p1 = left; p1 < j; p1++)
              for (int p2 = p1 + 1; p2 < j; p2++) {
                check((1 << p1) | (1 << p2));
              }
          }
        }
      } break;

      case pc_Flush_n: {
        const int n = *pat++;

        struct desc {
          int size;
          unsigned char mask[5];
        } partition[num_suits];

        // Partition the hand into the four suits,
        // Each parition constains the masks corresponding
        // to the cards of that suit.

        for (int j = 0; j < num_suits; j++) {
          partition[j].size = 0;
        }

        {
          for (int j = 0; j < hand_size; j++) {
            struct desc *d = partition + suit(hand[j]);

            d->mask[d->size++] = (1 << j);
          }
        }

        for (int j = 0; j < num_suits; j++)
          if (partition[j].size >= n) {
            // for each suit that has enough cards to make
            // a flush draw...

            struct desc *d = partition + j;
            int subset[5];
            // subset contains the n indices of the masks
            // we inted to use.

            int k;

            unsigned mask = 0;

            for (k = 0; k < n; k++) {
              subset[k] = k;
              mask |= d->mask[k];
            }

            // Invariant: mask contains the union of the
            // mask selected by subset

            for (;;) {
              check(mask);

              // Increment subset to the next value
              // in lexicographic order

              for (int z = 1;; z++) {
                int y = n - z;
                int index = subset[y];
                // Scan the indicies from the right

                mask ^= d->mask[index];
                // Turning off the corresponding bit in mask

                if (index < d->size - z) {
                  // Index is small enough to be incremented.
                  // From y on, insert conecutive indices
                  while (y < n) {
                    subset[y] = ++index;
                    mask |= d->mask[index];
                    y += 1;
                  }
                  break;
                }

                if (y == 0) {
                  goto done_flush;
                }
              }
            }
          done_flush: {}
          }
      } break;

      case pc_these_n: {
        const int n = *pat++;
        unsigned need = 0;

        int j;

        for (j = 0; j < n; j++) {
          need |= ((1 << *pat++));
        }

        struct iter_element {
          int left, right, iter;
        } list[5];
        int count = 0;

        unsigned mask = 0;

        for (j = 0;;) {
          int left = j++;
          int d = denom[left];
          if (d == end_marker) {
            break;
          }
          while (denom[j] == d) {
            j += 1;
          }

          if (need & (1 << d)) {
            need ^= (1 << d);

            _ASSERT(count < 5);
            list[count].left = left;
            list[count].right = j - 1;
            list[count].iter = left;

            mask |= (1 << left);
            count += 1;
          }
        }

        if (need == 0) {
          for (;;) {
            check(mask);
            for (j = 0;;) {
              struct iter_element *q = list + j;

              if (q->iter < q->right) {
                mask ^= (3 << q->iter);
                q->iter += 1;
                break;
              }

              mask &= ~(1 << q->iter);
              q->iter = q->left;
              mask |= (1 << q->iter);

              if (++j >= count) {
                goto done_these_n;
              }
            }
          }
        }
      done_these_n:
        break;
      }

      case pc_just_a_x: {
        int denom_mask = *pat++;
        denom_mask |= (*pat++) << 8;

        int j = 0;
        for (;;) {
          int d = denom[j];
          if (d == end_marker) {
            break;
          }

          if (denom_mask & (1 << d)) {
            check(1 << j);
          }

          j += 1;
        }
      } break;

      case pc_RF_n: {
        const int n = *pat++;

        struct desc {
          int size;
          unsigned char mask[5];
          unsigned char denom[5];
        } partition[num_suits];

        // Partition the hand into the four suits,
        // Each parition constains the masks corresponding
        // to the cards of that suit.  Only keep cards
        // that can be in a royal flush

        int j;
        for (j = 0; j < num_suits; j++) {
          partition[j].size = 0;
        }

        {
          for (j = 0; j < hand_size; j++) {
            struct desc *d = partition + suit(hand[j]);
            int v = pips(hand[j]);

            if (v == ace || v >= ten) {
              d->denom[d->size] = v;
              d->mask[d->size++] = (1 << j);
            }
          }
        }

        for (j = 0; j < num_suits; j++)
          if (partition[j].size >= n) {
            struct desc *st = partition + j;

            {
              int subset[5];
              unsigned mask = 0;

              // Subset contains the n indicies of queue
              // that we will iterate over.  Initialize to
              // the leftmost items

              {
                for (int z = 0; z < n; z++) {
                  subset[z] = z;
                  mask |= st->mask[z];
                }
              }

              for (;;) {
                check(mask);

                // Advance to the lexigraphically next subset
                // Search from the right to find the first
                // element of subset not at its max value

                for (int z = 1;; z++) {
                  int y = n - z;
                  int index = subset[y];

                  mask ^= st->mask[index];
                  // Turn off mask bit for index

                  if (index < st->size - z) {
                    // From y forward, fill out subset
                    // with consecutive values.
                    while (y < n) {
                      subset[y++] = ++index;
                      mask |= st->mask[index];
                    }
                    break;
                  }

                  if (y == 0) {
                    goto done_RF;
                  }
                }
              }

            done_RF: {}
            }
          }
      } break;

      case pc_SF_n: {
        const int n = *pat++;
        int reach = *pat++;
        bool any_flag = false;

        if (reach == 0) {
          reach = 5;
          any_flag = true;
        }

      another_reach: {}

        struct desc {
          int size;
          unsigned char mask[5];
          unsigned char denom[5];
        } partition[num_suits];

        // Partition the hand into the four suits,
        // Each parition constains the masks corresponding
        // to the cards of that suit.

        int j;
        for (j = 0; j < num_suits; j++) {
          partition[j].size = 0;
        }

        {
          for (j = 0; j < hand_size; j++) {
            struct desc *d = partition + suit(hand[j]);

            d->denom[d->size] = pips(hand[j]);
            d->mask[d->size++] = (1 << j);
          }
        }

        for (j = 0; j < num_suits; j++)
          if (partition[j].size >= n) {
            struct desc *st = partition + j;

            int queue[6];
            int q_rear = 0, q_front = 0;
            bool high_ace = 0;
            ace_is_low = true;

            for (int j = 0;;) {
              if (high_ace) {
                break;
              }

              int left;
              int d;
              int high;

              if (j >= st->size) {
                if (st->size == 0 || st->denom[0] != ace) {
                  break;
                }
                high_ace = true;
                ace_is_low = false;
                left = 0;
                j = 1;
                d = ace;
                high = king + 1;

              } else {
                left = j++;
                d = st->denom[left];
                high = d;
              }

              // Push the current denomination on the queue
              queue[q_rear] = left;
              q_rear += 1;

              while (q_front < q_rear - 1 &&
                     high - st->denom[queue[q_front]] + 1 > reach) {
                q_front += 1;
              }

              if (q_rear - q_front >= n &&
                  high - st->denom[queue[q_front]] + 1 == reach) {
                int subset[5];
                unsigned mask = 0;

                // Subset contains the n indicies of queue
                // that we will iterate over.  Initialize to
                // the leftmost items

                {
                  for (int z = 0; z < n - 1; z++) {
                    subset[z] = q_front + z;
                    mask |= st->mask[queue[q_front + z]];
                  }

                  subset[n - 1] = q_rear - 1;
                  mask |= st->mask[queue[q_rear - 1]];
                }

                for (;;) {
                  check(mask);

                  // Advance to the lexigraphically next subset
                  // Search from the right to find the first
                  // element of subset not at its max value

                  if (n == 2) {
                    goto done_SF;
                  }
                  // No iteration to be done for a SF 2

                  for (int z = 1;; z++) {
                    int y = n - 1 - z;
                    int index = subset[y];

                    mask ^= st->mask[index];
                    // Turn off mask bit for index

                    if (index < q_rear - 1 - z) {
                      // From y forward, fill out subset
                      // with consecutive values.
                      while (y < n - 1) {
                        subset[y++] = ++index;
                        mask |= st->mask[index];
                      }
                      break;
                    }

                    if (y == 1) {
                      goto done_SF;
                    }
                  }
                }

              done_SF: {}

                // Having iterated the current contents of the queue,
                // we now move on.
                q_front += 1;
              }
            }
          }

        if (any_flag && reach > n) {
          reach -= 1;
          goto another_reach;
        }
      } break;

      case pc_Straight_n: {
        const int size = *pat++;
        int reach = *pat++;
        bool any_flag = false;

        if (reach == 0) {
          reach = 5;
          any_flag = true;
        }

      another_reach_st: {}

        bool high_ace = false;
        ace_is_low = true;

        struct q_element {
          int left, iter, right;
        } queue[6];
        int q_rear = 0, q_front = 0;

        for (int j = 0;;) {
          if (high_ace) {
            break;
          }

          int left = j++;
          int d = denom[left];
          int high = d;

          if (d == end_marker) {
            if (hand_size == 0 || denom[0] != ace) {
              break;
            }

            // Wrap around and treat the ace as high

            high_ace = true;
            ace_is_low = false;
            left = 0;
            j = 1;
            d = ace;
            high = king + 1;
          }
          while (denom[j] == d) {
            j += 1;
          }

          // Push the current denomination on the queue
          queue[q_rear].left = left;
          queue[q_rear].right = j - 1;
          q_rear += 1;

          while (q_front < q_rear - 1 &&
                 high - denom[queue[q_front].left] + 1 > reach) {
            q_front += 1;
          }

          if (q_rear - q_front >= size &&
              high - denom[queue[q_front].left] + 1 == reach) {
            int subset[5];
            // Subset contains the "size" indicies of queue
            // that we will iterate over.  Initialize to
            // the leftmost items

            {
              for (int z = 0; z < size - 1; z++) {
                subset[z] = q_front + z;
              }

              subset[size - 1] = q_rear - 1;
            }

            for (;;) {
              {
                // Iterate the subset

                unsigned mask = 0;

                // Maintain the invariant that the mask
                // matches all the iter values in the subset

                int w;
                for (w = 0; w < size; w++) {
                  struct q_element *q = queue + subset[w];

                  q->iter = q->left;
                  mask |= (1 << q->iter);
                }

                for (;;) {
                  check(mask);

                  for (w = 0;;) {
                    struct q_element *q = queue + subset[w];

                    if (q->iter < q->right) {
                      mask ^= (3 << q->iter);
                      q->iter += 1;
                      break;
                    }

                    mask &= ~(1 << q->iter);
                    q->iter = q->left;
                    mask |= (1 << q->iter);

                    if (++w >= size) {
                      goto done_iter;
                    }
                  }
                }
              done_iter: {}
              }

              // Advance to the lexigraphically next subset
              // Search from the right to find the first
              // element of subset not at its max value

              if (size == 2) {
                goto done_subset;
              }
              // No iteration to be done for a ST 2

              for (int z = 1;; z++) {
                int y = size - 1 - z;
                int index = subset[y];
                if (index < q_rear - 1 - z) {
                  // From y forward, fill out subset
                  // with consecutive values.
                  while (y < size - 1) {
                    subset[y++] = ++index;
                  }
                  break;
                }

                if (y == 1) {
                  goto done_subset;
                }
              }
            }

          done_subset: {}

            // Having iterated the current contents of the queue,
            // we now move on.
            q_front += 1;
          }
        }

        if (any_flag && reach > size) {
          reach -= 1;
          goto another_reach_st;
        }

      } break;

      default:
        throw 0;
    }

    if (or_operand) {
      pat = or_operand;
      or_operand = 0;
      goto loop;
    }

    // Scan ahead to the end
    for (;;) {
      if (pat_eof == 0 || *pat_eof == pc_eof) {
        return;
      }

      if (*pat_eof == pc_prefer) {
        pat = pat_eof + 1;
        goto restart;
      }

      throw 0;
    }
  }
}
