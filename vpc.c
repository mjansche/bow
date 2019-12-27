/* Produce a vector-per-class description of the model data in a barrel */

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


/* Given a barrel of documents, create and return another barrel with
   only one vector per class. The classes will be represented as
   "documents" in this new barrel. */
bow_barrel *
bow_barrel_new_vpc (bow_barrel *doc_barrel)
{
  bow_barrel* vpc_barrel;	/* The vector per class barrel */
  int max_ci = -1;		/* The highest index of encountered classes */
  int num_classes = bow_barrel_num_classes (doc_barrel);
  int wi;
  int max_wi;
  int dvi;
  int ci;
  bow_dv *dv;
  bow_dv *vpc_dv;
  int di;
  int num_docs_per_ci[num_classes];

  assert (doc_barrel->classnames);

  for (ci = 0; ci < num_classes; ci++)
    num_docs_per_ci[ci] = 0;

  max_wi = MIN (doc_barrel->wi2dvf->size, bow_num_words ());

  /* Create an empty barrel; we fill fill it with vector-per-class
     data and return it. */
  vpc_barrel = bow_barrel_new (doc_barrel->wi2dvf->size,
			       num_classes,
			       doc_barrel->cdocs->entry_size,
			       doc_barrel->cdocs->free_func);
  vpc_barrel->method = doc_barrel->method;
  vpc_barrel->classnames = bow_int4str_new (0);

  bow_verbosify (bow_progress, "Making vector-per-class... words ::       ");

  /* Initialize the WI2DVF part of the VPC_BARREL.  Sum together the
     counts and weights for individual documents, grabbing only the
     training documents. */
  for (wi = 0; wi < max_wi; wi++)
    {
      dv = bow_wi2dvf_dv (doc_barrel->wi2dvf, wi);
      if (!dv)
	continue;
      for (dvi = 0; dvi < dv->length; dvi++)
	{
	  bow_cdoc *cdoc;
	  di = dv->entry[dvi].di;
	  cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	  ci = cdoc->class;
	  assert (ci >= 0);
	  assert (ci < num_classes);
	  if (ci > max_ci)
	    max_ci = ci;
	  if (cdoc->type == model)
	    {
	      bow_wi2dvf_add_wi_di_count_weight (&(vpc_barrel->wi2dvf), 
						 wi, ci, 
						 dv->entry[dvi].count,
						 dv->entry[dvi].weight);
	      num_docs_per_ci[ci]++;
	    }
	}
      /* Set the IDF of the class's wi2dvf directly from the doc's wi2dvf */
      vpc_dv = bow_wi2dvf_dv (vpc_barrel->wi2dvf, wi);
      if (vpc_dv)		/* xxx Why would this be NULL? */
	vpc_dv->idf = dv->idf;
      if (wi % 100 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", max_wi - wi);
    }
  assert (num_classes-1 == max_ci);
  if (max_ci < 0)
    {
      int i;
      bow_verbosify (bow_progress, "%s: No data found for ",
		     __PRETTY_FUNCTION__);
      for (i = 0; i < num_classes; i++)
	bow_verbosify (bow_progress, "%s ", 
		       bow_barrel_classname_at_index (doc_barrel, i));
      bow_verbosify (bow_progress, "\n");
    }


  /* Initialize the CDOCS and CLASSNAMES parts of the VPC_BARREL.
     Create BOW_CDOC structures for each class, and append them to the
     VPC->CDOCS array. */
  for (ci = 0; ci < num_classes; ci++)
    {
      bow_cdoc cdoc;
      const char *classname = NULL;

      cdoc.type = model;
      cdoc.normalizer = -1.0f;
      /* Make WORD_COUNT be the number of documents in the class. */
      cdoc.word_count = num_docs_per_ci[ci];
      if (doc_barrel->classnames)
	{
	  classname = bow_barrel_classname_at_index (doc_barrel, ci);
	  cdoc.filename = strdup (classname);
	  if (!cdoc.filename)
	    bow_error ("Memory exhausted.");
	}
      else
	{
	  cdoc.filename = NULL;
	}
      cdoc.class_probs = NULL;
      cdoc.class = ci;
      /* Add a CDOC for this class to the VPC_BARREL */
      bow_array_append (vpc_barrel->cdocs, &cdoc);
      /* Add an entry for this class into the VPC_BARREL->CLASSNAMES map. */
      bow_str2int (vpc_barrel->classnames, classname);
    }

  if (doc_barrel->method->vpc_set_priors)
    {
      /* Set the prior probabilities on classes, if we're doing
	 NaiveBayes or something else that needs them.  */
      (*doc_barrel->method->vpc_set_priors) (vpc_barrel, doc_barrel);
    }
  else
    {
      /* We don't need priors, so set them to obviously bogus values,
	 so we'll notice if they accidently get used. */
      for (ci = 0; ci < num_classes; ci++)
	{
	  bow_cdoc *cdoc;
	  cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
	  cdoc->prior = -1;
	}
    }

  bow_verbosify (bow_progress, "\n");

  return vpc_barrel;
}

/* Like bow_barrel_new_vpc(), but it also sets and normalizes the
   weights appropriately by calling SET_WEIGHTS from the METHOD of
   DOC_BARREL on the `vector-per-class' barrel that will be returned. */
bow_barrel *
bow_barrel_new_vpc_merge_then_weight (bow_barrel *doc_barrel)
{
  bow_barrel *vpc_barrel;

  assert (doc_barrel->method->name);
  /* Merge documents into classes, then set weights. */
  vpc_barrel = bow_barrel_new_vpc (doc_barrel);
  bow_barrel_set_weights (vpc_barrel);
  /* Scale the weights */
  bow_barrel_scale_weights (vpc_barrel, doc_barrel);
  /* Normalize the weights. */
  bow_barrel_normalize_weights (vpc_barrel);
  return vpc_barrel;
}

/* Same as above, but set the weights in the DOC_BARREL, create the
   `Vector-Per-Class' barrel, and set the weights in the VPC barrel by
   summing weights from the DOC_BARREL. */
bow_barrel *
bow_barrel_new_vpc_weight_then_merge (bow_barrel *doc_barrel)
{
  bow_barrel *vpc_barrel;

  /* Set weights, then merge documents into classes. */
  bow_barrel_set_weights (doc_barrel);
  vpc_barrel = bow_barrel_new_vpc (doc_barrel);
  bow_barrel_scale_weights (vpc_barrel, doc_barrel);
  bow_barrel_normalize_weights (vpc_barrel);
  return vpc_barrel;
}

/* Set the class prior probabilities by counting the number of
   documents of each class. */
void
bow_barrel_set_vpc_priors_by_counting (bow_barrel *vpc_barrel,
				       bow_barrel *doc_barrel)
					
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
      if (doc_cdoc->class >= vpc_barrel->cdocs->length)
	{
	  /* This can happen if all of the documents in a certain class
	     contain only words that are not in the vocabulary used
	     when running bow_barrel_new_vpc() above. */
	  continue;
	}
      vpc_cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
					   doc_cdoc->class);
      vpc_cdoc->prior += doc_cdoc->prior;
    }
  /* Sum them all. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      prior_sum += cdoc->prior;
    }
  assert (prior_sum > 0);
  /* Normalize to set the prior. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      cdoc->prior /= prior_sum;
      /*printf ("ci=%d  prior_sum=%f  prior=%f\n", ci,prior_sum,cdoc->prior);*/
      /* xxx We allow "cdoc->prior >= 0.0" because there may be no
	 training data for some class.  Is this good? */
      assert (cdoc->prior >= 0.0 && cdoc->prior <= 1.0);
    }
}
