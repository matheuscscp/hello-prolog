#ifndef SYNTAX_H
#define SYNTAX_H

#include "vector.h"
#include "lexical.h"

enum {
  N_NOTYPE = 0,
  // internal nodes
  NBEGIN_INTERNAL,
    N_FACT, // same as N_STRUCUTRE
    N_RULE, // left: N_PREDICATE; right: {N_PREDICATE,N_CUT}+
    N_PREDICATE, // same as N_STRUCUTRE
    N_STRUCTURE, // left: N_ATOM; right: {N_STRUCTURE,TERMS}*
    // arithmetic operators
    NBEGIN_ARITH,
      N_ADD,
      N_SUB,
      N_MUL,
      N_DIV,
    NEND_ARITH,
  NEND_INTERNAL,
  // leaves
  NBEGIN_LEAF,
    N_ATOM,       // SMALLATOM, NUMERAL and STRING tokens
    // TERMS
    NBEGIN_TERM,
      N_VARIABLE, // VARIABLE token
      N_DONTCARE, // '_' token
    NEND_TERM,
    N_CUT,        // '!' token
  NEND_LEAF
};
typedef struct {
  int type;
  token_t tok;
  int val;//semantic
  vector_t v;
} node;

void syntax_free_tree(vector_t tree);

int syntax_create_node();
int syntax_typed_node(int type);
int syntax_token_node(int type, token_t tok);
void syntax_push_child(int u, int v);

#endif
