/* Functions to calculate the information gain for each word in our corpus. */

/* Copyright (C) 1997 Andrew McCallum

   Written by:  Sean Slattery <slttery@cs.cmu.edu>
   and Andrew Kachites McCallum <mccallum@cs.cmu.edu>

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

/* A bogus hack.  If non-zero, leave out the last class from the infogain
   calculations. */
#define LEAVE_OUT_LAST_CLASS 0

#if !HAVE_LOG2F
#define log2f log
#endif

/* Helper function to calculate the entropy given counts for each type of
   element. */
double
bow_entropy (float counts[], int num_classes)
{
  double total = 0;		/* How many elements we have in total */
  double entropy = 0.0;
  double fraction;
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
  float grand_totals[num_classes];  /* Totals for each class. */
  float with_word[num_classes];	    /* Totals for the set of model docs
				       with this word. */
  float without_word[num_classes];  /* Totals for the set of model docs
				       without this word. */
  int max_wi;			    /* the highest "word index" in WI2DVF. */
  bow_cdoc *doc;                    /* The working cdoc. */
  double total_entropy;             /* The entropy of the total collection. */
  double with_word_entropy;         /* The entropy of the set of docs with
				       the word in question. */
  double without_word_entropy;      /* The entropy of the set of docs without
				       the word in question. */
  float grand_total = 0; 
  float with_word_total = 0;
  float without_word_total = 0;
  int i, j, wi, di;
  bow_dv *dv;
  float *ret;

  bow_verbosify (bow_progress, 
		 "Calculating info gain... words ::          ");

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
      if (doc->type == model 
#if LEAVE_OUT_LAST_CLASS
	  && doc->class != num_classes-1
#endif
	  ) 
	{
	  grand_totals[doc->class] += doc->prior;
	  grand_total += doc->prior;
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
	  if (doc->type == model
#if LEAVE_OUT_LAST_CLASS
	      && doc->class != num_classes-1
#endif
	      )
	    {
	      with_word[doc->class] += doc->prior;
	      with_word_total += doc->prior;
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
		 - ((((double)with_word_total / (double)grand_total) 
		     * with_word_entropy)
		    + (((double)without_word_total / (double)grand_total) 
		       * without_word_entropy)));
      /* Not comparing with 0 here because of round-off error. */
      assert (ret[wi] >= -1e-7);

      if (ret[wi] < 0)
	ret[wi] = 0;

      /* Reset arrays to zero */
      for(i = 0; i < num_classes; i++) 
	{
	  with_word[i] = 0;
	  without_word[i] = 0;
	}
      if (wi % 100 == 0)
	bow_verbosify (bow_progress,
		       "\b\b\b\b\b\b\b\b\b%9d", max_wi - wi);
    }
  bow_verbosify (bow_progress, "\n");
  return ret;
}

/* Return a malloc()'ed array containing an infomation-gain score for
   each word index, but the infogain scores are computing from
   co-occurance of word pairs. */
