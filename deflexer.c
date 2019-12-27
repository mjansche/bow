/* Define and set the default lexer. */

/* Copyright (C) 1997 Andrew McCallum

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

/* Default instances of the lexers that can be modified by libbow's
   argp cmdline argument processing. */
static bow_lexer_simple _bow_default_lexer_simple;
static bow_lexer_gram _bow_default_lexer_gram;
static bow_lexer_indirect _bow_default_lexer_html;
static bow_lexer_indirect _bow_default_lexer_email;

bow_lexer_simple *bow_default_lexer_simple;
bow_lexer_indirect *bow_default_lexer_indirect;
bow_lexer_gram *bow_default_lexer_gram;
bow_lexer_indirect *bow_default_lexer_html;
bow_lexer_indirect *bow_default_lexer_email;

/* The default lexer used by all library functions. */
/* NOTE: Be sure to set this to a value, otherwise some linkers (like
   SunOS's) will not actually include this .o file in the executable,
   and then _bow_default_lexer() will not get called, and then the
   lexer's will not get initialized properly.  Ug. */
bow_lexer *bow_default_lexer = (void*)-1;

void _bow_default_lexer_init ()  __attribute__ ((constructor));

void
_bow_default_lexer_init ()
{
  static int done = 0;

  if (done)
    return;
  done = 1;

  _bow_default_lexer_simple = *bow_alpha_lexer;
  _bow_default_lexer_gram = *bow_gram_lexer;
  _bow_default_lexer_html = *bow_html_lexer;
  _bow_default_lexer_email = *bow_email_lexer;

  _bow_default_lexer_gram.indirect_lexer.underlying_lexer =
    (bow_lexer*)(&_bow_default_lexer_simple);
  _bow_default_lexer_html.underlying_lexer =
    (bow_lexer*)(&_bow_default_lexer_simple);
  _bow_default_lexer_email.underlying_lexer =
    (bow_lexer*)(&_bow_default_lexer_simple);

  bow_default_lexer_simple = &_bow_default_lexer_simple;
  bow_default_lexer_gram = &_bow_default_lexer_gram;
  bow_default_lexer_html = &_bow_default_lexer_html;
  bow_default_lexer_email = &_bow_default_lexer_email;

  bow_default_lexer = (bow_lexer*) bow_default_lexer_gram;
  bow_default_lexer_indirect = &(bow_default_lexer_gram->indirect_lexer);
}
