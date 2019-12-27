/* Word vectors. */

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
#include <stdlib.h>		/* for qsort() */

static int
compare_ints (const void *int1, const void *int2)
{
  return *(int*)int1 - *(int*)int2;
}

/* Create and return a new "word vector" with uninitialized contents. */
bow_wv *
bow_wv_new (int capacity)
{
  bow_wv *ret;

  ret = bow_malloc (sizeof (bow_wv) + sizeof (bow_we) * capacity);
  ret->num_entries = capacity;
  ret->normalizer = 1;
  return ret;
}

/* Create and return a new "word vector" from a document buffer LEX. */
bow_wv *
bow_wv_new_from_lex (bow_lex *lex)
{
  int i, j;
  char word[BOW_MAX_WORD_LENGTH]; /* buffer for reading and stemming words */
  int wi;			/* a word index */
  int *wi_array;		/* all words in document, including repeats */
  int wi_array_length = 0;
  int wi_array_size = 1024;
  int num_unique_wi;		/* the number of different words in document */
  int prev_wi;			/* used when counting number of diff words */
  bow_wv *wv;			/* the word vector this function will return */

  wi_array = bow_malloc (wi_array_size * sizeof (int));

  /* Read words from the file, stem them, get their word index, and
     append each of them to `wi_array'. */
  while (bow_default_lexer->get_word (bow_default_lexer,
				      lex, word, BOW_MAX_WORD_LENGTH))
    {
      wi = bow_word2int_add_occurrence (word);
      if (wi < 0)
	continue;
      if (wi_array_length == wi_array_size-1)
	{
	  /* wi_array needs to grow in order to hold more word indices. */
	  wi_array_size *= 2;
	  wi_array = bow_realloc (wi_array, wi_array_size * sizeof (int));
	}
      wi_array[wi_array_length++] = wi;
    }

  /* If we didn't get any words from the file, return NULL. */
  if (wi_array_length == 0)
    {
      bow_free (wi_array);
      return NULL;
    }

  /* Sort the array of word indices. */
  qsort (wi_array, wi_array_length, sizeof (int), compare_ints);

  /* Find out how many of them are unique, (i.e. determine the correct
     value for NUM_UNIQUE_WI) so that we know how much space to allocate
     for the wv->entries. */
  for (num_unique_wi = 0, prev_wi = -1, i = 0; i < wi_array_length; i++)
    {
      if (wi_array[i] != prev_wi)
	{
	  num_unique_wi++;
	  prev_wi = wi_array[i];
	}
    }

  /* Allocate memory for the word vector we're creating. */
  wv = bow_malloc (sizeof (bow_wv) + sizeof (bow_we) * num_unique_wi);

  /* Fill in the word vector entries from WI_ARRAY. */
  wv->num_entries = num_unique_wi;
  for (i = 0, j = -1, prev_wi = -1; i < wi_array_length; i++)
    {
      if (wi_array[i] != prev_wi)
	{
	  j++;
	  wv->entry[j].wi = wi_array[i];
	  prev_wi = wi_array[i];
	  wv->entry[j].count = 1;
	}
      else
	{
	  (wv->entry[j].count)++;
	}
    }
  assert (j+1 == num_unique_wi);
  bow_free (wi_array);
  /* Initialize to a standard value. */
  wv->normalizer = 1;

  return wv;
}

bow_wv *
bow_wv_new_from_text_fp (FILE *fp)
{
  bow_wv *ret;			/* the word vector this function will return */
  bow_lex *lex;

  /* NOTE: This will read just the first document from the file. */
  lex = bow_default_lexer->open_text_fp (bow_default_lexer, fp);
  if (lex == NULL)
    return NULL;
  ret = bow_wv_new_from_lex (lex);
  bow_default_lexer->close (bow_default_lexer, lex);
  return ret;
}

bow_wv *
bow_wv_new_from_text_string (char *the_string)
{
  bow_wv *ret;	 /* the word vector this function will return */
  bow_lex *lex;
  
  if(!the_string || !*the_string)
    return NULL;
  lex = (bow_lex *) bow_malloc (sizeof (bow_lex));
  if (lex == NULL)
    return NULL;
  lex->document = strdup (the_string);
  assert (lex->document);
  lex->document_length = strlen (the_string);
  lex->document_position = 0;
  ret = bow_wv_new_from_lex (lex);
  free (lex->document);
  free (lex);
  return ret;
}

/* Return a pointer to the "word entry" with index WI in "word vector WV */
bow_we *
bow_wv_entry_for_wi (bow_wv *wv, int wi)
{
  int i;

  if (!wv)
    return NULL;
  for (i = 0; i < wv->num_entries; i++)
    {
      if (wv->entry[i].wi == wi)
	return &(wv->entry[i]);
      else if (wv->entry[i].wi > wi)
	return NULL;
    }
  return NULL;
}

/* Return the count entry of "word" with index WI in "word vector" WV */
int
bow_wv_count_for_wi (bow_wv *wv, int wi)
{
  bow_we *we;

  we = bow_wv_entry_for_wi (wv, wi);
  if (we)
    return we->count;
  return 0;
}

