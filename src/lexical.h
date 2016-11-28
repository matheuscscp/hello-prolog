#ifndef LEXICAL_H
#define LEXICAL_H

typedef struct {
  int ln,cl;
  char* data;
} token_t;

token_t lexical_create_token(int ln, int cl);
void lexical_delete_token(token_t tok);

#endif
