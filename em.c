/* Weight-setting and scoring implementation for EM classification */

/* Copyright (C) 1997 Andrew McCallum

   Written by:  Kamal Nigam <knigam@cs.cmu.edu>

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
#include <stdlib.h>

typedef enum
{ confusion,
  rank, 
  pr,				/* fancy with windows */
  simple,			/* 1 or 0 based on winning class */
  nb_score			/* score directly from naivebayes */
} bow_em_stat_method;

typedef enum { word, document } bow_em_event_model_type;

/* Don't touch this. */
static int ignored_model_are_false_unknown = 0;

/* If non-zero, use leave-one-out evaluation for score->prob mapping.
   Always do this if USE_TRAIN_FOR_STATS is 1. */
static int use_leave_one_out = 1;

/* If non-zero, classify type=model documents for building score->prob
   mapping.  If this is zero, then it uses the type=ignored documents
   for it. */
static int use_train_for_stats = 1;

/* Self explanatory.  If zero, then initial class probs are also zero,
   and these documents have no effect when building the initial model. */
static int use_priors_for_initial_class_probs = 1;

/* Whether to use a percentage of training documents as unlabeled, or
   to use a specified number per class as training.  If this is
   non-zero, use UNKNOWN_PERCENT, if this is zero, then use
   NOT_UNKNOWN_NUM_BY_CLASS */
static int use_unknown_percent = 0;

/* Is USE_TRAIN_FOR_STATS is zero, then use this percentage to specify
   how many of the training documents to hold for building the
   score->prob mapping. */
static int ignored_model_percent = 33;

/* To weight the unlabeled vs the training documents.  The relative
   weight an unlabeled document has relative to a labeled document. */
static float unlabeled_normalizer = 1;

/* Num EM iterations. */
static int bow_em_num_em_runs = 7;

/* for mapping scores->probs */
static int bow_em_pr_window_size = 20;

/* static int em_m_est = 1; */

/* Always leave zero.  If non-zero, remember best barrel by
   classification on the scoring document, i.e. either the
   leave-one-out or the ignore_model.  EM should get the best one last
   anyway, though */
static int bow_em_take_best_barrel = 0;

/* If non-zero, print the top words by class. */
static int bow_em_print_word_vector = 0;

/* If non-zero, print summary of the score->prob mapping. */
static int bow_em_print_stat_summary = 0;

typedef struct _bow_em_pr_struct {
  float score;
  int correct;
} bow_em_pr_struct;

/* For changing weight of unseen words.
   I really should implement `deleted interpolation' */
/* M_EST_P summed over all words in the vocabulary must sum to 1.0! */
#if 1
/* This is the special case of the M-estimate that is `Laplace smoothing' */
#define M_EST_M  (barrel->wi2dvf->num_words)
#define M_EST_P  (1.0 / barrel->wi2dvf->num_words)
#define WORD_PRIOR_COUNT 1.0
#else
#define M_EST_M  (cdoc->word_count \
		  ? (((float)barrel->wi2dvf->num_words) / cdoc->word_count) \
		  : 1.0)
#define M_EST_P  (1.0 / barrel->wi2dvf->num_words)
#endif

/* Command-line options specific to EM */

/* Default value for option "em-unlabeled-class". */
/* The variables that can be changed on the command line: */

static char * em_unlabeled_classname = NULL;
static int em_compare_to_nb = 0;
static int unknown_percent = 87;
static int not_unknown_num_by_class = 10;
static bow_em_event_model_type bow_em_event_model = document;
static bow_em_stat_method em_stat_method = nb_score;

/* The integer or single char used to represent this command-line option.
   Make sure it is unique across all libbow and rainbow. */
#define EM_UNLABELED_CLASS_KEY 2222 /* UNIQUE NUMBER HERE!!! */
#define EM_COMPARE_TO_NB 2223
#define EM_UNLABELED_PERCENT 2224
#define EM_UNLABELED_NUM_BY_CLASS 2225
#define EM_EVENT_MODEL 2226
#define EM_STAT_METHOD 2227

static struct argp_option em_options[] =
{
  {0,0,0,0,
   "EM options:", 1},
  {"em-unlabeled-class", EM_UNLABELED_CLASS_KEY, "CLASS", 0,
   "When using the EM method, the class in the document barrel "
   "that has unlabeled documents.  The default is `unlabeled`."},
  {"em-compare-to-nb", EM_COMPARE_TO_NB, 0, 0,
   "When building an EM class barrel, show doc stats for the naivebayes"
   "barrel equivalent"},
  {"em-unlabeled-percent", EM_UNLABELED_PERCENT, "PERCENT", 0,
   "When using the EM method, the percent of documents to make"
   "unlabeled.  The default is 87."},
  {"em-labeled-num-by-class", EM_UNLABELED_NUM_BY_CLASS, "NUM", 0,
   "When using the EM method, the number of documents per class to"
   "make labeled.  The default is 10."},
  {"em-event-model", EM_EVENT_MODEL, "MODEL", 0,
   "When using the EM method, use the 'document' event model or the"
   "'word' event model.  The default is 'document'."},
  {"em-stat-method", EM_STAT_METHOD, "STAT", 0,
   "When using the EM method, how to convert scores to probabilities."
   "The default is 'nb_score'."},
  {0, 0}
};

error_t
em_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case EM_UNLABELED_CLASS_KEY:
      em_unlabeled_classname = arg;
      break;
    case EM_COMPARE_TO_NB:
      em_compare_to_nb = 1;
      break;
    case EM_UNLABELED_PERCENT:
      unknown_percent = atoi(arg);
      break;
    case EM_UNLABELED_NUM_BY_CLASS:
      not_unknown_num_by_class = atoi(arg);
      break;
    case EM_EVENT_MODEL:
      if (!strcmp(arg, "document"))
	bow_em_event_model = document;
      else if (!strcmp(arg, "word"))
	bow_em_event_model = word;
      else
	bow_error("Invalid argument for --em-event-model");
      break;
    case EM_STAT_METHOD:
      if (!strcmp(arg, "nb_score"))
	em_stat_method = nb_score;
      else if (!strcmp(arg, "pr"))
	em_stat_method = pr;
      else if (!strcmp(arg, "confusion"))
	em_stat_method = confusion;
      else if (!strcmp(arg, "simple"))
	em_stat_method = simple;
      else if (!strcmp(arg, "rank"))
	em_stat_method = rank;
      else
	bow_error("Invalid argument for --em-stat-method");
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const struct argp em_argp =
{
  em_options,
  em_parse_opt
};

static struct argp_child em_argp_child =
{
  &em_argp,		/* This child's argp structure */
  0,			/* flags for child */
  0,			/* optional header in help message */
  0			/* arbitrary group number for ordering */
};

