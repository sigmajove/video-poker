#include "find_order.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <queue>
#include <set>
#include <string>
#include <vector>

struct denom {
  char val;
  char count;
};

std::string format_hand(const card *hand, int size) {
  std::string result;
  result.reserve(3 * size);
  for (int j = 0; j < size;) {
    result.push_back(denom_image[pips(*hand)]);
    result.push_back(suit_image[suit(*hand)]);
    ++hand;
    ++j;

    if (j < size) {
      result.push_back(' ');
    }
  }
  return result;
}

void print_hand(FILE *file, const card *hand, int size) {
  fprintf(file, "%s\n", format_hand(hand, size).c_str());
}

move_desc::move_desc()
    : conflicts(0),
      df_number(0),
      pop(0),
      scc_parent_desc(0),
      scc_parent_edge(0),
      scc_backlink(0),
      scc_id(-1),
      used(false),
      visited(false),
      stacked(false),
      line(-1),
      print_id(-1),
      value(-1),
      value_id(-1),
      scc_repr(NULL),
      scc_next(NULL),
      layer(-1) {}

typedef std::vector<int> int_vector;
typedef std::vector<bool> bool_vector;

static bool name_move_creates = true;

#if 0

move_desc* C_move_list::get_move (char *s) {
  string_map::iterator x = moves.find(s);

  if (x == moves.end()) {
    // if (!name_move_creates) return 0;

    char *copy = strcpy (new char[strlen(s)+1], s);
    move_desc* value = new move_desc (copy);
    moves[copy] = value;
    return value;
  } else {
    return x->second;
  }
}
#endif

void C_move_list::add_move(move_desc *m) { moves.push_back(m); }

#if 0
void C_move_list::print_moves(FILE *file) {
  string_map::iterator rover = moves.begin();
  while (rover != moves.end()) {
    fprintf (file, "%s\n", rover->first);
    ++rover;
  }
}
#endif

#ifdef NEVER
move_desc *resolve(char code, int *denom, int msize, int *discard, int dsize)

{
  char buffer[10];
  char *p = buffer;
  *p++ = code;

  for (int j = 0; j < msize; j++) {
    *p++ = denom_image[denom[j]];
  }

  *p++ = '/';

  for (j = 0; j < dsize; j++) {
    *p++ = denom_image[discard[j]];
  }

  *p++ = 0;

  return find_or_create(buffer);
}
#endif

bool_matrix::bool_matrix(int n) : order(n) {
  const int size = order * order;
  data = new bool[size];

  for (int i = 0; i < size; i++) {
    data[i] = false;
  }
}

bool_matrix::bool_matrix(const bool_matrix &t) : order(t.order) {
  const int size = order * order;
  data = new bool[size];

  for (int i = 0; i < size; i++) {
    data[i] = t.data[i];
  }
}

bool_matrix::~bool_matrix() { delete[] data; }

/// This should be a template ////

typedef class C_desc_matrix {
  mlist **data;
  int order;

 public:
  C_desc_matrix(int n);
  C_desc_matrix(const C_desc_matrix &);
  ~C_desc_matrix();

  inline mlist *&at(int i, int j) {
    _ASSERT(0 <= i && i < order);
    _ASSERT(0 <= j && j < order);
    return data[i * order + j];
  }

  inline int size() { return order; }
} desc_matrix;

C_desc_matrix::C_desc_matrix(int n) {
  order = n;
  const int size = n * n;
  data = new mlist *[size];

  for (int i = 0; i < size; i++) {
    data[i] = 0;
  }
}

C_desc_matrix::C_desc_matrix(const C_desc_matrix &t) {
  order = t.order;
  const int size = order * order;
  data = new mlist *[size];

  for (int i = 0; i < size; i++) {
    data[i] = t.data[i];
  }
}

C_desc_matrix::~C_desc_matrix() { delete[] data; }

/// end pseudo template ////

