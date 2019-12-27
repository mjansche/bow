/* Implementation of helping functions for lexers that use a nested,
   underlying lexer. */
/* Copyright (C) 1997, 1998 Andrew McCallum

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>

   This file is part of the Bag-Of-Words Library, `libbow'.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation, version 2.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA */


#include <bow/libbow.h>

#define SELF ((bow_lexer_indirect*)self)

/* Open the underlying lexer. */
bow_lex *
bow_lexer_indirect_open_text_fp (bow_lexer *self, FILE *fp,
				 const char *filename)
{
  return SELF->underlying_lexer->open_text_fp (self, fp, filename);
}

/* Open the underlying lexer. */
bow_lex *
bow_lexer_indirect_open_str (bow_lexer *self, char *buf)
{
  return SELF->underlying_lexer->open_str (self, buf);
}

/* Close the underlying lexer. */
void
bow_lexer_indirect_close (bow_lexer *self, bow_lex *lex)
{
  SELF->underlying_lexer->close (self, lex);
}