/* End of command-line options specific to EM */


/* Given a fully-specified file path name (all the way from `/'),
   return just the last filename part of it. */
static inline const char *
filename_to_classname (const char *filename)
{
  const char *ret;
  ret = strrchr (filename, '/');
  if (ret)
    return ret + 1;
  return filename;
}

int
bow_em_pr_struct_compare (const void *x, const void *y)
{
  if (((bow_em_pr_struct *)x)->score > ((bow_em_pr_struct *)y)->score)
    return -1;
  else if (((bow_em_pr_struct *)x)->score == ((bow_em_pr_struct *)y)->score)
    return 0;
  else
    return 1;
}
  


/* Create a class barrel with EM-style clustering on unlabeled
   docs */
bow_barrel *
bow_em_new_vpc_with_weights (bow_barrel *doc_barrel)
{
  int num_classes = bow_barrel_num_classes (doc_barrel);
  bow_barrel *vpc_barrel;   /* the vector-per-class barrel */
  int wi;                   /* word index */
  int max_wi;               /* max word index */
  int dvi;                  /* document vector index */
  int ci;                   /* class index */
  bow_dv *dv;               /* document vector */
  bow_dv *vpc_dv;           /* vector-per-class document vector */
  int di;                   /* document index */
  int ri;                   /* rank index */
  float num_words_per_ci[num_classes];
  int unlabeled_ci = -1;
  int old_to_new_class_map[num_classes];
  int new_to_old_class_map[num_classes];
  bow_dv_heap *test_heap=NULL;	/* we'll extract test WV's from here */
  bow_wv *query_wv;
  bow_score *hits;
  int num_hits_to_retrieve;
  int actual_num_hits;
  int hi;			/* hit index */
  bow_cdoc *doc_cdoc;
  int old_num_correct = -1;
  int num_correct = 0;
  int num_tested;
  int em_runs = 0;
  int class_by_rank_correctness[num_classes][num_classes];
  int confusion_matrix[num_classes][num_classes];
  int num_known_docs = 0;
  float confusion_percent_matrix[num_classes][num_classes];
  int max_new_ci;
  int max_old_ci;
  int (* stat_next_wv)(bow_dv_heap *, bow_barrel *, bow_wv **);
  int (* unknown_next_wv)(bow_dv_heap *, bow_barrel *, bow_wv **);
  int unknown_set_counter = 0;
  bow_em_pr_struct *pr_by_class[num_classes];
  int num_stat_docs = 0;
  bow_wi2dvf *best_wi2dvf = NULL;
  int best_num_correct = 0;
  float best_priors[num_classes];
  int best_word_counts[num_classes];
  float best_normalizers[num_classes];

  /* initialize some variables */
#if 0
  bow_uniform_class_priors = 1;
#endif
  if (use_train_for_stats)
    stat_next_wv = bow_model_next_wv;
  else
    stat_next_wv = bow_ignored_model_next_wv;

#if 0
  stat_next_wv = bow_ignore_next_wv;
#endif

  unknown_next_wv = bow_ignore_next_wv;

  max_old_ci = num_classes;
  
  if (NULL == em_unlabeled_classname)
    max_new_ci = max_old_ci;
  else
    max_new_ci = max_old_ci - 1;
  
  num_hits_to_retrieve = max_new_ci;

  for (ci = 0; ci < max_new_ci; ci++)
    num_words_per_ci[ci] = 0;
  
  max_wi = MIN (doc_barrel->wi2dvf->size, bow_num_words ());
  
  assert (!strcmp(doc_barrel->method->name, "em"));

  /* we should probably create a copy of the doc barrel, since we're
     going to be messing with it */

  /* Create an empty barrel; we fill it with vector-per-class
     data and return it. */

  /* should the free function be a real one? */

  vpc_barrel = bow_barrel_new (doc_barrel->wi2dvf->size,
			       doc_barrel->cdocs->length-1,
			       doc_barrel->cdocs->entry_size,
			       doc_barrel->cdocs->free_func); 
  vpc_barrel->method = doc_barrel->method;
  vpc_barrel->classnames = bow_int4str_new (0);

  /* setup the cdoc structure for the class barrel, except for the
     word counts and normalizer, which we'll do later.  Notice which
     is the unlabeled class (unlabeled_ci) */

  assert (doc_barrel->classnames);

  for (ci = 0; ci < max_old_ci; ci++)
    {
      if (max_old_ci != max_new_ci &&
	  -1 == unlabeled_ci && 
	  !strcmp(em_unlabeled_classname, 
		  filename_to_classname
		  (bow_barrel_classname_at_index (doc_barrel, ci))))
	{
	  unlabeled_ci = ci;
	  old_to_new_class_map[ci] = -1;
	}
      else
	{
	  bow_cdoc cdoc;
	  
	  /* create the cdoc structure & set the mapping */
	  if (unlabeled_ci == -1)
	    {
	      old_to_new_class_map[ci] = ci;
	      new_to_old_class_map[ci] = ci;
	    }
	  else
	    {
	      old_to_new_class_map[ci] = ci - 1;
	      new_to_old_class_map[ci-1] = ci;
	    }
	     
	  cdoc.type = model;
	  cdoc.normalizer = -1.0f; /* just a temporary measure */
	  cdoc.word_count = 0; /* just a temporary measure */
	  cdoc.filename = strdup (bow_barrel_classname_at_index (doc_barrel, 
								 ci));
	  bow_barrel_add_classname(vpc_barrel, cdoc.filename);
	  if (!cdoc.filename)
	    bow_error ("Memory exhausted.");
	  cdoc.class_probs = NULL;
	  cdoc.class = old_to_new_class_map[ci];
	  bow_array_append (vpc_barrel->cdocs, &cdoc);
	}
    }

  /* if there's an unlabeled class, make sure we found it */
  
  if (max_old_ci != max_new_ci && -1 == unlabeled_ci)
    bow_error ("No such unlabeled class %s.", em_unlabeled_classname);

  /* if there are test docs, move them to ignored_model for the 
     time being */

  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      
      if (cdoc->type == test)
	cdoc->type = ignored_model;
      else
	{
	  cdoc->type = model;
	  num_known_docs++;
	}
    }

  /* make sure all the docs we have now have word_count set correctly */

  query_wv = NULL;

  /* Create the heap from which we'll get WV's. */
  test_heap = bow_test_new_heap (doc_barrel);
  
  /* Loop once for each model document. */
  while ((di = bow_model_next_wv (test_heap, doc_barrel, &query_wv))
	 != -1)
    {
      int word_count = 0;
      int wvi;

      doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, 
					   di);
      bow_wv_set_weights (query_wv, vpc_barrel);
      bow_wv_normalize_weights (query_wv, vpc_barrel);
      
      for (wvi = 0; wvi < query_wv->num_entries; wvi++)
	{
	  word_count += query_wv->entry[wvi].count;
	}

      doc_cdoc->word_count = word_count;
    }


  /* if there's an unlabeled class, set all those docs to ignore docs.
     Otherwise, do a random split to set up ignore docs */
  
  if (-1 == unlabeled_ci)
    {
      
      /* take unknown_percent of the model docs to be unlabeled */
      
      if (use_unknown_percent && unknown_percent == 100)
	{
	  num_known_docs = 0;

	  /* if 100% then set all model docs to be test */
	  for (di=0; di < doc_barrel->cdocs->length; di++)
	    {
	      bow_cdoc *cdoc = bow_array_entry_at_index 
		(doc_barrel->cdocs, di);
	      
	      if (cdoc->type == model)
		cdoc->type = test;
	    }
	}
      else
	{
	  if (use_unknown_percent)
	    bow_test_split(doc_barrel, 
			   (num_known_docs * unknown_percent) / 100);
	  else
	    bow_test_split2(doc_barrel, not_unknown_num_by_class);
	  
	  /* set all the test docs to be ignore docs (unlabeled) */

	  num_known_docs = 0;

	  for (di=0; di < doc_barrel->cdocs->length; di++)
	    {
	      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	      
	      if (cdoc->type == test)
		cdoc->type = ignore;
	      else if (cdoc->type == model)
		num_known_docs++;
	    }
	}
    }
  else 
    {
      num_known_docs = 0;
      for (di=0; di < doc_barrel->cdocs->length; di++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	  
	  if (cdoc->class == unlabeled_ci)
	    cdoc->type = ignore;
	  else
	    {
	      num_known_docs++;
	      cdoc->type = model;
	    }
	}
    }