void Warshall(bool_matrix &m) {
  const int n = m.size();
  for (int j = 0; j < n; j++)
    for (int i = 0; i < n; i++)
      if (i != j) {
        if (m.at(i, j)) {
          for (int k = 0; k < n; k++) {
            m.at(i, k) |= m.at(j, k);
          }
        }
      }
}

void multiply(bool_matrix &dst, bool_matrix &x, bool_matrix &y) {
  int n = dst.size();
  _ASSERT(x.size() == n);
  _ASSERT(y.size() == n);

  for (int i = 0; i < n; i++)
    for (int k = 0; k < n; k++) {
      bool val = false;
      for (int j = 0; j < n; j++) {
        val |= x.at(i, j) & y.at(j, k);
      }

      dst.at(i, k) = val;
    }
}

C_move_list::move_pair::move_pair(move_desc *i1, move_desc *i2)
    : x1(i1 < i2 ? i1 : i2),
      x2(i1 < i2 ? i2 : i1)

{}

bool C_move_list::move_pair::operator<(const move_pair &r) const {
  // Lexicographic ordering
  if (x1.m < r.x1.m) {
    return true;
  }
  if (x1.m > r.x1.m) {
    return false;
  }
  return (x2.m < r.x2.m);
};

void C_move_list::add_conflict(move_desc *right, move_desc *wrong,
                               double weight, card *c_hand,
                               unsigned right_move) {
  _ASSERT(right != wrong);

  const move_pair &s = *(conflicts.insert(move_pair(right, wrong)).first);
  const move_info &const_i = (s.x1.m == right) ? s.x1 : s.x2;
  move_info &i = *const_cast<move_info *>(&const_i);

  if (weight < 0.0) {
    print_hand(stdout, c_hand, 5);
    printf("right = %s\n", right->name());
    printf("wrong = %s\n", wrong->name());
    printf("weight = %.5f\n", weight);
    throw 0;
  }

  i.total_weight += weight;

  if (weight > i.max_weight) {
    i.max_weight = weight;
    for (int j = 0; j < hand_size; j++) {
      i.hand[j] = c_hand[j];
    }
    i.mask = right_move;
  }
}

static void print_hand_edge(FILE *file, struct mlist *edge, int hand_size) {
  unsigned mask = edge->c_mask;

  fprintf(file, "(");
  for (int j = 0; j < hand_size; j++) {
    card c = edge->c_hand[j];
    fprintf(file, "%c%c", denom_image[pips(c)], suit_image[suit(c)]);
    if (mask & 1) {
      fprintf(file, "*");
    }
    if (j != hand_size - 1) {
      fprintf(file, " ");
    }

    mask >>= 1;
  }
  fprintf(file, ") ");
}

int C_move_list::new_scc_algorithm(move_desc *x) {
  if (x->df_number != 0) {
    return x->df_number;
  }

  int min_lowlink = ++df_counter;
  x->df_number = min_lowlink;

  x->pop = stack;
  stack = x;

  for (MoveDescList::iterator rover = x->ccc.begin(); rover != x->ccc.end();
       ++rover) {
    move_desc *y = *rover;
    if (y->scc_id == -1) {
      const int z = new_scc_algorithm(y);
      if (z < min_lowlink) {
        min_lowlink = z;
      }
    }
  }

  if (min_lowlink == x->df_number) {
    std::set<move_desc *> scc;
    for (move_desc *rover = stack;; rover = rover->pop) {
      scc.insert(rover);
      rover->scc_id = scc_counter;

      if (rover == x) {
        break;
      }
    }

    stack = x->pop;
    scc_counter += 1;

    if (scc.size() > 1) {
      greedy_cycle_killer(scc);
    }
  }

  return min_lowlink;
}

bool C_move_list::has_cycle(const std::set<move_desc *> &component) {
  bool result = false;
  for (std::set<move_desc *>::const_iterator iter = component.begin();
       iter != component.end(); ++iter) {
    result |= detect_cycle(*iter);
  }
  for (std::set<move_desc *>::const_iterator iter = component.begin();
       iter != component.end(); ++iter) {
    move_desc *const here = *iter;
    here->visited = false;
  }
  return result;
}

