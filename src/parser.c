#define PARSER_IMPL_H
#include "parser.h"

#include "syntax.h"

const char* parser_fn;
int parser_error_flag;
int parser_ln, parser_cl, parser_cln, parser_ccl;
vector_t parser_st;

void parser_init() {
  parser_error_flag = 0;
  parser_ln = 1, parser_cl = 1;
  vinit(parser_st);
  syntax_create_node(); // root = 0
}

void parser_close() {
  syntax_free_tree(parser_st);
}
