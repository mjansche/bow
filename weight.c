/* Code to assign weights to each element in each vector of documents */

#include "libbow.h"
#include <math.h>

#if !HAVE_LOG2F
#define log2f log
#endif

#if !HAVE_SQRTF
#define sqrtf sqrt
#endif

/* Helper function for bow_assign_tfidf_weights().  Add COUNT to TOTAL
   in the proper way, dependant on TYPE. */
static inline void
_bow_add_to_idf (int *total, int count, bow_method method)
{
  switch (method) 
    {
    case bow_method_tfidf_words:
    case bow_method_tfidf_log_words:
    case bow_method_tfidf_prtfidf:
      (*total) += count;
      break;
    case bow_method_tfidf_log_occur:
      (*total)++;
      break;
    default:
      bow_error ("Bad bow_method, %d", method);
    }
}

/* Helper function for the bow_*_set_weight_normalizer() functions.
   Add WEIGHT to TOTAL in the proper way, dependant on TYPE. */
static inline void
_bow_add_to_normalizer_total (float *total, float weight, 
			      bow_method method)
{
  switch (method)
    {
    case bow_method_naivebayes:
    case bow_method_prind:
      /* We are normalizing so that weights in doc sum to 1 */
      *total += weight;
      break;
    case bow_method_tfidf_words:
    case bow_method_tfidf_log_words:
    case bow_method_tfidf_log_occur:
    case bow_method_tfidf_prtfidf:
      /* We are normalizing so that weights in doc have vector length 1 */
      *total += weight * weight;
      break;
    default:
      bow_error ("Bad bow_method %d", method);
    }
}

/* Helper function for the bow_*_set_weight_normalizer() functions.
   Change TOTAL in the proper way, dependant on TYPE. */
static inline float
_bow_total_to_normalizer (float total, bow_method method)
{
  switch (method)
    {
    case bow_method_naivebayes:
    case bow_method_prind:
      /* We are normalizing so that weights in doc sum to 1 */
      return 1.0 / total;
    case bow_method_tfidf_words:
    case bow_method_tfidf_log_words:
    case bow_method_tfidf_log_occur:
    case bow_method_tfidf_prtfidf:
      /* We are normalizing so that weights in doc have vector length 1 */
      return 1.0 / sqrtf (total);
    default:
      bow_error ("Bad bow_method %d", method);
    }
  return -1.0;			/* Not reached */
}



/* Function to assign TFIDF weights to each element of each document
   vector. */
static void
_bow_barrel_set_weights_with_idf (bow_barrel *barrel)
{
  int wi;			/* a "word index" into WI2DVF */
  int max_wi;			/* the highest "word index" in WI2DVF. */
  float idf;			/* The IDF factor for a word */
  bow_dv *dv;			/* the "document vector" at index WI */
  int df;			/* "document frequency" */
  int total_word_count;		/* total "document frequency" over all words */
  int dvi;			/* an index into the DV */
  bow_cdoc *cdoc;

  bow_verbosify (bow_progress, "Setting weights over words:          ");
  max_wi = MIN(barrel->wi2dvf->size, bow_num_words());

  /* For certain cases we need to loop over all dv's to compute the
     total number of word counts across all words and all documents. */
  /* xxx Shouldn't this be changed, and put in the `for(wi..' loop below? */
  if (barrel->method != bow_method_tfidf_log_occur)
    {
      total_word_count = 0;
      for (wi = 0; wi < max_wi; wi++) 
	{
	  dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
	  if (dv == NULL)
	    continue;
	  if (barrel->cdocs)
	    {
	      /* We have the document information, so we can determine which
		 documents are part the the training set. */
	      for (dvi = 0; dvi < dv->length; dvi++) 
		{
		  cdoc = bow_cdocs_di2doc (barrel->cdocs, dv->entry[dvi].di);
		  if (cdoc->type == model)
		    _bow_add_to_idf (&total_word_count, dv->entry[dvi].count, 
				     barrel->method);
		}
	    }
	  else
	    {
	      /* We don't have the document information, so we assume
		 all documents are part of the training set. */
	      for (dvi = 0; dvi < dv->length; dvi++) 
		_bow_add_to_idf (&total_word_count, dv->entry[dvi].count, 
				 barrel->method);
	    }
	}
    }

  /* Loop over all vectors of documents (i.e. each word), calculate
     the IDF, then set the weights */
  for (wi = 0; wi < max_wi; wi++) 
    {
      /* Get the document vector for this word WI */
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);

      if (dv == NULL)
	continue;

      /* Calculate the IDF, the "inverse document frequency". */
      df = 0;
      if (barrel->cdocs)
	{
	  /* We have the document information, so we can determine which
	     documents are part the the training set. */
	  for (dvi = 0; dvi < dv->length; dvi++)
	    {
	      cdoc = bow_cdocs_di2doc (barrel->cdocs, dv->entry[dvi].di);
	      if (cdoc->type == model)
		_bow_add_to_idf (&df, dv->entry[dvi].count, barrel->method);
	    }
	}
      else
	{
	  /* xxx Can we get rid of this case? -am */
	  /* We don't have the document information, so we assume
	     all documents are part of the training set. */
	  for (dvi = 0; dvi < dv->length; dvi++) 
	    _bow_add_to_idf (&df, dv->entry[dvi].count, barrel->method);
	}
      
      if (df == 0) 
	{
	  /* There are no training documents with this word - ignore */
	  idf = 0.0;
	}
      else
	{
	  /* BARREL->CDOCS->LENGTH is the total number of documents. */
	  switch (barrel->method)
	    {
	    case bow_method_tfidf_words:
	      idf = ((float)total_word_count / (float)df);
	      break;
	    case bow_method_tfidf_log_words:
	      idf = log2f ((float)total_word_count / (float)df);
	      break;
	    case bow_method_tfidf_log_occur:
	      idf = log2f ((float)barrel->cdocs->length / (float)df);
	      break;
	    case bow_method_tfidf_prtfidf:
	      idf = sqrtf ((float)total_word_count / (float)df);
	      break;
	    default:
	      bow_error ("Bad bow_method %d", barrel->method);
	    }
	}

      /* Now loop through all the elements, setting their weights */
      for (dvi = 0; dvi < dv->length; dvi++)
	dv->entry[dvi].weight = dv->entry[dvi].count * idf;
      
      /* Record this word's idf */
      assert (idf == idf);	/* Make sure we don't have NaN. */
      dv->idf = idf;

      if (wi % 10 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", max_wi - wi - 1); 
    }
  bow_verbosify (bow_progress, "\n");
}

