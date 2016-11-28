#include <stdio.h>

#include "code.h"

#include "parser.h"
#include "syntax.h"
#include "symbol.h"

#define get_node(X)   (&vat(node,parser_st,(X)))
#define child_id(X,Y) (vat(int,(X)->v,(Y)))
#define child(X,Y)    (&vat(node,parser_st,child_id(X,Y)))

#define mark(X)   X |= (1<<31)
#define seen(X)   ((X)&(1<<31))
#define reg(X)    ((X)&~(1<<31))

// symbol tables
static int nxtprm,nxtreg;
static symbol_table_t prmvar,tmpvar;

// permanent variables
static void permanent_variables_dfs(node* u) {
  if (u->type == N_VARIABLE) symget(prmvar,u->tok.data);
  else for (int i = 0; i < u->v.n; i++) {
    permanent_variables_dfs(child(u,i));
  }
}
static void save_permanent() {
  for (int i = 0; i < prmvar.table.n; i++) {
    symbol_t* ps = symat(prmvar,i);
    symbol_t* ts = symfind(tmpvar,ps->sym);
    if (ts && ts->val) {
      mark(ts->val);
      ps->val = ts->val;
    }
  }
}
static void load_permanent() {
  for (int i = 0; i < prmvar.table.n; i++) {
    symbol_t* ps = symat(prmvar,i);
    if (ps->val) symget(tmpvar,ps->sym)->val = ps->val;
  }
}

// query code
static void goal_allocate(node* u, int ispred) {
  if (ispred) nxtreg = u->v.n+1;
  for (int i = 0; i < u->v.n; i++) {
    node* v = child(u,i);
    // non-variable roots
    if (ispred && v->type != N_VARIABLE && v->type != N_DONTCARE) {
      v->val = i+1;
      continue;
    }
    // non-variable non-roots or don't cares
    if (v->type != N_VARIABLE) { // !ispred || v->type == N_DONTCARE
      v->val = nxtreg++;
      continue;
    }
    // variables
    symbol_t* s = symget(tmpvar,v->tok.data);
    if (!s->val) s->val = symfind(prmvar,v->tok.data) ? nxtprm++ : nxtreg++;
  }
}
static void goal_dfs(node* u) {
  // code from children goes before
  for (int i = 0; i < u->v.n; i++) goal_dfs(child(u,i));
  // generate code
  if (u->type == N_STRUCTURE) {
    char* func = child(u,0)->tok.data;
    node* trms = child(u,1);
    printf("  put_structure %s/%d, X%d\n",func,trms->v.n,u->val);
    for (int i = 0; i < trms->v.n; i++) {
      node* v = child(trms,i);
      if (v->type == N_DONTCARE) printf("  set_variable X%d\n",v->val);
      else if (v->type == N_VARIABLE) {
        char c = 'X';
        if (symfind(prmvar,v->tok.data)) c = 'Y';
        int* val = &symget(tmpvar,v->tok.data)->val;
        if (seen(*val)) printf("  set_value %c%d\n",c,reg(*val));
        else printf("  set_variable %c%d\n",c,reg(*val));
        mark(*val);
      }
      else printf("  set_value X%d\n",v->val);
    }
  }
}
static void goal_roots(node* u) {
  char* func = child(u,0)->tok.data;
  node* trms = child(u,1);
  // for each root
  for (int i = 0; i < trms->v.n; i++) {
    node* v = child(trms,i);
    if (v->type == N_DONTCARE) printf("  put_variable X%d, X%d\n",v->val,i+1);
    else if (v->type == N_VARIABLE) {
      char c = 'X';
      if (symfind(prmvar,v->tok.data)) c = 'Y';
      int* val = &symget(tmpvar,v->tok.data)->val;
      if (seen(*val)) printf("  put_value %c%d, X%d\n",c,reg(*val),i+1);
      else printf("  put_variable %c%d, X%d\n",c,reg(*val),i+1);
      mark(*val);
    }
    else goal_dfs(v);
  }
  printf("  call %s/%d\n",func,trms->v.n);
}
static void goals(node* u) {
  // for each goal
  for (int i = 0; i < u->v.n; i++) {
    syminit(tmpvar);
    load_permanent();
    int g = child_id(u,i);
    // BFS for register allocation
    vector(Q); // queue
    vpush(int,Q,g);
    for (int front = 0; front < Q.n; front++) {
      node* v = get_node(vat(int,Q,front));
      if (v->type == N_PREDICATE || v->type == N_STRUCTURE) {
        goal_allocate(child(v,1),v->type == N_PREDICATE);
      }
      for (int i = 0; i < v->v.n; i++) vpush(int,Q,child_id(v,i));
    }
    vdelete(Q);
    goal_roots(get_node(g));
    save_permanent();
    symdel(tmpvar);
  }
}
static void query() {
  node* u = child(get_node(0),1);
  if (u->v.n) {
    syminit(prmvar);
    permanent_variables_dfs(u);
    nxtprm = 1;
    printf("query:\n");
    printf("  allocate %d\n",prmvar.table.n);
    goals(u);
    for (int i = 0; i < prmvar.table.n; i++) {
      symbol_t* s = symat(prmvar,i);
      printf("  print_variable Y%d, %s\n",reg(s->val),s->sym);
    }
    printf("  flush_variables\n");
    printf("  wait_user\n");
    symdel(prmvar);
  }
}

