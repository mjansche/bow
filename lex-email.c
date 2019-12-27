/* A lexer with special features for handling e-mail and newsgroup messages.
   Specifically, this lexer will remove headers which are in the
   BOW_EMAIL_HEADERS_TO_REMOVE array. 

   Copyright (C) 1997 Andrew McCallum and Jason Rennie

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>
                and Jason Rennie <jr6b@andrew.cmu.edu>

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
#include <ctype.h>		/* for tolower() */

char **bow_email_headers_to_remove = NULL;

/* String compare function used to compare headers.  Ignores case distinctions
   1 => strings don't match.  0 => strings do match. */
int
strcmp_ignore_case (char *str1, char *str2)
{
  int pos = 0;

  assert(str1 != NULL);  assert(str2 != NULL);

  while (tolower(str1[pos]) == tolower(str2[pos]))
    {
      if (str1[pos] == '\0') { return 0; }
      pos++;
    }

  return 1;
}

/* Assumes that lex->document[lex->document_position] is at the
   beginning of a newline (immediately after a '\n').  Compares the
   characters up to (but not including) the first colon against each
   string in the BOW_EMAIL_HEADERS_TO_REMOVE array.  If any match is
   found, the document_position value is moved to the beginning of the
   next line.  Otherwise, returns document_position to where it was */
void
bow_lexer_email_check_header (bow_lex *lex)
{
  int old_document_position = lex->document_position;

  char *header = bow_malloc(sizeof(char) * BOW_MAX_WORD_LENGTH);
  int header_position = 0;
  char byte;
  int i;

  /* Put header name into HEADER character array.  Stop at a non-printable
     character, a colon, or an overflow of characters */
  do 
    {
      byte = lex->document[lex->document_position++];
      header[header_position++] = byte;
    }
  while (((byte >= 33 && byte <= 57) || (byte >= 59 && byte <= 126))
	 && header_position < BOW_MAX_WORD_LENGTH);

  /* If it is a proper header, check to see if it is in the list, else
     return the document position back to its original value.  See rfc822
     for information on lexing e-mail headers. */
  if (byte == 58)
    {
      /* Step through NULL-terminated array. */
      for (i = 0; bow_email_headers_to_remove[i]; i++)
	if (strcmp_ignore_case (bow_email_headers_to_remove[i], header) == 0)
	  {
	    while (lex->document[lex->document_position] != '\n'
		   || lex->document[lex->document_position+1] == 32
		   || lex->document[lex->document_position+1] == 9)
	      { lex->document_position++; }
	    break;
	  }
    }
  else
    {
      lex->document_position = old_document_position;
    }

  free(header);

  return;
}

/* Get the raw token from the document buffer by scanning forward
   until we get a start character, and filling the buffer until we get
   an ending character.  The resulting token in the buffer is
   NULL-terminated.  Return the length of the token. */
int
bow_lexer_email_get_raw_word (bow_lexer_simple *self, bow_lex *lex, 
			       char *buf, int buflen)
{
  int byte;			/* characters read from the FP */
  int wordlen;			/* number of characters in the word so far */

  /* Ignore characters until we get a beginning character. */
  do
    {
      byte = lex->document[lex->document_position++];
      if (byte == 0)
	return 0;
      if (byte == '\n')
	bow_lexer_email_check_header (lex);
    }
  while (! self->true_to_start (byte));

  /* Add the first alphabetic character to the word. */
  buf[0] = (self->case_sensitive) ? byte : tolower (byte);

  /* Add all the following satisfying characters to the word. */
  for (wordlen = 1; wordlen < buflen; wordlen++)
    {
      byte = lex->document[lex->document_position++];
      if (byte == 0)
	return 0;
      if (byte == '\n')
	bow_lexer_email_check_header (lex);
      if (! self->false_to_end (byte))
	break;
      buf[wordlen] = tolower (byte);
    }

  if (wordlen >= buflen)
    bow_error ("Encountered word longer than buffer length=%d", buflen);

  /* Back up to point at the character that caused the end of the word. */
  lex->document_position--;

  /* Terminate it. */
  buf[wordlen] = '\0';

  return wordlen;
}

/* Scan a single token from the LEX buffer, placing it in BUF, and
   returning the length of the token.  BUFLEN is the maximum number of
   characters that will fit in BUF.  If the token won't fit in BUF,
   an error is raised. */
int
bow_lexer_email_get_word (bow_lexer *self, bow_lex *lex, 
			 char *buf, int buflen)
{
#define SELF ((bow_lexer_indirect*)self)
  int wordlen;			/* number of characters in the word so far */

  do 
    {
      /* Yipes, this UNDERLYING_LEXER had better be a BOW_LEXER_SIMPLE! */
      wordlen = bow_lexer_email_get_raw_word 
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

/* A lexer that ignores headers specified in BOW_EMAIL_HEADERS_TO_REMOVE */
const bow_lexer_indirect _bow_email_lexer =
{
  {
    sizeof (typeof (_bow_email_lexer)),
    bow_lexer_simple_open_text_fp,
    bow_lexer_simple_open_str,
    bow_lexer_email_get_word,
    bow_lexer_simple_close,
    "",			/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  (bow_lexer*)&_bow_alpha_lexer, /* default UNDERLYING_LEXER */
};
const bow_lexer_indirect *bow_email_lexer = &_bow_email_lexer;
