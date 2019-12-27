/* Weight-setting and scoring implementation for Naive-Bayes classification */

#include "libbow.h"
#include <values.h>
#include <math.h>

/* Set the class prior probabilities.  This function is called from
   vpc.c:bow_barrel_new_vpc(). */
void
_bow_barrels_set_naivebayes_vpc_priors (bow_barrel *doc_barrel,
					bow_barrel *vpc_barrel)
{
  float prior_sum = 0;
  int ci;
  int max_ci = vpc_barrel->cdocs->length - 1;
  int di;

  /* Zero them. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      cdoc->prior = 0;
    }
  /* Add in document counts. */
  for (di = 0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *doc_cdoc;
      bow_cdoc *vpc_cdoc;
      doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      vpc_cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
					   doc_cdoc->class);
      (vpc_cdoc->prior)++;
    }
  /* Sum them. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      prior_sum += cdoc->prior;
    }
  /* Normalize them. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      cdoc->prior /= prior_sum;
    }
}


/* Function to assign `Naive Bayes'-style weights to each element of
   each document vector. */
void
_bow_barrel_set_naivebayes_weights (bow_barrel *barrel)
{
  int ci;
  bow_cdoc *cdoc;
  int wi;			/* a "word index" into WI2DVF */
  int max_wi;			/* the highest "word index" in WI2DVF. */
  bow_dv *dv;			/* the "document vector" at index WI */
  int dvi;			/* an index into the DV */

  /* We assume that we have already called BOW_BARREL_NEW_VPC() on
     BARREL, so BARREL already has one-document-per-class. */
    
  assert (barrel->method == bow_method_naivebayes);
  max_wi = bow_num_words ();

  /* The CDOC->PRIOR should have been set in bow_barrel_new_vpc();
     verify it. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      assert (cdoc->prior >= 0);
    }

  /* Get the total number of terms in each class; temporarily
     store this in cdoc->normalizer.  Beware,
     bow_barrel_normalize_weights will use this memory location
     for something different.  */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      cdoc->normalizer = 0;
    }
  for (wi = 0; wi < max_wi; wi++) 
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;
      for (dvi = 0; dvi < dv->length; dvi++) 
	{
	  cdoc = bow_array_entry_at_index (barrel->cdocs, 
					   dv->entry[dvi].di);
	  cdoc->normalizer += dv->entry[dvi].count;
	}
    }

  /* Set the weights in the BARREL's WI2DVF so that they are
     equal to P(w|C), the probability of a word given a class. */
  for (wi = 0; wi < max_wi; wi++) 
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;
      /* Now loop through all the elements, setting their weights */
      for (dvi = 0; dvi < dv->length; dvi++) 
	{
	  cdoc = bow_array_entry_at_index (barrel->cdocs, 
					   dv->entry[dvi].di);
	  /* Here CDOC->NORMALIZER is the total number of words in the class */
	  dv->entry[dvi].weight = ((float)
				   (1 + dv->entry[dvi].count)
				   / (max_wi + cdoc->normalizer));
	  assert (dv->entry[dvi].weight > 0);
	}
      /* Set the IDF.  NaiveBayes doesn't use it; make it have no effect */
      dv->idf = 1.0;
    }
}

