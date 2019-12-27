/* Produce a vector-per-class description of the model data in a barrel */

#include "libbow.h"

extern void
_bow_barrels_set_naivebayes_vpc_priors (bow_barrel *doc_barrel,
					bow_barrel *vpc_barrel);

/* Given a barrel of documents, create and return another barrel with
   only one vector per class. The classes will be represented as
   "documents" in this new barrel. */
bow_barrel *
bow_barrel_new_vpc (bow_barrel *doc_barrel, const char **classnames)
{
  bow_barrel* vpc_barrel;	/* The vector per class barrel */
  int max_ci = -1;		/* The highest index of encountered classes */
  int wi;
  int max_wi;
  int dvi;
  int ci;
  bow_dv *dv;
  bow_dv *vpc_dv;
  int di;

  max_wi = MIN (doc_barrel->wi2dvf->size, bow_num_words ());

  /* Create an empty barrel; we fill fill it with vector-per-class
     data and return it. */
  vpc_barrel = bow_barrel_new (doc_barrel->wi2dvf->size,
			       doc_barrel->cdocs->length,
			       doc_barrel->cdocs->entry_size,
			       doc_barrel->cdocs->free_func);
  vpc_barrel->method = doc_barrel->method;

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
	  if (ci > max_ci)
	    max_ci = ci;
	  if (cdoc->type == model)
	    bow_wi2dvf_add_wi_di_count_weight (&(vpc_barrel->wi2dvf), 
					       wi, ci, 
					       dv->entry[dvi].count,
					       dv->entry[dvi].weight);
	}
      /* Set the IDF of the class's wi2dvf directly from the doc's wi2dvf */
      vpc_dv = bow_wi2dvf_dv (vpc_barrel->wi2dvf, wi);
      if (vpc_dv)		/* xxx Why would this be NULL? */
	vpc_dv->idf = dv->idf;
      if (wi % 100 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", max_wi - wi);
    }

  /* Initialize the CDOCS part of the VPC_BARREL.  Create BOW_CDOC
     structures for each class, and append them to the VPC->CDOCS
     array. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc cdoc;
      cdoc.type = model;
      cdoc.normalizer = -1.0f;
      if (classnames)
	{
	  assert (classnames[ci]);
	  cdoc.filename = strdup (classnames[ci]);
	  if (!cdoc.filename)
	    bow_error ("Memory exhausted.");
	}
      else
	{
	  abort ();		/* xxx temporarily */
	  cdoc.filename = NULL;
	}
      cdoc.class = ci;
      bow_array_append (vpc_barrel->cdocs, &cdoc);
    }

  if (doc_barrel->method == bow_method_naivebayes
      || doc_barrel->method == bow_method_prind)
    {
      /* If we're doing NaiveBayes, set the prior probabilities on classes. */
      _bow_barrels_set_naivebayes_vpc_priors (doc_barrel, vpc_barrel);
    }
  else
    {
      /* We don't need priors for the other methods.  Set them to
	 obviously bogus values, so we'll notice if the accidently get
	 used. */
      for (ci = 0; ci <= max_ci; ci++)
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
   weights appropriately. */
bow_barrel *
bow_barrel_new_vpc_with_weights (bow_barrel *doc_barrel, 
				 const char **classnames)
{
  bow_barrel *vpc_barrel;

  if (doc_barrel->method == bow_method_naivebayes
      || doc_barrel->method == bow_method_prind)
    {
      /* Merge documents into classes, then set weights. */
      vpc_barrel = bow_barrel_new_vpc (doc_barrel, classnames);
      bow_barrel_set_weights (vpc_barrel);
      bow_barrel_set_weight_normalizers (vpc_barrel);

    }
  else
    {
      /* Set weights, then merge documents into classes. */
      bow_barrel_set_weights (doc_barrel);
      vpc_barrel = bow_barrel_new_vpc (doc_barrel, classnames);
      bow_barrel_set_weight_normalizers (vpc_barrel);
    }

  return vpc_barrel;
}
