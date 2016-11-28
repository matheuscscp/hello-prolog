#include <stdlib.h>
#include <string.h>

#include "syntax.h"

#include "parser.h"

void syntax_free_tree(vector_t tree) {
  for (int i = 0; i < tree.n; i++) {
    node nd = vat(node,tree,i);
    lexical_delete_token(nd.tok);
    vdelete(nd.v);
  }
  vdelete(tree);
}

int syntax_create_node() {
  return syntax_typed_node(N_NOTYPE);
}

int syntax_typed_node(int type) {
  return syntax_token_node(type,lexical_create_token(0,0));
}

int syntax_token_node(int type, token_t tok) {
  int u = parser_st.n;
  node nd;
  nd.type = type;
  nd.tok = tok;
  nd.val = 0;
  vinit(nd.v);
  vpush(node,parser_st,nd);
  return u;
}

void syntax_push_child(int u, int v) {
  vpush(int,vat(node,parser_st,u).v,v);
}
