#include <stdlib.h>
#include <string.h>

#include "vector.h"

void vector_construct(vector_t* this) {
  this->c = 0;
  this->n = 0;
  this->buf = NULL;
}

void vector_push(int s, vector_t* this, void* data) {
  if (this->c == 0) {
    this->c = 1;
    this->n = 1;
    this->buf = malloc(s);
    memcpy(this->buf,data,s);
    return;
  }
  if (this->n == this->c) {
    this->c <<= 1;
    this->buf = realloc(this->buf,s*this->c);
  }
  memcpy(this->buf+this->n*s,data,s);
  this->n++;
}

void vector_clear(vector_t* this) {
  this->n = 0;
}

void vector_delete(vector_t* this) {
  free(this->buf);
}
