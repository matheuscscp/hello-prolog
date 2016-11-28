#include <iostream>
#include <sstream>
#include <set>
#include <functional>

#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

extern "C" {
#include "compiler.h"
}
#include "machine.hpp"
#include "helper.hpp"

using namespace std;

static const char* a0;
static vector<string> arg;

static void welcome() {
  printf("===============================\n");
  printf("hello Prolog by Matheus Pimenta\n");
  printf("===============================\n");
  printf("\n");
  printf("Type \"help\" for instructions.\n");
  printf("\n");
}

static void usage() {
  printf("\n");
  printf("Usage: %s [COMMAND LINE OPTION]\n",a0);
  printf("\n");
  printf("Command line options:\n");
  printf("  -h\n");
  printf("     Display usage mode and shell commands.\n");
  printf("  -a <Prolog text between double quotes> (like -a \"?- p(X)\")\n");
  printf("     Compile one line of Prolog text from command line.\n");
  printf("  -c <file paths, wildcard * allowed> (like -c src/*.prolog)\n");
  printf("     Compile Prolog text from a set of files to .wam files.\n");
  printf("  -i <file paths>\n");
  printf("     Interpret Prolog assembly from a set of files.\n");
  printf("  -r <file paths>\n");
  printf("     Compile and interpret Prolog text from a set of files.\n");
  printf("  [file paths]\n");
  printf("     Start shell with an optional set of Prolog text files.\n");
  printf("\n");
  printf("Shell commands:\n");
  printf("  exit - Terminate shell.\n");
  printf("  help - Display usage mode and shell commands.\n");
  printf("  load <file paths> - Run Prolog file(s).\n");
  printf("  more [list of <functor>[/<arity>]] - Display clauses with more.\n");
  printf("  less [list of <functor>[/<arity>]] - Display clauses with less.\n");
  printf("  togl <functor>/<arity> [0-based indexes] - Toggle clause(s).\n");
  printf("  <one line of Prolog text> - Run Prolog.\n");
  printf("\n");
}

// expand file names with ls
static set<string> expand(const string& line) {
  int pd[2];
  pipe(pd);
  int rd=pd[0],wd=pd[1];
  pid_t pid = fork();
  if (!pid) {
    close(rd); // child won't read from pipe
    dup2(wd,1); // child will write to pipe, via stdout (1)
    execl("/bin/sh","/bin/sh","-c",("ls "+line).c_str(),(char*)0);
  }
  close(wd); // parent won't write to pipe
  FILE* fp = fdopen(rd,"r");
  set<string> ans;
  for (string fn; getline(fp,fn);) {
    char* tmp = realpath(fn.c_str(),nullptr);
    ans.insert(tmp);
    free(tmp);
  }
  fclose(fp);
  waitpid(pid,nullptr,0);
  return ans;
}
static set<string> expand_args(int fst) {
  set<string> ans;
  for (int i = fst; i < arg.size(); i++) for (auto& fn : expand(arg[i])) {
    ans.insert(fn);
  }
  return ans;
}

// compile and run Prolog
static void run(const char* mode, const char* in) {
  int pd[2];
  pipe(pd);
  int rd=pd[0],wd=pd[1];
  pid_t pid = fork();
  if (!pid) {
    close(rd); // child won't read from pipe
    dup2(wd,1); // child will write to pipe, via stdout (1)
    execl(a0,a0,mode,in,(char*)0);
  }
  close(wd); // parent won't write to pipe
  FILE* fp = fdopen(rd,"r");
  machine_run(fp);
  fclose(fp);
  waitpid(pid,nullptr,0);
}

// load set of Prolog files
static void load(const set<string>& fns) {
  static set<string> files;
  for (auto& fn : fns) if (!files.count(fn)) {
    run("-c",fn.c_str());
    files.insert(fn);
  }
}

