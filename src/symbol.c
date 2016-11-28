#include "symbol.h"

void symbol_table_init(symbol_table_t* this) {
  this->htable = NULL;
  vinit(this->table);
}

void symbol_table_delete(symbol_table_t* this) {
  symbol_htable* cur;
  symbol_htable* tmp;
  HASH_ITER(hh,this->htable,cur,tmp) {
    HASH_DEL(this->htable,cur);
    symbol_t* s = &vat(symbol_t,this->table,cur->i);
    free(cur);
    free((char*)s->sym);
  }
  vdelete(this->table);
}

symbol_t* symbol_table_get(symbol_table_t* this, const char* sym) {
  symbol_htable* tmp;
  HASH_FIND_STR(this->htable,sym,tmp);
  if (!tmp) {
    // table
    symbol_t s = {strdup(sym),this->table.n,0};
    vpush(symbol_t,this->table,s);
    // htable
    tmp = (symbol_htable*)malloc(sizeof(symbol_htable));
    tmp->sym = s.sym;
    *(int*)&tmp->i = s.i;
    HASH_ADD_KEYPTR(hh,this->htable,tmp->sym,strlen(tmp->sym),tmp);
  }
  return &vat(symbol_t,this->table,tmp->i);
}

symbol_t* symbol_table_find(symbol_table_t* this, const char* sym) {
  symbol_htable* tmp;
  HASH_FIND_STR(this->htable,sym,tmp);
  if (!tmp) return NULL;
  return &vat(symbol_t,this->table,tmp->i);
}

symbol_t* symbol_table_at(symbol_table_t* this, int i) {
  return (i < this->table.n ? &vat(symbol_t,this->table,i) : NULL);
}