#if 1
  /* if we're comparing to naivebayes, do that now */
  
  if (em_compare_to_nb == 1)
    {
      assert (unlabeled_ci == -1);
      
      /* set the ignored_model docs back to test docs */

      for (di=0; di < doc_barrel->cdocs->length; di++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	  
	  if (cdoc->type == ignored_model)
	    cdoc->type = test;
	}

      bow_em_compare_to_nb(doc_barrel);

      /* set the test docs back to ignored_model */

      for (di=0; di < doc_barrel->cdocs->length; di++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	  
	  if (cdoc->type == test)
	    cdoc->type = ignored_model;
	}
    }
#endif

  if (!use_train_for_stats)
    {
      /* now split the known docs into model & false unknown or model confusion
	 model docs */
      
      bow_test_split(doc_barrel, 
		     (num_known_docs * ignored_model_percent) / 100);
    }

  /* switch ignored_model and test docs and count num_stat_docs */

  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      
      if (cdoc->type == test)
	{
	  cdoc->type = ignored_model;
	  if (!use_train_for_stats)
	    num_stat_docs++;
	}
      else if (cdoc->type == ignored_model)
	cdoc->type = test;
      else if (use_train_for_stats && (cdoc->type == model))
	num_stat_docs++;
    }

#if 0
  /* if we're comparing to naivebayes, do that now.  Note that doing
     this here is badness.  It makes the comparison after the
     ignored_model docs have been taken away. */
  
  if (em_compare_to_nb == 1)
    {
      assert (unlabeled_ci == -1);
      
      bow_em_compare_to_nb(doc_barrel);

    }
#endif 

#if 0
  /* only do this badness if you want to use ignore docs for stats */

  num_stat_docs = 0;

  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      
      if (cdoc->type == ignore)
	num_stat_docs++;
    }
