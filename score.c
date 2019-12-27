/* Finding the best matches for a query. */

#include "libbow.h"
#include <math.h>

/* Defined in naivebayes.c */
extern int
_bow_score_naivebayes_from_wv (bow_barrel *barrel, bow_wv *query_wv, 
			       bow_doc_score *scores, int best);

/* Defined in prind.c */
extern int
_bow_score_prind_from_wv (bow_barrel *barrel, bow_wv *query_wv, 
			    bow_doc_score *scores, int best);

/* Function to fill an array of the best matches to the document
   described by wv from the corpus in wi2dvf. There are 'best' elements
   in this array, in decreaseing order of their score. Nothing we can
   do if the we need more space to hold docs with the same scores as the
   final document in this list. The number of elements in the array is
   returned. The cdocs array is checked to make sure the
   document is in the model before getting it's vector product with
   the wv. Also, if the length field in cdocs is non-zero, then the
   product is divided by that length. */
int
bow_get_best_matches (bow_barrel *barrel, bow_wv *query_wv, 
		      bow_doc_score *scores, int best)
{
  bow_dv_heap *heap;
  bow_cdoc *doc;
  int num_scores = 0;		/* How many elements are in this array */
  int current_di, wi, current_index, i;
  double current_score = 0.0, target_weight;
  float idf;

  if (barrel->method == bow_method_naivebayes)
    return _bow_score_naivebayes_from_wv (barrel, query_wv,
					  scores, best);
  else if (barrel->method == bow_method_prind)
    return _bow_score_prind_from_wv (barrel, query_wv,
					  scores, best);

#if 0
  /* xxx Perhaps remove this and assume user has already done it. */
  /* Set weights and normalizer for the QUERY_WV */
  bow_wv_set_weights (query_wv, idf_type);
  bow_wv_set_weight_normalizer (query_wv, norm_type);
#endif
  if (query_wv->normalizer == 0)
    bow_error ("You forgot to set the weight normalizer of the QUERY_WV");

  /* Create the Heap of vectors of documents */
  heap = bow_make_dv_heap_from_wv (barrel->wi2dvf, query_wv);

  /* Keep going until the heap is emptied */
  while (heap->length > 0)
    {
      /* Get the index of the document we're currently working on */
      current_di = heap->entry[0].current_di;

      /* Get the document */
      doc = bow_cdocs_di2doc (barrel->cdocs, current_di);
    
      /* If it's not a model document, then move on to next one */
      if (doc->type != model)
	{
	  do 
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((current_di == heap->entry[0].current_di)
		 && (heap->length > 0));
	
	  /* Try again */
	  continue;
	}

      /* Reset the index into out word vector */
      current_index = 0;

      /* Reset the weight */
      current_score = 0.0;

      /* Loop over all the words in this document, summing up the score */
      do
	{
	  wi = heap->entry[0].wi;
	  target_weight = 
	    heap->entry[0].dv->entry[heap->entry[0].index].weight;

	  /* Find the correspoding word in our word vector */
	  while (wi > (query_wv->entry[current_index].wi))
	    current_index++;
	  assert (wi == query_wv->entry[current_index].wi);

	  /* Put in the contribution of this word */
	  /* xxx Under what conditions will IDF be zero?  Does the
	     right thing happen? */
	  idf = heap->entry[0].dv->idf;
	  assert (idf == idf);	/* testing for NaN */
	  assert (idf && idf > 0);
	  current_score += 
	    (target_weight
	     * (query_wv->entry[current_index].weight * idf));
	  /* We could also multiply by QUERY_WV->NORMALIZER here. */

	  /* A test to make sure we haven't got NaN. */
	  assert (current_score == current_score);

	  /* Now we need to update the heap - moving this element on to its
	     new position */
	  bow_dv_heap_update (heap);
	}
      while ((current_di == heap->entry[0].current_di)
	     && (heap->length > 0));

      /* It is OK to normalize here instead of inside do-while loop 
	 above because we are summing the weights, and we can just
	 factor out the NORMALIZER. */
      assert (doc->normalizer > 0);
      current_score *= doc->normalizer;

      assert (current_score == current_score); /* checking for NaN */

      /* Store the result in the SCORES array */
      /* If we haven't filled the list, or we beat the last item in the list */
      if ((num_scores < best)
	  || (scores[num_scores - 1].weight < current_score))
	{
	  /* We're going to search up the list comparing element i-1 with
	     our current score and moving it down the list if it's worse */
	  if (num_scores < best)
	    {
	      i = num_scores;
	      num_scores++;
	    }
	  else
	    i = num_scores - 1;

	  /* Shift down all the bits of the array that need shifting */
	  for (; (i > 0) && (scores[i - 1].weight < current_score); i--)
	    scores[i] = scores[i-1];

	  /* Insert our new score */
	  scores[i].weight = current_score;
	  scores[i].di = current_di;
	}
    }

  bow_free (heap);

  /* All done - return the number of elements we have */
  return num_scores;
}

