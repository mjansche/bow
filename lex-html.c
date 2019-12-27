/* A lexer will special features for handling HTML. */

#include "libbow.h"
#include <ctype.h>		/* for tolower() */

int
bow_lexer_html_get_raw_word (bow_lexer_simple *self, bow_lex *lex, 
			     char *buf, int buflen)
{
  int byte;			/* characters read from the FP */
  int wordlen;			/* number of characters in the word so far */
  int html_bracket_nestings = 0;

  /* Ignore characters until we get an beginning character. */
  do
    {
      byte = lex->document[lex->document_position++];
      if (byte == 0)
	{
	  if (html_bracket_nestings)
	    bow_verbosify (bow_quiet,
			   "Found unterminated `<' in HTML\n");
	  return 0;
	}
      if (byte == '<')
	html_bracket_nestings++;
      else if (byte == '>')
	{
	  if (html_bracket_nestings == 0)
	    bow_verbosify (bow_verbose,
			   "Found `>' outside HTML token\n");
	  html_bracket_nestings--;
	}
    }
  while (html_bracket_nestings || !self->true_to_start (byte));

  /* Add the first alphabetic character to the word. */
  buf[0] = (self->case_sensitive) ? byte : tolower (byte);

  /* Add all the following satisfying characters to the word. */
  for (wordlen = 1; wordlen < buflen; wordlen++)
    {
      byte = lex->document[lex->document_position++];;
      if (byte == 0)
	return 0;
      if (byte == '<' || !self->false_to_end (byte))
	{
	  lex->document_position--;
	  break;
	}
      buf[wordlen] = tolower (byte);
    }

  if (wordlen >= buflen)
    bow_error ("Encountered word longer than buffer length=%d", buflen);

  /* Terminate it. */
  buf[wordlen] = '\0';

  return wordlen;
}

/* Scan a single token from the LEX buffer, placing it in BUF, and
   returning the length of the token.  BUFLEN is the maximum number of
   characters that will fit in BUF.  If the token won't fit in BUF,
   an error is raised. */
int
bow_lexer_html_get_word (bow_lexer *self, bow_lex *lex, 
			 char *buf, int buflen)
{
#define SELF ((bow_lexer_indirect*)self)
  int wordlen;			/* number of characters in the word so far */

  do 
    {
      /* Yipes, this UNDERLYING_LEXER had better be a BOW_LEXER_SIMPLE! */
      wordlen = bow_lexer_html_get_raw_word 
	((bow_lexer_simple*)SELF->underlying_lexer, lex, buf, buflen);
      if (wordlen == 0)
	return 0;
    }
  while ((wordlen = bow_lexer_simple_postprocess_word
	  ((bow_lexer_simple*)SELF->underlying_lexer, lex, buf, buflen))
	 == 0);
  return wordlen;
#undef SELF
}

/* This is declared in lex-simple.c */
extern bow_lexer_simple _bow_alpha_lexer;

/* A lexer that ignores all HTML directives, ignoring all characters
   between angled brackets: < and >. */
const bow_lexer_indirect _bow_html_lexer =
{
  {
    sizeof (typeof (_bow_html_lexer)),
    bow_lexer_simple_open_text_fp,
    bow_lexer_html_get_word,
    bow_lexer_simple_close,
    "",				/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  (bow_lexer*)&_bow_alpha_lexer,/* default UNDERLYING_LEXER */
};
const bow_lexer_indirect *bow_html_lexer = &_bow_html_lexer;