int
_bow_score_naivebayes_from_wv (bow_barrel *barrel, bow_wv *query_wv, 
			       bow_doc_score *doc_scores, int doc_scores_len)
{
  double *scores;		/* will become prob(class), indexed over CI */
  int ci;			/* a "class index" (document index) */
  int wvi;			/* an index into the entries of QUERY_WV. */
  int dvi;			/* an index into a "document vector" */
  float pr_w_c;			/* P(w|C), prob a word is in a class */
  double pr_tf;			/* P(w|C)^TF, ditto, by occurr's in QUERY_WV */
  double log_pr_tf;		/* log(P(w|C)^TF), ditto, log() of it */
  double rescaler;		/* Rescale SCORES by this after each word */
  double new_score;		/* a temporary holder */
  int num_doc_scores;		/* number of entries placed in DOC_SCORES */
  int max_wi;			/* number of words in the vocabulary */

  /* Allocate space to store scores for *all* classes (documents) */
  scores = alloca (barrel->cdocs->length * sizeof (double));

  /* Instead of multiplying probabilities, we will sum up
     log-probabilities, (so we don't loose floating point resolution),
     and then take the exponent of them to get probabilities back. */

  /* Initialize the SCORES to the class prior probabilities. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      scores[ci] = log (cdoc->prior);
      assert (scores[ci] > -MAXFLOAT + 1.0e5);
    }

  /* Loop over each word in the word vector QUERY_WV, putting its
     contribution into SCORES. */
  max_wi = bow_num_words ();
  for (wvi = 0; wvi < query_wv->num_entries; wvi++)
    {
      int wi;			/* the word index for the word at WVI */
      bow_dv *dv;		/* the "document vector" for the word WI */

      /* Get information about this word. */
      wi = query_wv->entry[wvi].wi;
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);

      rescaler = MAXDOUBLE;

      /* Loop over all classes, putting this word's (WI's)
	 contribution into SCORES. */
      for (ci = 0, dvi = 0; ci < barrel->cdocs->length; ci++)
	{
	  /* Both these values are pretty arbitrary small numbers. */
	  static const double min_pr_tf = MINFLOAT * 1.0e5;

	  /* Assign PR_W_C to P(w|C), either using a DV entry, or, if
	     there is no DV entry for this class, using the Laplace
	     estimator */
	  if (dv)
	    while (dvi < dv->length && dv->entry[dvi].di < ci)
	      dvi++;
	  if (dv && dvi < dv->length && dv->entry[dvi].di == ci)
	    pr_w_c = dv->entry[dvi].weight;
	  else
	    pr_w_c = 1.0 / max_wi;
	  assert (pr_w_c > 0 && pr_w_c <= 1);

	  pr_tf = pow (pr_w_c, query_wv->entry[wvi].count);
	  /* PR_TF can be zero due to round-off error, when PR_W_C is
	     very small and QUERY_WV->ENTRY[CURRENT_INDEX].COUNT is
	     very large.  Here we fudgingly avoid this by insisting
	     that PR_TF not go below some arbitrary small number. */
	  if (pr_tf < min_pr_tf)
	    pr_tf = min_pr_tf;

	  log_pr_tf = log (pr_tf);
	  assert (log_pr_tf > MAXFLOAT * -1.0e5);

	  scores[ci] += log_pr_tf;

	  /* Keep track of the minimum score updated for this word. */
	  if (rescaler > scores[ci])
	    rescaler = scores[ci];
	}

      /* Loop over all classes, re-scaling SCORES so that they
	 don't get so small we loose floating point resolution.
	 This scaling always keeps all SCORES positive. */
      if (rescaler < 0)
	{
	  for (ci = 0; ci < barrel->cdocs->length; ci++)
	    {
	      /* Add to SCORES to bring them close to zero.  RESCALER is
		 expected to often be less than zero here. */
	      /* xxx If this doesn't work, we could keep track of the min
		 and the max, and sum by their average. */
	      scores[ci] += -rescaler;
	      assert (scores[ci] > -MAXDOUBLE + 1.0e5
		      && scores[ci] < MAXDOUBLE - 1.0e5);
	    }
	}
    }
  /* Now SCORES[] contains a (unnormalized) log-probability for each class. */

  /* Rescale the SCORE one last time, this time making them all 0 or
     negative, so that exp() will work well, especially around the
     higher-probability classes. */
  {
    rescaler = -MAXDOUBLE;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      if (scores[ci] > rescaler) 
	rescaler = scores[ci];
    /* RESCALER is now the maximum of the SCORES. */
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      scores[ci] -= rescaler;
  }

  /* Use exp() on the SCORES to get probabilities from log-probabilities. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      new_score = exp (scores[ci]);
      /* assert (new_score > 0 && new_score < MAXDOUBLE - 1.0e5); */
      scores[ci] = new_score;
    }

  /* Normalize the SCORES so they all sum to one. */
  {
    double scores_sum = 0;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      scores_sum += scores[ci];
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      {
	scores[ci] /= scores_sum;
	/* assert (scores[ci] > 0); */
      }
  }

  /* Return the SCORES by putting them (and the `class indices') into
     DOC_SCORES in sorted order. */
  {
    num_doc_scores = 0;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      {
	if (num_doc_scores < doc_scores_len
	    || doc_scores[num_doc_scores-1].weight < scores[ci])
	  {
	    /* We are going to put this score and CI into DOC_SCORES
	       because either: (1) there is empty space in DOC_SCORES,
	       or (2) SCORES[CI] is larger than the smallest score
	       there currently. */
	    int dsi;		/* an index into DOC_SCORES */
	    if (num_doc_scores < doc_scores_len)
	      num_doc_scores++;
	    dsi = num_doc_scores - 1;
	    /* Shift down all the entries that are smaller than SCORES[CI] */
	    for (; dsi > 0 && doc_scores[dsi-1].weight < scores[ci]; dsi--)
	      doc_scores[dsi] = doc_scores[dsi-1];
	    /* Insert the new score */
	    doc_scores[dsi].weight = scores[ci];
	    doc_scores[dsi].di = ci;
	  }
      }
  }

  return num_doc_scores;
}

