#pragma once

#include "vpoker.h"
#include <list>
#include <set>
#include <vector>

typedef class C_bool_matrix {
  bool *data;
  int order;
public:
  C_bool_matrix (int n);
  C_bool_matrix (const C_bool_matrix&);
  ~C_bool_matrix();

  inline bool& at(int i, int j) {
    _ASSERT (0<= i && i< order);
    _ASSERT (0<=j && j<order);
    return data[i*order+j];
  }

  inline int size() {
    return order;
  }
} bool_matrix;

struct move_desc;

struct mlist {
  move_desc *head;
  struct mlist *tail;
  card c_hand[5];
  unsigned char c_mask;
};

typedef std::list<move_desc*> mmm;

struct move_desc {
  virtual char *name() = 0;
  mmm ccc;
  mmm prefer;

  // During cycle-killing, some of ccc get moved to cyclic.
  mmm cyclic;

  struct mlist *conflicts;
  int df_number;
  move_desc *pop;
  move_desc *scc_parent_desc;
  struct mlist *scc_parent_edge;
  struct mlist *scc_backlink;
  int scc_id;
  bool visited;
  bool stacked;
  int line;
  int print_id;
  int value;
  int value_id;
  bool used;

  // The scc's form a equivalence class over the move_desc objects.
  // For each x, x->scc_repr points to the representative of x (which may
  // be x). Each representative is the head of a singly-linked list of elements
  // that is linked thorugh scc_next.
  move_desc *scc_repr;
  move_desc *scc_next;

  // Layers get assigned so that if there is an edge from x to y, if x and y
  // are in the same SCC, then x->layer == y->layer, else x->layer > y->layer.
  int layer;

  move_desc();
  //_move_desc(char *n);
};


#if 0
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};


typedef std::map<const char*, move_desc*, ltstr> string_map;
#endif

typedef std::list<move_desc*> move_iter;;
typedef std::map<int, move_desc*> int_map;

typedef std::map<move_desc*, int> move_map;

typedef class C_move_list {
  struct move_info {
    move_desc *m;
    double total_weight;
    double max_weight;
    card hand[5];
    unsigned char mask;

    move_info (move_desc *i) : m(i), total_weight(0.0),
      max_weight (0.0) {};
  };

  struct move_pair {
    move_info x1, x2;
    // By convention x1.m < x2.m;

    bool operator< (const move_pair &r) const;
    // To allow sets

    // A measure of how much this conflict matters.
    double significance() const {
      const double diff = x1.total_weight - x2.total_weight;
      return diff < 0.0 ? -diff : diff;
      // return std::abs(x1.total_weight - x2.total_weight);
    }

    move_pair(move_desc *i1, move_desc*i2);
  };

  struct sort_by_significance {
    bool operator()(const move_pair &left, const move_pair &right) const {
      return left.significance() > right.significance();
    }
  };

  typedef std::vector<const move_pair*> move_pair_vector;

  struct move_pair_lt {
    bool operator() (const move_pair* &l, const move_pair* &r) const;
  };

  typedef std::set<move_pair> move_pair_set;
  move_pair_set conflicts;

  typedef std::set<move_desc*> move_set;
  move_set good_moves;

  int hand_size;
  move_map move_number;
  int number_moves;

  int scc_algorithm (move_desc *x);
  //struct mlist *parent_edge,
  //move_desc *parent_desc);

  int new_scc_algorithm(move_desc *x);

  void greedy_cycle_killer(const std::set<move_desc*> &component);
  bool has_cycle(const std::set<move_desc*> &component);
  bool detect_cycle(move_desc *m);

  typedef std::vector<move_desc*> move_vector;
  move_vector print_order;
  move_vector haas_index;
  int print_counter;

  void remove_cycles(move_desc *m);

  void find_closure(FILE *file);
  move_iter moves;
  void print_moves(FILE *file);
  void print_the_answer (FILE *file, bool_matrix &haas);

  int df_counter;
  move_desc *stack;
  int scc_counter;
  bool do_print_haas;
  bool do_print_value;

  FILE *output_file;

public:
  C_move_list(int hsize);

  int_map move_index;

  void add_move (move_desc *m);
  // Register a move with the class.
  // Must register all moves before creating
  // any conflicts.

  void add_conflict (move_desc* right,
                     move_desc* wrong,
                     double weight,
                     card *c_hand,
                     unsigned right_move);

  void display(FILE *file, bool deuces, bool print_haas, bool print_value);
  void sort_moves(FILE *file);

} move_list;