int
bow_get_best_matches_euclidian(bow_barrel *barrel,
			       bow_wv *wv, bow_doc_score *scores, int best)
{
  bow_dv_heap *heap;
  bow_cdoc *doc;
  int num_scores = 0;   /* How many elements do we have in this array */
  int current_di, wi, current_index, i;
  float current_score = 0.0, target_weight;
  float idf;

  /* Create the Heap of vectors of documents */
  heap = bow_make_dv_heap_from_wv (barrel->wi2dvf, wv);

  /* Keep going until the heap is emptied */
  while (heap->length > 0)
    {
      /* Set the current document we're working on */
      current_di = heap->entry[0].current_di;

      /* Here we should check if this di is part of some training set
	 and move on if it isn't. */

      /* Get the document */
      doc = bow_cdocs_di2doc(barrel->cdocs, current_di);
    
      /* If it's not a model document, then move on to next one */
      if (doc->type != model)
	{
	  do 
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((current_di == heap->entry[0].current_di)
		 && (heap->length > 0));
	
	  /* Try again */
	  continue;
	}
    
      /* Reset the index into out word vector */
      current_index = 0;

      /* Reset the weight */
      current_score = 0.0;

      /* Loop summing up the score */
      do
	{
	  wi = heap->entry[0].wi;

	  idf = heap->entry[0].dv->idf;
	  target_weight = heap->entry[0].dv->entry[heap->entry[0].index].weight;

	  /* Find the correspoding word in our word vector */
	  while (wi > (wv->entry[current_index].wi))
	    {
	      current_score += 
		pow(((double)wv->entry[current_index].count * idf), 2);
	      current_index++;
	    }
	  assert (wi == wv->entry[current_index].wi);

	  /* Add in the product */
	  current_score += pow(target_weight
			       * ((float)wv->entry[current_index].count * idf),
			       2);

	  /* Now we need to update the heap - moving this element on to its
	     new position */
	  bow_dv_heap_update (heap);
	}
      while ((current_di == heap->entry[0].current_di)
	     && (heap->length > 0));

      /* Normalise if the length is non-zero */
      if (doc->normalizer != 0)
	current_score /= doc->normalizer;

      current_score = sqrt(current_score);

      /* Do something to store the result */
      /* If we haven't filled the list, or we beat the last item in the list */
      if ((num_scores < best) || (scores[num_scores - 1].weight < current_score))
	{
	  /* We're going to search up the list comparing element i-1 with
	     our current score and moving it down the list if it's worse */
	  if (num_scores < best)
	    {
	      i = num_scores;
	      num_scores++;
	    }
	  else
	    i = num_scores - 1;

	  /* Shift down all the bits of the array that need shifting */
	  for (; (i > 0) && (scores[i - 1].weight < current_score); i--)
	    scores[i] = scores[i-1];

	  /* Insert our new score */
	  scores[i].weight = current_score;
	  scores[i].di = current_di;
	}
    }

  /* All done - return the number of elements we have */
  return num_scores;
}
