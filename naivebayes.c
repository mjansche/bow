/* Weight-setting and scoring implementation for Naive-Bayes classification */

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
#include <math.h>
#include <argp/argp.h>

/* Command-line options specific to NaiveBayes */

/* Default value for option "naivebayes-m-est-m".  When zero, then use
   size-of-vocabulary instead. */
static int naivebayes_argp_m_est_m = 0;

/* The integer or single char used to represent this command-line option.
   Make sure it is unique across all libbow and rainbow. */
#define NB_M_EST_M_KEY 3001

static struct argp_option naivebayes_options[] =
{
  {"naivebayes-m-est-m", NB_M_EST_M_KEY, "M", 0,
   "When using `m'-estimates for smoothing in NaiveBayes, use M as the "
   "value for `m'.  The default is the size of vocabulary."},
  {0, 0}
};

error_t
naivebayes_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case NB_M_EST_M_KEY:
      naivebayes_argp_m_est_m = atoi (arg);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const struct argp naivebayes_argp =
{
  naivebayes_options,
  naivebayes_parse_opt
};

static struct argp_child naivebayes_argp_child =
{
  &naivebayes_argp,		/* This child's argp structure */
  0,				/* flags for child */
  0,				/* optional header in help message */
  0				/* arbitrary group number for ordering */
};

/* End of command-line options specific to NaiveBayes */

/* For changing weight of unseen words.
   I really should implement `deleted interpolation' */
/* M_EST_P summed over all words in the vocabulary must sum to 1.0! */
#if 1
/* This is the special case of the M-estimate that is `Laplace smoothing' */
#define M_EST_M  (naivebayes_argp_m_est_m \
		  ? naivebayes_argp_m_est_m \
		  : barrel->wi2dvf->num_words)
#define M_EST_P  (1.0 / barrel->wi2dvf->num_words)
#else
/* This is a version of M-estimates where the value of M depends on the
   number of words in the class. */
#define M_EST_M  (cdoc->word_count \
		  ? (((float)barrel->wi2dvf->num_words) / cdoc->word_count) \
		  : 1.0)
#define M_EST_P  (1.0 / barrel->wi2dvf->num_words)
#endif

/* Return the probability of word WI in class CI. 
   If LOO_CLASS is non-negative, then we are doing 
   leave-out-one-document evaulation.  LOO_CLASS is the index
   of the class from which the document has been removed.
   LOO_WI_COUNT is the number of WI'th words that are in the document
   LOO_W_COUNT is the total number of words in the docment

   The last two argments help this function avoid searching for
   the right entry in the DV from the beginning each time.
   LAST_DV is a pointer to the DV to use.
   LAST_DVI is a pointer to the index into the LAST_DV that is
   guaranteed to have class index less than CI.
*/
float
bow_naivebayes_pr_wi_ci (bow_barrel *barrel,
			 int wi, int ci,
			 int loo_class,
			 int loo_wi_count, int loo_w_count,
			 bow_dv **last_dv, int *last_dvi)
{
  bow_dv *dv;
  bow_cdoc *cdoc;
  float num_wi_ci;		/* the number of times wi occurs in class */
  float num_w_ci;		/* the number of words in class. */
  int dvi;
  float m_est_m;
  float m_est_p;
  float pr_w_c;

  cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
  if (last_dv && *last_dv)
    {
      dv = *last_dv;
      dvi = *last_dvi;
      /* No, not always true. assert (dv->entry[dvi].di <= ci); */
    }
  else
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      dvi = 0;
      if (last_dv)
	*last_dv = dv;
    }

  /* If the model doesn't know about this word, return 0. */
  if (!dv)
    return -1.0;

  /* Find the index of entry for this class. */
  while (dvi < dv->length && dv->entry[dvi].di < ci)
    dvi++;
  /* Remember this index value for future calls to this function */
  if (last_dvi)
    *last_dvi = dvi;

  if (dvi < dv->length && dv->entry[dvi].di == ci)
    {
      /* There is an entry in DV for class CI. */
      num_wi_ci = dv->entry[dvi].count;
    }
  else
    {
      /* There is no entry in DV for class CI. */
      num_wi_ci = 0;
      if (loo_class == ci)
	bow_error ("There should be data for WI,CI");
    }
  num_w_ci = cdoc->word_count;

  if (loo_class == ci)
    {
      num_wi_ci -= loo_wi_count;
      num_w_ci -= loo_w_count;
      if (!(num_wi_ci >= 0 && num_w_ci >= 0))
	bow_error ("foo %g %g\n", num_wi_ci, num_w_ci);
    }
  /* xxx This is not exactly right, because 
     BARREL->WI2DVF->NUM_WORDS might have changed with the
     removal of QUERY_WV's document. */
  m_est_m = barrel->wi2dvf->num_words;
  m_est_p = 1.0 / m_est_m;

  pr_w_c = ((num_wi_ci + m_est_m * m_est_p)
	    / (num_w_ci + m_est_m));

  if (pr_w_c <= 0)
    bow_error ("A negative word probability was calculated. "
	       "This can happen if you are using\n"
	       "--test-files-loo and the test files are "
	       "not being lexed in the same way as they\n"
	       "were when the model was built");
  assert (pr_w_c > 0 && pr_w_c <= 1);

  return pr_w_c;
}