#endif

  /* cycle through the document barrel and make sure that each
     document has a correctly initialized class_probs structure. 
     set class_probs of model docs.
     */
  
  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);

      /* note that these class_probs indexes are indexes into 
	 the NEW class indexes not the OLD ones! */

      if (!cdoc->class_probs)
	cdoc->class_probs = bow_malloc (sizeof (float) * max_new_ci);

      /* initialize the class_probs to zeros - need if we're using
	 priors to set unknown class_probs */

      for (ci=0; ci < max_new_ci; ci++)
	cdoc->class_probs[ci] = 0.0;
      
      if (cdoc->type == model ||
	  (!ignored_model_are_false_unknown && cdoc->type == ignored_model))
	{
	  /* if it's a known doc, set its class_probs that way */
	  
	  cdoc->class_probs[old_to_new_class_map[cdoc->class]] = 1.0;
	}
    }

  /* set priors on just the known docs, since we'll need them 
     if using them for setting class_probs */

  (*doc_barrel->method->vpc_set_priors) (vpc_barrel, doc_barrel);

  /* set the class probs of all the unknown and false unknown docs */

  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);

      if (!(cdoc->type == ignore ||
	    (ignored_model_are_false_unknown && cdoc->type == ignored_model)))
	continue;

      if (use_priors_for_initial_class_probs)
	{
	  /* distribute class_probs according to priors on just the known */
	  
	  for (ci=0; ci < max_new_ci; ci++)
	    {
	      bow_cdoc *class_cdoc = bow_array_entry_at_index 
		(vpc_barrel->cdocs, ci);
	      
	      cdoc->class_probs[ci] = class_cdoc->prior;
	    }
	}
      else
	{
	  /* set class_probs as all zeros (ignore them for first M step) */
	  for (ci=0; ci < max_new_ci; ci++)
	    cdoc->class_probs[ci] = 0.0;
	}
    }


  if (em_stat_method == pr)
    {
      /* malloc some space for pr stats */
      for (ci = 0; ci < max_new_ci; ci++)
	pr_by_class[ci] = bow_malloc(sizeof(bow_em_pr_struct) * num_stat_docs);
    }

  /* let's do some EM */
  while (em_runs < bow_em_num_em_runs)
  
  /*  while (num_correct > old_num_correct) */
    {
      em_runs++;

      /* the M-step */
      
      /* get a new wi2dvf structure for our class barrel */

      /* save the best wi2dvf so far */

      if (num_correct > best_num_correct)
	{
	  if (best_wi2dvf != NULL)
	    bow_wi2dvf_free(best_wi2dvf);
	  best_wi2dvf = vpc_barrel->wi2dvf;
	  best_num_correct = num_correct;
	  for (ci = 0; ci < max_new_ci; ci++)
	    {
	      bow_cdoc *cdoc = bow_array_entry_at_index(vpc_barrel->cdocs, ci);

	      best_priors[ci] = cdoc->prior;
	      best_word_counts[ci] = cdoc->word_count;
	      best_normalizers[ci] = cdoc->normalizer;
	    }
	}
      else
	{
	  bow_wi2dvf_free (vpc_barrel->wi2dvf);
	}
      
      vpc_barrel->wi2dvf = bow_wi2dvf_new (doc_barrel->wi2dvf->size);
	
      /* Initialize the WI2DVF part of the VPC_BARREL.  Sum together the
	 counts and weights for individual documents, grabbing only the
	 training documents. */

      bow_verbosify (bow_progress, 
		     "Making class barrel by counting words:       ");

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

	      /* only collate the docs not being held aside */

	      if (!(cdoc->type == test ||
		    (cdoc->type == ignored_model && 
		     !ignored_model_are_false_unknown)))
		{

		  assert(cdoc->word_count > 0);
		  for (ci=0; ci < max_new_ci; ci++)
		    if (0.0 != cdoc->class_probs[ci])
		      {

			/* normalize for document length if
			   event model is document */

			if (bow_em_event_model == document)
			  bow_wi2dvf_add_wi_di_count_weight 
			    (&(vpc_barrel->wi2dvf), 
			     wi, ci, 
			     1,  /* hopelessly dummy value */
			     (cdoc->class_probs[ci] *
			      (float) dv->entry[dvi].count * 200 / 
			      cdoc->word_count));
			else if (bow_em_event_model == word)
			  bow_wi2dvf_add_wi_di_count_weight 
			    (&(vpc_barrel->wi2dvf), 
			     wi, ci, 
			     1,  /* hopelessly dummy value */
			     (cdoc->class_probs[ci] *
			      (float) dv->entry[dvi].count));
		      }
		}
	    }

	  /* Set the IDF of the class's wi2dvf directly from the doc's
             wi2dvf */
	  
	  vpc_dv = bow_wi2dvf_dv (vpc_barrel->wi2dvf, wi);
	  if (vpc_dv)		/* xxx Why would this be NULL? */
	    vpc_dv->idf = dv->idf;
	  if (wi % 100 == 0)
	    bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", max_wi - wi);
	}

      bow_verbosify (bow_progress, "\n");
      
      /* cycle through each word in vpc barrel, collecting word_count
	 and unique words per class for the normalizer and set them in
	 the cdoc for that class */

      for (ci=0; ci < max_new_ci; ci++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);

	  num_words_per_ci[ci] = 0;
	  cdoc->normalizer = 0;
	}

      for (wi = 0; wi < max_wi; wi++)
	{
	  dv = bow_wi2dvf_dv (vpc_barrel->wi2dvf, wi);
	  if (!dv)
	    continue;
	  for (dvi = 0; dvi < dv->length; dvi++)
	    {
	      if (0 != dv->entry[dvi].weight)
		{
		  bow_cdoc *cdoc = 
		    bow_array_entry_at_index (vpc_barrel->cdocs, 
					      dv->entry[dvi].di);
		  
		  if (cdoc->type != test)
		    {
		      num_words_per_ci[dv->entry[dvi].di] += 
			dv->entry[dvi].weight;
		      cdoc->normalizer++;
		    }
		}
	    }
	}

      for (ci=0 ; ci < max_new_ci; ci++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);

	  cdoc->word_count = (int) rint(num_words_per_ci[ci]);
	}
	      
      /* set priors */

      if (doc_barrel->method->vpc_set_priors)
	{
	  /* Set the prior probabilities on classes, if we're doing
	     NaiveBayes or something else that needs them.  */
	  (*doc_barrel->method->vpc_set_priors) (vpc_barrel, doc_barrel);
	}
      else
	{
	  /* We don't need priors for the other methods.  Set them to
	     obviously bogus values, so we'll notice if they accidently
	     get used. */
	  for (ci = 0; ci <= max_new_ci; ci++)
	    {
	      bow_cdoc *cdoc;
	      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
	      cdoc->prior = -1;
	    }
	}

      /* print top words by class */

      if (bow_em_print_word_vector)
	bow_em_print_log_odds_ratio(vpc_barrel, 10);

      /* OK.  we're done with our M-step.  We have a new vpc barrel to 
	 use.  Let's now do the E-step, and classify all our documents. */

      /* Initialize QUERY_WV so BOW_TEST_NEXT_WV() knows not to try to free */
      query_wv = NULL;
      
      hits = alloca (sizeof (bow_score) * num_hits_to_retrieve);
      
      old_num_correct = num_correct;
      num_correct = 0;
      num_tested = 0;

      /* collect stats so we can estimate probabilities of classification 
	 classify model or ignored_model docs to build stats */

      if (em_stat_method == confusion)
	{
	  /* reset the confusion matrix */
	  for (ci=0; ci < max_new_ci; ci++)
	    {
	      int ci2;
	      
	      for (ci2=0; ci2 < max_new_ci; ci2++)
		confusion_matrix[ci][ci2] = 0;
	    }
	}
      else if (em_stat_method == rank)
	{
	  /* reset the class_by_rank matrix */
	  for (ci = 0; ci < max_new_ci; ci++)
	    for (ri = 0; ri < max_new_ci; ri++)
	      class_by_rank_correctness[ci][ri] = 0;
	}

      bow_verbosify(bow_progress, "Classifying documents for stats:       ");

      /* Create the heap from which we'll get WV's. */
      test_heap = bow_test_new_heap (doc_barrel);
      
      /* Loop once for each model document. */
      while ((di = (*stat_next_wv) (test_heap, doc_barrel, &query_wv))
	     != -1)
	{
	  doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, 
					       di);
	  bow_wv_set_weights (query_wv, vpc_barrel);
	  bow_wv_normalize_weights (query_wv, vpc_barrel);
	  actual_num_hits = 
	    bow_barrel_score (vpc_barrel, 
			      query_wv, hits,
			      num_hits_to_retrieve, 
			      (use_leave_one_out ? 
			       old_to_new_class_map[doc_cdoc->class] : -1));
	    
	  assert (actual_num_hits == num_hits_to_retrieve);

	  /* see if it was correctly classified */

	  if (doc_cdoc->class == new_to_old_class_map[hits[0].di])
	    num_correct++;
	  
	  if (em_stat_method == confusion)
	    {
	      /* Put it in the confusion matrix */
	  
	      confusion_matrix[old_to_new_class_map[doc_cdoc->class]]
		[hits[0].di] += 1;
	    }
	  else if (em_stat_method == rank)
	    {
	      /* note what rank the correct class was in */
	      ri = 0;
	      while (doc_cdoc->class !=
		     new_to_old_class_map[hits[ri].di])
		ri++;
	      class_by_rank_correctness[hits[ri].di][ri] += 1;
	    }
	  else if (em_stat_method == pr)
	    {
	      for (hi = 0; hi < num_hits_to_retrieve; hi++)
		{
		  pr_by_class[hits[hi].di][num_tested].score = hits[hi].weight;
		  pr_by_class[hits[hi].di][num_tested].correct = 
		    (doc_cdoc->class == new_to_old_class_map[hits[hi].di] 
		     ? 1 : 0);
		}
	    }

	  if (num_tested % 100 == 0)
	    bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", num_tested);
	
	  num_tested++;	  
	}
      

      num_stat_docs = num_tested;

      /*      assert(num_tested == num_stat_docs); */

      bow_verbosify(bow_progress, 
		    "\b\b\b\b\b\b%6d\nCorrectly Classified: %6d\n\n", 
		    num_tested, num_correct);

      /* finish collating and normalizing the probability stats */

      if (em_stat_method == confusion)
	{
	  /* print the confusion matrix  */
	  
	  if (bow_em_print_stat_summary)
	    {
	      for (ci = 0; ci < max_new_ci; ci++)
		{
		  int ci2;
		  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
							     ci);
		  
		  bow_verbosify(bow_progress, "%25s ", 
				filename_to_classname(cdoc->filename));
		  
		  for (ci2 = 0; ci2 < max_new_ci; ci2++)
		    {
		      bow_verbosify(bow_progress, "%3d ", 
				    confusion_matrix[ci][ci2]);
		    }
		  bow_verbosify(bow_progress, "\n");
		}
	      bow_verbosify(bow_progress, "\n");
	    }
      
	  /* calculate the confusion matrix with percentages */
      
	  for (ci = 0; ci < max_new_ci; ci++)
	    {
	      int total = 0;
	      int ci2;
	      float total2 = 0;
	  
	      /* get total for a column */
	      for (ci2 = 0; ci2 < max_new_ci; ci2++)
		total += confusion_matrix[ci2][ci];
	  
	      /* calculate m-estimate of classification */
	      for (ci2 = 0; ci2 < max_new_ci; ci2++)
		{
		  confusion_percent_matrix[ci2][ci] = 
		    (1.0 + (float) confusion_matrix[ci2][ci]) / 
		    (total + num_tested);
		  total2 += confusion_percent_matrix[ci2][ci];
		}
	  
	      /* normalize so probabilities sum to 1.0 */
	      for (ci2 = 0; ci2 < max_new_ci; ci2++)
		confusion_percent_matrix[ci2][ci] = 
		  confusion_percent_matrix[ci2][ci] / total2;
	    }
      
	  /* print the confusion percentage matrix */
	  
	  if (bow_em_print_stat_summary)
	    {
	      for (ci = 0; ci < max_new_ci; ci++)
		{
		  int ci2;
		  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
							     ci);
		  
		  bow_verbosify(bow_progress, "%25s ", 
				filename_to_classname(cdoc->filename));
		  
		  for (ci2 = 0; ci2 < max_new_ci; ci2++)
		    bow_verbosify(bow_progress, "%3.0f ", 
				  confusion_percent_matrix[ci][ci2]*100 );
		  bow_verbosify(bow_progress, "\n");
		}
	    }
	}
      else if (em_stat_method == rank)
	{
#if 0
	  /* ok.  now normalize class_by_rank_correctness */
	  
	  for (ci = 0; ci < max_new_ci; ci++)
	    {
	      float total = 0.0;
	      
	      for (ri = 0; ri < max_new_ci; ri++)
		total += class_by_rank_correctness[ci][ri];
	      
	      for (ri = 0; ri < max_new_ci; ri++)
		class_by_rank_correctness[ci][ri] = 
		  class_by_rank_correctness[ci][ri] / total;
	    }
#endif	  
	  /* print class_by_rank correctness */

	  if (bow_em_print_stat_summary)
	    {
	      for (ci = 0; ci < max_new_ci; ci++)
		{
		  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
							     ci);
		  
		  bow_verbosify(bow_progress, "%25s ", 
				filename_to_classname(cdoc->filename));
		  for (ri = 0; ri < max_new_ci; ri++)
		    bow_verbosify(bow_progress, "%5d ", 
				  class_by_rank_correctness[ci][ri] );
		  bow_verbosify(bow_progress, "\n");
		}
	    }
	}
      else if (em_stat_method == pr)
	{
	  /* sort the scores by descending score */

	  for (ci = 0; ci < max_new_ci; ci ++)
	    qsort(pr_by_class[ci], num_stat_docs, sizeof (bow_em_pr_struct),
		  bow_em_pr_struct_compare);

	  /* print out a summary of the stats */
	  
	  if (bow_em_print_stat_summary)
	    {
	      for (ci = 0; ci < max_new_ci; ci++)
		{
		  int pr_index;
		  int correct=0;
		  int count=0;
		  bow_cdoc *cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, 
							     ci);
		  
		  bow_verbosify(bow_progress, "%25s", 
				filename_to_classname(cdoc->filename));
		  
		  for (pr_index = 0; pr_index < num_stat_docs; pr_index++)
		    {
		      
		      if (pr_index % bow_em_pr_window_size == 0)
			{
			  if (pr_index != 0)
			    {
			      while (pr_index < num_stat_docs &&
				     pr_by_class[ci][pr_index-1].score == 
				     pr_by_class[ci][pr_index].score)
				{
				  correct += pr_by_class[ci][pr_index].correct;
				  count++;
				  pr_index++;
				}
			      bow_verbosify(bow_progress, " %3.0f (%1.3f)", 
					    (float) correct * 100.0 / count,
					    pr_by_class[ci][pr_index].score);
			      if (!(pr_index < num_stat_docs))
				break;
			    }
			  correct = 0;
			  count = 0;
			}
		      correct += pr_by_class[ci][pr_index].correct;
		      count++;
		      
		      if (pr_by_class[ci][pr_index].correct != 0 &&
			  pr_by_class[ci][pr_index].correct != 1)
			bow_error("Big Problem");
		    }
		  
		  bow_verbosify(bow_progress, "\n");
		}
	    } 
	}

      /* now classify the unknown documents and use stats to assign
	 proabilities */
      
      num_tested = 0;
      bow_verbosify(bow_progress, "\nClassifying unlabeled documents:       ");
      
      for (unknown_next_wv = bow_ignore_next_wv, unknown_set_counter = 0;
	   (ignored_model_are_false_unknown ? unknown_set_counter < 2 : 
	    unknown_set_counter < 1);
	   unknown_next_wv = bow_ignored_model_next_wv, unknown_set_counter++)
	{
	  /* Create the heap from which we'll get WV's. */

	  test_heap = bow_test_new_heap (doc_barrel);
	  
	  /* Loop once for each unlabeled document. */

	  while ((di = (*unknown_next_wv) (test_heap, doc_barrel, &query_wv))
		 != -1)
	    {
	      int total = 0;
	      
	      doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, 
						   di);
	      bow_wv_set_weights (query_wv, vpc_barrel);
	      bow_wv_normalize_weights (query_wv, vpc_barrel);
	      actual_num_hits = 
		bow_barrel_score (vpc_barrel, 
				  query_wv, hits,
				  num_hits_to_retrieve, -1);
	      assert (actual_num_hits == num_hits_to_retrieve);
	      
	      if (em_stat_method == confusion)
		{
		  /* set the class_probs by looking at the most likely
		     classification and calculating from the confusion
		     percent matrix */
		  
		  for (ci = 0; ci < max_new_ci; ci++)
		    {
		      doc_cdoc->class_probs[ci] = unlabeled_normalizer *
			confusion_percent_matrix[hits[0].di][ci];
		    }
		}
	      else if (em_stat_method == rank)
		{
		  /* set the class_probs appropriately for that
		     document.  Use the rank order stats to
		     approximate */
		  
		  for (hi = 0; hi < actual_num_hits; hi++)
		    total += class_by_rank_correctness[hits[hi].di][hi];
		  
		  for (hi = 0; hi < actual_num_hits; hi++)
		    {
		      doc_cdoc->class_probs[hits[hi].di] = (float)
			class_by_rank_correctness[hits[hi].di][hi] * 
			unlabeled_normalizer / total;
		    }
		}
	      else if (em_stat_method == simple)
		{
		  /* set the class probs to 1 for the maximally likely class */
		  
		  for (ci = 0; ci < max_new_ci; ci++)
		    doc_cdoc->class_probs[ci] = 0.0;

		  doc_cdoc->class_probs[hits[0].di] = unlabeled_normalizer;
		}
	      else if (em_stat_method == nb_score)
		{
		  /* set the class probs to the naive bayes score */

		  for (hi = 0; hi < actual_num_hits; hi++)
		    doc_cdoc->class_probs[hits[hi].di] = hits[hi].weight;
		}
	      else if (em_stat_method == pr)
		{
		  float prob_by_ci[max_new_ci];
		  float total = 0.0;

		  /* set the class_probs by picking numbers from the pr 
		     charts */

		  for (hi = 0; hi < actual_num_hits; hi++)
		    {
		      float score = hits[hi].weight;
		      int pr_index_low;
		      int pr_index_high;
		      int pr_index = 0;
		      int correct_count = 1; /* for the m-estimate */
		      int num_docs_in_window = 0;
		      int pri;
		      
		      while ((pr_index < num_stat_docs) && 
			     (pr_by_class[hits[hi].di][pr_index].score > score))
			pr_index++;

		      pr_index_low = pr_index;

		      while ((pr_index < num_stat_docs) &&
			     pr_by_class[hits[hi].di][pr_index].score == score)
			pr_index++;

		      pr_index_high = pr_index;

#if 0		      
		      if (10 > pr_index)
			correct_count += 10 - pr_index;
#endif
		      for (pri = MAX (0, MIN(pr_index_low, 
					     ((pr_index_low + pr_index_high - 
					       bow_em_pr_window_size) / 2))); 
			   pri < MIN (MAX(pr_index_high,
					  ((pr_index_high + pr_index_low + 
					    bow_em_pr_window_size) / 2)),
				      num_stat_docs);
			   pri++)
			{
			  correct_count += pr_by_class[hits[hi].di][pri].correct;
			  num_docs_in_window++;
			}
		      
		      prob_by_ci[hits[hi].di] = (float) correct_count / 
			((float) num_docs_in_window + 6);
		    }

		  /* normalize the probs to sum to one */

		  for (ci = 0; ci < max_new_ci; ci++)
		    total += prob_by_ci[ci];

		  for (ci = 0; ci < max_new_ci; ci++)
		    doc_cdoc->class_probs[ci] = 
		      unlabeled_normalizer * prob_by_ci[ci] / total;
		}
	      
	      if (num_tested % 100 == 0)
		bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", num_tested);
	      
	      num_tested++;	  
	    }
	}

      bow_verbosify(bow_progress, "\b\b\b\b\b\b%6d\n", num_tested);
    }      
      
  /* fix back up the doc barrel... dealloc class_probs (wrong size!), 
     reset doc type, clear out weights, etc. */
  
  for (di=0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      
      if (cdoc->type != test)
	cdoc->type = model;
    }
  
  if (em_stat_method == pr)
    {
      /* free space for pr stats */
      for (ci = 0; ci < max_new_ci; ci++)
	bow_free(pr_by_class[ci]);
    }

  /* set the wi2dvf to the best so far */
  if (bow_em_take_best_barrel &&
      best_num_correct >= num_correct)
    {
      bow_wi2dvf_free(vpc_barrel->wi2dvf);
      vpc_barrel->wi2dvf = best_wi2dvf;

      for (ci = 0; ci < max_new_ci; ci++)
	{
	  bow_cdoc *cdoc = bow_array_entry_at_index(vpc_barrel->cdocs, ci);
	  
	  cdoc->prior = best_priors[ci];
	  cdoc->word_count = best_word_counts[ci];
	  cdoc->normalizer = best_normalizers[ci];
	}
    }

