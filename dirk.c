/* Rainbow classification method with a Dirichlet kernel on each 
   training document, as worked on by Jerry Xiaojin Zhu. */

/* 
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
//#include <bow/train_dirichlet.h>
//#include <bow/bpe.h>
#include <math.h>
#include <argp/argp.h>

static int dirk_print_alphas = 0;

/* Extra alpha in addition to alphas learned above.  This prevents zero's
   when the trained Dirichlet really should have zero alphas. */
static double dirk_prior_alpha = 1.0;
#define PRIOR_ALPHA dirk_prior_alpha


enum {
  DIRK_PRINT_ALPHAS_KEY = 5700,
  DIRK_PRIOR_ALPHA_KEY,
};

static struct argp_option dirk_options[] =
{
  {0,0,0,0,
   "Dirichlet Kernel options, --method=dirk:", 910},
  {"dirk-prior-alpha", DIRK_PRIOR_ALPHA_KEY, "NUM", 0,
   "Set the prior alpha parameter.  Defaults to 1.0."},
  {"dirk-print-alphas", DIRK_PRINT_ALPHAS_KEY, 0, 0,
   "Print the alphas of the Beta distribution learned for each word "
   "of the learned prior."},
  {0, 0}
};

error_t
dirk_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case DIRK_PRINT_ALPHAS_KEY:
      dirk_print_alphas = 1;
      break;
    case DIRK_PRIOR_ALPHA_KEY:
      dirk_prior_alpha = atof (arg);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static const struct argp dirk_argp =
{
  dirk_options,
  dirk_parse_opt
};

static struct argp_child dirk_argp_child =
{
  &dirk_argp,			/* This child's argp structure */
  0,				/* flags for child */
  0,				/* optional header in help message */
  0				/* arbitrary group number for ordering */
};


/* End of command-line options specific to DIRK */


/* Logarithm of the gamma function.
   References:
   1) W. J. Cody and K. E. Hillstrom, 'Chebyshev Approximations for
      the Natural Logarithm of the Gamma Function,' Math. Comp. 21,
      1967, pp. 198-203.
   2) K. E. Hillstrom, ANL/AMD Program ANLC366S, DGAMMA/DLGAMA, May,
      1969.
   3) Hart, Et. Al., Computer Approximations, Wiley and sons, New
      York, 1968.
*/
static double
log_gamma (double x)
{
  double result, y, xnum, xden;
  int i;
  static int cache_initialized = 0;
#define cache_size 100
  static double cache[cache_size];
  static double d1 = -5.772156649015328605195174e-1;
  static double p1[] = { 
    4.945235359296727046734888e0, 2.018112620856775083915565e2, 
    2.290838373831346393026739e3, 1.131967205903380828685045e4, 
    2.855724635671635335736389e4, 3.848496228443793359990269e4, 
    2.637748787624195437963534e4, 7.225813979700288197698961e3 
  };
  static double q1[] = {
    6.748212550303777196073036e1, 1.113332393857199323513008e3, 
    7.738757056935398733233834e3, 2.763987074403340708898585e4, 
    5.499310206226157329794414e4, 6.161122180066002127833352e4, 
    3.635127591501940507276287e4, 8.785536302431013170870835e3
  };
  static double d2 = 4.227843350984671393993777e-1;
  static double p2[] = {
    4.974607845568932035012064e0, 5.424138599891070494101986e2, 
    1.550693864978364947665077e4, 1.847932904445632425417223e5, 
    1.088204769468828767498470e6, 3.338152967987029735917223e6, 
    5.106661678927352456275255e6, 3.074109054850539556250927e6
  };
  static double q2[] = {
    1.830328399370592604055942e2, 7.765049321445005871323047e3, 
    1.331903827966074194402448e5, 1.136705821321969608938755e6, 
    5.267964117437946917577538e6, 1.346701454311101692290052e7, 
    1.782736530353274213975932e7, 9.533095591844353613395747e6
  };
  static double d4 = 1.791759469228055000094023e0;
  static double p4[] = {
    1.474502166059939948905062e4, 2.426813369486704502836312e6, 
    1.214755574045093227939592e8, 2.663432449630976949898078e9, 
    2.940378956634553899906876e10, 1.702665737765398868392998e11, 
    4.926125793377430887588120e11, 5.606251856223951465078242e11
  };
  static double q4[] = {
    2.690530175870899333379843e3, 6.393885654300092398984238e5, 
    4.135599930241388052042842e7, 1.120872109616147941376570e9, 
    1.488613728678813811542398e10, 1.016803586272438228077304e11, 
    3.417476345507377132798597e11, 4.463158187419713286462081e11
  };
  static double c[] = {
    -1.910444077728e-03, 8.4171387781295e-04, 
    -5.952379913043012e-04, 7.93650793500350248e-04, 
    -2.777777777777681622553e-03, 8.333333333333333331554247e-02, 
    5.7083835261e-03
  };
  static double a = 0.6796875;

  if (!cache_initialized)
    {
      int i;
      for (i = 0; i < cache_size; i++)
	cache[i] = 0;
      cache_initialized = 1;
    }

  if (fmod (x, 1.0) == 0 && x < cache_size)
    {
      int i = (int)x;
      assert (i < cache_size);
      if (cache[i] != 0)
	return cache[i];
    }

  if((x <= 0.5) || ((x > a) && (x <= 1.5))) {
    if(x <= 0.5) {
      result = -log(x);
      /*  Test whether X < machine epsilon. */
      if(x+1 == 1) {
	return result;
      }
    }
    else {
      result = 0;
      x = (x - 0.5) - 0.5;
    }
    xnum = 0;
    xden = 1;
    for(i=0;i<8;i++) {
      xnum = xnum * x + p1[i];
      xden = xden * x + q1[i];
    }
    result += x*(d1 + x*(xnum/xden));
  }
  else if((x <= a) || ((x > 1.5) && (x <= 4))) {
    if(x <= a) {
      result = -log(x);
      x = (x - 0.5) - 0.5;
    }
    else {
      result = 0;
      x -= 2;
    }
    xnum = 0;
    xden = 1;
    for(i=0;i<8;i++) {
      xnum = xnum * x + p2[i];
      xden = xden * x + q2[i];
    }
    result += x*(d2 + x*(xnum/xden));
  }
  else if(x <= 12) {
    x -= 4;
    xnum = 0;
    xden = -1;
    for(i=0;i<8;i++) {
      xnum = xnum * x + p4[i];
      xden = xden * x + q4[i];
    }
    result = d4 + x*(xnum/xden);
  }
  /*  X > 12  */
  else {
    y = log(x);
    result = x*(y - 1) - y*0.5 + .9189385332046727417803297;
    x = 1/x;
    y = x*x;
    xnum = c[6];
    for(i=0;i<6;i++) {
      xnum = xnum * y + c[i];
    }
    xnum *= x;
    result += xnum;
  }

  if ((i = fmod (x, 1.0) == 0) && x < cache_size
      && (i = (int)x) && cache[i] == 0)
    cache[i] = result;

  return result;
}