bool C_move_list::detect_cycle(move_desc *m) {
  bool has_cycle = false;
  m->visited = true;
  m->stacked = true;

  for (MoveDescList::iterator rover = m->cyclic.begin();
       rover != m->cyclic.end(); ++rover) {
    move_desc *const here = *rover;

    if (here->visited) {
      if (here->stacked) {
        has_cycle = true;
      }
    } else {
      has_cycle |= detect_cycle(here);
    }
  }
  m->stacked = false;
  return has_cycle;
}

void C_move_list::greedy_cycle_killer(const std::set<move_desc *> &component) {
  std::vector<move_pair> edges;
  for (std::set<move_desc *>::const_iterator iter = component.begin();
       iter != component.end(); ++iter) {
    move_desc *const vertex1 = *iter;
    MoveDescList &ccc = vertex1->ccc;
    MoveDescList keep;

    while (!ccc.empty()) {
      move_desc *const vertex2 = ccc.front();
      ccc.pop_front();

      if (component.find(vertex2) == component.end()) {
        // Not an intra component edge.  Keep it.
        keep.push_front(vertex2);
      } else {
        const move_pair_set::const_iterator found =
            conflicts.find(move_pair(vertex1, vertex2));
        _ASSERT(found != conflicts.end());
        edges.push_back(*found);
      }
    }

    ccc.splice(ccc.begin(), keep);
  }

  // Sort the edges with the most significant at the beginning.
  std::sort(edges.begin(), edges.end(), sort_by_significance());

  for (std::vector<move_pair>::iterator rover = edges.begin();
       rover != edges.end(); rover++) {
    const move_pair &q = *rover;
    const move_info *good;
    const move_info *bad;
    if (q.x1.total_weight > q.x2.total_weight) {
      good = &q.x1;
      bad = &q.x2;
    } else {
      good = &q.x2;
      bad = &q.x1;
    }
  }

  // Iterate the edges in order of significance, permanantely
  // adding back ones that don't create cycles.
  for (std::vector<move_pair>::iterator rover = edges.begin();
       rover != edges.end(); rover++) {
    move_pair &q = *rover;
    move_info *good;
    move_info *bad;
    if (q.x1.total_weight > q.x2.total_weight) {
      good = &q.x1;
      bad = &q.x2;
    } else {
      good = &q.x2;
      bad = &q.x1;
    }
    good->m->cyclic.push_front(bad->m);
    if (has_cycle(component)) {
      // Take it back out.
      good->m->cyclic.pop_front();
      _ASSERT(!has_cycle(component));

      fprintf(output_file, "%.8f Exception: %s << %s [%s]\n",
              rover->significance(), bad->m->name(), good->m->name(),
              format_hand(good->hand, hand_size).c_str());
    }
  }

  // Add the non-cycle producing edges to the real graph.
  for (std::set<move_desc *>::const_iterator iter = component.begin();
       iter != component.end(); ++iter) {
    move_desc *const here = *iter;
    here->ccc.splice(here->ccc.begin(), here->cyclic);
  }
}

FILE *debug_file;
MoveDescList debug_stack;

void C_move_list::remove_cycles(move_desc *m) {
  m->visited = true;
  m->stacked = true;
  debug_stack.push_front(m);
  int value = 0;

  for (MoveDescList::iterator rover = m->ccc.begin(); rover != m->ccc.end();) {
    MoveDescList::iterator here = rover++;
    move_desc *h = *here;

    // if (h->scc_id == m->scc_id)
    {
      if (h->visited) {
        if (h->stacked) {
          h->prefer.push_back(m);
          m->ccc.erase(here);

          fprintf(debug_file, "Begin cycle\n");
          MoveDescList::iterator rover = debug_stack.begin();
          for (;;) {
            fprintf(debug_file, "%s\n", (*rover)->name());
            if ((*rover) == h) {
              break;
            }
            rover++;
          }
          fprintf(debug_file, "End cycle\n");
        } else {
          if (h->value >= value) {
            value = h->value + 1;
          }
        }
      } else {
        remove_cycles(h);
        if (h->value >= value) {
          value = h->value + 1;
        }
      }
    }
  };

  m->value = value;
  m->print_id = --print_counter;
  print_order.at(print_counter) = m;

  m->stacked = false;
  debug_stack.pop_front();
}