#if 0
  else
    {
      bow_error("Why are we stopping now?");
    }
#endif
  return vpc_barrel;
}


/* Run test trials, outputing results to TEST_FP.  The results are
   indended to be read and processed by the Perl script
   ./rainbow-stats. */
void
bow_em_compare_to_nb (bow_barrel *doc_barrel)
{
  bow_dv_heap *test_heap;	/* we'll extract test WV's from here */
  bow_wv *query_wv;
  int di;			/* a document index */
  bow_score *hits;
  int num_hits_to_retrieve = bow_barrel_num_classes (doc_barrel);
  int actual_num_hits;
  int hi;			/* hit index */
  bow_cdoc *doc_cdoc;
  bow_cdoc *class_cdoc;
  FILE *test_fp = stdout;
  bow_barrel *class_barrel;

  hits = alloca (sizeof (bow_score) * num_hits_to_retrieve);

  doc_barrel->method = bow_method_at_name ("naivebayes");
  
#if 0  
  if (bow_uniform_class_priors)
    bow_barrel_set_cdoc_priors_to_class_uniform (doc_barrel);
#endif
  
  /* Re-create the vector-per-class barrel in accordance with the
     new train/test settings. */
  class_barrel = 
    bow_barrel_new_vpc_with_weights (doc_barrel);

  /* Create the heap from which we'll get WV's. */
  test_heap = bow_test_new_heap (doc_barrel);

  /* Initialize QUERY_WV so BOW_TEST_NEXT_WV() knows not to try to free */
  query_wv = NULL;

  /* Loop once for each test document. */
  while ((di = bow_test_next_wv (test_heap, doc_barrel, &query_wv))
	 != -1)
    {
      doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, 
					   di);
      class_cdoc = bow_array_entry_at_index (class_barrel->cdocs, 
					     doc_cdoc->class);
      bow_wv_set_weights (query_wv, class_barrel);
      bow_wv_normalize_weights (query_wv, class_barrel);
      actual_num_hits = 
	bow_barrel_score (class_barrel, 
			  query_wv, hits,
			  num_hits_to_retrieve, -1);
      assert (actual_num_hits == num_hits_to_retrieve);
#if 0
      printf ("%8.6f %d %8.6f %8.6f %d ",
	      class_cdoc->normalizer, 
	      class_cdoc->word_count, 
	      class_cdoc->normalizer / class_cdoc->word_count, 
	      class_cdoc->prior,
	      doc_cdoc->class);
      if (hits[0].di == doc_cdoc->class)
	printf ("1\n");
      else
	printf ("0\n");
#endif
      fprintf (test_fp, "%s %s ", 
	       doc_cdoc->filename, 
	       filename_to_classname(class_cdoc->filename));
      for (hi = 0; hi < actual_num_hits; hi++)
	{
	  class_cdoc = 
	    bow_array_entry_at_index (class_barrel->cdocs,
				      hits[hi].di);
	  /* For the sake CommonLisp, don't print numbers smaller than
	     1e-35, because it can't `(read)' them. */
	  if (hits[hi].weight < 1e-35
	      && hits[hi].weight > 0)
	    hits[hi].weight = 0;
	  fprintf (test_fp, "%s:%g ", 
		   filename_to_classname (class_cdoc->filename),
		   hits[hi].weight);
	}
      fprintf (test_fp, "\n");

    }
  bow_barrel_free (class_barrel);

  doc_barrel->method = bow_method_at_name ("em");

  fprintf(test_fp, "#0\n");
}



