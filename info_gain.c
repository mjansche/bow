
/* Functions to calculate the information gain for each word in our corpus. */

#include "libbow.h"
#include <math.h>

#if !HAVE_LOG2F
#define log2f log
#endif

/* Helper function to calculate the entropy given counts for each type of
   element. */
float
bow_entropy(int counts[], int num_classes)
{
  int total = 0;                 /* How many elements we have in total */
  float entropy = 0.0;
  float fraction;
  int i;

  /* First total the array. */
  for (i = 0; i < num_classes; i++)
    total += counts[i];

  /* If we have no elements, then the entropy is zero. */
  if (total == 0) {
    return 0.0;
  }

  /* Now calculate the entropy */
  for (i = 0; i < num_classes; i++)
    {
      if (counts[i] != 0)
	{
	  fraction = (float)counts[i] / (float)total;
	  entropy -= (fraction * log2f (fraction));
	}
    }

  return entropy;
}

/* Return a malloc()'ed array containing an infomation-gain score for
   each word index. */
float *
bow_infogain_per_wi_new (bow_barrel *barrel, int num_classes, int *size)
{
  int grand_totals[num_classes];    /* Totals for each class. */
  int with_word[num_classes];       /* Totals for the set of model docs
				      with this word. */
  int without_word[num_classes];    /* Totals for the set of model docs
				      without this word. */
  int max_wi;			   /* the highest "word index" in WI2DVF. */
  bow_cdoc *doc;                   /* The working cdoc. */
  float total_entropy;             /* The entropy of the total collection. */
  float with_word_entropy;         /* The entropy of the set of docs with
				      the word in question. */
  float without_word_entropy;      /* The entropy of the set of docs without
				      the word in question. */
  int grand_total = 0; 
  int with_word_total = 0;
  int without_word_total = 0;
  int i, j, wi, di;
  bow_dv *dv;
  float *ret;

  bow_verbosify (bow_progress, 
		 "Calculating info gain... words ::       ");

  max_wi = MIN (barrel->wi2dvf->size, bow_num_words());
  *size = max_wi;
  ret = bow_malloc (max_wi * sizeof (int));

  /* First set all the arrays to zero */
  for(i = 0; i < num_classes; i++) 
    {
      grand_totals[i] = 0;
      with_word[i] = 0;
      without_word[i] = 0;
    }

  /* Now set up the grand totals. */
  for (i = 0; i < barrel->cdocs->length ; i++)
    {
      doc = bow_cdocs_di2doc (barrel->cdocs, i);
      if (doc->type == model) 
	{
	  grand_totals[doc->class]++;
	  grand_total++;
	}
    }

  /* Calculate the total entropy */
  total_entropy = bow_entropy (grand_totals, num_classes);

  /* Now loop over all words. */
  for (wi = 0; wi < max_wi; wi++) 
    {
      /* Get this document vector */
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	{
	  ret[wi] = 0;
	  continue;
	}

      with_word_total = 0;

      /* Create totals for this dv. */
      for (j = 0; j < dv->length; j++)
	{
	  di = dv->entry[j].di;
	  doc = bow_cdocs_di2doc (barrel->cdocs, di);
	  if (doc->type == model) 
	    {
	      with_word[doc->class]++;
	      with_word_total++;
	    }
	}

      /* Create without word totals. */
      for (j = 0; j < num_classes; j++)
	{
	  without_word[j] = grand_totals[j] - with_word[j];
	}
      without_word_total = grand_total - with_word_total;

      /* Calculate entropies */
      with_word_entropy = bow_entropy(with_word, num_classes);
      without_word_entropy = bow_entropy(without_word, num_classes);

      /* Calculate and store the information gain. */
      ret[wi] = (total_entropy 
		 - ((((float)with_word_total / (float)grand_total) 
		     * with_word_entropy)
		    + (((float)without_word_total / (float)grand_total) 
		       * without_word_entropy)));
      assert (ret[wi] >= 0);

      /* Reset arrays to zero */
      for(i = 0; i < num_classes; i++) 
	{
	  with_word[i] = 0;
	  without_word[i] = 0;
	}
      if (wi % 100 == 0)
	bow_verbosify (bow_progress,
		       "\b\b\b\b\b\b%6d", max_wi - wi);
    }
  bow_verbosify (bow_progress, "\n");
  return ret;
}

/* Print to stdout the sorted results of bow_infogain_per_wi_new().
   It will print the NUM_TO_PRINT words with the highest infogain. */
void
bow_infogain_per_wi_print (bow_barrel *barrel, int num_classes, 
			   int num_to_print)
{
  float *wi2ig;			/* the array of information gains */
  int wi2ig_size;
  int wi, i;
  struct wiig { int wi; float ig; } *wiigs;
  int wiig_compare (const void *wiig1, const void *wiig2)
    {
      if (((struct wiig*)wiig1)->ig > ((struct wiig*)wiig2)->ig)
	return -1;
      else if (((struct wiig*)wiig1)->ig == ((struct wiig*)wiig2)->ig)
	return 0;
      else
	return 1;
    }

  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  /* Create and fill and array of `word-index and information-gain
     structures' that can be sorted. */
  wiigs = bow_malloc (wi2ig_size * sizeof (struct wiig));
  for (wi = 0; wi < wi2ig_size; wi++)
    {
      wiigs[wi].wi = wi;
      wiigs[wi].ig = wi2ig[wi];
    }

  /* Sort it. */
  qsort (wiigs, wi2ig_size, sizeof (struct wiig), wiig_compare);

  /* Print it. */
  for (i = 0; i < num_to_print; i++)
    {
      printf ("%8.5f %s\n", wiigs[i].ig, bow_int2word (wiigs[i].wi));
    }
  bow_free (wi2ig);
}

/* Function to calculate the information gain for each word in the corpus
   (looking only at documents in the model) and multiply each weight by the
   gain. */
void
bow_barrel_scale_by_info_gain (bow_barrel *barrel, int num_classes)
{
  float *wi2ig;			/* the array of information gains */
  int wi2ig_size;
  int wi, max_wi, j;
  bow_dv *dv;

  bow_verbosify (bow_progress, 
		 "Scaling weights by information gain over words:          ");
  
  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  for (wi = 0; wi < max_wi; wi++) 
    {
      /* Get this document vector */
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;

      /* Scale all the elements using this gain. */
      for (j = 0; j < dv->length; j++)
	{
	  dv->entry[j].weight *= wi2ig[wi];
	}

      /* Done */
      bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", max_wi - wi - 1); 
    }
  bow_verbosify (bow_progress, "\n");
}
