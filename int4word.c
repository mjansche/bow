/* A convient interface to int4str.c, specifically for words. */

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
#include <bow/hdb.h>
#include <assert.h>
#include <stdio.h>

/* The int/string mapping for bow's vocabulary words. */
static bow_int4str *word_map = NULL;

/* An array, holding the occurrence counts of all words in vocabulary. */
static int *word_map_counts = NULL;
static int word_map_counts_size = 0;

/* If this is non-zero, then bow_word2int() will return -1 when asked
   for the index of a word that is not already in the mapping. */
int bow_word2int_do_not_add = 0;

static inline void
_bow_int4word_initialize ()
{
  static const int WORD_MAP_COUNTS_INITIAL_SIZE = 1000;
  int wi;

  word_map = bow_int4str_new (0);
  word_map_counts_size = WORD_MAP_COUNTS_INITIAL_SIZE;
  word_map_counts = bow_malloc (word_map_counts_size * sizeof (int));
  for (wi = 0; wi < WORD_MAP_COUNTS_INITIAL_SIZE; wi++)
    word_map_counts[wi] = 0;
}

/* Replace the current word/int mapping with MAP. */
void
bow_words_set_map (bow_int4str *map, int free_old_map)
{
  int wi;

  /* Do this so we are sure to initalize the counts array too. */
  /* xxx This is messy way to doing this, though. */
  if (!word_map)
    _bow_int4word_initialize ();

  if (free_old_map)
    bow_int4str_free (word_map);
  assert (word_map_counts);
  for (wi = 0; wi < word_map_counts_size; wi++)
    word_map_counts[wi] = 0;

  word_map = map;
}

const char *
bow_int2word (int index)
{
  if (!word_map)
    bow_error ("No words yet added to the int-word mapping.\n");
  if (index >= word_map->str_array_length)
    return NULL;
  return bow_int2str (word_map, index);
}

int
bow_word2int (const char *word)
{
  if (!word_map)
    _bow_int4word_initialize ();
  if (bow_word2int_do_not_add)
    return bow_str2int_no_add (word_map, word);
  return bow_str2int (word_map, word);
}

/* Given a WORD, return its "word index", WI, according to the global
   word-int mapping; if it's not yet in the mapping, return -1. */
int
bow_word2int_no_add (const char *word)
{
  if (!word_map)
    _bow_int4word_initialize ();
  return bow_str2int_no_add (word_map, word);
}

/* Like bow_word2int(), except it also increments the occurrence count 
   associated with WORD. */
int
bow_word2int_add_occurrence (const char *word)
{
  int ret = bow_word2int (word);
  
  if (ret < 0)
    return ret;
  if (word_map->str_array_length >= word_map_counts_size)
    {
      /* WORD_MAP_COUNTS must grow to accomodate the new entry */
      int wi, old_size = word_map_counts_size;
      word_map_counts_size *= 2;
      word_map_counts = bow_realloc (word_map_counts,
				     word_map_counts_size * sizeof (int));
      for (wi = old_size; wi < word_map_counts_size; wi++)
	word_map_counts[wi] = 0;
    }
  (word_map_counts[ret])++;
  return ret;
}

/* Return the number of times bow_word2int_add_occurrence() was
   called with the word whose index is WI. */
int
bow_words_occurrences_for_wi (int wi)
{
  assert (wi >= 0);
  if (wi >= word_map_counts_size)
    return 0;
  return word_map_counts[wi];
}

int
bow_num_words ()
{
  if (!word_map)
    return 0;
  return word_map->str_array_length;
}

void
bow_words_write (FILE *fp)
{
  int wi;

  bow_int4str_write (word_map, fp);
  bow_fwrite_int (word_map_counts_size, fp);
  for (wi = 0; wi < word_map_counts_size; wi++)
    bow_fwrite_int (word_map_counts[wi], fp);
}

void
bow_words_write_to_file (const char *filename)
{
  FILE *fp;
  
  fp = bow_fopen (filename, "w");
  bow_words_write (fp);
  fclose (fp);
}

