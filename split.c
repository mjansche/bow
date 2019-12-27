/* Splitting the documents into training and test sets. */

/* Copyright (C) 1997 Andrew McCallum

   Written by:  Sean Slattery <jslttery@cs.cmu.edu>

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
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef RAND_MAX
#define RAND_MAX INT_MAX
#endif 

/* This takes a bow_array of bow_cdoc's and first sets them all to be in the
   model. It then randomly choses 'no_test' bow_cdoc's to be in the test set
   and sets their type to be test. */
void
bow_test_split (bow_barrel *barrel, int num_test)
{
  bow_array *cdocs = barrel->cdocs;
  int i, j, index;
  bow_cdoc *doc = NULL;
  int max_ci;
  /* All the below include only the test/model docs, not the ignore docs.*/
  int *num_docs_per_class;
  int *num_test_docs_allowed_per_class;
  int total_num_docs;

  /* assert (num_test < cdocs->length * 0.9); */

  /* Seed the random number generator */
  {
    struct timeval tv;
    struct timezone tz;
    long seed;

    gettimeofday (&tv, &tz);
    seed = tv.tv_usec;
    srand(seed);
  }

  /* First reset every test cdoc to be in the model */
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      /* Be sure to preserve `ignore' labels on documents. */
      if (doc->type == test)
	doc->type = model;
    }
  
  /* Find out the largest class index. */
  max_ci = 0;
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      if (doc->class > max_ci)
	max_ci = doc->class;
    }
  /* Make it not actually the `max', but appropriate for an array length */
  max_ci++;

  /* Initialize class/doc counts to zero. */
  num_docs_per_class = alloca (max_ci * sizeof (int));
  num_test_docs_allowed_per_class = alloca (max_ci * sizeof (int));
  for (i = 0; i < max_ci; i++)
    {
      num_docs_per_class[i] = 0;
      num_test_docs_allowed_per_class[i] = 0;
    }
  total_num_docs = 0;

  /* Count the number of documents in each class. */
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      if (doc->type == model)
	{
	  (num_docs_per_class[doc->class])++;
	  total_num_docs++;
	}
    }

  /* Calculate the number of test docments allowed for each class. 
     The +2 gives us some room for slop and round-off-error*/
  for (i = 0; i < max_ci; i++)
    num_test_docs_allowed_per_class[i] = 
      (((float)num_docs_per_class[i] / total_num_docs) * num_test) + 2;

  /* Print the number of documents in each class. */
  fprintf (stderr, "Number of docs per class: ");
  for (i = 0; i < max_ci; i++)
    fprintf (stderr, "%d:%d ",
	     num_docs_per_class[i],
	     num_test_docs_allowed_per_class[i]);
  fprintf (stderr, "\n");

  /* Now loop until we have created a test set of size num_test */
  for (i = 0, j = 0; i < num_test; j++)
    {
      index = rand() % cdocs->length;

      doc = bow_cdocs_di2doc (cdocs, index);
      assert (doc);
      if (doc->type == model
	  && num_test_docs_allowed_per_class[doc->class] > 0)
	{
	  doc->type = test;
	  i++;
	  num_test_docs_allowed_per_class[doc->class]--;
	}
      if (j > cdocs->length * 1000)
	bow_error ("Random number generator could not find enough "
		   "model document indices with balanced classes");
    }

  /* All done */
}

/* This takes a bow_array of bow_cdoc's and first sets them all to be
   in the test. It then randomly choses 'num_test' bow_cdoc's per
   class to be in the model set and sets their type to be model. */
