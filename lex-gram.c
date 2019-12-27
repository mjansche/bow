/* A simple N-gram lexer. */

#include "libbow.h"

#define SELF ((bow_lexer_gram*)self)
#define LEX ((bow_lex_gram*)lex)

bow_lex *
bow_lexer_gram_open_text_fp (bow_lexer *self, FILE *fp)
{
  bow_lex *lex = bow_lexer_indirect_open_text_fp (self, fp);
  if (lex == NULL)
    return NULL;
  LEX->gram_size_this_time = SELF->gram_size;
  return lex;
}

int
bow_lexer_gram_get_word (bow_lexer *self, bow_lex *lex, 
			 char *buf, int buflen)
{
  int i;
  char **tokens;
  int s;
  int len;
  
  tokens = alloca (sizeof (char*) * LEX->gram_size_this_time);
  for (i = 0; i < LEX->gram_size_this_time; i++)
    tokens[i] = alloca (BOW_MAX_WORD_LENGTH);

  /* Remember where we started. */
  s = LEX->lex.document_position;

  /* Get the first token. */
  if (SELF->indirect_lexer.underlying_lexer->get_word 
      (SELF->indirect_lexer.underlying_lexer, lex,
       tokens[0], BOW_MAX_WORD_LENGTH)
      == 0)
    return 0;

  /* Get the next n-1 tokens. */
  for (i = 1; i < LEX->gram_size_this_time; i++)
    if (SELF->indirect_lexer.underlying_lexer->get_word
	(SELF->indirect_lexer.underlying_lexer, lex,
	 tokens[i], BOW_MAX_WORD_LENGTH)
	== 0)
      *(tokens[i]) = '\0';

  /* Make sure it will fit. */
  for (i = 0, len = 0; i < LEX->gram_size_this_time; i++)
    len += strlen (tokens[i]) + 1;
  assert (len < BOW_MAX_WORD_LENGTH);

  /* Fill buf with the tokens concatenated. */
  strcpy (buf, tokens[0]);
  for (i = 1; i < LEX->gram_size_this_time; i++)
    {
      strcat (buf, ";");
      strcat (buf, tokens[i]);
    }

  /* Put us back to the second token so we can get it with the next call */
  if (LEX->gram_size_this_time > 1)
    LEX->lex.document_position = s;

  if (LEX->gram_size_this_time == 1)
    LEX->gram_size_this_time = SELF->gram_size;
  else
    LEX->gram_size_this_time--;

  return strlen (buf);
}

/* This is declared in lex-simple.c */
extern bow_lexer_simple _bow_alpha_lexer;

const bow_lexer_gram _bow_gram_lexer =
{
  {
    {
      sizeof (typeof (_bow_gram_lexer)),
      bow_lexer_gram_open_text_fp,
      bow_lexer_gram_get_word,
      bow_lexer_indirect_close,
      "",			/* document start pattern begins right away */
      NULL			/* document end pattern goes to end */
    },
    (bow_lexer*)&_bow_alpha_lexer,/* default UNDERLYING_LEXER */
  },
  1				/* default gram-size is 1 */
};
const bow_lexer_gram *bow_gram_lexer = &_bow_gram_lexer;