void
bow_words_read_from_fp (FILE *fp)
{
  int wi;

  if (word_map)
    bow_error ("The vocabulary map has already been created.");
  word_map = bow_int4str_new_from_fp (fp);
  bow_fread_int (&word_map_counts_size, fp);
  word_map_counts = bow_malloc (word_map_counts_size * sizeof (int));
  for (wi = 0; wi < word_map_counts_size; wi++)
    bow_fread_int (&(word_map_counts[wi]), fp);
}

void
bow_words_read_from_file (const char *filename)
{
  FILE *fp;

  fp = bow_fopen (filename, "r");
  bow_words_read_from_fp (fp);
  fclose (fp);
}

void
bow_words_reread_from_file (const char *filename, int force_update)
{
  FILE *fp;
  static char *last_file = NULL;

  if (!filename || !*filename)
    return;
  if (last_file && !strcmp (filename, last_file) && !force_update)
    return;
  if (last_file)
    free (last_file);
  last_file = strdup (filename);
  assert (last_file);
#if 0
  /* This is bogus -- bow_fopen will use bow_error if the open fails
     which in turn will call abort(3), which we MUST NOT DO. */
  fp = bow_fopen (filename, "r");
#else
  if ((fp = fopen (filename, "r")))
#endif /* 0 */
  bow_words_read_from_fp (fp);
  fclose (fp);
}


/* Modify the int/word mapping by removing all words that occurred 
   less than OCCUR number of times.  WARNING: This totally changes
   the word/int mapping; any WV's, WI2DVF's or BARREL's you build
   with the old mapping will have bogus WI's afterward. */
void
bow_words_remove_occurrences_less_than (int occur)
{
  bow_int4str *new_map;
  int wi;
  int max_wi;

  if (word_map == NULL)
    {
      bow_verbosify (bow_quiet,
		     "%s: Trying to remove words from an empty word map\n",
		     __FUNCTION__);
      return;
    }
  max_wi = word_map->str_array_length;
  new_map = bow_int4str_new (0);
  for (wi = 0; wi < max_wi; wi++)
    {
      /* If there are enough occurrences, add it to the new map. */
      if (word_map_counts[wi] >= occur)
	bow_str2int (new_map, bow_int2str (word_map, wi));
    }
  /* Replace the old map with the new map. */
  bow_words_set_map (new_map, 1);
}

/* Modify the int/word mapping by removing all words except the
   NUM_WORDS_TO_KEEP number of words that have the top information
   gain. */
void
bow_words_keep_top_by_infogain (int num_words_to_keep, 
				bow_barrel *barrel, int num_classes)
{
  float *wi2ig;
  int wi2ig_size;
  bow_int4str *new_map;
  int wi;
  struct wiig_list_entry {
    float ig;
    int wi;
  } *wiig_list;
  /* For sorting the above entries. */
  int compare_wiig_list_entry (const void *e1, const void *e2)
    {
      if (((struct wiig_list_entry*)e1)->ig >
	  ((struct wiig_list_entry*)e2)->ig)
	return -1;
      else if (((struct wiig_list_entry*)e1)->ig ==
	  ((struct wiig_list_entry*)e2)->ig)
	return 0;
      else return 1;
    }
  
  new_map = bow_int4str_new (0);
  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  /* Make a list of the info gain numbers paired with their WI's,
     in prepartion for sorting. */
  wiig_list = alloca (sizeof (struct wiig_list_entry) * wi2ig_size);
  for (wi = 0; wi < wi2ig_size; wi++)
    {
      wiig_list[wi].wi = wi;
      wiig_list[wi].ig = wi2ig[wi];
    }
  /* Sort the list */
  qsort (wiig_list, wi2ig_size, sizeof (struct wiig_list_entry), 
	 compare_wiig_list_entry);


  if (num_words_to_keep > wi2ig_size || num_words_to_keep <= 0)
    num_words_to_keep = wi2ig_size;

  /* Add NUM_WORDS_TO_KEEP words to the new vocabulary. */
  for (wi = 0; wi < num_words_to_keep; wi++)
    if (bow_wi2dvf_dv (barrel->wi2dvf, wiig_list[wi].wi))
      bow_str2int (new_map, bow_int2word (wiig_list[wi].wi));

  /* Replace the old map with the new map. */
  bow_words_set_map (new_map, 1);
  bow_free (wi2ig);
}

