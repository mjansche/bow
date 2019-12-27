/* Determine if a word is on the stoplist or not. */

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

/* Empty the default stoplist, and add space-delimited words from FILENAME. */
void
bow_stoplist_replace_with_file (const char *filename)
{
  if (stopword_map)
    bow_int4str_free (stopword_map);
  stopword_map = bow_int4str_new (0);
  bow_stoplist_add_from_file (filename);
}

void
bow_stoplist_add_word (const char *word)
{
  bow_str2int (stopword_map, word);
  bow_verbosify (bow_screaming, "Added to stoplist: `%s'\n", word);
}

int
bow_stoplist_present (const char *word)
{
  if (!stopword_map)
    init_stopwords ();

  return ((bow_str2int_no_add (stopword_map, word) == -1)
	  ? 0 : 1);
}
