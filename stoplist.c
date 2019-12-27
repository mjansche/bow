/* Determine if a word is on the stoplist or not. */

#include "libbow.h"
#include <stdlib.h>
#include <ctype.h>		/* for isupper */

static bow_int4str *stopword_map;

/* This is defined in stopwords.c */
extern char *_bow_builtin_stopwords[];

static void
init_stopwords ()
{
  char **word_ptr;

  stopword_map = bow_int4str_new (0);
#if 0
  return;			/* xxx temporary for missy-demo data! */
#endif

  for (word_ptr = _bow_builtin_stopwords; *word_ptr; word_ptr++)
    {
      bow_str2int (stopword_map, *word_ptr);
    }
}

/* Add to the stoplist the white-space delineated words from FILENAME.
   Return the number of words added.  If the file could not be opened,
   return -1. */
int
bow_stoplist_add_from_file (const char *filename)
{
  FILE *fp;
  char word[BOW_MAX_WORD_LENGTH];
  int count = 0;

  if ((fp = fopen (filename, "r")) == NULL)
    return -1;

  if (!stopword_map)
    init_stopwords ();

  while (fscanf (fp, "%s", word) == 1)
    {
      bow_str2int (stopword_map, word);
      count++;
      bow_verbosify (bow_screaming, "Added to stoplist: `%s'\n", word);
    }
  bow_verbosify (bow_verbose, "Added %d words from `./.bow-stopwords'\n",
		 count);
  return count;
}

int
bow_stoplist_present (const char *word)
{
  if (!stopword_map)
    init_stopwords ();

  return ((bow_str2int_no_add (stopword_map, word) == -1)
	  ? 0 : 1);
}