void
bow_naivebayes_print_word_probabilities_for_class (bow_barrel *barrel,
						   const char *classname)
{
  int wi;
  int ci = bow_str2int_no_add (barrel->classnames, classname);
  float pr_w;

  assert (ci >= 0);
  for (wi = 0; wi < barrel->wi2dvf->size; wi++)
    {
      pr_w = bow_naivebayes_pr_wi_ci (barrel, wi, ci, -1, 0, 0, NULL, NULL);
      if (pr_w >= 0)
	printf ("%-30s  %10.8f\n", bow_int2word (wi), pr_w);
    }
}

/* Function to assign `Naive Bayes'-style weights to each element of
   each document vector. */
void
bow_naivebayes_set_weights (bow_barrel *barrel)
{
  int ci;
  bow_cdoc *cdoc;
  int wi;			/* a "word index" into WI2DVF */
  int max_wi;			/* the highest "word index" in WI2DVF. */
  bow_dv *dv;			/* the "document vector" at index WI */
  int dvi;			/* an index into the DV */
  int weight_setting_num_words = 0;
  float *pr_all_w_c = alloca (barrel->cdocs->length * sizeof (float));
  float pr_w_c;

  /* We assume that we have already called BOW_BARREL_NEW_VPC() on
     BARREL, so BARREL already has one-document-per-class. */
    
  assert (!strcmp (barrel->method->name, "naivebayes")
	  || !strcmp (barrel->method->name, "crossentropy"));
  max_wi = MIN (barrel->wi2dvf->size, bow_num_words());

  /* The CDOC->PRIOR should have been set in bow_barrel_new_vpc();
     verify it. */
  /* Get the total number of unique terms in each class; store this in
     CDOC->NORMALIZER. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      assert (cdoc->prior >= 0);
      pr_all_w_c[ci] = 0;
      cdoc->normalizer = 0;
    }

#if 0
  /* For Shumeet, make all counts either 0 or 1. */
  for (wi = 0; wi < max_wi; wi++) 
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;
      for (dvi = 0; dvi < dv->length; dvi++) 
	{
	  assert (dv->entry[dvi].count);
	  dv->entry[dvi].count = 1;
	}
    }  
  /* And set uniform priors */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      cdoc->prior = 1.0;
    }
