/* Implementation of helping functions for lexers that use a nested,
   underlying lexer. */

#include "libbow.h"

#define SELF ((bow_lexer_indirect*)self)

/* Open the underlying lexer. */
bow_lex *
bow_lexer_indirect_open_text_fp (bow_lexer *self, FILE *fp)
{
  return SELF->underlying_lexer->open_text_fp (self, fp);
}

/* Close the underlying lexer. */
void
bow_lexer_indirect_close (bow_lexer *self, bow_lex *lex)
{
  SELF->underlying_lexer->close (self, lex);
}
