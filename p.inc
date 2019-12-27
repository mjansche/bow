/* -*- c -*-
   This file provides the code for p-alpha.c, p-alonly.c and p-white.c.
   The code's behavior is changed by setting the following macros.

   This should be the unique part of the parser name, like `alpha_only'

   BOW_P_NAME

   These should be function names, like `isalpha', `isgraph' or
   `bow_stoplist_present':

   BOW_P_TRUE_TO_START
   BOW_P_FALSE_TO_END
   BOW_P_STOPLIST_CHECKER

   If you don't want to use a stoplist checker, leave
   BOW_P_STOPLIST_CHECKER undefined.


   These should be either 0 for `no' or 1 for `yes':

   BOW_P_TOSS_WORDS_CONTAINING_NON_ALPHAS
   BOW_P_TOSS_WORDS_WITH_MORE_THAN_3_DIGITS
   BOW_P_STRIP_NON_ALPHAS_FROM_END
   BOW_P_STEM_WORD

 */

#include "libbow.h"
#include <ctype.h>		/* for isalpha() */

/* All this to get the BOW_P_NAME to expand properly.  Read cpp info
   files about Separate Expansion of Macro Arguments. */
#define __BOW_P_OPEN_NAME(N) bow_ ## N ## _parser_open_text_fp 
#define _BOW_P_OPEN_NAME(N) __BOW_P_OPEN_NAME(N)
#define BOW_P_OPEN_NAME _BOW_P_OPEN_NAME(BOW_P_NAME)

int BOW_P_NAME;

extern int bow_scan_fp_for_string (FILE *fp, const char *string, int oneline);


bow_parse *
BOW_P_OPEN_NAME (FILE *fp)
{
  bow_parse *ret;

  ret = bow_malloc (sizeof (bow_parse));
  ret->fp = fp;
  if (bow_parser_skip_net_header)
    bow_scan_fp_for_string (fp, "\n\n", 0);
  return ret;
}


/* All this to get the BOW_P_NAME to expand properly.  Read cpp info
   files about Separate Expansion of Macro Arguments. */
#define __BOW_P_CLOSE_NAME(N) bow_ ## N ## _parser_close 
#define _BOW_P_CLOSE_NAME(N) __BOW_P_CLOSE_NAME(N)
#define BOW_P_CLOSE_NAME _BOW_P_CLOSE_NAME(BOW_P_NAME)

void
BOW_P_CLOSE_NAME (bow_parse *parse)
{
  bow_free (parse);
}


/* All this to get the BOW_P_NAME to expand properly.  Read cpp info
   files about Separate Expansion of Macro Arguments. */
#define __BOW_P_GET_WORD_NAME(N) bow_ ## N ## _parser_get_word
#define _BOW_P_GET_WORD_NAME(N) __BOW_P_GET_WORD_NAME(N)
#define BOW_P_GET_WORD_NAME _BOW_P_GET_WORD_NAME(BOW_P_NAME)

int
BOW_P_GET_WORD_NAME (bow_parse *parse, 
		     char *buf, int buflen)
{
  int byte;			/* characters read from the FP */
  int wordlen;			/* number of characters in the word so far */

again:
  /* Keep getting words until we get a word that's not in the stoplist. */
  do
    {
      /* Ignore characters until we get an beginning character. */
      do
	{
	  byte = fgetc (parse->fp);
	  if (byte == EOF)
	    return 0;
	}
      while (! BOW_P_TRUE_TO_START (byte));

      /* Add the first alphabetic character to the word. */
      buf[0] = tolower (byte);

      /* Add all the following satisfying characters to the word. */
      for (wordlen = 1; wordlen < buflen; wordlen++)
	{
	  byte = fgetc (parse->fp);
	  if (byte == EOF)
	    return 0;
	  if (! BOW_P_FALSE_TO_END (byte))
	    break;
	  buf[wordlen] = tolower (byte);
	}

      if (wordlen >= buflen)
	bow_error ("Encountered word longer than buffer length=%d", buflen);

#if BOW_P_STRIP_NON_ALPHAS_FROM_END
      /* Strip any non-alphabetic characters off the end of the word */
      while (!isalpha(buf[wordlen-1]))
	wordlen--;
#endif /* BOW_P_STRIP_NON_ALPHAS_FROM_END */

      /* Terminate it. */
      buf[wordlen] = '\0';

#if BOW_P_TOSS_WORDS_CONTAINING_NON_ALPHAS
      /* If the word contains any non-alphabetic characters, get
         another word instead. */
      {
	char *bufp;
	for (bufp = buf; *bufp; bufp++)
	  {
	    if (!isalpha (*bufp))
	      goto again;
	  }
      }
#endif /* BOW_P_TOSS_WORDS_CONTAINING_NON_ALPHAS */

#if BOW_P_TOSS_WORDS_WITH_MORE_THAN_3_DIGITS
      /* If the word contains more than 3 digits, get another word instead.
         (Here the variable BYTE holds the count of the number of digits.) */
      {
	char *bufp;
	for (bufp = buf, byte = 0; *bufp; bufp++)
	  {
	    if (isdigit (*bufp))
	      if (++byte > 3)
		goto again;
	  }
      }
#endif /* BOW_P_TOSS_WORDS_WITH_MORE_THAN_3_DIGITS */

    }
  while (
#ifdef BOW_P_STOPLIST_CHECKER 
	 BOW_P_STOPLIST_CHECKER (buf)
#else
	 0
#endif
	 );

#if BOW_P_STEM_WORD
  /* Apply Porter's stemming algorithm to the word. */
  bow_stem (buf);

  /* If the result of stemming is on the stoplist, go back and start again. */
  if (
#ifdef BOW_P_STOPLIST_CHECKER 
	 BOW_P_STOPLIST_CHECKER (buf)
#else
	 0
#endif /* BOW_P_STOPLIST_CHECKER */
	 )
    goto again;

  /* If the result of stemming is only one letter long, go back and
     start again. */
  if (buf[1] == '\0')
    goto again;
#endif /* BOW_P_STEM_WORD */
  
  /* Return the length of the word we found. */
  return strlen (buf);
}
