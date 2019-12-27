/* Splitting the documents into training and test sets. */

#include "libbow.h"
#include <math.h>
#include <time.h>

/* Prototypes needed for SunOS */
#if __sun__
double drand48();
void srand48 (long);
#endif /* __sun__ */


/* This takes a bow_array of bow_cdoc's and first sets them all to be in the
   model. It then randomly choses 'no_test' bow_cdoc's to be in the test set
   and sets their type to be test. */
void
bow_test_split (bow_barrel *barrel, int no_test)
{
  bow_array *cdocs = barrel->cdocs;
  long seed;
  int i, j, k, index;
  bow_cdoc *doc;

  /* Seed the random number generator */
  seed = (long)time(NULL);
  srand48(seed);

  /* First reset every cdoc to be in the model */
  for (i = 0; i < cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (cdocs, i);
      doc->type = model;
    }
  
  /* Now loop until we have created a test set of size no_test */
  for (i = 0; i < no_test; i++)
    {
      index = floor(drand48() * (cdocs->length - i));

      j = -1;
      k = -1;
      while (j != index) 
	{
	  k++;
	  doc = bow_cdocs_di2doc (cdocs, k);
	  if (doc->type == model)
	    j++;
	}
      
      /* At this point doc points to the index'th model doc - we're
	 going to put this in the test set */
      doc->type = test;
    }

  /* All done */
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
  int current_di, i;

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