// program code
static void head_code(node* u) {
  if (u->type == N_DONTCARE) { // one of the roots
    printf("  get_variable X%d, X%d\n",nxtreg++,u->val);
  }
  else if (u->type == N_VARIABLE) { // one of the roots
    char c = 'X';
    int* nxt = &nxtreg;
    if (symfind(prmvar,u->tok.data)) c = 'Y', nxt = &nxtprm;
    symbol_t* s = symget(tmpvar,u->tok.data);
    if (s->val) printf("  get_value %c%d, X%d\n",c,s->val,u->val);
    else {
      s->val = (*nxt)++;
      printf("  get_variable %c%d, X%d\n",c,s->val,u->val);
    }
  }
  else { // N_STRUCTURE
    char* func = child(u,0)->tok.data;
    node* trms = child(u,1);
    printf("  get_structure %s/%d, X%d\n",func,trms->v.n,u->val);
    for (int i = 0; i < trms->v.n; i++) {
      node* v = child(trms,i);
      if (v->type != N_VARIABLE) {
        v->val = nxtreg++;
        printf("  unify_variable X%d\n",v->val);
      }
      else {
        char c = 'X';
        int* nxt = &nxtreg;
        if (symfind(prmvar,v->tok.data)) c = 'Y', nxt = &nxtprm;
        symbol_t* s = symget(tmpvar,v->tok.data);
        if (s->val) printf("  unify_value %c%d\n",c,s->val);
        else {
          s->val = (*nxt)++;
          printf("  unify_variable %c%d\n",c,s->val);
        }
      }
    }
  }
}
static void head(node* u) {
  nxtreg = u->v.n+1;
  syminit(tmpvar);
  // BFS
  vector(Q); // queue
  // handle roots separately and push roots' children
  for (int i = 0; i < u->v.n; i++) {
    node* v = child(u,i);
    v->val = i+1;
    head_code(v);
    for (int j = 0; j < v->v.n; j++) vpush(int,Q,child_id(v,j));
  }
  // BFS loop
  for (int front = 0; front < Q.n; front++) {
    node* v = get_node(vat(int,Q,front));
    if (v->type == N_STRUCTURE) head_code(v);
    for (int i = 0; i < v->v.n; i++) vpush(int,Q,child_id(v,i));
  }
  vdelete(Q);
  save_permanent();
  symdel(tmpvar);
}
static void print_dfs(node* u) {
  if (u->type == N_DONTCARE) printf("_");
  else if (u->type == N_ATOM || u->type == N_VARIABLE) printf("%s",u->tok.data);
  else if (
    u->type == N_PREDICATE ||
    u->type == N_FACT ||
    u->type == N_STRUCTURE
  ) {
    print_dfs(child(u,0));
    node* trms = child(u,1);
    if (!trms->v.n) return;
    printf("(");
    for (int i = 0; i < trms->v.n; i++) {
      if (i > 0) printf(",");
      print_dfs(child(trms,i));
    }
    printf(")");
  }
}
static void fact(node* u) {
  char* func = child(u,0)->tok.data;
  node* trms = child(u,1);
  printf("%s/%d: ",func,trms->v.n);
  print_dfs(u);
  printf(".\n");
  head(trms);
  printf("  proceed\n");
}
static void rule(node* u) {
  node* hd = child(u,0);
  node* bd = child(u,1);
  char* func = child(hd,0)->tok.data;
  node* trms = child(hd,1);
  permanent_variables_dfs(hd);
  permanent_variables_dfs(bd);
  nxtprm = 1;
  printf("%s/%d: ",func,trms->v.n);
  print_dfs(hd);
  printf(" :- ");
  for (int i = 0; i < bd->v.n; i++) {
    if (i > 0) printf(", ");
    print_dfs(child(bd,i));
  }
  printf(".\n");
  printf("  allocate %d\n",prmvar.table.n);
  head(trms);
  goals(bd);
  printf("  deallocate\n");
}
static void program() {
  // for each clause
  node* cls = child(get_node(0),0);
  for (int i = 0; i < cls->v.n; i++) {
    syminit(prmvar);
    node* u = child(cls,i);
    if(u->type == N_FACT) fact(u);
    else rule(u);
    symdel(prmvar);
  }
}

void code() {
  program();
  query();
}
