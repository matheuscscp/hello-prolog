#ifndef SYMBOL_H
#define SYMBOL_H

#include "uthash.h"
#include "vector.h"

// constructor
#define symbol_table(X) symbol_table_t X = {NULL,{0,0,NULL}}
#define syminit(X)      symbol_table_init(&(X))

// access
#define  symget(X,Y)  symbol_table_get(&(X),Y)
#define symfind(X,Y) symbol_table_find(&(X),Y)
#define   symat(X,Y)   symbol_table_at(&(X),Y)

// destructor
#define symdel(X) symbol_table_delete(&(X))

typedef struct {
  const char* sym;
  const int i;
  UT_hash_handle hh;
} symbol_htable;

typedef struct {
  const char* sym;
  const int i;
  int val;
} symbol_t;

typedef struct {
  symbol_htable* htable;
  vector_t table;
} symbol_table_t;

void symbol_table_init(symbol_table_t* this);
void symbol_table_delete(symbol_table_t* this);
symbol_t* symbol_table_get(symbol_table_t* this, const char* sym);
symbol_t* symbol_table_find(symbol_table_t* this, const char* sym);
symbol_t* symbol_table_at(symbol_table_t* this, int i);

#endif