int
_old_bow_score_naivebayes_from_wv (bow_barrel *barrel, bow_wv *query_wv, 
				   bow_doc_score *scores, int best)
{
  bow_dv_heap *heap;
  bow_cdoc *cdoc;
  int num_scores = 0;		/* How many elements are in this array */
  int current_di, wi, current_index, i;
  double current_score = 0.0, target_weight;
  double max_nb_log_score = -MAXDOUBLE;
  int max_wi = bow_num_words ();

  /* Create the heap of document vectors using the words in QUERY_WV. */
  heap = bow_make_dv_heap_from_wv (barrel->wi2dvf, query_wv);

  /* Loop once for each document (actually, a class).  We'll only look
     at the classes that have at least one word in QUERY_WV. */
  while (heap->length > 0)
    {
      /* Get the index of the document we're currently working on */
      current_di = heap->entry[0].current_di;

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
	  {
	    double pr_word_in_class;    /* P(w|C) */
	    double pr_tf;	        /* P(w|C)^TF */
	    double log_pr_tf;	        /* log (P(w|C)^TF) */
	    static const double min_pr_tf = MINFLOAT * 1.0e5;

	    /* Here we do the Laplace estimator for `weight-smoothing'.
	       It is also done in _bow_barrel_set_naivebayes_weights()
	       in weight.c */
	    pr_word_in_class = ((target_weight > 0) 
				? target_weight
				: 1.0 / max_wi);
	    pr_tf = pow (pr_word_in_class, 
			 query_wv->entry[current_index].count);

	    /* PR_TF can be zero due to round-off error, when
	       PR_WORD_IN_CLASS is very small and
	       query_wv->entry[current_index].count is very large.
	       Here we fudge it by insisting that PR_TF not go below
	       some arbitrary small number. */
	    if (pr_tf < min_pr_tf)
	      pr_tf = min_pr_tf;

	    log_pr_tf = log (pr_tf);
	    assert (log_pr_tf > MAXFLOAT * -1.0e5);
	    current_score += log_pr_tf;
	    assert (current_score > -MAXFLOAT);
	    assert (current_score != 0);
	  }
	  /* Make sure we haven't got NaN. */
	  assert (current_score == current_score);

	  /* Now we need to update the heap - moving this element on to its
	     new position */
	  bow_dv_heap_update (heap);
	}
      while ((current_di == heap->entry[0].current_di)
	     && (heap->length > 0));

      current_score += log (cdoc->prior);
      assert (current_score == current_score); /* checking for NaN */

      if (max_nb_log_score < current_score)
	max_nb_log_score = current_score;

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
  /* Done with all classes. */

  /* Re-exp() and normalize */
  {
    double sum = 0.0;
    for (i = 0; i < num_scores; i++)
      {
	scores[i].weight = exp (scores[i].weight - max_nb_log_score);
	assert (scores[i].weight >= 0);
	sum += scores[i].weight;
      }
    /* xxx Instead of crashing we could just set scores[] to priors. */
    assert (sum > 0 && !isinf (sum));
    for (i = 0; i < num_scores; i++)
      {
	scores[i].weight /= sum;
	assert (scores[i].weight == scores[i].weight); /* Checking for NaN */
      }
  }

  bow_free (heap);

  /* All done - return the number of elements we have */
  return num_scores;
}
