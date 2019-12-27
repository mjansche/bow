/* A convient interface to int4str.c, specifically for words. */

#include "libbow.h"
#include <assert.h>
#include <values.h>

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
bow_words_set_map (bow_int4str *map)
{
  int wi;
  if (word_map)
    {
      bow_int4str_free (word_map);
      assert (word_map_counts);
      for (wi = 0; wi < word_map_counts_size; wi++)
	word_map_counts[wi] = 0;
    }
  word_map = map;
}

const char *
bow_int2word (int index)
{
  if (!word_map)
    bow_error ("No words yet added to the int-word mapping.\n");
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
  assert (wi < word_map_counts_size);
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

  max_wi = word_map->str_array_length;
  new_map = bow_int4str_new (0);
  for (wi = 0; wi < max_wi; wi++)
    {
      /* If there are enough occurrences, add it to the new map. */
      if (word_map_counts[wi] >= occur)
	bow_str2int (new_map, bow_int2str (word_map, wi));
    }
  /* Replace the old map with the new map. */
  bow_words_set_map (new_map);
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
  float max_ig;
  int wi, max_ig_wi;

  new_map = bow_int4str_new (0);
  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  /* Add NUM_WORDS_TO_KEEP words to the new vocabulary. */
  while (num_words_to_keep--)
    {
      max_ig = -1.0f;
      /* Find the word with the highest info gain. */
      for (wi = 0; wi < wi2ig_size; wi++)
	{
	  assert (wi2ig[wi] > 0 || wi2ig[wi] == -MAXFLOAT);
	  if (wi2ig[wi] > max_ig)
	    {
	      max_ig = wi2ig[wi];
	      max_ig_wi = wi;
	    }
	}
      assert (max_ig > 0);
      /* Add the highest info gain word. */
      bow_str2int (new_map, bow_int2word (max_ig_wi));
      /* Punch WI's info gain to the ground so we can find the next highest. */
      wi2ig[max_ig_wi] = -MAXFLOAT;
    }
  /* Replace the old map with the new map. */
  bow_words_set_map (new_map);
  bow_free (wi2ig);
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
		  (bow_default_lexer, fp)))
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