/* The density at possible QUERY_WV of the kernel centered at
   KERNEL_WV.  KERNEL_WV is d_i; QUERY_WV is d_test. */
double
bow_dirk_log_kernel (int vocab_size,
		     bow_wv *kernel_wv, bow_wv *query_wv)
{
  /* The parameters of the kernel: */
  double b = 1;
  double a = kernel_wv->num_entries;

  double *alphas = alloca (vocab_size * sizeof (double));
  int wi, wvi;
  double kernel_wv_word_count, query_wv_word_count, increment;
  double alphas_sum, density;

  /* Calculate the total number of word occurrences in each of the
     QUERY_WV and the KERNEL_WV documents. */
  kernel_wv_word_count = 0;
  for (wvi = 0; wvi < kernel_wv->num_entries; wvi++)
    kernel_wv_word_count += kernel_wv->entry[wvi].count;
  query_wv_word_count = 0;
  for (wvi = 0; wvi < query_wv->num_entries; wvi++)
    query_wv_word_count += query_wv->entry[wvi].count;

  /* Set the alphas of the Dirichlet distribution that is the kernel
     for this KERNEL_WV document */

  /* Initialize them all to the B parameter (typically 1) */
  for (wi = 0; wi < vocab_size; wi++)
    alphas[wi] = b;
  alphas_sum = b * vocab_size;

  /* Add mass from words in the KERNEL_WV document to the alphas */
  for (wvi = 0; wvi < kernel_wv->num_entries; wvi++)
    {
      increment = a * (kernel_wv->entry[wvi].count / kernel_wv_word_count); 
      alphas[kernel_wv->entry[wvi].wi] += increment;
      alphas_sum += increment;
    }

  /* Do the Dirichlet integral */

  /* Start with the ratio of Gamma functions that is independent of
     the individual word counts in the QUERY_WV document. */
  density = (log_gamma (alphas_sum)
	     - log_gamma (query_wv_word_count + alphas_sum));
  /* Put in the contribution of each word in the QUERY_WV document. */
  for (wvi = 0; wvi < query_wv->num_entries; wvi++)
    density += (log_gamma (query_wv->entry[wvi].count
			   + alphas[query_wv->entry[wvi].wi])
		- log_gamma (alphas[query_wv->entry[wvi].wi]));

  /* Density is now a log_probability; simply return it without
     turning it into a probability */
  return density;
}

