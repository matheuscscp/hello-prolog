
%{

#include <ctype.h>

#include "lexical.h"
#include "parser.h"
#include "parser.tab.h"

// local functions
static int token(int tok, int data);
static void inc();
static void newline();
static void begin_mlcomment();
static void end_mlcomment();
static void eof_mlcomment();
static void wildcard();

%}

lowercase_letter  [a-z]
uppercase_letter  [A-Z_]
digit             [0-9]
special           [+\-*\/\\^~:.? #$&]
alphanum          {lowercase_letter}|{uppercase_letter}|{digit}
character         {alphanum}|{special}

smallatom {lowercase_letter}{alphanum}*
variable  {uppercase_letter}{alphanum}*
numeral   {digit}+
string    '{character}+'
punct     [.,\(\)!]|[:?][\-]
oper      [\+\-*\/]

whitespace  [ \t]+
comment     [%][^\n]*[\n]

%x MLCOMMENT

%%

{smallatom}     return token(SMALLATOM,1);
{variable}      return token(VARIABLE,1);
{numeral}       return token(NUMERAL,1);
{string}        return token(STRING,1);
{punct}|{oper}  return token(yytext[0],0);

{whitespace}            inc();
<INITIAL,MLCOMMENT>"\n" newline();

{comment}           newline();
"/*"                begin_mlcomment();
<MLCOMMENT>"*/"     end_mlcomment();
<MLCOMMENT><<EOF>>  { eof_mlcomment(); yyterminate(); }
<MLCOMMENT>.        inc();

. wildcard();

%%

static int token(int tok, int data) {
  yylval.tok = lexical_create_token(parser_ln,parser_cl);
  inc();
  if (tok == VARIABLE && !strcmp(yytext,"_")) tok = '_', data = 0;
  if (!data) return tok;
  if (tok == NUMERAL) {
    int tmp;
    sscanf(yytext,"%d",&tmp);
    char buf[16];
    sprintf(buf,"%d",tmp);
    if (strcmp(yytext,buf)) {
      parser_error_flag = 1;
      fprintf(
        stderr,
        "%s:%d:%d: lexical error, NUMERAL cannot be parsed to 32-bit integer\n",
        parser_fn,
        yylval.tok.ln,
        yylval.tok.cl
      );
    }
  }
  yylval.tok.data = strdup(yytext);
  return tok;
}

static void inc() {
  parser_cl += yyleng;
}

static void newline() {
  parser_ln++, parser_cl = 1;
}

static void begin_mlcomment() {
  parser_cln = parser_ln, parser_ccl = parser_cl;
  inc();
  BEGIN(MLCOMMENT);
}

static void end_mlcomment() {
  inc();
  BEGIN(INITIAL);
}

static void eof_mlcomment() {
  parser_error_flag = 1;
  fprintf(
    stderr,
    "%s:%d:%d: lexical error, multiline comment started but not ended\n",
    parser_fn,
    parser_cln,
    parser_ccl
  );
}

static void wildcard() {
  parser_error_flag = 1;
  fprintf(
    stderr,
    "%s:%d:%d: lexical error, invalid token starting with ",
    parser_fn,
    parser_ln,
    parser_cl
  );
  if (isprint(yytext[0])) fprintf(stderr,"'%c'",yytext[0]);
  else fprintf(stderr,"0x%02x",(int)yytext[0]);
  fprintf(stderr,"\n");
  inc();
}