float *
bow_infogain_per_wi_new_using_pairs (bow_barrel *barrel, int num_classes,
				     int *size)
{
  /* `count' == Counts of documents.
     `pair'== Pair of words. */
  float count[num_classes];
  float count_with_pair[num_classes];
  float count_without_pair[num_classes];
  bow_cdoc *doc1, *doc2;
  double entropy_unconditional;
  double entropy_with_pair;
  double entropy_without_pair;

  int max_wi = MIN (barrel->wi2dvf->size, bow_num_words());
  float count_total = 0; 
  float count_with_pair_total = 0;
  float count_without_pair_total = 0;
  int i, j, wi1, wi2, dvi1, dvi2;
  bow_dv *dv1, *dv2;
#if 0
  struct _igpair {
    float ig;
    int wi1;
    int wi2;
  } igpair[max_wi*max_wi];
#else
  float ig;
#endif

  bow_verbosify (bow_progress, 
		 "Calculating info gain... words ::          ");

  *size = max_wi;

  /* First set all the arrays to zero */
  for(i = 0; i < num_classes; i++) 
    {
      count[i] = 0;
      count_with_pair[i] = 0;
      count_without_pair[i] = 0;
    }

  /* Now set up the unconditional counts totals. */
  for (i = 0; i < barrel->cdocs->length ; i++)
    {
      doc1 = bow_cdocs_di2doc (barrel->cdocs, i);
      if (doc1->type == model) 
	{
	  count[doc1->class] += doc1->prior;
	  count_total += doc1->prior;
	}
    }

  /* Calculate the unconditional entropy */
  entropy_unconditional = bow_entropy (count, num_classes);

  /* Now loop over all pairs of words. */
  for (wi1 = 0; wi1 < max_wi; wi1++) 
    {
      for (wi2 = wi1+1; wi2 < max_wi; wi2++)
	{
	  /* Get the document vectors */
	  dv1 = bow_wi2dvf_dv (barrel->wi2dvf, wi1);
	  dv2 = bow_wi2dvf_dv (barrel->wi2dvf, wi2);
	  if (dv1 == NULL || dv2 == NULL)
	    {
	      /* igpair[wi1][wi2] = 0; */
	      continue;
	    }

	  count_with_pair_total = 0;

	/* Create totals for this pair of dv's.
	   ...i.e. find documents in which both WI1 and WI2 occur. */
	  for (dvi1 = 0, dvi2 = 0; dvi1 < dv1->length; dvi1++)
	    {
	      /* Find the entry in DV2 for the same document, if it exists. */
	      while (dv1->entry[dvi1].di > dv2->entry[dvi2].di
		     && dvi2 < dv2->length)
		dvi2++;
	      if (dv1->entry[dvi1].di != dv2->entry[dvi2].di)
		continue;
	      doc1 = bow_cdocs_di2doc (barrel->cdocs, dv1->entry[dvi1].di);
	      doc2 = bow_cdocs_di2doc (barrel->cdocs, dv2->entry[dvi2].di);
	      /* We found a document with both WI1 and WI2 */
	      if (doc1->type == model && doc2->type == model)
		{
		  count_with_pair[doc1->class] += doc1->prior;
		  count_with_pair_total += doc1->prior;
		}
	    }

	  /* Set the without-pair totals. */
	  for (j = 0; j < num_classes; j++)
	    {
	      count_without_pair[j] = count[j] - count_with_pair[j];
	    }
	  count_without_pair_total = count_total - count_with_pair_total;

	/* Calculate entropies */
	  entropy_with_pair = bow_entropy (count_with_pair, num_classes);
	  entropy_without_pair = bow_entropy (count_without_pair, num_classes);

	/* Calculate and store the information gain. */
	  ig =
	    (entropy_unconditional 
	     - ((((double)count_with_pair_total / count_total)
		 * entropy_with_pair)
		+ (((double)count_without_pair_total / count_total) 
		   * entropy_without_pair)));
	  /* Not comparing with 0 here because of round-off error. */
	  assert (ig >= -1e-7);

	  if (ig < 0)
	    ig = 0;

	  if (ig > 0.01)
	    printf ("%12.9f  %20s %20s\n", 
		    ig, bow_int2word (wi1), bow_int2word (wi2));

	  /* Reset arrays to zero */
	  for(i = 0; i < num_classes; i++) 
	    {
	      count_with_pair[i] = 0;
	      count_without_pair[i] = 0;
	    }
	}
      if (wi1 % 100 == 0)
	bow_verbosify (bow_progress,
		       "\b\b\b\b\b\b\b\b\b%9d", max_wi - wi1);
    }
  bow_verbosify (bow_progress, "\n");

#if 0
  /* Now loop over all pairs of words, printing the result. */
  for (wi1 = 0; wi1 < max_wi; wi1++) 
    for (wi2 = 0; wi2 < max_wi; wi2++)
      {
	printf ("%8.5f %20s %20s\n",
		igpair[wi1][wi2],
		bow_int2word (wi1),
		bow_int2word (wi2));
      }
#endif
  return NULL;
}


/* Print to stdout the sorted results of bow_infogain_per_wi_new().
   It will print the NUM_TO_PRINT words with the highest infogain. */
void
bow_infogain_per_wi_print (FILE *fp, bow_barrel *barrel, int num_classes, 
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
  if (num_to_print == 0)
    num_to_print = wi2ig_size;

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
      fprintf (fp, "%8.5f %s\n", wiigs[i].ig, bow_int2word (wiigs[i].wi));
    }
  bow_free (wi2ig);
}

