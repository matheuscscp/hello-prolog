#include <stdlib.h>

#include "lexical.h"

token_t lexical_create_token(int ln, int cl) {
  token_t ans;
  ans.ln = ln;
  ans.cl = cl;
  ans.data = NULL;
  return ans;
}

void lexical_delete_token(token_t tok) {
  free(tok.data);
}