// list all clauses with more and less
inline bool find_func(const set<string>& S, const string& f) {
  return S.count(f) || S.count(machine_functor_name(f));
}
static void more(istream& in) {
  int pd[2];
  pipe(pd);
  int rd=pd[0],wd=pd[1];
  pid_t pid = fork();
  if (!pid) {
    close(wd); // child won't write to pipe
    dup2(rd,0); // child will read from pipe, via stdin (0)
    execl("/bin/more","/bin/more",(char*)0);
  }
  close(rd); // parent won't read from pipe
  set<string> S; for (string s; in >> s; S.insert(s));
  FILE* fp = fdopen(wd,"w");
  for (auto& kv : symbol_table) if (S.size() == 0 || find_func(S,kv.first)) {
    fprintf(fp,"%10lu clause(s) for %s\n",kv.second.size(),kv.first.c_str());
  }
  fclose(fp);
  waitpid(pid,nullptr,0);
}
static void less_(istream& in) {
  int pd[2];
  pipe(pd);
  int rd=pd[0],wd=pd[1];
  pid_t pid = fork();
  if (!pid) {
    close(wd); // child won't write to pipe
    dup2(rd,0); // child will read from pipe, via stdin (0)
    execl("/bin/less","/bin/less",(char*)0);
  }
  close(rd); // parent won't read from pipe
  set<string> S; for (string s; in >> s; S.insert(s));
  FILE* fp = fdopen(wd,"w");
  for (auto& kv : symbol_table) if (S.size() == 0 || find_func(S,kv.first)) {
    fprintf(fp,"%10lu clause(s) for %s:\n",kv.second.size(),kv.first.c_str());
    int i = 0;
    for (auto& cl : kv.second) fprintf(
      fp,
      "%20d %s: %s\n",
      i++,
      cl.on?" (on)":"(off)",
      cl.src.c_str()
    );
    fprintf(fp,"\n");
  }
  fclose(fp);
  waitpid(pid,nullptr,0);
}

// toggle clauses
static void togl(istream& in) {
  string f = machine_read_functor(in);
  if (!symbol_table.count(f)) printf("procedure %s not defined.\n",f.c_str());
  else {
    auto& clauses = symbol_table[f];
    int cnt = 0;
    for (int i; in >> i;) {
      cnt++;
      if (0 <= i && i < clauses.size()) clauses[i].on = !clauses[i].on;
      else printf("procedure %s[%d] not defined.\n",f.c_str(),i);
    }
    if (cnt == 0) for (auto& cl : clauses) cl.on = !cl.on;
  }
}

// my shell
static int shell(const string& line, string& name, bool& quit) {
  stringstream ss(line);
  string cmd;
  ss >> cmd;
  if (cmd == "exit") quit = true;
  else if (cmd == "help") usage();
  else if (cmd == "load") load(expand(line.substr(5,string::npos)));
  else if (cmd == "more") more(ss);
  else if (cmd == "less") less_(ss);
  else if (cmd == "togl") togl(ss);
  else run("-a",line.c_str());
  return 0;
}

// prompt loop for shells
static int prompt(string name, function<int(const string&, string&, bool&)> f) {
  bool quit = false;
  auto rline = [&]() {
    string ans;
    if (quit) return ans;
    char* s = readline((name+"$ ").c_str()); // GNU readline library
    if (!s) {
      quit = true;
      cout << endl;
      return ans;
    }
    ans = s;
    free(s);
    static string last;
    if (ans != "" && ans != last) {
      add_history(ans.c_str()); // GNU history library
      last = ans;
    }
    return ans;
  };
  int status = 0;
  for (string line = rline(); !quit && !status; line = rline()) {
    if (line != "") status = f(line,name,quit);
  }
  return status;
}

int main(int argc, char** argv) {
  // init args
  a0 = argv[0];
  for (int i = 0; i < argc; i++) arg.push_back(argv[i]);
  string arg1; if (argc > 1) arg1 = argv[1];
  // help
  if (argc > 1 && arg1 == "-h") { usage(); return 0; }
  // compile command line Prolog text
  if (argc > 2 && arg1 == "-a") return compile(argv[2]);
  // compile set of files
  if (argc > 2 && arg1 == "-c") {
    auto fns = expand_args(2);
    if (fns.size() == 1) return compile_file(fns.begin()->c_str());
    int st = 0;
    for (auto& fn : fns) {
      pid_t pid = fork();
      if (!pid) {
        freopen((fn+".wam").c_str(),"w",stdout);
        execl(a0,a0,"-c",fn.c_str(),(char*)0);
      }
      int s;
      waitpid(pid,&s,0);
      s = WEXITSTATUS(s);
      if (s == 1) remove((fn+".wam").c_str());
      st += s;
    }
    return st;
  }
  // interpret set of files
  if (argc > 2 && arg1 == "-i") {
    for (auto& fn : expand_args(2)) {
      FILE* fp = fopen(fn.c_str(),"r");
      machine_run(fp);
      fclose(fp);
    }
    machine_close();
    return 0;
  }
  // compile and interpret set of files
  if (argc > 2 && arg1 == "-r") {
    load(expand_args(2));
    machine_close();
    return 0;
  }
  // shell
  welcome();
  load(expand_args(1));
  int status = prompt(a0,shell);
  machine_close();
  return status;
}