/* Add to the word occurrence counts reading all entries in HDB
   database DIRNAME and parsing all the text files; skip any files
   matching EXCEPTION_NAME. */
int
bow_words_add_occurrences_from_hdb (const char *dirname,
				    const char *exception_name)
{
  int text_document_count = 0;
  int total_word_count = 0;
  char *filename, *data;

  /* Open HDB database */
  if (! hdb_open ((char *) dirname, 0))
    {
      bow_error ("bow_words_add_occurrences_from_hdb: Not able to open %s"
		 " as an HDB\n  database\n", dirname);
    }
  bow_verbosify (bow_progress,
		 "Counting words... files : unique-words :: "
		 "                 ");
  /* Loop through all filename/data pairs in the database */
  while (hdb_each (&filename, &data, 0))
    {
      char word[BOW_MAX_WORD_LENGTH];
      int wi;
      bow_lex lex;

      /* If the filename matches the exception name, return immediately. */
      if (exception_name && !strcmp (filename, exception_name))
	continue;

      if (bow_str_is_text (data))
	{
	  lex.document = data;
	  lex.document_length = strlen (data);
	  lex.document_position = 0;
	  
	  /* Loop once for each lexical token in this document. */
	  while (bow_default_lexer->get_word (bow_default_lexer, 
					      &lex, word, 
					      BOW_MAX_WORD_LENGTH))
	    {
	      /* Increment the word's occurrence count. */
	      wi = bow_word2int_add_occurrence (word);
	      if (wi < 0)
		continue;
	      /* Increment total word count */
	      total_word_count++;
	    }
	  text_document_count++;
	  if (text_document_count % 2 == 0)
		bow_verbosify (bow_progress,
			       "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			       "%6d : %6d", 
			       text_document_count, bow_num_words ());
	}
      free (filename);
      free (data);
    }

  hdb_close ();
  bow_verbosify (bow_progress, "\n");
  return total_word_count;
}

/* Add to the word occurrence counts by recursively decending directory 
   DIRNAME and parsing all the text files; skip any files matching
   EXCEPTION_NAME. */
int
bow_words_add_occurrences_from_text_dir (const char *dirname,
					 const char *exception_name)
{
  int text_document_count = 0;
  int total_word_count = 0;
  int words_index_file (const char *filename, void *context)
    {
      FILE *fp;
      char word[BOW_MAX_WORD_LENGTH];
      int wi;
      bow_lex *lex;

      /* If the filename matches the exception name, return immediately. */
      if (exception_name && !strcmp (filename, exception_name))
	return 0;

      fp = bow_fopen (filename, "r");
      if (bow_fp_is_text (fp))
	{
	  /* Loop once for each document in this file. */
	  while ((lex = bow_default_lexer->open_text_fp
		  (bow_default_lexer, fp, filename)))
	    {
	      /* Loop once for each lexical token in this document. */
	      while (bow_default_lexer->get_word (bow_default_lexer, 
						  lex, word, 
						  BOW_MAX_WORD_LENGTH))
		{
		  /* Increment the word's occurrence count. */
		  wi = bow_word2int_add_occurrence (word);
		  if (wi < 0)
		    continue;
		  /* Increment total word count */
		  total_word_count++;
		}
	      bow_default_lexer->close (bow_default_lexer, lex);
	      text_document_count++;
	      if (text_document_count % 2 == 0)
		bow_verbosify (bow_progress,
			       "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			       "%6d : %6d", 
			       text_document_count, bow_num_words ());
	    }
	}
      fclose (fp);
      return 0;
    }

  bow_verbosify (bow_progress,
		 "Counting words... files : unique-words :: "
		 "                 ");
  bow_map_filenames_from_dir (words_index_file, 0, dirname, exception_name);
  bow_verbosify (bow_progress, "\n");
  return total_word_count;
}