/* Classify QUERY_WV.  This is d_test */
int
bow_dirk_score (bow_barrel *doc_barrel, bow_wv *query_wv, 
		bow_score *bscores, int bscores_len,
		int loo_class)
{
  int num_classes = bow_barrel_num_classes (doc_barrel);
  bow_dv_heap *heap;
  int di, ci, num_scores;
  bow_wv *wv;
  double *scores;		/* will become prob(class), indexed over CI */
  bow_cdoc *cdoc;
  int vocab_size = MIN(doc_barrel->wi2dvf->size, bow_num_words ());
  int max_score_di, max_score_ci;
  double max_score, score_increment;
  int num_documents = doc_barrel->cdocs->length;
  double *di_scores = alloca (num_documents * sizeof (double));

  scores = alloca (num_classes * sizeof (double));
  for (ci = 0; ci < num_classes; ci++)
    scores[ci] = 0;

  /* This implementation current assumes a uniform class prior.
     Otherwise we would put in a class prior here. */
  
  /* Loop over all training documents, measuring the distance
     from the QUERY_WV to each of the training documents */
  for (di = 0; di < num_documents; di++)
    di_scores[di] = -DBL_MAX;

  max_score = -DBL_MAX;
  heap = bow_make_dv_heap_from_wi2dvf (doc_barrel->wi2dvf);
  while ((di = bow_heap_next_wv (heap, doc_barrel, &wv, 
				 bow_cdoc_is_train)) != -1)
    {
      /* DI is the index of the current training document.  CDOC will contain
	 information about document DI. */
      cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
      /* Here we get the class index of DI */
      ci = cdoc->class;
      /* WV is the word vector for DI */
      score_increment = (bow_dirk_log_kernel (vocab_size, wv, query_wv));
      di_scores[di] = score_increment;
      if (score_increment > max_score)
	{
	  max_score_di = di;
	  max_score_ci = ci;
	  max_score = score_increment;
	}
      if (di % 20 == 0)
	printf ("di=%d ci=%d scores=%g\n", di, ci, score_increment);
      //scores[ci] += score_increment;
    }
  cdoc = bow_array_entry_at_index (doc_barrel->cdocs, max_score_di);
  printf ("max di=%d ci=%d scores=%g\nfilename= %s", 
	  max_score_di, max_score_ci, max_score,
	  cdoc->filename);
  printf ("|d| = %g\n", bow_wv_weight_sum (query_wv));
  
  /* Find the document with the maximum log-density, and add to all
     document's log-densities to make them near zero. */
  for (di = 0; di < num_documents; di++)
    {
      if (di_scores[di] != -DBL_MAX)
	{
	  di_scores[di] -= max_score;
	  cdoc = bow_array_entry_at_index (doc_barrel->cdocs, di);
	  ci = cdoc->class;
	  if (di % 20 == 0)
	    printf ("di=%d ci=%d log-score=%g score=%g\n", di, ci, 
		    di_scores[di], exp (di_scores[di]));
	  scores[ci] += exp (di_scores[di]);
	}
    }

#if 0
  /* Normalize the SCORES[] distribution so that it sums to one */
  {
    double sum;
    sum = 0; 
    for (ci = 0; ci < num_classes; ci++)
      sum += scores[ci];
    for (ci = 0; ci < num_classes; ci++)
      scores[ci] /= sum;
  }
#endif

  /* Return the scores by putting them into BSCORES in sorted order */
  num_scores = 0;
  for (ci = 0; ci < num_classes; ci++)
    {
      /* Check that scores[ci] is not NaN */
      assert (scores[ci] == scores[ci]);
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
  return num_scores;
}


bow_barrel *
bow_dirk_new_vpc (bow_barrel *doc_barrel)
{
  return doc_barrel;
}

rainbow_method bow_method_dirk = 
{
  "dirk",
  NULL,
  0,				/* no weight scaling function */
  NULL, /* bow_barrel_normalize_weights_by_summing, */
  bow_dirk_new_vpc,
  bow_barrel_set_vpc_priors_by_counting,
  bow_dirk_score,
  bow_wv_set_weights_to_count,
  NULL,				/* no need for extra weight normalization */
  bow_barrel_free,
  NULL
};

void _register_method_dirk () __attribute__ ((constructor));
void _register_method_dirk ()
{
  bow_method_register_with_name ((bow_method*)&bow_method_dirk,
				 "dirk",
				 sizeof (rainbow_method),
				 &dirk_argp_child);
  bow_argp_add_child (&dirk_argp_child);
}

