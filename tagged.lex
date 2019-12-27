%{

#include <string.h>
#include <ctype.h>
#include "bow/archer.h"

#define FILENAME_LEN 256

static long pos = 0;
static long line = 0;
static char filename[FILENAME_LEN]; 

void pop_label();
void push_label();
void strip_entity(char entity[]);
int parse_entity(void);
%}

%option noyywrap
%s PARSE_TAGS

NAME            [[:alpha:]][_[:alnum:]]+
ENTITY          "&"(lt|gt|amp|quot|apos)";"

%%

<PARSE_TAGS>{ENTITY}        pos++;
              /* <PARSE_TAGS>"&"{NAME}";"    if (!parse_entity()) return 1; */
<PARSE_TAGS>"<"{NAME}">"    push_label(); return 2;
<PARSE_TAGS>"</"{NAME}">"   pop_label(); return 3;
[[:alnum:]]+                pos += yyleng; return 1;
\n                          pos++; line++;
.                           pos++;

%%

int is_entity (char entity[]) 
{
  int ret = 0;
  if (!strcmp(entity, "&lt;") ||
      !strcmp(entity, "&gt;") ||
      !strcmp(entity, "&amp;") ||
      !strcmp(entity, "&quot;") ||
      !strcmp(entity, "&apos;"))
    ret = 1;
  
  return ret;
}

void strip_entity (char entity[])
{
  memmove (entity, &entity[1], strlen (entity));
  entity[strlen(entity) - 1] = '\0';
}

int parse_entity(void)
{
  if (is_entity (yytext)) 
  {
    pos += 1; 
    return 1;
  }
  else 
  { 
    strip_entity (yytext); 
    pos += yyleng; 
    return 0;
  }
}

void strip_label (char label[])
{
  memmove (label, &label[(label[1] == '/' ? 2 : 1)], strlen (label));
  label[strlen (label) - 1] = '\0';
}

void push_label ()
{
  strip_label (yytext);
  bow_push_label (yytext);
}

void pop_label ()
{
  char buf[100];

  bow_pop_label (buf, 100);
  strip_label (yytext);
  if(strcmp (buf, yytext)) printf("ERROR: at line %ld, expected </%s>, instead got </%s>. This is invalid XML.\n", line, yytext, buf);
}

void tagged_lex_open_dont_parse_tags (FILE * fp, const char * name)
{
  yyin = fp;
  pos = 0;
  line = 0;
  strncpy (filename, name, FILENAME_LEN);
  BEGIN (INITIAL);
}


void tagged_lex_open (FILE * fp, const char * name)
{
  yyin = fp;
  pos = 0;
  line = 0;
  strncpy (filename, name, FILENAME_LEN);
  BEGIN (PARSE_TAGS);
}

int tagged_lex_get_word_extended(char buf[], int bufsz, long *start, long *end)
{ 
  int ret;

  ret = yylex();
  if (ret == 1 || ret == 2 || ret == 3)
  {
    int i, len;

    strncpy(buf, (ret == 1 ? yytext : bow_last_label()), bufsz);
    buf[bufsz - 1] = 0;
    len = ret == 1 ? yyleng : strlen(buf);
    for (i = 0; i < len; ++i) buf[i] = tolower(buf[i]);
    
    if(start != NULL)
    {
      *end = pos - 1;
      *start = pos - yyleng;
    }
  }

  return ret;
}

int tagged_lex_get_word(char buf[], int bufsz) {
  return tagged_lex_get_word_extended(buf, bufsz, NULL, NULL);
}
