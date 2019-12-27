/* Implementation of some simple, context-free lexers. */

#include "libbow.h"
#include <ctype.h>		/* for isalpha() */

#define NO 0
#define YES 1

#define SELF ((bow_lexer_simple*)self)

/* This function is defined in scan.c */
extern int bow_scan_fp_for_string (FILE *fp, const char *string, int oneline);

/* Create and return a BOW_LEX, filling the document buffer from
   characters in FP, starting after the START_PATTERN, and ending with
   the END_PATTERN. */
bow_lex *
bow_lexer_simple_open_text_fp (bow_lexer *self, 
			       FILE *fp)
{
  int document_size = 2048;	/* the initial size of the document buffer */
  int len;			/* an index into RET->DOCUMENT */
  bow_lex *ret;			/* the BOW_LEX we will return.  */
  const char *end_pattern_ptr;
  int byte;			/* a character read from FP */

  if (feof (fp))
    return NULL;

  /* Create space for the document buffer. */
  ret = bow_malloc (self->sizeof_lex);
  ret->document = bow_malloc (document_size);

  /* Make sure DOCUMENT_START_PATTERN is not NULL; this would cause
     it to scan forward to EOF. */
  assert (self->document_start_pattern);

  /* Scan forward in the file until we find the start pattern. */
  bow_scan_fp_for_string (fp, self->document_start_pattern, 0);

  /* Make sure the DOCUMENT_END_PATTERN isn't the empty string; this
     would cause it to match and finish filling immediately. */
  assert (!self->document_end_pattern || self->document_end_pattern[0]);

  /* Fill the document buffer until we get EOF, or until we get to the
     DOCUMENT_END_PATTERN. */
  for (len = 0, end_pattern_ptr = self->document_end_pattern;
       /* We got EOF */
       (((byte = fgetc (fp)) != EOF)
	/* We found the DOCUMENT_END_PATTERN */ 
	&& !(end_pattern_ptr 
	     && *end_pattern_ptr == byte && end_pattern_ptr+1 == '\0'));
       len++)
    {
      if (len >= document_size)
	{
	  /* The RET->DOCUMENT buffer must grow to accommodate more chars. */
	  document_size *= 2;
	  ret->document = bow_realloc (ret->document, document_size);
	}

      /* Put the byte in the document buffer. */
      ret->document[len] = byte;

      /* If the byte matches the next character of the DOCUMENT_END_PATTERN
	 then prepare to match the next character of the pattern,
	 otherwise reset to the beginning of the pattern. */
      if (end_pattern_ptr)
	{
	  if (byte == *end_pattern_ptr)
	    end_pattern_ptr++;
	  else if (byte == self->document_end_pattern[0])
	    end_pattern_ptr = self->document_end_pattern+1;
	  else
	    end_pattern_ptr = self->document_end_pattern;
	}
    }

  if (len == 0)
    {
      bow_free (ret->document);
      bow_free (ret);
      return NULL;
    }

  /* If we found the DOCUMENT_END_PATTERN, push it back into the input
     stream, so we'll see it next time we read from this file. */
  /* xxx Will this work for stdin? */
  if (byte != EOF)
    {
      int end_pattern_len = (self->document_end_pattern
			     ? strlen (self->document_end_pattern)
			     : 0);
      if (end_pattern_len && fseek (fp, -end_pattern_len, SEEK_CUR) != 0)
	perror (__PRETTY_FUNCTION__);
      len -= end_pattern_len;
    }

  /* Remember, it may be the case that LEN is zero. */
  ret->document_position = 0;
  ret->document_length = len;
  ((char*)ret->document)[ret->document_length] = '\0';
  return ret;
}

/* Close the LEX buffer, freeing the memory held by it. */
void
bow_lexer_simple_close (bow_lexer *self, bow_lex *lex)
{
  bow_free (lex->document);
  bow_free (lex);
}

/* Get the raw token from the document buffer by scanning forward
   until we get a start character, and filling the buffer until we get
   an ending character.  The resulting token in the buffer is
   NULL-terminated.  Return the length of the token. */
int
bow_lexer_simple_get_raw_word (bow_lexer_simple *self, bow_lex *lex, 
			       char *buf, int buflen)
{
  int byte;			/* characters read from the FP */
  int wordlen;			/* number of characters in the word so far */

  /* Ignore characters until we get an beginning character. */
  do
    {
      byte = lex->document[lex->document_position++];
      if (byte == 0)
	return 0;
    }
  while (! self->true_to_start (byte));

      /* Add the first alphabetic character to the word. */
  buf[0] = (self->case_sensitive) ? byte : tolower (byte);

  /* Add all the following satisfying characters to the word. */
  for (wordlen = 1; wordlen < buflen; wordlen++)
    {
      byte = lex->document[lex->document_position++];;
      if (byte == 0)
	return 0;
      if (! self->false_to_end (byte))
	break;
      buf[wordlen] = tolower (byte);
    }

  if (wordlen >= buflen)
    bow_error ("Encountered word longer than buffer length=%d", buflen);

  /* Terminate it. */
  buf[wordlen] = '\0';

  return wordlen;
}

/* Perform all the necessary postprocessing after the initial token
   boundaries have been found: strip non-alphas from end, toss words
   containing non-alphas, toss words containing certaing many digits,
   toss words appearing in the stop list, stem the word, check the
   stoplist again, toss words of length one.  If the word is tossed,
   return zero, otherwise return the length of the word. */