#endif

  /* If BOW_BINARY_WORD_COUNTS is true, then we'll just use the value 
     of WORD_COUNT set in bow_barrel_new_vpc(), which is the total 
     number of *documents* in the class, not the number of words. */
  if (!bow_binary_word_counts)
    {
      /* Get the total number of terms in each class; store this in
	 CDOC->WORD_COUNT. */
      /* Calculate the total number of unique words, and make sure it is
	 the same as BARREL->WI2DVF->NUM_WORDS. */
      int num_unique_words = 0;

      for (ci = 0; ci < barrel->cdocs->length; ci++)
	{
	  cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
	  cdoc->word_count = 0;
	}
      for (wi = 0; wi < max_wi; wi++) 
	{
	  dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
	  if (dv == NULL)
	    continue;
	  num_unique_words++;
	  for (dvi = 0; dvi < dv->length; dvi++) 
	    {
	      cdoc = bow_array_entry_at_index (barrel->cdocs, 
					       dv->entry[dvi].di);
	      cdoc->word_count += dv->entry[dvi].count;
	      cdoc->normalizer++;
	    }
	}
      assert (num_unique_words == barrel->wi2dvf->num_words);
    }

  /* Set the weights in the BARREL's WI2DVF so that they are
     equal to P(w|C), the probability of a word given a class. */
  for (wi = 0; wi < max_wi; wi++) 
    {
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);

      /* If the model doesn't know about this word, skip it. */
      if (dv == NULL)
	continue;

      /* Now loop through all the classes, setting their weights */
      for (ci = 0, dvi = 0; ci < barrel->cdocs->length; ci++)
	{
	  cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
	  while (dvi < dv->length && dv->entry[dvi].di < ci)
	    dvi++;
	  if (dv && dvi < dv->length && dv->entry[dvi].di == ci)
	    {
	      pr_w_c = ((float)
			((M_EST_M * M_EST_P) + dv->entry[dvi].count)
			/ (M_EST_M + cdoc->word_count));
	      dv->entry[dvi].weight = pr_w_c;
	    }
	  else
	    {
	      pr_w_c = ((M_EST_M * M_EST_P)
			/ (M_EST_M + cdoc->word_count));
	    }
	  assert (pr_w_c <= 1);
	  pr_all_w_c[ci] += pr_w_c;
	}
      weight_setting_num_words++;
      /* Set the IDF.  NaiveBayes doesn't use it; make it have no effect */
      dv->idf = 1.0;
    }

  /* Check to make sure that [Sum_w Pr(w|c)] sums to 1 for all classes. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      /* Is this too much round-off error to expect? */
      assert (pr_all_w_c[ci] < 1.01 && pr_all_w_c[ci] > 0.99);
    }

#if 0
  fprintf (stderr, "wi2dvf num_words %d, weight-setting num_words %d\n",
	   barrel->wi2dvf->num_words, weight_setting_num_words);
#endif
}