/* Return the number of bytes required for writing the "word vector" WV. */
int
bow_wv_write_size (bow_wv *wv)
{
  if (wv == NULL)
    return sizeof (int);
  return (sizeof (int)		/* for NUM_ENTRIES */
	  + sizeof (float)	/* for NORM */
	  + (wv->num_entries	/* for the entries */
	     * (sizeof (int)	/* for WI */
		+ sizeof (int)	/* for COUNT */
		+ sizeof (float)))); /* for WEIGHT */
}

/* Write "word vector" DV to the stream FP. */
void
bow_wv_write (bow_wv *wv, FILE *fp)
{
  int i;

  if (wv == NULL)
    {
      bow_fwrite_int (0, fp);
      return;
    }

  bow_fwrite_int (wv->num_entries, fp);
  bow_fwrite_float (wv->normalizer, fp);
  for (i = 0; i < wv->num_entries; i++)
    {
      bow_fwrite_int (wv->entry[i].wi, fp);
      bow_fwrite_int (wv->entry[i].count, fp);
      bow_fwrite_float (wv->entry[i].weight, fp);
    }
}

/* Return a new "word vector" read from a pointer into a data file, FP. */
bow_wv *
bow_wv_new_from_data_fp (FILE *fp)
{
  int i;
  int len;
  bow_wv *ret;

  bow_fread_int (&len, fp);
  
  if (len == 0)
    return NULL;

  ret = bow_wv_new (len);
  bow_fread_float (&(ret->normalizer), fp);

  for (i = 0; i < len; i++)
    {
      bow_fread_int (&(ret->entry[i].wi), fp);
      bow_fread_int (&(ret->entry[i].count), fp);
      bow_fread_float (&(ret->entry[i].weight), fp);
    }
  return ret;
}

void
bow_wv_free (bow_wv *wv)
{
  bow_free (wv);
}

/* Print "word vector" WV on stream FP in a human-readable format. */
void
bow_wv_fprintf (FILE *fp, bow_wv *wv)
{
  int i;

  for (i = 0; i < wv->num_entries; i++)
    fprintf (fp, "%4d %30s %4d\n",
	     wv->entry[i].wi,
	     bow_int2word (wv->entry[i].wi),
	     wv->entry[i].count);
}

/* 
   Print "word vector" WV to a string in human readable format.
   Set max_size_for_string to 0 if you don't want to
   limit it.
*/
char *
bow_wv_sprintf (bow_wv *wv, unsigned int max_size_for_string)
{
  int i;
  int wami=0;
  char *ret_string = NULL, *tmp_string=NULL;
  unsigned int nentries=0;
  if(!wv)
    return NULL;
  assert((max_size_for_string >= 10) /* || 
	 (max_size_for_string == 0)  */ );
  nentries = wv->num_entries;
  ret_string = malloc(max_size_for_string+1);
  assert(ret_string);
  tmp_string = ret_string;
  for (i = 0; i < nentries; i++)
  {
    sprintf (tmp_string, "%d %d %n",
	     wv->entry[i].wi, wv->entry[i].count, &wami);
    tmp_string += wami;
    /* Ensure we haven't stepped off the end of the output string.
       Allow for some slop at the end. */
    if (tmp_string - ret_string > max_size_for_string - 20)
	break;
  }
  return ret_string;
}

/* 
   Print "word vector" WV to a string in human readable format.
   Set max_size_for_string to 0 if you don't want to
   limit it.
*/
char *
bow_wv_sprintf_words (bow_wv *wv, unsigned int max_size_for_string)
{
  int i;
  int wami=0;
  char *ret_string = NULL, *tmp_string=NULL;
  unsigned int nentries=0;
  if(!wv)
    return NULL;
  assert((max_size_for_string >= 10) /* || 
	 (max_size_for_string == 0) */ );
  nentries = wv->num_entries;
  ret_string = malloc(max_size_for_string+1);
  assert(ret_string);
  tmp_string = ret_string;
  for (i = 0; i < nentries; i++)
  {
    sprintf (tmp_string, "%s %d %n",
	     bow_int2word(wv->entry[i].wi), wv->entry[i].count, &wami);
    tmp_string += wami;
    /* Ensure we haven't stepped off the end of the output string.
       Allow for some slop at the end. */
    if (tmp_string - ret_string > max_size_for_string - 20)
	break;
  }
  return ret_string;
}

/* Assign the values of the "word vector entry's" WEIGHT field
   equal to the COUNT. */
void
bow_wv_set_weights_to_count (bow_wv *wv)
{
  int wvi;

  for (wvi = 0; wvi < wv->num_entries; wvi++)
    wv->entry[wvi].weight = wv->entry[wvi].count;
}

/* Assign the values of the "word vector entry's" WEIGHT field
   equal to the COUNT times the word's IDF, taken from the BARREL. */
void
bow_wv_set_weights_to_count_times_idf (bow_wv *wv, bow_barrel *barrel)
{
  int wvi;
  int wi;
  bow_dv *dv;

  for (wvi = 0; wvi < wv->num_entries; wvi++)
    {
      wi = wv->entry[wvi].wi;
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv)
	{
	  wv->entry[wvi].weight = 
	    (wv->entry[wvi].count * dv->idf);
	}
      else
	{
	  /* This word was not part of the model at all. */
	  wv->entry[wvi].weight = 0;
	}
    }
}
