#include "helper.hpp"

using namespace std;

bool getline(FILE* fp, string& buf) {
  buf = "";
  if (feof(fp)) return false;
  for (char c = fgetc(fp); !feof(fp) && c != EOF && c != '\n'; c = fgetc(fp)) {
    buf += c;
  }
  if (feof(fp) && buf == "") return false;
  return true;
}
