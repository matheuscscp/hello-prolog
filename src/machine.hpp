#ifndef MACHINE_HPP
#define MACHINE_HPP

#include <map>
#include <string>
#include <vector>

struct clause {
  bool on;
  int P;
  std::string src;
};

extern std::map<std::string,std::vector<clause>> symbol_table;
std::string machine_read_functor(std::istream&);
std::string machine_functor_name(const std::string&);
void machine_close();
void machine_run(FILE*);

#endif