/* Function to assign `Naive Bayes'-style weights to each element of
   each document vector. */
void
bow_em_print_log_odds_ratio (bow_barrel *barrel, int num_to_print)
{
  int ci;
  bow_cdoc *cdoc;
  int wi;			/* a "word index" into WI2DVF */
  int max_wi;			/* the highest "word index" in WI2DVF. */
  bow_dv *dv;			/* the "document vector" at index WI */
  int dvi;			/* an index into the DV */
  int weight_setting_num_words = 0;
  int total_num_words = 0;
  struct lorth { int wi; float lor; } lors[barrel->cdocs->length][num_to_print];
  int wci;


  /* We assume that we have already called BOW_BARREL_NEW_VPC() on
     BARREL, so BARREL already has one-document-per-class. */
    
  max_wi = MIN (barrel->wi2dvf->size, bow_num_words());

  for (ci = 0; ci < barrel->cdocs->length; ci++)
    for (wci = 0; wci < num_to_print; wci++)
      {
	lors[ci][wci].lor = 0.0;
	lors[ci][wci].wi = -1;
      }

  /* assume that word_count, normalizer are already set */

  /* Calculate the total number of occurrences of each word; store this
     int DV->IDF. */

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
	  total_num_words += dv->entry[dvi].weight;
	  dv->idf += dv->entry[dvi].weight;
	}
    }


  bow_verbosify(bow_progress, "Calculating word weights:        ");

  /* Set the weights in the BARREL's WI2DVF so that they are
     equal to P(w|C), the probability of a word given a class. */
  for (wi = 0; wi < max_wi; wi++) 
    {
      double pr_w = 0.0;
      dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);

      if (wi % 100 == 0)
	bow_verbosify(bow_progress, "\b\b\b\b\b\b%6d", wi);

      /* If the model doesn't know about this word, skip it. */
      if (dv == NULL)
	continue;

      pr_w = ((double)dv->idf) / total_num_words;

      /* Now loop through all the elements, setting their weights */
      for (dvi = 0; dvi < dv->length; dvi++) 
	{
	  double pr_w_c;
	  double pr_w_not_c;
	  double log_likelihood_ratio;
	  cdoc = bow_array_entry_at_index (barrel->cdocs, 
					   dv->entry[dvi].di);
	  /* Here CDOC->WORD_COUNT is the total number of words in the class */
	  /* We use Laplace Estimation. */
	  pr_w_c = ((double)dv->entry[dvi].weight 
		    / (cdoc->word_count + cdoc->normalizer));
	  pr_w_c = (((double)dv->entry[dvi].weight + 1)
		    / (cdoc->word_count + barrel->wi2dvf->num_words));
	  pr_w_not_c = ((dv->idf - dv->entry[dvi].weight 
			 + barrel->cdocs->length - 1)
			/ 
			(total_num_words - cdoc->word_count
			 + (barrel->wi2dvf->num_words
			    * (barrel->cdocs->length - 1))));
	  log_likelihood_ratio = log (pr_w_c / pr_w_not_c);
	
	  wci = num_to_print - 1;

	  while (wci >= 0 && 
		 (lors[dv->entry[dvi].di][wci].lor < pr_w_c * log_likelihood_ratio))
	    wci--;

	  if (wci < num_to_print - 1)
	    {
	      int new_wci = wci + 1;

	      for (wci = num_to_print-1; wci > new_wci; wci--)
		{
		  lors[dv->entry[dvi].di][wci].lor = 
		    lors[dv->entry[dvi].di][wci - 1].lor;
		  lors[dv->entry[dvi].di][wci].wi = 
		    lors[dv->entry[dvi].di][wci - 1].wi;
		}

	      lors[dv->entry[dvi].di][new_wci].lor = pr_w_c * log_likelihood_ratio;
	      lors[dv->entry[dvi].di][new_wci].wi = wi;
	    }
	}
      weight_setting_num_words++;
      /* Set the IDF.  Kl doesn't use it; make it have no effect */
      dv->idf = 1.0;
    }