;

bool C_move_list::move_pair_lt::

operator()(const move_pair *&l, const move_pair *&r) const {
  double l_key = l->x1.total_weight < l->x2.total_weight ? l->x1.total_weight
                                                         : l->x2.total_weight;

  double r_key = r->x1.total_weight < r->x2.total_weight ? r->x1.total_weight
                                                         : r->x2.total_weight;

  return l_key > r_key;
}

struct SortByValue {
  bool operator()(const move_desc *left, const move_desc *right) const {
    if (left->value > right->value) return true;
    if (left->value < right->value) return false;
    return left->line < right->line;
  }
};

void C_move_list::sort_moves(FILE *file) {
  move_pair_vector bad_boyz;

  for (move_pair_set::iterator zzz = conflicts.begin(); zzz != conflicts.end();
       ++zzz) {
    const move_pair &q = *zzz;
    const move_info &good =
        (q.x1.total_weight > q.x2.total_weight ? q.x1 : q.x2);
    const move_info &bad =
        (q.x1.total_weight > q.x2.total_weight ? q.x2 : q.x1);

    good.m->ccc.push_front(bad.m);
    good_moves.insert(good.m);
    good_moves.insert(bad.m);

    if (bad.total_weight > 0.0) {
      bad_boyz.push_back(&q);
    }
  }

  std::sort<move_pair_vector::iterator>(bad_boyz.begin(), bad_boyz.end(),
                                        move_pair_lt());

  if (bad_boyz.size() != 0) {
    fprintf(file, "Inaccurate moves\n\n");
    for (move_pair_vector::iterator rover = bad_boyz.begin();
         rover != bad_boyz.end(); rover++) {
      const move_pair &q = **rover;
      std::string move1 =
          std::format("{}\n{}", q.x1.m->name(),
                      move_image(q.x1.hand, hand_size, q.x1.mask));
      std::string move2 =
          std::format("{}\n{}", q.x2.m->name(),
                      move_image(q.x2.hand, hand_size, q.x2.mask));

      // For some reason these don't show up in a consistent order.
      // Swap them for test reproducibility.
      if (move1 > move2) {
        std::swap(move1, move2);
      }

      fputs(move1.c_str(), file);
      fputs(move2.c_str(), file);

      fprintf(file, "weight %.8f\n\n",
              q.x1.total_weight < q.x2.total_weight ? q.x1.total_weight
                                                    : q.x2.total_weight);
    }
  }

  print_counter = good_moves.size();
  print_order.resize(print_counter);

  // New code:  locate the SCCs of the graph
  // and greedily remove any cycles.
  for (move_set::iterator rover = good_moves.begin(); rover != good_moves.end();
       rover++) {
    if ((*rover)->scc_id == -1) {
      new_scc_algorithm(*rover);
    }
  }

  for (move_set::iterator rover = good_moves.begin(); rover != good_moves.end();
       rover++) {
    if (!(*rover)->visited) {
      remove_cycles(*rover);
    }
  };
  haas_index = print_order;
  std::sort(print_order.begin(), print_order.end(), SortByValue());
  int id_counter = -1;
  for (move_vector::iterator iter = print_order.begin();
       iter != print_order.end(); ++iter) {
    if (iter == print_order.begin() || iter[0]->value != iter[-1]->value) {
      id_counter = 0;
    }
    if (id_counter == 0 &&
        (iter + 1 == print_order.end() || iter[0]->value != iter[1]->value)) {
      (*iter)->value_id = -1;  // No need for a disambiguator
    } else {
      (*iter)->value_id = id_counter++;
    }
  }
}

