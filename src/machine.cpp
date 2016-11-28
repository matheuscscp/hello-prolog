#include <iostream>
#include <sstream>
#include <functional>

#include "machine.hpp"

#include "helper.hpp"

using namespace std;

// =============================================================================
// helper templates
// =============================================================================

#ifndef RELEASE
#define dbg(X)  cout << ">>> (" << #X << ") = (" << X << ")\n"
#define _       << " _ " <<
#endif

enum fatal_error {
  INVALID_REGISTER = 1,
  EMPTY_INSTRUCTION
};
#define fatal(ERR,fmt,...) {\
  fprintf(stderr,"fatal error (%s): ",#ERR);\
  fprintf(stderr,fmt,##__VA_ARGS__);\
  fprintf(stderr,"\n");\
  exit(ERR);\
}

struct cvt {
  stringstream ss;
  template <typename T>
  cvt(T x) {
    ss << x;
  }
  template <typename T>
  operator T() {
    T ans;
    ss >> ans;
    return ans;
  }
  string str() {
    string ans;
    ss >> ans;
    return ans;
  }
};

// =============================================================================
// types and globals
// =============================================================================

#define MAXN  (1<<20)
#define X0    (0)
#define H0    (1<<18)
#define YA0   (3<<18)

// code
struct instruction {
  virtual ~instruction() {}
  virtual void run() const = 0;
};
static instruction* CODE[MAXN] = {};
static int CODE_SIZE = 0;
static int P, CP; // instruction pointers
static void free_code(int beg = 0) {
  while (beg < CODE_SIZE) {
    CODE_SIZE--;
    delete CODE[CODE_SIZE];
    CODE[CODE_SIZE] = nullptr;
  }
}
static void free_query() {
  free_code(symbol_table["query"][0].P);
  symbol_table.erase("query");
}

// data types
enum tag {
  NAT, // Not A Tag
  REF,
  STR,
  FCT
};
typedef pair<tag,int>    data;
typedef pair<string,int> functor;
static string func2lab(const functor& f) {
  return f.first+"/"+cvt(f.second).str();
}
static functor lab2func(const string& lab) {
  for (int i = lab.size()-1; 0 <= i; i--) if (lab[i] == '/') {
    return functor(lab.substr(0,i),int(cvt(&lab[i+1])));
  }
  return functor("",0);
}
static string read_functor(istream& in) {
  string s;
  char c = in.get();
  while (in && c == ' ') c = in.get();
  s += c;
  if (c == '\'') {
    for (c = in.get(); in && c != '\''; c = in.get()) s += c;
    s += '\'';
  }
  string tmp;
  in >> tmp;
  s += tmp;
  return func2lab(lab2func(s)); // remove trailing stuff
}
struct cell {
  data d;
  functor f;
  cell() : d(NAT,0), f("",0) {}
  cell& operator=(const data& o) { d = o; f = functor("",0); }
  bool operator==(const data& o) { return d == o; }
  bool operator!=(const data& o) { return d != o; }
  cell& operator=(const functor& o) { d = data(FCT,0); f = o; }
  bool operator==(const functor& o) { return f == o; }
  bool operator!=(const functor& o) { return f != o; }
  operator data&() { return d; }
  operator functor&() { return f; }
};

// memory
static cell STORE[MAXN];

// heap
struct {
  cell& operator[](int a) { return STORE[a]; }
} static HEAP;
static int H, S, HB; // pointers

// stack
struct {
  int CE,CP,n;
  int YA; // custom: offset for local vars/args
  int nxtYA() const { // compute Y offset for env/choice on top of this one
    return YA+n;
  }
  
  // for choice points only
  int B,BP,TR,H;
  string L; // custom: address label
  int NC;   // custom: index of next clause. symbol_table[L][NC].P == BP
} static STACK[MAXN];
static int E, B; // pointers

// registers
typedef pair<char,int> reg;
static reg read_register(istream& in) {
  string s;
  in >> s;
  return reg(s[0],int(cvt(&s[1])));
}
struct {
  cell& operator[](const reg& i) { return STORE[addr(i)]; }
  int addr(const reg& i) {
    switch (i.first) {
      case 'X': return X0+i.second-1;           // global registers
      case 'Y': return STACK[E].YA+i.second-1;  // stack local variables
      case 'A': return STACK[B].YA+i.second-1;  // stack arguments
    }
    fatal(INVALID_REGISTER,"%c%d",i.first,i.second);
  }
} static X;

// trail
static int TRAIL[MAXN], TR;

// mode
enum wam_mode {
  READ,
  WRITE
};
static wam_mode mode;

// termination flags
static bool halt, fail;

// query variables
static vector<pair<string,reg>> query_vars;
static map<int,string> query_unbound_vars;
static void clear_query() { query_vars.clear(); query_unbound_vars.clear(); }

// =============================================================================
// machine functions
// =============================================================================

// for injection of choice instructions
static void call_try_me_else(istream&);
static void call_retry_me_else(istream&);
static void call_trust_me(istream&);

static int deref(int a) {
  data tmp = STORE[a];
  while (tmp.first == REF && tmp.second != a) {
    a = tmp.second;
    tmp = STORE[a];
  }
  return a;
}

static void trail(int a) {
  if (a < HB || (H < a && a < B)) {
    TRAIL[TR] = a;
    TR = TR+1;
  }
}

static void unwind_trail(int a1, int a2) {
  for (int i = a1; i <= a2-1; i++) STORE[TRAIL[i]] = data(REF,TRAIL[i]);
}

static void bind(int a1, int a2) {
  tag t1 = STORE[a1].d.first, t2 = STORE[a2].d.first;
  if (t1 == REF && (t2 != REF || a2 < a1)) STORE[a1] = STORE[a2], trail(a1);
  else STORE[a2] = STORE[a1], trail(a2);
}

static void unify(int a1, int a2) {
  static int PDL[MAXN];
  int size = 0;
  auto push = [&](int x) { PDL[size++] = x; };
  auto pop = [&]() { return PDL[--size]; };
  auto empty = [&]() { return size == 0; };
  push(a1); push(a2);
  while (!empty()) {
    int d1 = deref(pop()); int d2 = deref(pop());
    if (d1 == d2) continue;
    tag t1 = STORE[d1].d.first; int v1 = STORE[d1].d.second;
    tag t2 = STORE[d2].d.first; int v2 = STORE[d2].d.second;
    if (t1 == REF || t2 == REF) { bind(d1,d2); continue; }
    const functor& f1 = STORE[v1]; const functor& f2 = STORE[v2];
    if (f1 != f2) { fail = true; break; }
    for (int i = 1; i <= f1.second; i++) { push(v1+i); push(v2+i); }
  }
}

static int next_clause(const string& label, int first = 0) {
  if (!symbol_table.count(label)) return -1;
  const auto& clauses = symbol_table[label];
  while (first < clauses.size() && !clauses[first].on) first++;
  if (first == clauses.size()) return -1;
  return first;
}

static void backtrack() {
  P = STACK[B].BP;
  // retry_me_else and trust_me injection for multi-clause definitions
  P = P-1;
  int nxt = next_clause(STACK[B].L,STACK[B].NC+1);
  stringstream ss;
  ss << nxt;
  if (nxt >= 0) call_retry_me_else(ss);
  else call_trust_me(ss); // nxt is not used here!
}

// =============================================================================
// L0 query instructions
// =============================================================================

struct put_structure : instruction {
  functor f_n;
  reg i;
  put_structure(istream& in) {
    f_n = lab2func(read_functor(in));
    i = read_register(in);
  }
  void run() const {
    HEAP[H] = data(STR,H+1);
    HEAP[H+1] = f_n;
    X[i] = HEAP[H];
    H = H+2;
    P = P+1;
  }
};

struct set_variable : instruction {
  reg i;
  set_variable(istream& in) {
    i = read_register(in);
  }
  void run() const {
    HEAP[H] = data(REF,H);
    X[i] = HEAP[H];
    H = H+1;
    P = P+1;
  }
};

struct set_value : instruction {
  reg i;
  set_value(istream& in) {
    i = read_register(in);
  }
  void run() const {
    HEAP[H] = X[i];
    H = H+1;
    P = P+1;
  }
};

// =============================================================================
// L0 program instructions
// =============================================================================

struct get_structure : instruction {
  functor f_n;
  reg i;
  get_structure(istream& in) {
    f_n = lab2func(read_functor(in));
    i = read_register(in);
  }
  void run() const {
    int addr = deref(X.addr(i));
    const data& tmp = STORE[addr];
    switch (tmp.first) {
      case REF: {
        HEAP[H] = data(STR,H+1);
        HEAP[H+1] = f_n;
        bind(addr,H);
        H = H+2;
        mode = WRITE;
        break;
      }
      case STR: {
        int a = tmp.second;
        if (HEAP[a] == f_n) {
          S = a+1;
          mode = READ;
        }
        else fail = true;
        break;
      }
      default: fail = true;
    }
    P = P+1;
  }
};

struct unify_variable : instruction {
  reg i;
  unify_variable(istream& in) {
    i = read_register(in);
  }
  void run() const {
    if (mode == READ) X[i] = HEAP[S];
    else {
      HEAP[H] = data(REF,H);
      X[i] = HEAP[H];
      H = H+1;
    }
    S = S+1;
    P = P+1;
  }
};

struct unify_value : instruction {
  reg i;
  unify_value(istream& in) {
    i = read_register(in);
  }
  void run() const {
    if (mode == READ) unify(X.addr(i),S);
    else {
      HEAP[H] = X[i];
      H = H+1;
    }
    S = S+1;
    P = P+1;
  }
};

// =============================================================================
// L1 control instructions
// =============================================================================

struct call : instruction {
  string L;
  call(istream& in) {
    L = read_functor(in);
  }
  void run() const {
    int fst = next_clause(L,0);
    if (fst < 0) {
      fail = true;
      printf("procedure %s not defined. backtracking.\n",L.c_str());
    }
    else {
      CP = P+1;
      P = symbol_table[L][fst].P;
      int nxt = next_clause(L,fst+1);
      // try_me_else injection for multi-clause definitions
      if (nxt > fst) {
        P = P-1;
        stringstream ss;
        ss << L << " " << nxt;
        call_try_me_else(ss);
      }
    }
  }
};

struct proceed : instruction {
  proceed(istream&) {}
  void run() const {
    P = CP;
  }
};

// =============================================================================
// L1 query instructions
// =============================================================================

struct put_variable : instruction {
  reg n,i;
  put_variable(istream& in) {
    n = read_register(in);
    i = read_register(in);
  }
  void run() const {
    HEAP[H] = data(REF,H);
    X[n] = HEAP[H];
    X[i] = HEAP[H];
    H = H+1;
    P = P+1;
  }
};

struct put_value : instruction {
  reg n,i;
  put_value(istream& in) {
    n = read_register(in);
    i = read_register(in);
  }
  void run() const {
    X[i] = X[n];
    P = P+1;
  }
};

// =============================================================================
// L1 program instructions
// =============================================================================

struct get_variable : instruction {
  reg n,i;
  get_variable(istream& in) {
    n = read_register(in);
    i = read_register(in);
  }
  void run() const {
    X[n] = X[i];
    P = P+1;
  }
};

struct get_value : instruction {
  reg n,i;
  get_value(istream& in) {
    n = read_register(in);
    i = read_register(in);
  }
  void run() const {
    unify(X.addr(n),X.addr(i));
    P = P+1;
  }
};

// =============================================================================
// L2 control instructions
// =============================================================================

struct allocate : instruction {
  int N;
  allocate(istream& in) {
    in >> N;
  }
  void run() const {
    int EB = max(E,B);
    int newE = EB+1;
    STACK[newE].CE = E;
    STACK[newE].CP = CP;
    STACK[newE].n = N;
      STACK[newE].YA = (EB == -1 ? YA0 : STACK[EB].nxtYA()); // AFTER field n
    E = newE; // "the" push
    P = P+1;
  }
};

struct deallocate : instruction {
  deallocate(istream&) {}
  void run() const {
    P = STACK[E].CP;
    E = STACK[E].CE; // "the" pop
  }
};

// =============================================================================
// L3 choice instructions
// =============================================================================

struct try_me_else : instruction {
  string L;
  int NC;
  try_me_else(istream& in) {
    L = read_functor(in);
    in >> NC;
  }
  void run() const {
    int EB = max(E,B);
    int newB = EB+1;
    STACK[newB].n = lab2func(L).second;
      STACK[newB].YA = (EB == -1 ? YA0 : STACK[EB].nxtYA()); // AFTER field n
    STACK[newB].CE = E;
    STACK[newB].CP = CP;
    STACK[newB].B = B;
      STACK[newB].L = L;
      STACK[newB].NC = NC;
    STACK[newB].BP = symbol_table[ STACK[newB].L ][ STACK[newB].NC ].P;
    STACK[newB].TR = TR;
    STACK[newB].H = H;
    int n = STACK[newB].n;
    B = newB; // "the" push
    for (int i = 1; i <= n; i++) X[reg('A',i)] = X[reg('X',i)]; // AFTER B=newB
    HB = H;
    P = P+1;
  }
};
static void call_try_me_else(istream& in) { try_me_else(in).run(); }

struct retry_me_else : instruction {
  int NC;
  retry_me_else(istream& in) {
    in >> NC;
  }
  void run() const {
    int n = STACK[B].n;
    for (int i = 1; i <= n; i++) X[reg('X',i)] = X[reg('A',i)];
    E = STACK[B].CE;
    CP = STACK[B].CP;
      STACK[B].NC = NC;
    STACK[B].BP = symbol_table[ STACK[B].L ][ STACK[B].NC ].P;
    unwind_trail(STACK[B].TR,TR);
    TR = STACK[B].TR;
    H = STACK[B].H;
    HB = H;
    P = P+1;
  }
};
static void call_retry_me_else(istream& in) { retry_me_else(in).run(); }

struct trust_me : instruction {
  trust_me(istream&) {}
  void run() const {
    int n = STACK[B].n;
    for (int i = 1; i <= n; i++) X[reg('X',i)] = X[reg('A',i)];
    E = STACK[B].CE;
    CP = STACK[B].CP;
    unwind_trail(STACK[B].TR,TR);
    TR = STACK[B].TR;
    H = STACK[B].H;
    B = STACK[B].B; // "the" pop
    if (B != -1) HB = STACK[B].H;
    P = P+1;
  }
};
static void call_trust_me(istream& in) { trust_me(in).run(); }

// =============================================================================
// custom instructions
// =============================================================================

struct print_variable : instruction {
  reg i;
  string var;
  print_variable(istream& in) {
    i = read_register(in);
    in >> var;
  }
  void run() const {
    query_vars.emplace_back(var,i);
    int a = deref(X.addr(i));
    if (STORE[a] == data(REF,a) && !query_unbound_vars.count(a)) {
      query_unbound_vars[a] = var;
    }
    P = P+1;
  }
};

struct flush_variables : instruction {
  flush_variables(istream& in) {}
  void run() const {
    printf("\n");
    if (query_vars.size() == 0) printf("true.\n");
    else for (const auto& var : query_vars) {
      printf("%s = ",var.first.c_str());
      int a = deref(X.addr(var.second));
      if (STORE[a] != data(REF,a)) dfs(a);
      else {
        const auto& s = query_unbound_vars[a];
        printf("%s",var.first == s ? "<unbound>" : s.c_str());
      }
      printf("\n");
    }
    printf("\n");
    clear_query();
    P = P+1;
  }
  void dfs(int a) const {
    const auto& c = STORE[a = deref(a)].d;
    if (c.first == REF) {
      if (!query_unbound_vars.count(a)) printf("<unbound>");
      else printf("%s",query_unbound_vars[a].c_str());
    }
    else {
      const auto& f = STORE[c.second].f;
      printf("%s",f.first.c_str());
      if (f.second) {
        printf("(");
        for (int i = 1; i <= f.second; i++) {
          if (i > 1) printf(",");
          dfs(c.second+i);
        }
        printf(")");
      }
    }
  }
};

struct wait_user : instruction {
  wait_user(istream&) {}
  void run() const {
    if (B == -1) halt = true; // no more choices
    else {
      printf("Backtrack? (y/n) ");
      char c;
      system("/bin/stty raw");
      c = getchar();
      system("/bin/stty cooked");
      printf("\n");
      if (c == 'y' || c == 'Y') fail = true; // next choice, backtrack
      else halt = true;
    }
  }
};

// =============================================================================
// assembler
// =============================================================================

#define ASSEMBLER(X) {#X,[](istream& in) { return new X(in); }}
static map<string,function<instruction*(istream&)>> assembler{
  ASSEMBLER(put_structure),
  ASSEMBLER(set_variable),
  ASSEMBLER(set_value),
  ASSEMBLER(get_structure),
  ASSEMBLER(unify_variable),
  ASSEMBLER(unify_value),
  ASSEMBLER(call),
  ASSEMBLER(proceed),
  ASSEMBLER(put_variable),
  ASSEMBLER(put_value),
  ASSEMBLER(get_variable),
  ASSEMBLER(get_value),
  ASSEMBLER(allocate),
  ASSEMBLER(deallocate),
  ASSEMBLER(try_me_else),
  ASSEMBLER(retry_me_else),
  ASSEMBLER(trust_me),
  ASSEMBLER(print_variable),
  ASSEMBLER(flush_variables),
  ASSEMBLER(wait_user)
};
#undef ASSEMBLER

// =============================================================================
// API
// =============================================================================

map<string,vector<clause>> symbol_table;

string machine_read_functor(istream& in) { return read_functor(in); }

string machine_functor_name(const string& f) { return lab2func(f).first; }

void machine_close() { free_code(); }

static void push_label(const string& label, istream& in) {
  in.get();
  symbol_table[label].push_back({
    true,
    CODE_SIZE,
    string(istreambuf_iterator<char>(in),{})
  });
}
void machine_run(FILE* fp) {
  // assemble
  for (string s; getline(fp,s);) {
    stringstream ss(s);
    if (s[0] == '\'') { // string label
      string label = read_functor(ss);
      push_label(label,ss);
    }
    else {
      ss >> s;
      if (s[s.size()-1] == ':') { // regular label
        s.pop_back();
        push_label(s,ss);
      }
      else if (assembler.count(s)) CODE[CODE_SIZE++] = assembler[s](ss);
      else fprintf(stderr,"error (INVALID_INSTRUCTION): %s\n",s.c_str());
    }
  }
  // run query
  if (symbol_table.count("query")) {
    P = symbol_table["query"][0].P;
    H = H0;
    HB = H0;
    E = -1;
    B = -1;
    TR = 0;
    halt = false;
    fail = false;
    clear_query();
    while (!halt && !fail && CODE[P]) {
      CODE[P]->run();
      if (fail && B != -1) fail = false, backtrack();
    }
    if (!halt && !fail && !CODE[P]) fatal(EMPTY_INSTRUCTION,"at %d",P);
    if (fail) printf("false.\n");
    free_query();
  }
}