int
bow_lexer_simple_postprocess_word (bow_lexer_simple *self, bow_lex *lex, 
				   char *buf, int buflen)
{
  int wordlen;
  int byte;

  if (self->strip_non_alphas_from_end)
    {
      /* Strip any non-alphabetic characters off the end of the word */
      while (!isalpha(buf[wordlen-1]))
	wordlen--;
      /* Terminate it. */
      buf[wordlen] = '\0';
    }

  if (self->toss_words_containing_non_alphas)
    {
      /* If the word contains any non-alphabetic characters, get
	 another word instead. */
      {
	char *bufp;
	for (bufp = buf; *bufp; bufp++)
	  {
	    if (!isalpha (*bufp))
	      return 0;
	  }
      }
    }

  /* If the word contain TOSS_WORDS_CONTAINING_THIS_MANY_DIGITS
         number of digits, get another word instead.  (Here the
         variable BYTE holds the count of the number of digits.) */
  if (self->toss_words_containing_this_many_digits)
    {
      char *bufp;
      for (bufp = buf, byte = 0; *bufp; bufp++)
	{
	  if (isdigit (*bufp))
	    if (++byte > self->toss_words_containing_this_many_digits)
	      return 0;
	}
    }

  if (self->stoplist_func && self->stoplist_func (buf))
    return 0;

  /* Apply the stemming algorithm to the word. */
  if (self->stem_func)
    self->stem_func (buf);

  /* If the result of stemming is on the stoplist, go back and start again. */
  if (self->stoplist_func && self->stoplist_func (buf))
    return 0;

  /* If the result of stemming is only one letter long, go back and
     start again. */
  if (buf[1] == '\0')
    return 0;
  
  /* Return the length of the word we found. */
  return strlen (buf);
}

/* Scan a single token from the LEX buffer, placing it in BUF, and
   returning the length of the token.  BUFLEN is the maximum number of
   characters that will fit in BUF.  If the token won't fit in BUF,
   an error is raised. */
int
bow_lexer_simple_get_word (bow_lexer *self, bow_lex *lex, 
			   char *buf, int buflen)
{
  int wordlen;			/* number of characters in the word so far */

  do 
    {
      wordlen = bow_lexer_simple_get_raw_word ((bow_lexer_simple*)self, 
					       lex, buf, buflen);
      if (wordlen == 0)
	return 0;
    }
  while ((wordlen = bow_lexer_simple_postprocess_word 
	  ((bow_lexer_simple*)self, lex, buf, buflen))
	 == 0);
  return wordlen;
}

/* The end of the bow_lex_simple_ functions. */
#undef SELF


/* A function wrapper around POSIX's `isalpha' macro. */
int
bow_isalpha (int character)
{
  return isalpha (character);
}

/* A function wrapper around POSIX's `isgraph' macro. */
int
bow_isgraph (int character)
{
  return isgraph (character);
}


/* A lexer that keeps all alphabetic strings, delimited by
   non-alphabetic characters.  For example, the string
   `http://www.cs.cmu.edu' will result in the tokens `http', `www',
   `cs', `cmu', `edu'. */
const bow_lexer_simple _bow_alpha_lexer =
{
  {
    sizeof (typeof (_bow_alpha_lexer)),
    bow_lexer_simple_open_text_fp,
    bow_lexer_simple_get_word,
    bow_lexer_simple_close,
    "",				/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  bow_isalpha,			/* begin words with an alphabetic char */
  bow_isalpha,			/* end words with any non-alphabetic char */
  bow_stoplist_present,		/* use the default stoplist */
  bow_stem_porter,		/* use the Porter stemming algorithm */
  NO,				/* be case-INsensitive */
  NO,				/* don't strip non-alphas from end */
  NO,				/* don't toss words w/ non-alphas */
  0				/* don't toss words with digits */
};
const bow_lexer_simple *bow_alpha_lexer = &_bow_alpha_lexer;

/* A lexer that throws out all space-delimited strings that have any
   non-alphabetical characters.  For example, the string `obtained
   from http://www.cs.cmu.edu' will result in the tokens `obtained'
   and `from', but the URL will be skipped. */
const bow_lexer_simple _bow_alpha_only_lexer =
{
  {
    sizeof (typeof (_bow_alpha_only_lexer)),
    bow_lexer_simple_open_text_fp,
    bow_lexer_simple_get_word,
    bow_lexer_simple_close,
    "",				/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  bow_isalpha,			/* begin words with an alphabetic char */
  bow_isgraph,			/* end words with any non-alphabetic char */
  bow_stoplist_present,		/* use the default stoplist */
  bow_stem_porter,		/* use the Porter stemming algorithm */
  NO,				/* be case-INsensitive */
  YES,				/* strip non-alphas from end */
  YES,				/* toss words w/ non-alphas */
  3				/* toss words with 3 digits */
};
const bow_lexer_simple *bow_alpha_only_lexer = &_bow_alpha_only_lexer;

/* A lexer that keeps all strings that begin and end with alphabetic
   characters, delimited by white-space.  For example,
   the string `http://www.cs.cmu.edu' will be a single token. */
const bow_lexer_simple _bow_white_lexer =
{
  {
    sizeof (typeof (_bow_white_lexer)),
    bow_lexer_simple_open_text_fp,
    bow_lexer_simple_get_word,
    bow_lexer_simple_close,
    "",				/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  bow_isalpha,			/* begin words with an alphabetic char */
  bow_isgraph,			/* end words with any non-alphabetic char */
  bow_stoplist_present,		/* use the default stoplist */
  bow_stem_porter,		/* use the Porter stemming algorithm */
  NO,				/* be case-INsensitive */
  YES,				/* strip non-alphas from end */
  NO,				/* don't toss words w/ non-alphas */
  4				/* toss words with 4 digits */
};
const bow_lexer_simple *bow_white_lexer = &_bow_white_lexer;