extern void _bow_barrel_set_naivebayes_weights (bow_barrel *barrel);
extern void _bow_barrel_set_prind_weights (bow_barrel *barrel);

void
bow_barrel_set_weights (bow_barrel *barrel)
{
  if (barrel->method == bow_method_naivebayes)
    _bow_barrel_set_naivebayes_weights (barrel);
  else if (barrel->method == bow_method_prind)
    _bow_barrel_set_prind_weights (barrel);
  else
    _bow_barrel_set_weights_with_idf (barrel);
}


/* Calculate the normalizing factor by which each weight should be 
   multiplied.  Store it in each cdoc->normalizer. */
void
bow_barrel_set_weight_normalizers (bow_barrel *barrel)
{
  int current_di;		/* the index of the document for which
				   we are currently normalizing the
				   "word vector". */
  float norm_total;		/* the length of the word vector */
  float weight;			/* the weight of a single wi/di entry */
  bow_dv_heap *heap;		/* a heap of "document vectors" */
  bow_cdoc *cdoc;		/* The document we're working on */

  heap = bow_make_dv_heap_from_wi2dvf (barrel->wi2dvf);

  bow_verbosify (bow_progress, "Normalizing weights:          ");

  /* Keep going until the heap is empty */
  while (heap->length > 0)
    {
      /* Set the current document we're working on */
      current_di = heap->entry[0].current_di;
      assert (heap->entry[0].dv->idf == heap->entry[0].dv->idf);  /* NaN */

      if (current_di % 10 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", current_di);

      /* Here we should check if this di is part of some training set and
	 move on if it isn't. */
    
      /* Get the document */
      cdoc = bow_cdocs_di2doc (barrel->cdocs, current_di);

      /* If it's not a model document, then move on to next one */
      if (cdoc->type != model)
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
    
      /* Reset the length */
      norm_total = 0.0;

      /* Loop over all words in this document, summing up the score */
      do 
	{
	  weight = heap->entry[0].dv->entry[heap->entry[0].index].weight;
	  _bow_add_to_normalizer_total (&norm_total, weight, barrel->method);

	  /* Update the heap, we are done with this di, move it to its
	     new position */
	  bow_dv_heap_update (heap);
	} 
      while ((current_di == heap->entry[0].current_di)
	     && (heap->length > 0));

      /* xxx Why isn't this always true? -am */
      /* assert (norm_total != 0); */

      /* Do final processing of, and store the result. */
      cdoc->normalizer = _bow_total_to_normalizer (norm_total, barrel->method);

    }

  /* xxx We could actually re-set the weights using the normalizer now
     and avoid storing the normalizer.  This would be easier than
     figuring out the normalizer, because we don't have to use the heap
     again, we can just loop through all the WI's and DVI's. */

  bow_free (heap);
  bow_verbosify (bow_progress, "\n"); 
}

/* xxx Why not fold this into bow_barrel_set_weights?
   When would we want to set weights and not normalize them? */
/* xxx This function is deprecated, and is just here for compatibility. */
/* Calculate the length of each word vector. The cdocs structure
   is used first of all to only calculate the lengths of the documents in the
   model, and second, to store the lengths of the documents. */
void
bow_barrel_normalize_weights (bow_barrel *barrel)
{
  bow_barrel_set_weight_normalizers (barrel);
}


/* Functions for setting weights in WV's. */

/* Assign a value to the "word vector's" NORMALIZER field, according
   to the weights.  Return the value of the NORMALIZER field. */
void
bow_wv_set_weight_normalizer (bow_wv *wv, bow_method method)
{
  float total = 0.0f;
  int wvi;

  for (wvi = 0; wvi < wv->num_entries; wvi++)
    {
      _bow_add_to_normalizer_total (&total, wv->entry[wvi].weight, method);
    }
  if (total == 0)
    bow_error ("You forgot to set the weights before normalizing the WV.");
  wv->normalizer = _bow_total_to_normalizer (total, method);
}

/* Assign the values of the "word vector entry's" WEIGHT field,
   according to the COUNT divided by the "word vector's" NORM. */
void
bow_wv_set_weights (bow_wv *wv, bow_method method)
{
  int wvi;

  for (wvi = 0; wvi < wv->num_entries; wvi++)
    wv->entry[wvi].weight = wv->entry[wvi].count;
}