#if 0
  fprintf (stderr, "wi2dvf num_words %d, weight-setting num_words %d\n",
	   barrel->wi2dvf->num_words, weight_setting_num_words);
#endif

  for (ci = 0; ci < barrel->cdocs->length; ci++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index(barrel->cdocs, ci);

      bow_verbosify(bow_progress, "\n%s\n", filename_to_classname(cdoc->filename));
      for (wci = 0; wci < num_to_print; wci++)
	bow_verbosify(bow_progress, "%1.4f %s\n", lors[ci][wci].lor, 
		      bow_int2word (lors[ci][wci].wi));
    }
		      

}


/* Set the class prior probabilities by counting the number of
   documents of each class. note this counts all non-test docs, 
   not just model docs.  Also excludes ignored_model docs if
   ignored_model_are_known == 1 */

void
bow_em_set_priors_using_class_probs (bow_barrel *vpc_barrel,
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
  /* Add in document class_probs. */
  for (di = 0; di < doc_barrel->cdocs->length; di++)
    {
      bow_cdoc *doc_cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      bow_cdoc *vpc_cdoc;
      
      if (!(doc_cdoc->type == test ||
	    (!ignored_model_are_false_unknown && 
	     doc_cdoc->type == ignored_model)))
	{
	  /* note that class probs correspond to CLASS barrel class indices */
	  for (ci = 0; ci <= max_ci; ci++)
	    {
	      vpc_cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci); 
	      vpc_cdoc->prior += doc_cdoc->class_probs[ci];
	    }
	}
    }
  
  /* Sum them all. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      prior_sum += cdoc->prior;
    }
  /* Normalize to set the prior. */
  for (ci = 0; ci <= max_ci; ci++)
    {
      bow_cdoc *cdoc;
      cdoc = bow_array_entry_at_index (vpc_barrel->cdocs, ci);
      if (prior_sum != 0)
	cdoc->prior /= prior_sum;
      else
	cdoc->prior = 1.0 / (float) max_ci;
      assert (cdoc->prior > 0.0 && cdoc->prior < 1.0);
    }
}