void
bow_test_split2 (bow_barrel *barrel, int num_test)
{
  bow_array *cdocs = barrel->cdocs;
  int i, j, index;
  bow_cdoc *doc = NULL;
  int max_ci;
  /* All the below include only the test/model docs, not the ignore docs.*/
  int *num_docs_per_class;
  int *num_test_docs_allowed_per_class;
  int total_num_docs;

  /* assert (num_test < cdocs->length * 0.9); */

  /* Seed the random number generator */
  {
    struct timeval tv;
    struct timezone tz;
    long seed;

    gettimeofday (&tv, &tz);
    seed = tv.tv_usec;
    srand(seed);
  }

  /* First reset every model cdoc to be in the test */
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      /* Be sure to preserve `ignore' labels on documents. */
      if (doc->type == model)
	doc->type = test;
    }
  
  /* Find out the largest class index. */
  max_ci = 0;
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      if (doc->class > max_ci)
	max_ci = doc->class;
    }
  /* Make it not actually the `max', but appropriate for an array length */
  max_ci++;

  /* Initialize class/doc counts to zero. */
  num_docs_per_class = alloca (max_ci * sizeof (int));
  num_test_docs_allowed_per_class = alloca (max_ci * sizeof (int));
  for (i = 0; i < max_ci; i++)
    {
      num_docs_per_class[i] = 0;
      num_test_docs_allowed_per_class[i] = 0;
    }
  total_num_docs = 0;

  /* Count the number of documents in each class. */
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      if (doc->type == model)
	{
	  (num_docs_per_class[doc->class])++;
	  total_num_docs++;
	}
    }

  /* Calculate the number of model docments allowed for each class. 
     The +2 gives us some room for slop and round-off-error*/
  for (i = 0; i < max_ci; i++)
    num_test_docs_allowed_per_class[i] = num_test;


  /* Print the number of documents in each class. */
  fprintf (stderr, "Number of docs per class: ");
  for (i = 0; i < max_ci; i++)
    fprintf (stderr, "%d:%d ",
	     num_docs_per_class[i],
	     num_test_docs_allowed_per_class[i]);
  fprintf (stderr, "\n");

  /* Now loop until we have created a test set of size num_test */
  for (i = 0, j = 0; i < num_test * max_ci; j++)
    {
      index = rand() % cdocs->length;

      doc = bow_cdocs_di2doc (cdocs, index);
      assert (doc);
      if (doc->type == test
	  && num_test_docs_allowed_per_class[doc->class] > 0
	  && doc->word_count > 0)
	{
	  doc->type = model;
	  i++;
	  num_test_docs_allowed_per_class[doc->class]--;
	}
      if (j > cdocs->length * 1000)
	bow_error ("Random number generator could not find enough "
		   "model document indices with balanced classes");
    }

  /* All done */
}

#define BASENAME_ONLY 1

#if BASENAME_ONLY
static inline const char *
bow_basename (const char *str)
{
  const char *b;
  b = strrchr (str, '/');
  if (!b)
    b = str;
  return b;
}
#endif /* BASENAME_ONLY */

/* Set all the cdoc's named in TEST_FILES_FILENAME to type test, and
   all the other (non-ignored) cdoc's to model.  BARREL should be a
   doc barrel, not a class barrel. */
void
bow_test_set_files (bow_barrel *barrel, const char *test_files_filename)
{
  bow_int4str *map;
  bow_cdoc *cdoc;
  int di;
  int test_files_count = 0;
  int model_files_count = 0;
  const char *filename;

  map = bow_int4str_new_from_string_file (test_files_filename);
#if BASENAME_ONLY
  {
    /* Convert the filename strings in map to only the basenames of
       the files. */
    int si, index;
    bow_int4str *map2 = bow_int4str_new (0);
    for (si = 0; si < map->str_array_length; si++)
      {
	index = bow_str2int_no_add (map2, bow_basename(bow_int2str (map, si)));
	if (index != -1)
	  bow_verbosify (bow_quiet, "WARNING: Repeated file basename `%s'\n", 
			 bow_int2str (map, si));
	bow_str2int (map2, bow_basename (bow_int2str (map, si)));
      }
    bow_int4str_free (map);
    map = map2;
  }  
#endif /*BASENAME_ONLY */
  
  for (di = 0; di < barrel->cdocs->length; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      if (cdoc->type == ignore)
	continue;
#if BASENAME_ONLY
      filename = bow_basename (cdoc->filename);
#else
      filename = cdoc->filename;
#endif /* BASENAME_ONLY */
      if (bow_str2int_no_add (map, filename) == -1)
	{
	  /* This filename is not in the map; this cdoc is in the model. */
	  cdoc->type = model;
	  model_files_count++;
	}
      else
	{
	  /* This filename is in the map; this cdoc is in the test set. */
	  cdoc->type = test;
	  test_files_count++;
	}
    }

  bow_verbosify (bow_progress, 
		 "Using %s, placed %d documents in the test set, "
		 "%d in model set.\n", 
		 test_files_filename, test_files_count, model_files_count);
  bow_int4str_free (map);
  return;
}

