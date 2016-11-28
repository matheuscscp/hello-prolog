
%{

#include <stdio.h>

#include "syntax.h"
#include "parser.h"

// flex stuff
int yylex();

// bison stuff
static void yyerror(const char*);

%}

%union {
  int u;
  token_t tok;
}

%type <u> program
%type <u> clause_list
%type <u> clause
%type <u> predicate
%type <u> structure
%type <u> atom
%type <u> term_list
%type <u> term
%type <u> predicate_list
%type <u> predicate_list_item
%type <u> query
%type <u> arith_expr
%type <u> arith_term
%type <u> arith_fact
%type <u> arith_add
%type <u> arith_mul
%type <u> arith_op

%token <tok> SMALLATOM
%token <tok> VARIABLE
%token <tok> NUMERAL
%token <tok> STRING

%define parse.lac full
%define parse.error verbose

%%

program:
  clause_list query {
    syntax_push_child(0,$1);
    syntax_push_child(0,$2);
  }
  | clause_list {
    syntax_push_child(0,$1);
    syntax_push_child(0,syntax_create_node());
  }
  | query {
    syntax_push_child(0,syntax_create_node());
    syntax_push_child(0,$1);
  }
  | %empty {
    syntax_push_child(0,syntax_create_node());
    syntax_push_child(0,syntax_create_node());
  }
  ;

clause_list:
  clause_list clause {
    $$ = $1;
    syntax_push_child($$,$2);
  }
  | clause {
    $$ = syntax_create_node();
    syntax_push_child($$,$1);
  }
  ;

clause:
  predicate '.' {
    $$ = $1;
    vat(node,parser_st,$$).type = N_FACT;
  }
  | predicate ':' predicate_list '.' {
    $$ = syntax_typed_node(N_RULE);
    syntax_push_child($$,$1);
    syntax_push_child($$,$3);
  }
  | error '.' {
    $$ = -1;
    yyerrok;
  }
  ;

predicate:
  structure {
    $$ = $1;
    vat(node,parser_st,$$).type = N_PREDICATE;
  }
  | arith_expr arith_add arith_term {
    $$ = $2;
    syntax_push_child($$,$1);
    syntax_push_child($$,$3);
  }
  ;

structure:
  atom {
    $$ = syntax_typed_node(N_STRUCTURE);
    syntax_push_child($$,$1);
    syntax_push_child($$,syntax_create_node());
  }
  | atom '(' term_list ')' {
    $$ = syntax_typed_node(N_STRUCTURE);
    syntax_push_child($$,$1);
    syntax_push_child($$,$3);
  }
  ;

atom:
  SMALLATOM {
    $$ = syntax_token_node(N_ATOM,$1);
  }
  | NUMERAL {
    $$ = syntax_token_node(N_ATOM,$1);
  }
  | STRING {
    $$ = syntax_token_node(N_ATOM,$1);
  }
  ;

term_list:
  term_list ',' term {
    $$ = $1;
    syntax_push_child($$,$3);
  }
  | term {
    $$ = syntax_create_node();
    syntax_push_child($$,$1);
  }
  ;

term:
  arith_expr
  | '_' {
    $$ = syntax_typed_node(N_DONTCARE);
  }
  ;

predicate_list:
  predicate_list ',' predicate_list_item {
    $$ = $1;
    syntax_push_child($$,$3);
  }
  | predicate_list_item {
    $$ = syntax_create_node();
    syntax_push_child($$,$1);
  }
  ;

predicate_list_item:
  predicate
  | '!' {
    $$ = syntax_typed_node(N_CUT);
  }
  ;

query:
  '?' predicate_list {
    $$ = $2;
  }
  ;

arith_expr:
  arith_expr arith_add arith_term {
    $$ = $2;
    syntax_push_child($$,$1);
    syntax_push_child($$,$3);
  }
  | arith_term
  ;

arith_term:
  arith_term arith_mul arith_fact {
    $$ = $2;
    syntax_push_child($$,$1);
    syntax_push_child($$,$3);
  }
  | arith_fact
  ;

arith_fact:
  '(' arith_expr ')' {
    $$ = $2;
  }
  | arith_op
  ;

arith_add:
  '+' {
    $$ = syntax_typed_node(N_ADD);
  }
  | '-' {
    $$ = syntax_typed_node(N_SUB);
  }
  ;

arith_mul:
  '*' {
    $$ = syntax_typed_node(N_MUL);
  }
  | '/' {
    $$ = syntax_typed_node(N_DIV);
  }
  ;

arith_op:
  structure
  | VARIABLE {
    $$ = syntax_token_node(N_VARIABLE,$1);
  }
  ;

%%

static void yyerror(const char* s) {
  parser_error_flag = 1;
  fprintf(stderr,"%s:%d:%d: %s\n",parser_fn,yylval.tok.ln,yylval.tok.cl,s);
}