/* this is just naivebayes score using weights, not counts */
int
bow_em_score (bow_barrel *barrel, bow_wv *query_wv, 
	      bow_score *bscores, int bscores_len,
	      int loo_class)
{
  double *scores;		/* will become prob(class), indexed over CI */
  int ci;			/* a "class index" (document index) */
  int wvi;			/* an index into the entries of QUERY_WV. */
  int dvi;			/* an index into a "document vector" */
  float pr_w_c = 0.0;			/* P(w|C), prob a word is in a class */
  double log_pr_tf;		/* log(P(w|C)^TF), ditto, log() of it */
  double rescaler;		/* Rescale SCORES by this after each word */
  double new_score;		/* a temporary holder */
  int num_scores;		/* number of entries placed in SCORES */
  int num_words = 0;

  /* Allocate space to store scores for *all* classes (documents) */
  scores = alloca (barrel->cdocs->length * sizeof (double));

  if (loo_class >= 0)
    {
      for (wvi = 0; wvi < query_wv->num_entries; wvi++)
	num_words += query_wv->entry[wvi].count;
    }

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
	  bow_cdoc *cdoc;

	  cdoc = bow_array_entry_at_index (barrel->cdocs, ci);
	  assert (cdoc->type == model);

	  /* Assign PR_W_C to P(w|C), either using a DV entry, or, if
	     there is no DV entry for this class, using M-estimate 
	     smoothing */
	  if (dv)
	    while (dvi < dv->length && dv->entry[dvi].di < ci)
	      dvi++;
	  if (dv && dvi < dv->length && dv->entry[dvi].di == ci)
	    {
	      if (loo_class == ci)
		{
		  /* xxx This is not exactly right, because 
		     BARREL->WI2DVF->NUM_WORDS might have changed with the
		     removal of QUERY_WV's document. */
		  if (bow_em_event_model == document)
		    pr_w_c = ((float)
			      ((M_EST_M * M_EST_P) + dv->entry[dvi].weight 
			       - (query_wv->entry[wvi].count * 
				  200 / num_words))
			      / (M_EST_M + cdoc->word_count
				 - (query_wv->entry[wvi].count * 
				    200 / num_words)));
		  else if (bow_em_event_model == word)
		    pr_w_c = ((float)
			      ((M_EST_M * M_EST_P) + dv->entry[dvi].weight 
			       - (query_wv->entry[wvi].count))
			      / (M_EST_M + cdoc->word_count
				 - (query_wv->entry[wvi].count)));
		  else
		    bow_error("Unrecognized bow_em_event_mode");

		  if (pr_w_c <= 0)
		    bow_error ("A negative word probability was calculated. "
			       "This can happen if you are using\n"
			       "--test-files-loo and the test files are "
			       "not being lexed in the same way as they\n"
			       "were when the model was built");
		  assert (pr_w_c > 0 && pr_w_c <= 1);
		}
	      else
		{
		  pr_w_c = ((float)
			    ((M_EST_M * M_EST_P) + dv->entry[dvi].weight)
			    / (M_EST_M + cdoc->word_count));
		  assert (pr_w_c > 0 && pr_w_c <= 1);
		}
	    }
	  else
	    {
	      if (loo_class == ci)
		{
		  /* xxx This is not exactly right, because 
		     BARREL->WI2DVF->NUM_WORDS might have changed with the
		     removal of QUERY_WV's document. */
		  if (bow_em_event_model == document)
		    pr_w_c = ((M_EST_M * M_EST_P)
			      / (M_EST_M + cdoc->word_count
				 - (query_wv->entry[wvi].count * 
				    200 / num_words)));
		  else if (bow_em_event_model == word)
		    pr_w_c = ((M_EST_M * M_EST_P)
			      / (M_EST_M + cdoc->word_count
				 - (query_wv->entry[wvi].count)));
		  assert (pr_w_c > 0 && pr_w_c <= 1);
		}
	      else
		{
		  pr_w_c = ((M_EST_M * M_EST_P)
			    / (M_EST_M + cdoc->word_count));
		  assert (pr_w_c > 0 && pr_w_c <= 1);
		}
	    }
	  assert (pr_w_c > 0 && pr_w_c <= 1);

	  log_pr_tf = log (pr_w_c);
	  assert (log_pr_tf > -FLT_MAX + 1.0e5);
	  /* Take into consideration the number of times it occurs in 
	     the query document */
	  log_pr_tf *= query_wv->entry[wvi].count;
	  assert (log_pr_tf > -FLT_MAX + 1.0e5);

	  scores[ci] += log_pr_tf;

	  if (bow_print_word_scores)
	    printf (" %8.2e %7.2f %-40s  %10.9f\n", 
		    pr_w_c,
		    log_pr_tf, 
		    (strrchr (cdoc->filename, '/') ? : cdoc->filename),
		    scores[ci]);

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



/* what about em parameters?  How should those be used */

bow_method bow_method_em = 
{
  "em",
  NULL, /* bow_leave_weights_alone_since_theyre_really_counts */
  0,				/* no weight scaling function */
  NULL, /* bow_barrel_normalize_weights_by_summing, */
  bow_em_new_vpc_with_weights,
  bow_em_set_priors_using_class_probs,
  bow_em_score,
  bow_wv_set_weights_to_count,
  NULL,				/* no need for extra weight normalization */
  NULL  /* is this right?  should we have em parameters? */
};

void _register_method_em () __attribute__ ((constructor));
void _register_method_em ()
{
  static int done = 0;
  if (done) 
    return;
  bow_method_register_with_name (&bow_method_em, "em");
  bow_argp_add_child (&em_argp_child);
  done = 1;
}