/* This function sets up the data structure so we can step through the word
   vectors for each test document easily. */
bow_dv_heap *
bow_test_new_heap (bow_barrel *barrel)
{
  return bow_make_dv_heap_from_wi2dvf (barrel->wi2dvf);
}

/* We only need this struct within the following function. */
typedef struct _bow_tmp_word_struct {
  int wi;
  int count;
} bow_tmp_word_struct;

/* This function takes the heap returned by bow_initialise_test_set and
   creates a word vector corresponding to the next document in the test set.
   The index of the test document is returned. If the test set is empty, 0
   is returned and *wv == NULL. Also, when the test set is exhausted, the
   heap is free'd (since it can't be used for anything else anways.
   This can't really deal with vectors which are all zero, since they
   are not represented explicitly in our data structure. Not sure what
   we should/can do. */
int
bow_test_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv)
{
  bow_array *cdocs = barrel->cdocs;
  bow_array *word_array;
  bow_cdoc *doc;
  bow_tmp_word_struct word, *wptr;
  int current_di = -1;
  int i;

  word_array = bow_array_new (50, sizeof(bow_tmp_word_struct), 0);

  /* Keep going until we exhaust the heap or we find a test document. */
  while ((heap->length > 0) && (word_array->length == 0))
    {
      current_di = heap->entry[0].current_di;
      doc = bow_cdocs_di2doc(cdocs, current_di);

      if (doc->type == test)
	{
	  /* We have the first entry for the next test document */
	  do
	    {
	      word.wi = heap->entry[0].wi;
	      word.count = 
		heap->entry[0].dv->entry[heap->entry[0].index].count;
	      bow_array_append (word_array, &word);
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
      else
	{
	  /* This is not a test document, go on to next document */
	  do
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
    }

  /* Here we either have a word_array or else we've run out of test
     documents. */

  if (word_array->length != 0)
    {
      if (*wv)
	bow_wv_free (*wv);

      /* We now have all the words for this test document in the word
	 array - need to create a bow_wv */
      (*wv) = bow_wv_new (word_array->length);
      for (i = 0; i < word_array->length; i++)
	{
	  wptr = bow_array_entry_at_index (word_array, i);
	  (*wv)->entry[i].wi = wptr->wi;
	  (*wv)->entry[i].count = wptr->count;
	  (*wv)->entry[i].weight = wptr->count;
	}
    }
  else
    {
      /* Since we've run out of docs, might as well free the test set. */
      bow_free (heap);
      if (*wv)
	bow_wv_free (*wv);
      current_di = -1;
      (*wv) = NULL;
    }
  /* Should be finished with the word array now */

#if 1
  /* XXX this causes a seg fault - don't know why. */
  bow_array_free (word_array);
#else
  /* This does the same job for me. */
  bow_free (word_array->entries);
  bow_free (word_array);
#endif

  return current_di;
}

typedef struct _bow_tmp_word_struct2 {
  int wi;
  int count;
} bow_tmp_word_struct2;

/* like bow_test_next_wv, but for type==model instead of type==test */
int
bow_model_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv)
{
  bow_array *cdocs = barrel->cdocs;
  bow_array *word_array;
  bow_cdoc *doc;
  bow_tmp_word_struct2 word, *wptr;
  int current_di = -1;
  int i;

  word_array = bow_array_new (50, sizeof(bow_tmp_word_struct2), 0);

  /* Keep going until we exhaust the heap or we find a non-test document. */
  while ((heap->length > 0) && (word_array->length == 0))
    {
      current_di = heap->entry[0].current_di;
      doc = bow_cdocs_di2doc(cdocs, current_di);

      if (doc->type == model)
	{
	 if (doc->type != model && doc->type != ignore && doc->type != ignored_model)
	    fprintf(stderr, "\nWARNING: Around line %d of %s. Unanticipated.\n\n",
			__LINE__, __FILE__);
		
	  /* We have the first entry for the next model document */
	  do
	    {
	      word.wi = heap->entry[0].wi;
	      word.count = 
		heap->entry[0].dv->entry[heap->entry[0].index].count;
	      bow_array_append (word_array, &word);
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
      else
	{
	  /* This is not a model document, go on to next document */
	  do
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
    }

  /* Here we either have a word_array or else we've run out of non-test
     documents. */

  if (word_array->length != 0)
    {
      if (*wv)
	bow_wv_free (*wv);

      /* We now have all the words for this non-test document in the word
	 array - need to create a bow_wv */
      (*wv) = bow_wv_new (word_array->length);
      for (i = 0; i < word_array->length; i++)
	{
	  wptr = bow_array_entry_at_index (word_array, i);
	  (*wv)->entry[i].wi = wptr->wi;
	  (*wv)->entry[i].count = wptr->count;
	  (*wv)->entry[i].weight = wptr->count;
	}
    }
  else
    {
      /* Since we've run out of docs, might as well free the non-test set. */
      bow_free (heap);
      if (*wv)
	bow_wv_free (*wv);
      current_di = -1;
      (*wv) = NULL;
    }
  /* Should be finished with the word array now */

#if 1
  /* XXX this causes a seg fault - don't know why. */
  bow_array_free (word_array);
#else
  /* This does the same job for me. */
  bow_free (word_array->entries);
  bow_free (word_array);
#endif

  return current_di;
}


/* Like bow_test_next_wv, but for type!=test instead of type==test */
int
bow_nontest_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv)
{
  bow_array *cdocs = barrel->cdocs;
  bow_array *word_array;
  bow_cdoc *doc;
  bow_tmp_word_struct2 word, *wptr;
  int current_di = -1;
  int i;

  word_array = bow_array_new (50, sizeof(bow_tmp_word_struct2), 0);

  /* Keep going until we exhaust the heap or we find a non-test document. */
  while ((heap->length > 0) && (word_array->length == 0))
    {
      current_di = heap->entry[0].current_di;
      doc = bow_cdocs_di2doc(cdocs, current_di);

      if (doc->type != test)
	{
	 if (doc->type != model && doc->type != ignore && doc->type != ignored_model)
	    fprintf(stderr, "\nWARNING: Around line %d of %s. Unanticipated.\n\n",
			__LINE__, __FILE__);
		
	  /* We have the first entry for the next non-test document */
	  do
	    {
	      word.wi = heap->entry[0].wi;
	      word.count = 
		heap->entry[0].dv->entry[heap->entry[0].index].count;
	      bow_array_append (word_array, &word);
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
      else
	{
	  /* This is not a non-test document, go on to next document */
	  do
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
    }

  /* Here we either have a word_array or else we've run out of non-test
     documents. */

  if (word_array->length != 0)
    {
      if (*wv)
	bow_wv_free (*wv);

      /* We now have all the words for this non-test document in the word
	 array - need to create a bow_wv */
      (*wv) = bow_wv_new (word_array->length);
      for (i = 0; i < word_array->length; i++)
	{
	  wptr = bow_array_entry_at_index (word_array, i);
	  (*wv)->entry[i].wi = wptr->wi;
	  (*wv)->entry[i].count = wptr->count;
	  (*wv)->entry[i].weight = wptr->count;
	}
    }
  else
    {
      /* Since we've run out of docs, might as well free the non-test set. */
      bow_free (heap);
      if (*wv)
	bow_wv_free (*wv);
      current_di = -1;
      (*wv) = NULL;
    }
  /* Should be finished with the word array now */

#if 1
  /* XXX this causes a seg fault - don't know why. */
  bow_array_free (word_array);
#else
  /* This does the same job for me. */
  bow_free (word_array->entries);
  bow_free (word_array);
#endif

  return current_di;
}

/* like bow_test_next_wv, but for type==ignore instead of type==test */
int
bow_ignore_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv)
{
  bow_array *cdocs = barrel->cdocs;
  bow_array *word_array;
  bow_cdoc *doc;
  bow_tmp_word_struct2 word, *wptr;
  int current_di = -1;
  int i;

  word_array = bow_array_new (50, sizeof(bow_tmp_word_struct2), 0);

  /* Keep going until we exhaust the heap or we find a non-test document. */
  while ((heap->length > 0) && (word_array->length == 0))
    {
      current_di = heap->entry[0].current_di;
      doc = bow_cdocs_di2doc(cdocs, current_di);

      if (doc->type == ignore)
	{
		
	  /* We have the first entry for the next ignore document */
	  do
	    {
	      word.wi = heap->entry[0].wi;
	      word.count = 
		heap->entry[0].dv->entry[heap->entry[0].index].count;
	      bow_array_append (word_array, &word);
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
      else
	{
	  /* This is not an ignore document, go on to next document */
	  do
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
    }

  /* Here we either have a word_array or else we've run out of ignore
     documents. */

  if (word_array->length != 0)
    {
      if (*wv)
	bow_wv_free (*wv);

      /* We now have all the words for this non-test document in the word
	 array - need to create a bow_wv */
      (*wv) = bow_wv_new (word_array->length);
      for (i = 0; i < word_array->length; i++)
	{
	  wptr = bow_array_entry_at_index (word_array, i);
	  (*wv)->entry[i].wi = wptr->wi;
	  (*wv)->entry[i].count = wptr->count;
	  (*wv)->entry[i].weight = wptr->count;
	}
    }
  else
    {
      /* Since we've run out of docs, might as well free the non-test set. */
      bow_free (heap);
      if (*wv)
	bow_wv_free (*wv);
      current_di = -1;
      (*wv) = NULL;
    }
  /* Should be finished with the word array now */

#if 1
  /* XXX this causes a seg fault - don't know why. */
  bow_array_free (word_array);
#else
  /* This does the same job for me. */
  bow_free (word_array->entries);
  bow_free (word_array);
#endif

  return current_di;
}

/* like bow_test_next_wv, but for type==ignored_model instead of type==test */
int
bow_ignored_model_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv)
{
  bow_array *cdocs = barrel->cdocs;
  bow_array *word_array;
  bow_cdoc *doc;
  bow_tmp_word_struct2 word, *wptr;
  int current_di = -1;
  int i;

  word_array = bow_array_new (50, sizeof(bow_tmp_word_struct2), 0);

  /* Keep going until we exhaust the heap or we find a non-test document. */
  while ((heap->length > 0) && (word_array->length == 0))
    {
      current_di = heap->entry[0].current_di;
      doc = bow_cdocs_di2doc(cdocs, current_di);

      if (doc->type == ignored_model)
	{
		
	  /* We have the first entry for the next ignored_model document */
	  do
	    {
	      word.wi = heap->entry[0].wi;
	      word.count = 
		heap->entry[0].dv->entry[heap->entry[0].index].count;
	      bow_array_append (word_array, &word);
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
      else
	{
	  /* This is not an ignored_model document, go on to next document */
	  do
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((heap->length > 0)
		 && (current_di == heap->entry[0].current_di));
	}
    }

  /* Here we either have a word_array or else we've run out of ignored_model
     documents. */

  if (word_array->length != 0)
    {
      if (*wv)
	bow_wv_free (*wv);

      /* We now have all the words for this non-test document in the word
	 array - need to create a bow_wv */
      (*wv) = bow_wv_new (word_array->length);
      for (i = 0; i < word_array->length; i++)
	{
	  wptr = bow_array_entry_at_index (word_array, i);
	  (*wv)->entry[i].wi = wptr->wi;
	  (*wv)->entry[i].count = wptr->count;
	  (*wv)->entry[i].weight = wptr->count;
	}
    }
  else
    {
      /* Since we've run out of docs, might as well free the non-test set. */
      bow_free (heap);
      if (*wv)
	bow_wv_free (*wv);
      current_di = -1;
      (*wv) = NULL;
    }
  /* Should be finished with the word array now */

#if 1
  /* XXX this causes a seg fault - don't know why. */
  bow_array_free (word_array);
#else
  /* This does the same job for me. */
  bow_free (word_array->entries);
  bow_free (word_array);
#endif

  return current_di;
}
