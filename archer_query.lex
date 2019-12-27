%{
#include <string.h>
#include <bow/archer_query.h>
#include <archer_query_y.h>
%}

%option noyywrap

WORD            [_[:alnum:]]+
NUMBER          [[:digit:]]+
WS              [[:space:]\n]+
WEIGHT          [[:digit:]]*"."[[:digit:]]+
 
%%

{NUMBER}        archer_query_lval.string = strdup(archer_query_text); return NUMBER;
{WORD}          archer_query_lval.string = strdup(archer_query_text); return WORD;
{WS}
":"             return ':';
"["             return '[';
"]"             return ']';
"+"             return '+';
"-"             return '-';
"\""            return '"';
","             return ',';
"~"             return '~';
">"             return '>';
"<"             return '<';
"("             return '(';
")"             return ')';
.               {
                  char buf[256];

		  sprintf(buf, "Invalid character: %s\n", yytext);
		  archer_query_error(buf);
                }
%%

void archer_query_setup(const char* string) {
  static YY_BUFFER_STATE buffer = NULL;

  if(buffer) yy_delete_buffer(buffer);
  buffer = yy_scan_string(string);
}

