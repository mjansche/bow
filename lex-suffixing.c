#include <bow/libbow.h>
#include <ctype.h>

/* bow_default_lexer_suffixing should be an indirect lexer, with a
   simple lexer as its underlying lexer.  However, I got lazy, and it
   should be considered that the bow_default_lexer_simple is always
   the underlying lexer */

#define HEADER_TWICE 1

static int suffixing_doing_headers;
static int suffixing_appending_headers;
static char suffixing_suffix[BOW_MAX_WORD_LENGTH];
static int suffixing_suffix_length;

int bow_lexer_html_get_raw_word (bow_lexer_simple *self, bow_lex *lex, 
				 char *buf, int buflen);

/* Put the string before the ':' into SUFFIXING_SUFFIX, and replace the
   following newline with a '\0' */
static void
suffixing_snarf_suffix (bow_lex *lex)
{
  int i;

  /* Hack to get arrow to work on 23k+ research paper index */
  if (! isalpha (lex->document[0]))
    {
      /*lex->document[0] = '\0';*/
      return;
    }
  /*  assert (isalpha (lex->document[0])); */
  suffixing_suffix[0] = 'x';
  suffixing_suffix[1] = 'x';
  suffixing_suffix[2] = 'x';
  suffixing_suffix_length = 3;
  /* Put characters into the suffix until we get to the colon */
  while (lex->document[lex->document_position] != ':')
    {
      assert (lex->document[lex->document_position] != '\n');
      suffixing_suffix[suffixing_suffix_length++] = 
	tolower (lex->document[lex->document_position++]);
      assert (suffixing_suffix_length < BOW_MAX_WORD_LENGTH);
    }
  suffixing_suffix[suffixing_suffix_length] = '\0';
  /* Throw away everything else until end of string */
  i = 0;
  while (lex->document[lex->document_position + i] != '\n'
	 /* This second condition is necessary if we are going through
	    the header twice (when HEADER_TWICE=1) */
	 && lex->document[lex->document_position + i] != '\0')
    {
      i++;
      assert (lex->document_position + i < lex->document_length);
    }
  lex->document[lex->document_position + i] = '\0';
}

/* Create and return a BOW_LEX, filling the document buffer from
   characters in FP, starting after the START_PATTERN, and ending with
   the END_PATTERN. */
bow_lex *
bow_lexer_suffixing_open_text_fp (bow_lexer *self, 
				  FILE *fp,
				  const char *filename)
{
  bow_lex *ret;

  ret = bow_lexer_simple_open_text_fp ((bow_lexer*)bow_default_lexer_simple, 
				       fp, filename);

  if (ret)
    {
      suffixing_doing_headers = 1;
      suffixing_appending_headers = 1;
      suffixing_snarf_suffix (ret);
    }
  return ret;
}


/* Create and return a BOW_LEX, filling the document buffer from
   characters in FP, starting after the START_PATTERN, and ending with
   the END_PATTERN. */
bow_lex *
bow_lexer_suffixing_open_str (bow_lexer *self, char *buf)
{
  bow_lex *ret;

  ret = bow_lexer_simple_open_str ((bow_lexer*)bow_default_lexer_simple, buf);

  if (ret)
    {
      suffixing_doing_headers = 1;
      suffixing_appending_headers = 1;
      suffixing_snarf_suffix (ret);
    }
  return ret;
}


int
bow_lexer_suffixing_postprocess_word (bow_lexer_simple *self, bow_lex *lex, 
				      char *buf, int buflen)
{
  int len;

  len = bow_lexer_simple_postprocess_word (bow_default_lexer_simple,
					   lex, buf, buflen);
  if (len != 0 && suffixing_doing_headers && suffixing_appending_headers)
    {
      strcat (buf, suffixing_suffix);
      len = strlen (buf);
      assert (len < buflen);
    }

  if (suffixing_doing_headers && lex->document[lex->document_position] == '\0')
    {
      if (lex->document[lex->document_position + 1] == '\n')
	{
#if HEADER_TWICE
	  if (!suffixing_appending_headers)
	    suffixing_doing_headers = 0;
	  else
	    {
	      lex->document_position = 0;
	      suffixing_appending_headers = 0;
	      suffixing_snarf_suffix (lex);
	    }
#else
	  lex->document_position++;
#endif
	  suffixing_suffix[0] = '\0';
	}
      else
	{
	  lex->document_position++;
	  suffixing_snarf_suffix (lex);
	}
    }

  return len;
}

/* Scan a single token from the LEX buffer, placing it in BUF, and
   returning the length of the token.  BUFLEN is the maximum number of
   characters that will fit in BUF.  If the token won't fit in BUF,
   an error is raised. */
int
bow_lexer_suffixing_get_word (bow_lexer *self, bow_lex *lex, 
			      char *buf, int buflen)
{
  int wordlen;			/* number of characters in the word so far */

  do 
    {
      /* Hmm... this looks like a hack.  Shouldn't we have something like
	 bow_default_lexer->get_word ? */
      wordlen = bow_lexer_html_get_raw_word ((bow_lexer_simple*)
					     bow_default_lexer_simple, 
					     lex, buf, buflen);
      if (wordlen == 0)
	{
	  if (suffixing_doing_headers)
	    /* We are just at the end of the headers, not at the end
               of the file.  bow_lexer_suffixing_postprocess_word()
               will deal with this */
	    buf[0] = '\0';
	  else
	    return 0;
	}
    }
  while (((wordlen = bow_lexer_suffixing_postprocess_word 
	   ((bow_lexer_simple*)self, lex, buf, buflen)) == 0)
	 || strstr (suffixing_suffix, "URL"));

  wordlen = strlen (buf);
  return wordlen;
}



/* A lexer that keeps all alphabetic strings, delimited by
   non-alphabetic characters.  For example, the string
   `http://www.cs.cmu.edu' will result in the tokens `http', `www',
   `cs', `cmu', `edu'. */
const bow_lexer_simple _bow_suffixing_lexer =
{
  {
    sizeof (bow_lexer_simple),
    bow_lexer_suffixing_open_text_fp,
    bow_lexer_suffixing_open_str,
    bow_lexer_suffixing_get_word,
    bow_lexer_simple_close,
    "",				/* document start pattern begins right away */
    NULL			/* document end pattern goes to end */
  },
  bow_isalpha,			/* begin words with an alphabetic char */
  bow_isalpha,			/* end words with any non-alphabetic char */
  bow_stoplist_present,		/* use the default stoplist */
  0,				/* don't use the Porter stemming algorithm */
  0,				/* be case-INsensitive */
  0,				/* don't strip non-alphas from end */
  0,				/* don't toss words w/ non-alphas */
  0,				/* don't toss words with digits */
  59				/* toss words longer than 59 chars, uuenc=60 */
};
const bow_lexer_simple *bow_suffixing_lexer = &_bow_suffixing_lexer;
