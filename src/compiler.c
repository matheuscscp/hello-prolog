#include <stdio.h>

#include "compiler.h"

#include "parser.h"
#include "code.h"

// flex/bison stuff
int yyparse();
void yy_scan_string(const char*);
extern FILE* yyin;

static int do_compile() {
  parser_init();
  yyparse();
  if (!parser_error_flag) code();
  parser_close();
  return parser_error_flag;
}

int compile(const char* src) {
  parser_fn = "stdin";
  yy_scan_string(src);
  return do_compile();
}

int compile_file(const char* fn) {
  parser_fn = fn;
  yyin = fopen(fn,"r");
  return do_compile();
}
