%{
#include <ctype.h>
#include "bow/archer.h"

static long pos = 0;

%}

%option noyywrap
%x BODY BINARY

ANUM		[A-Za-z0-9]+
PUNCT		[^A-Za-z0-9 \t\f\r\n]
WS		[ \t\f\r\n]
CTYPE		Content-Type:{WS}+
NONWS		[^ \t\f\r\n]
PS		%!PS

%%

^{WS}*$		bow_reset_labels(); BEGIN(BODY); pos += flex_mail_leng;
^{NONWS}+/:     bow_reset_labels(); bow_push_label(flex_mail_text); pos += flex_mail_leng;
{ANUM}		pos += flex_mail_leng; return 1;
.               pos += flex_mail_leng;
\n              pos += flex_mail_leng;
<BODY>^{CTYPE}application[/]	BEGIN(BINARY); pos += flex_mail_leng;
<BODY>^{CTYPE}image[/]		BEGIN(BINARY); pos += flex_mail_leng;
<BODY>^{PS}			BEGIN(BINARY); pos += flex_mail_leng;
<BODY>{ANUM}			pos += flex_mail_leng; return 1;
<BODY>.                         pos += flex_mail_leng;
<BODY>\n                        pos += flex_mail_leng;
<BINARY>^{CTYPE}text[/]         BEGIN(BODY); pos += flex_mail_leng;
<BINARY>.                       pos += flex_mail_leng;
<BINARY>\n                      pos += flex_mail_leng;
 
%%

void flex_mail_open(FILE *fp, const char * name)
{
  flex_mail_in = fp;
  pos = 0;
  BEGIN(INITIAL);
}

int flex_mail_get_word_extended(char buf[], int bufsz, long *start, long *end)
{
  int i;

  if (!flex_mail_lex())
    return 0;

  strncpy(buf, flex_mail_text, bufsz);
  buf[bufsz-1] = 0;
  for (i = 0; i < flex_mail_leng; ++i)
    buf[i] = tolower(buf[i]);

  if(start != NULL) {
    *end = pos - 1;
    *start = pos - flex_mail_leng;
  }

  return 1;
}

int flex_mail_get_word(char buf[], int bufsz) {
  return flex_mail_get_word_extended(buf, bufsz, NULL, NULL);
}
