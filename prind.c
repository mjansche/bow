/* Weight-setting and scoring implementation for PrInd classification
   (Fuhr's Probabilistic Indexing) */

#include "libbow.h"
#include <values.h>
#include <math.h>

/* If this is non-zero, use uniform class priors. */
int bow_prind_uniform_priors = 1;


/* Function to assign `PrInd'-style weights to each element of
   each document vector. */
void
_bow_barrel_set_prind_weights (bow_barrel *barrel)
{
  int ci;
  bow_cdoc *cdoc;
  int wi;			/* a "word index" into WI2DVF */
  int max_wi;			/* the highest "word index" in WI2DVF. */
  bow_dv *dv;			/* the "document vector" at index WI */
  int dvi;			/* an index into the DV */
  int total_term_count;

  /* We assume that we have already called BOW_BARREL_NEW_VPC() on
     BARREL, so BARREL already has one-document-per-class. */
    
  assert (barrel->method == bow_method_prind);
  max_wi = bow_num_words ();

  /* The CDOC->PRIOR should have been set in bow_barrel_new_vpc();
     verify it. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      assert (cdoc->prior >= 0);
    }

  /* Get the total number of terms in each class; temporarily store
     this in cdoc->normalizer.  WARNING: bow_barrel_normalize_weights
     will use this memory location for something different.  Also, get
     the total count for each term across all classes, and store it in
     the IDF of the respective word vector.  WARNING: this is an odd
     use of the IDF variable.  Also, get the total number of terms,
     across all terms, all classes, put it in TOTAL_TERM_COUNT. */

  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      cdoc->normalizer = 0;
    }
  total_term_count = 0;
  for (wi = 0; wi < max_wi; wi++) 
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;
      dv->idf = 0;
      for (dvi = 0; dvi < dv->length; dvi++) 
	{
	  cdoc = bow_array_entry_at_index (barrel->cdocs, 
					   dv->entry[dvi].di);
	  /* Summing total number of words in each class */
	  cdoc->normalizer += dv->entry[dvi].count;
	  /* Summing total word occurrences across all classes. */
	  dv->idf += dv->entry[dvi].count;
	  assert (dv->idf > 0);
	  total_term_count += dv->entry[dvi].count;
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
	  float pr_x_c;
	  float pr_x;
	  cdoc = bow_array_entry_at_index (barrel->cdocs, 
					   dv->entry[dvi].di);
	  /* Here CDOC->NORMALIZER is the total number of words in the class.
	     Here DV->IDF is total num of occurrences of WI across classes. */
	  /* The probability of a word given a class */
	  pr_x_c = (float)(dv->entry[dvi].count) / cdoc->normalizer;
	  /* The probability of a word across all classes. */
	  pr_x = dv->idf / total_term_count;
	  dv->entry[dvi].weight = pr_x_c / pr_x;
	  assert (dv->entry[dvi].weight > 0);
	}

      /* Don't set the DV->IDF; we'll use its current value later */
    }
}

int
_bow_score_prind_from_wv (bow_barrel *barrel, bow_wv *query_wv, 
			    bow_doc_score *doc_scores, int doc_scores_len)
{
  double *scores;		/* will become prob(class), indexed over CI */
  int ci;			/* a "class index" (document index) */
  int wvi;			/* an index into the entries of QUERY_WV. */
  int dvi;			/* an index into a "document vector" */
  float pr_w_c;			/* P(w|C), prob a word is in a class */
  float pr_c;			/* P(C), prior prob of a class */
  int num_doc_scores;		/* number of entries placed in DOC_SCORES */
  int max_wi;			/* number of words in the vocabulary */

  /* Allocate space to store scores for *all* classes (documents) */
  scores = alloca (barrel->cdocs->length * sizeof (double));

  /* Initialize the SCORES to zero. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      scores[ci] = 0;
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
      /* Note: move this test down if we add Laplace estimator. */
      if (!dv)
	continue;

      /* Loop over all classes, putting this word's (WI's)
	 contribution into SCORES. */
      for (ci = 0, dvi = 0; ci < barrel->cdocs->length; ci++)
	{
	  bow_cdoc *cdoc;
	  /* Assign PR_W_C to P(w|C), either using a DV entry, or, if
	     there is no DV entry for this class, using the Laplace
	     estimator */
	  while (dvi < dv->length && dv->entry[dvi].di < ci)
	    dvi++;
	  if (!(dv && dvi < dv->length && dv->entry[dvi].di == ci))
	    continue;
	  cdoc = bow_array_entry_at_index (barrel->cdocs, ci);

	  if (bow_prind_uniform_priors)
	    pr_c = 1.0f;
	  else
	    pr_c = cdoc->prior;

	  /* Instead of continuing, this would be the place to do Laplace
	     pr_w_c = 1 / max_wi; */
	  pr_w_c = dv->entry[dvi].weight;

	  /* Here DV->IDF is an unnormalized P(wi), probability of word WI.
	     Here CDOC->PRIOR is P(C), prior probability of class. */
	  scores[ci] += ((pr_w_c * pr_c) *
			 (query_wv->entry[wvi].weight * query_wv->normalizer));
	}
    }
  /* Now SCORES[] contains a (unnormalized) probability for each class. */

  /* Normalize the SCORES so they all sum to one. */
  {
    double scores_sum = 0;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      scores_sum += scores[ci];
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      scores[ci] /= scores_sum;
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
