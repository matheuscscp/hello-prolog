#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>

// constructor
#define vector(X) vector_t X = {0,0,NULL}
#define vinit(X)  vector_construct(&(X))
#define VECTOR    {0,0,NULL}

// destructor
#define vdelete(X) vector_delete(&(X))

// push
#define vpush(X,Y,Z) {X _=Z;vector_push(sizeof(X),&(Y),&_);}

// clear
#define vclear(X) vector_clear(&(X))

// access
#define vat(X,Y,Z) (((X*)(Y).buf)[Z])

// sort
#define vsort(X,Y,Z) qsort((Y).buf,(Y).n,sizeof(X),Z)

typedef struct {
  int c,n;
  void* buf;
} vector_t;

void vector_construct(vector_t* this);
void vector_push(int s, vector_t* this, void* data);
void vector_clear(vector_t* this);
void vector_delete(vector_t* this);

#endif