void C_move_list::display(FILE *file, bool deuces, bool print_haas,
                          bool print_value) {
  do_print_haas = print_haas;
  do_print_value = print_value;
  debug_file = file;
  output_file = file;
  sort_moves(file);

  if (deuces) {
    int d = 5 - hand_size;
    fprintf(file, "%d Deuce%s\n", d, d == 1 ? "" : "s");
  }

  find_closure(file);

  if (deuces) {
    fprintf(file, "\n");
  };

  output_file = 0;
}

typedef std::queue<int> int_queue;

static void print_dfs(int id, int scc_counter, bool_vector &printed,
                      int_queue &result, int_vector &index, bool_matrix &haas) {
  if (printed[id]) {
    return;
  }
  printed[id] = true;

  for (int n = 0; n < scc_counter; n++) {
    if (haas.at(id, n)) {
      print_dfs(n, scc_counter, printed, result, index, haas);
    }
  }

  index[id] = result.size();
  result.push(id);
}

class conflict_dfs {
 public:
  FILE *file;
  int hand_size;
  desc_matrix C;
  move_desc **index;
  bool *done;
  conflict_dfs(int n, move_desc *root);
  ~conflict_dfs();
  void dfs(int i);
};

conflict_dfs::conflict_dfs(int n, move_desc *root) : C(n) {
  index = new move_desc *[n];
  done = new bool[n];

  move_desc *rover = root;
  int j = 0;
  while (rover) {
    index[j] = rover;
    done[j] = false;
    mlist *x = rover->conflicts;

    while (x) {
      move_desc *here = x->head;
      if (here->scc_id == root->scc_id) {
        C.at(rover->df_number, here->df_number) = x;
      }
      x = x->tail;
    }

    rover = rover->pop;
    j += 1;
  }
}

conflict_dfs::~conflict_dfs() {
  delete index;
  delete done;
}

void conflict_dfs::dfs(int i) {
  if (done[i]) {
    return;
  }
  done[i] = true;

  for (int j = 0; j < C.size(); j++) {
    if (C.at(j, i)) {
      mlist *i_good = C.at(j, i);
      mlist *j_good = C.at(i, j);

      C.at(i, j) = 0;
      C.at(j, i) = 0;

      if (i_good != 0 && j_good != 0) {
        fputs(index[i]->name(), file);
        fputc(' ', file);
        print_hand_edge(file, i_good, hand_size);
        fputs("?? ", file);
        fputs(index[j]->name(), file);
        fputc(' ', file);
        print_hand_edge(file, j_good, hand_size);
        fputc('\n', file);
      } else {
        fputs(index[i]->name(), file);
        fputc(' ', file);
        print_hand_edge(file, i_good, hand_size);
        fputs(">> ", file);
        fputs(index[j]->name(), file);
        fputc('\n', file);
      }

      dfs(j);
    }
  }
}

static void new_print_conflict(FILE *file, move_desc *root, int hand_size) {
  move_desc *rover = root;
  int n = 0;

  while (rover) {
    rover->df_number = n;  // Reuse this field because we are done with it
    rover = rover->pop;
    n += 1;
  }

  conflict_dfs CD(n, root);
  CD.file = file;
  CD.hand_size = hand_size;

  for (int i = 0; i < n; i++) {
    CD.dfs(i);
  }
}