int
bow_naivebayes_score (bow_barrel *barrel, bow_wv *query_wv, 
		      bow_score *bscores, int bscores_len,
		      int loo_class)
{
  double *scores;		/* will become prob(class), indexed over CI */
  int ci;			/* a "class index" (document index) */
  int wvi;			/* an index into the entries of QUERY_WV. */
  int dvi;			/* an index into a "document vector" */
  float pr_w_c;			/* P(w|C), prob a word is in a class */
  double log_pr_tf;		/* log(P(w|C)^TF), ditto, log() of it */
  double rescaler;		/* Rescale SCORES by this after each word */
  double new_score;		/* a temporary holder */
  int num_scores;		/* number of entries placed in SCORES */
  int num_words_in_query = 0;

  /* Allocate space to store scores for *all* classes (documents) */
  scores = alloca (barrel->cdocs->length * sizeof (double));

  /* Instead of multiplying probabilities, we will sum up
     log-probabilities, (so we don't loose floating point resolution),
     and then take the exponent of them to get probabilities back. */

  /* Initialize the SCORES to the class prior probabilities. */
  if (bow_print_word_scores)
    printf ("%s\n",
	    "(CLASS PRIOR PROBABILIES)");
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
      if (bow_uniform_class_priors)
	/* Uniform prior means each class has probability 1/#classes. */
	scores[ci] = - log (barrel->cdocs->length);
      else
	{
#if 0 /* For now forget about this little detail, because rainbow-h
	 trips up on it. */
	  /* LOO_CLASS is not implemented for cases in which we are
	     not doing uniform class priors. */
	  assert (loo_class == -1);
#endif
	  assert (cdoc->prior > 0.0f && cdoc->prior <= 1.0f);
	  scores[ci] = log (cdoc->prior);
	}
      assert (scores[ci] > -FLT_MAX + 1.0e5);
      if (bow_print_word_scores)
	printf ("%16s %-40s  %10.9f\n", 
		"",
		(strrchr (cdoc->filename, '/') ? : cdoc->filename),
		scores[ci]);
    }

  /* If we are doing leave-one-out evaluation, get the total number of
     words in this query. */
  if (loo_class >= 0)
    {
      bow_dv *dv;
      num_words_in_query = 0;
      for (wvi = 0; wvi < query_wv->num_entries; wvi++)
	{
	  /* Only count those words that are in the model's vocabulary. */
	  dv = bow_wi2dvf_dv (barrel->wi2dvf, query_wv->entry[wvi].wi);
	  if (dv)
	    num_words_in_query += query_wv->entry[wvi].count;
	}
    }

  /* Loop over each word in the word vector QUERY_WV, putting its
     contribution into SCORES. */
  for (wvi = 0; wvi < query_wv->num_entries; wvi++)
    {
      int wi;			/* the word index for the word at WVI */
      bow_dv *dv;		/* the "document vector" for the word WI */

      /* Get information about this word. */
      wi = query_wv->entry[wvi].wi;
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);

      /* If the model doesn't know about this word, skip it. */
      if (!dv)
	continue;

      if (bow_print_word_scores)
	printf ("%-30s (queryweight=%.8f)\n",
		bow_int2word (wi), 
		query_wv->entry[wvi].weight * query_wv->normalizer);

      rescaler = DBL_MAX;

      /* Loop over all classes, putting this word's (WI's)
	 contribution into SCORES. */
      for (ci = 0, dvi = 0; ci < barrel->cdocs->length; ci++)
	{
	  pr_w_c = bow_naivebayes_pr_wi_ci (barrel, wi, ci, 
					    loo_class, 
					    query_wv->entry[wvi].count, 
					    num_words_in_query,
					    &dv, &dvi);
	  assert (pr_w_c > 0 && pr_w_c <= 1);

	  log_pr_tf = log (pr_w_c);
	  assert (log_pr_tf > -FLT_MAX + 1.0e5);
	  /* Take into consideration the number of times it occurs in 
	     the query document */
	  log_pr_tf *= query_wv->entry[wvi].count;
	  assert (log_pr_tf > -FLT_MAX + 1.0e5);

	  scores[ci] += log_pr_tf;

	  if (bow_print_word_scores)
	    {
	      bow_cdoc *cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
	      printf (" %8.2e %7.2f %-40s  %10.9f\n", 
		      pr_w_c,
		      log_pr_tf, 
		      (strrchr (cdoc->filename, '/') ? : cdoc->filename),
		      scores[ci]);
	    }

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
	      assert (scores[ci] > -DBL_MAX + 1.0e5
		      && scores[ci] < DBL_MAX - 1.0e5);
	    }
	}
    }
  /* Now SCORES[] contains a (unnormalized) log-probability for each class. */

  /* Rescale the SCORE one last time, this time making them all 0 or
     negative, so that exp() will work well, especially around the
     higher-probability classes. */
  {
    rescaler = -DBL_MAX;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      if (scores[ci] > rescaler) 
	rescaler = scores[ci];
    /* RESCALER is now the maximum of the SCORES. */
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      scores[ci] -= rescaler;
  }

  /* Use exp() on the SCORES to get probabilities from
         log-probabilities. */
  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      new_score = exp (scores[ci]);
      /* assert (new_score > 0 && new_score < DBL_MAX - 1.0e5); */
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
     SCORES in sorted order. */
  {
    num_scores = 0;
    for (ci = 0; ci < barrel->cdocs->length; ci++)
      {
	if (num_scores < bscores_len
	    || bscores[num_scores-1].weight < scores[ci])
	  {
	    /* We are going to put this score and CI into SCORES
	       because either: (1) there is empty space in SCORES, or
	       (2) SCORES[CI] is larger than the smallest score there
	       currently. */
	    int dsi;		/* an index into SCORES */
	    if (num_scores < bscores_len)
	      num_scores++;
	    dsi = num_scores - 1;
	    /* Shift down all the entries that are smaller than SCORES[CI] */
	    for (; dsi > 0 && bscores[dsi-1].weight < scores[ci]; dsi--)
	      bscores[dsi] = bscores[dsi-1];
	    /* Insert the new score */
	    bscores[dsi].weight = scores[ci];
	    bscores[dsi].di = ci;
	  }
      }
  }

  return num_scores;
}


bow_params_naivebayes bow_naivebayes_params =
{
  bow_no,			/* no uniform priors */
  bow_yes,			/* normalize_scores */
};

bow_method bow_method_naivebayes = 
{
  "naivebayes",
  bow_naivebayes_set_weights,
  0,				/* no weight scaling function */
  NULL, /* bow_barrel_normalize_weights_by_summing, */
  bow_barrel_new_vpc_merge_then_weight,
  bow_barrel_set_vpc_priors_by_counting,
  bow_naivebayes_score,
  bow_wv_set_weights_to_count,
  NULL,				/* no need for extra weight normalization */
  bow_barrel_free,
  &bow_naivebayes_params
};

void _register_method_naivebayes () __attribute__ ((constructor));
void _register_method_naivebayes ()
{
  bow_method_register_with_name (&bow_method_naivebayes, "naivebayes");
  bow_argp_add_child (&naivebayes_argp_child);
}
