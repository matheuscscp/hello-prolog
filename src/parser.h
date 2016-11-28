#ifndef PARSER_H
#define PARSER_H

#include "vector.h"

#ifndef PARSER_IMPL_H
extern const char* parser_fn;
extern int parser_error_flag;
extern int parser_ln, parser_cl, parser_cln, parser_ccl;
extern vector_t parser_st;
#endif

void parser_init();
void parser_close();

#endif