static void print_answer(FILE *file, int scc_counter, int_queue &order,
                         int_vector &index, int_map move_index,
                         bool_matrix haas, int hand_size, bool print_haas) {
  int seqno = 0;

  while (!order.empty()) {
    move_desc *x = move_index[order.front()];

    if (print_haas) {
      fprintf(file, "%d) ", seqno);
    }

    fprintf(file, "%s", x->pop == 0 ? x->name() : "== Begin Conflict ==");

    if (print_haas) {
      fprintf(file, " ->");
      for (int i = 0; i < scc_counter; i++) {
        if (haas.at(i, x->scc_id)) {
          fprintf(file, " %d", index[i]);
        }
      }
    }
    fprintf(file, "\n");

    if (x->pop) {
      new_print_conflict(file, x, hand_size);

      if (false)
        for (;;) {
          if (x->scc_parent_edge) {
            fprintf(file, "%s ", x->scc_parent_desc->name());
            print_hand_edge(file, x->scc_parent_edge, hand_size);
          }

          if (x->scc_parent_edge || x->scc_backlink) {
            fprintf(file, "%s ", x->name());
          }

          if (x->scc_backlink) {
            print_hand_edge(file, x->scc_backlink, hand_size);
            fprintf(file, "%s ", x->scc_backlink->head->name());
          }

          if (x->scc_parent_edge || x->scc_backlink) {
            fprintf(file, "\n");
          }

          x = x->pop;
          if (x == 0) {
            break;
          }
        }

      if (print_haas) {
        fprintf(file, "%d) ", seqno);
      }

      fprintf(file, "== End Conflict ==\n");
    }

    order.pop();
    seqno += 1;
  }
}

static std::string FormatId(const move_desc *move) {
  if (move->value_id < 0) {
    return std::format("{}", move->value);
  } else {
    return std::format("{}{}", move->value,
                       static_cast<char>('a' + move->value_id));
  }
}

void C_move_list::print_the_answer(FILE *file, bool_matrix &haas) {
  for (move_vector::iterator rover = print_order.begin();
       rover != print_order.end(); rover++) {
    if (do_print_haas) {
      fprintf(file, "%s) ", FormatId(*rover).c_str());
    } else if (do_print_value) {
      fprintf(file, "%d) ", (*rover)->value);
    }

    fprintf(file, "%s", (*rover)->name());

    for (MoveDescList::iterator inner = (*rover)->prefer.begin();
         inner != (*rover)->prefer.end(); inner++) {
      fprintf(file, " << %s", (*inner)->name());
    }

    if (do_print_haas) {
      fprintf(file, " ->");
      std::vector<std::string> pieces;
      for (int i = 0; i < haas.size(); i++) {
        if (haas.at((*rover)->print_id, i)) {
          // Print the hand that inspired this edge.
          move_pair_set::iterator found =
              conflicts.find(move_pair(*rover, haas_index[i]));
          _ASSERT(found != conflicts.end());
          const move_info &print_desc =
              found->x1.total_weight > found->x2.total_weight ? found->x1
                                                              : found->x2;
          pieces.push_back(
              std::format(" {}[{}]", FormatId(haas_index[i]),
                          format_hand(print_desc.hand, hand_size)));
        }
      }
      // Sort the pieces for test reproducibility
      std::sort(pieces.begin(), pieces.end());
      for (const std::string &p : pieces) {
        fprintf(file, "%s", p.c_str());
      }
      fprintf(file, "\n");
    }
  }
}

void C_move_list::find_closure(FILE *file) {
  const int N = print_order.size();
  bool_matrix adj(N);

  for (move_vector::iterator rover = print_order.begin();
       rover != print_order.end(); rover++) {
    for (MoveDescList::iterator inner = (*rover)->ccc.begin();
         inner != (*rover)->ccc.end(); inner++) {
      adj.at((*rover)->print_id, (*inner)->print_id) = true;
    }
  }

  Warshall(adj);

  bool_matrix close(adj);
  Warshall(close);

  bool_matrix squared(N);
  multiply(squared, close, close);

  bool_matrix haas(close);

  {
    for (int i = 0; i < N; i++)
      for (int j = 0; j < N; j++) {
        haas.at(i, j) &= !squared.at(i, j);
      }
  }

  print_the_answer(file, haas);
}

C_move_list::C_move_list(int hsize) {
  hand_size = hsize;
  df_counter = 0;
  stack = 0;
  scc_counter = 0;
  output_file = 0;
}
