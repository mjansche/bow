/* Copyright (C) 1999 Greg Schohn - gcs@jprc.com */

/* ******************** svm_al.c *******************
 * Active learning add-ons for SVM's. */

#include <bow/svm.h>

#define NDIM_INSPECTED 14
int dim_map(int i) {
  static int map[] = {1,2,3,4,6,8,12,16,24,32,48,64,128,256};
  return (map[i]);
}

/* compute the prec. recall breakeven point shifting the value of b */
double prec_recall_breakeven(double *test_evals, int *test_yvect, int n, int total_pos) {
  struct di *ey;
  double max;
  int npos;
  int i;

  ey = (struct di *) malloc(sizeof(struct di)*n);

  for (i=0; i<n; i++) {
    ey[i].d = -1*test_evals[i]; /* -1 is to force the sort the way i want it */
    ey[i].i = test_yvect[i];
  }

  qsort(ey,n,sizeof(struct di),di_cmp);

  max = -1.0;
  for (i=npos=0; i<n; i++) {
    double min;
    if (ey[i].i > 0) {
      npos ++;
    }
    min = MIN(((double)npos)/(i+1), ((double)npos)/total_pos);
    if (min > max) {
      max = min;
    }
  }
  free(ey);
  return max;
}
/*
(docs, NULL, yvect, NULL, weights, b, W, 7
 ndocs, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 10
 NULL, NULL, NULL, NULL, 0, NULL, NULL, do_rlearn)
 */
struct al_test_data {
  int ntest;
  int ndim_sat;

  int *docs_added;
  int *test_yvect, *apvect, *anvect;
  int *nsv_vect, *nbsv_vect, *time_vect, *nkce_vect;
  int *npos_added, *nneg_added;

  double *prb, *scores_added;
  int **sv_dim_sat_vect, **train_dim_sat_vect;

  double **test_scores;
  bow_wv **test_docs;
};

/* this fn. does active learning by selecting which docs to pass into 
 * the svm solver (smo or not)... */
/* most of the code & arguments are for logging & testing */
int al_svm_guts(bow_wv **train_docs, int *train_yvect, double *weights, 
		double *b, bow_wv **W_wv, int ndocs, struct al_test_data *astd,
		int do_random_learning) {
  int          dec;
  int          last_subndocs;
  int          nleft;
  int          nsv;
  int          num_words;
  int         *old_svbitmap;
  int          qsize;    /* query size, size of chunks to grow training set by */
  int          remove_wrong;
  struct di   *train_scores, *train_cscores;
  int         *train_sat_vect;
  int         *sv_sat_vect; /* shows how many  */
  bow_wv     **sub_docs; /* those docs that should be learned upon */
  int          sub_ndocs;
  double      *sub_weights;
  int         *sub_yvect;
  double       tb;
  int         *tdocs;    /* translation table */
  double      *tvals;
  double      *W;
  int         *used;     /* bitmap of those elements being learned */

  int i,j,k,n,nloop;

  sub_ndocs = MIN(ndocs,svm_init_al_tset);
  sub_docs = (bow_wv **) malloc(sizeof(bow_wv *)*ndocs);
  sub_weights = (double *) malloc(sizeof(double)*ndocs);
  sub_yvect = (int *) malloc(sizeof(int)*ndocs);

  train_scores = (struct di *) malloc(sizeof(struct di)*ndocs);
  tdocs = (int *) malloc(sizeof(int)*ndocs);
  tvals = (double *) malloc(sizeof(double)*ndocs);
  used = (int *) malloc((ndocs+7)/8);
  num_words = bow_num_words();

  /* this is for accounting/experiments */
  if (astd->sv_dim_sat_vect) {
    train_sat_vect = (int *) malloc(sizeof(int)*num_words);
  } else {
    train_sat_vect = NULL;
  }

  astd->ndim_sat = NDIM_INSPECTED;
  if (astd->train_dim_sat_vect) {
    old_svbitmap = (int *) malloc((ndocs+7)/8);
    sv_sat_vect = (int *) malloc(sizeof(int)*num_words);
  } else {
    old_svbitmap = (int *) malloc((ndocs+7)/8);
    sv_sat_vect = (int *) malloc(sizeof(int)*num_words);
  }

  if (svm_remove_misclassified) {
    remove_wrong = 1;
    svm_remove_misclassified = 0;
  } else {
    remove_wrong = 0;
  }

  /* initialize... */
  nsv = 0;
  memset(used, 0, (ndocs+7)/8);
  for (i=0; i<ndocs; i++) {
    sub_weights[i] = weights[i] = 0.0;
    if (!svm_use_smo) {
      tvals[i] = 0.0;
    }
  }

  /* initialize accounting stuff */
  if (sv_sat_vect) {
    memset(old_svbitmap, 0, (ndocs+7)/8);
    for (i=0; i<num_words; i++) {
      sv_sat_vect[i] = 0.0;
    }
  }
  if (train_sat_vect) {
    for (i=0; i<num_words; i++) {
      train_sat_vect[i] = 0.0;
    }
  }

  qsize = svm_al_qsize;

  /* select an initial set of things to classify */
  /* the following is equivalent to asking the user to classify 1/2 the
   * documents as positive & the other half as negative */
  for (k=-1, i=0, n=sub_ndocs/2; k<2; n=sub_ndocs,k=k+2) {
    for (j=0; i<n && j<ndocs; j++) {
      if (train_yvect[j] != k) {
	continue;
      }
      SETVALID(used,j);
      tdocs[i] = j;
      sub_yvect[i] = train_yvect[j];
      sub_docs[i] = train_docs[j];
      if (svm_use_smo) {
	tvals[i] = -1*train_yvect[j];
      }
      i++;
    }
  }
  sub_ndocs = i;
  last_subndocs = 0;

  for (nloop=0; ;nloop++) {
    struct tms t1, t2;
    int changed;

    /* this is done at the beginning of the loop so that the base case
       (ie. after initial set) works too... */
    if (astd->npos_added && astd->nneg_added) {
      astd->npos_added[nloop] = 0;
      astd->nneg_added[nloop] = 0;
      for (i=last_subndocs; i<sub_ndocs; i++) {
	if (sub_yvect[i] == 1) {
	  astd->npos_added[nloop] ++;
	} else {
	  astd->nneg_added[nloop] ++;
	}
      }
    }

    /* add the document indices */
    if (astd->docs_added) {
      for (i=last_subndocs; i<sub_ndocs; i++) {
	astd->docs_added[i] = tdocs[i];
      }
    }

    if (train_sat_vect) {
      for (i=last_subndocs; i<sub_ndocs; i++) {
	for (j=0; j<sub_docs[i]->num_entries; j++) {
	  train_sat_vect[sub_docs[i]->entry[j].wi] ++;
	}
      }
    }

    fprintf(stderr,"\r%dth AL iteration",nloop);
    svm_nkc_calls = 0;

    times(&t1);
    if (svm_use_smo) {
      changed = smo(sub_docs, sub_yvect, sub_weights, &tb, &W, sub_ndocs, tvals, &nsv);
    } else {
#ifdef HAVE_LOQO
      changed = build_svm_guts(sub_docs, sub_yvect, sub_weights, &tb, &W, sub_ndocs, tvals, &nsv);
#else
      fprintf(stderr, "Cannot build model using loqo solver, rebuild with pr_loqo,"
	      " use the smo solver\n");
#endif
    }
    times(&t2);

    /* a couple of accounting things that are independent of a test/validation set */
    if (astd->time_vect)
      astd->time_vect[nloop] = (int) (t2.tms_utime - t1.tms_utime + t2.tms_stime - t1.tms_stime);
    if (astd->nsv_vect)
      astd->nsv_vect[nloop] = nsv;
    if (astd->nbsv_vect) {
      astd->nbsv_vect[nloop] = 0;
      for (j=0; j<sub_ndocs; j++) {
	if (sub_weights[j] >= svm_C - svm_epsilon_a)
	  astd->nbsv_vect[nloop] ++;
      }
    }
    if (astd->nkce_vect) 
      astd->nkce_vect[nloop] = svm_nkc_calls;

    /* find the next example that is closest to the hyperplane that we just found */

    /* the scores need to be recalculated if any of the weights changed... */
    if (changed) {
      train_cscores = train_scores;
      if (svm_kernel_type == 0) {
	for (j=nleft=0; j<ndocs; j++) {
	  if (!GETVALID(used,j)) {
	    train_scores[nleft].d = fabs(evaluate_model_hyperplane(W, tb, train_docs[j]));
	    train_scores[nleft].i = j;
	    nleft ++;
	  }
	}
      } else {
	for (j=k=nleft=0; j<ndocs; j++) {
	  if (!GETVALID(used,j)) {
	    train_scores[nleft].d = fabs(evaluate_model_cache(sub_docs, sub_weights, sub_yvect, tb, train_docs[j], nsv));
	    train_scores[nleft].i = j;
	    nleft ++;
	  }
	}
      }

      /* lets figure out the change in fdim saturation... */
      if (sv_sat_vect) {
	for (i=0; i<sub_ndocs; i++) {
	  if ((sub_weights[i] == 0.0) && (GETVALID(old_svbitmap,i))) {
	    SETINVALID(old_svbitmap,i);
	    for (j=0; j<sub_docs[i]->num_entries; j++) {
	      sv_sat_vect[sub_docs[i]->entry[j].wi] --;
	    }
	  } else if ((sub_weights[i] != 0.0) && (!GETVALID(old_svbitmap,i))) {
	    SETVALID(old_svbitmap,i);
	    for (j=0; j<sub_docs[i]->num_entries; j++) {
	      sv_sat_vect[sub_docs[i]->entry[j].wi] ++;
	    }
	  }
	}      
	
	/* this could be smarter - but it would involve more arrays... */
	/* update the history vector... */
	for (i=0; i<astd->ndim_sat; i++) {
	  astd->sv_dim_sat_vect[i][nloop] = 0;
	}
	
	for (j=0; j<num_words; j++) {
	  for (i=0; sv_sat_vect[j]>=dim_map(i) && i<astd->ndim_sat; i++) {
	    astd->sv_dim_sat_vect[i][nloop] ++;
	  }
	}
      }
      
      if (astd->train_dim_sat_vect) {
	for (i=0; i<astd->ndim_sat; i++) {
	  astd->train_dim_sat_vect[i][nloop] = 0;
	}

	for (j=0; j<num_words; j++) {
	  for (i=0; train_sat_vect[j]>=dim_map(i) && i<astd->ndim_sat; i++) {
	    astd->train_dim_sat_vect[i][nloop] ++;
	  }
	}
      }

      /* now lets find the accuracy... */
      if (astd->prb) {
	int npos;
	double *test_evals = (double *) malloc(sizeof(double)*astd->ntest);
	
	astd->anvect[nloop] = astd->apvect[nloop] = 0;
	if (svm_kernel_type == 0) {
	  for (j=0; j<astd->ntest; j++) {
	    test_evals[j] = evaluate_model_hyperplane(W, tb, astd->test_docs[j]);
	  }
	} else {
	  for (j=0; j<astd->ntest; j++) {
	    test_evals[j] = evaluate_model(sub_docs, sub_weights, sub_yvect, 
					   tb, astd->test_docs[j], nsv);
	  }
	}

	if (astd->test_scores) {
	  for (j=0; j<astd->ntest; j++) {
	    astd->test_scores[nloop][j] = test_evals[j];
	  }
	}

	for (j=npos=0; j<astd->ntest; j++) {
	  if (astd->test_yvect[j] * test_evals[j] > 0.0) {
	    if (astd->test_yvect[j] == -1) 
	      astd->anvect[nloop] ++;
	    else
	      astd->apvect[nloop] ++;
	  }
	  if (astd->test_yvect[j] > 0) {
	    npos ++;
	  }
	}

	/* precision recall breakevens too */
	astd->prb[nloop] = prec_recall_breakeven(test_evals, astd->test_yvect, 
						 astd->ntest, npos);
	free(test_evals);
      }
    } else {
      /* we can use the scores that we got last time (they'll still be the same) - just
       * remove the previous ones from the scores array... */
      /* since nothing changed, we don't need to recalculate the test accuracy */
      if (astd->prb) {
	astd->apvect[nloop] = astd->apvect[nloop-1];
	astd->anvect[nloop] = astd->anvect[nloop-1];
	astd->prb[nloop] = astd->prb[nloop-1];
      }
	
      if (astd->train_dim_sat_vect) {
	for (i=0; i<astd->ndim_sat; i++) {
	  astd->train_dim_sat_vect[i][nloop] = astd->train_dim_sat_vect[i][nloop-1];
	}
      } 

      if (astd->sv_dim_sat_vect) {
	for (i=0; i<astd->ndim_sat; i++) {
	  astd->sv_dim_sat_vect[i][nloop] = astd->sv_dim_sat_vect[i][nloop-1];
	}
      }

      if (astd->test_scores) {
	for (i=0; i<astd->ntest; i++) {
	  astd->test_scores[nloop][i] = astd->test_scores[nloop-1][i];
	}
      }

      /* this code doesn't get touched till after stuff was added 
       * (ie. ignore gcc's warnings about uninitialized memory) */
      nleft -= dec;
      train_cscores = &(train_cscores[dec]);
    }

    if (sub_ndocs == ndocs) {
      break;
    }

    /* now use the scores (& possibly other things) to chose the next examples to learn */
    if (nleft < qsize) {
      dec = nleft;
    } else {
      dec = qsize;
    }

    /* do this even if nleft<qsize to find the min... */
    if (!do_random_learning) {
      get_top_n(train_cscores, nleft, dec);
    }

    /* this is where the termination criteria goes - right now its pretty dumb... */
    /* (it would be a fn, but since bookkeeping & setting up need to go on in here
     * anyway, the crux also does */
    if ((train_cscores[0].d > 1) && (0)) {
      break;
    }
    
    for (j=0; j<dec; j++) {
      SETVALID(used, train_cscores[j].i);
      tdocs[sub_ndocs+j] = train_cscores[j].i;
      sub_docs[sub_ndocs+j] = train_docs[train_cscores[j].i];
      sub_yvect[sub_ndocs+j] = train_yvect[train_cscores[j].i];

      if (astd->scores_added) 
	astd->scores_added[sub_ndocs+j] = train_cscores[j].d;
    }

    last_subndocs = sub_ndocs;

    if (svm_use_smo) {
      struct svm_smo_model model;
      
      model.docs = sub_docs;
      model.ndocs = sub_ndocs;
      model.weights = sub_weights;
      model.yvect = sub_yvect;
      model.W = W;

      for (j=0; j<dec; j++) {
	tvals[sub_ndocs] += smo_evaluate_error(&model, sub_ndocs) - tb;
	sub_ndocs ++;
      }	
    } else {
      int n;
      for (n=0; n<dec; n++) {
	for (j=k=0; k<nsv; j++) {
	  if (sub_weights[j] != 0.0) {
	    tvals[sub_ndocs] += sub_weights[j] * sub_yvect[j] * 
	      svm_kernel_cache(sub_docs[sub_ndocs],sub_docs[j]);
	    k++;
	  }
	}
	sub_ndocs++;
      }
    }

    if (svm_kernel_type == 0) {
      free(W);
    }
  }

  printf("Queried for a total of %d labels.\n",sub_ndocs);

  /* once the active learning is done, the inconsistent examples may be removed */
  if (remove_wrong) {
    svm_remove_misclassified = 1;
    fprintf(stderr,"Running again to remove inconsistent examples.\n");
    if (svm_use_smo) {
      nsv = smo(sub_docs, sub_yvect, sub_weights, &tb, &W, sub_ndocs, tvals, &nsv);
    } else {
#ifdef HAVE_LOQO
      nsv = build_svm_guts(sub_docs, sub_yvect, sub_weights, &tb, &W, sub_ndocs, tvals, &nsv);
#else
      printf("Must build rainbow with pr_loqo to use this solver!\n");
#endif
    }
  }

  free(train_scores);
  free(sub_docs);
  free(sub_yvect);
  free(tvals);
  free(used);

  free(sv_sat_vect);
  free(old_svbitmap);

  if (svm_kernel_type == 0) {
    for (i=j=0; i<num_words; i++) {
      if (W[i] != 0.0) 
	j++;
    }

    (*W_wv) = bow_wv_new(j);
    for (i=j=0; j<(*W_wv)->num_entries; i++) {
      if (W[i] != 0.0) {
	(*W_wv)->entry[j].wi = i;
	(*W_wv)->entry[j].count = 1; /* just so that an assertion doesn't throw up later */
	(*W_wv)->entry[j].weight = W[i];
	j++;
      }
    }
    free(W);
  }

  /* fill everything back in - including the weight vector in the order that the
   * caller is expecting... */
  for (i=0; i<sub_ndocs; i++) {
    /* if we haven't looked at it (ie. not present in tdocs), then we won't 
     * reset it & it was already initialized to 0... */
    weights[tdocs[i]] = sub_weights[i];
  }
  free(tdocs);
  free(sub_weights);

  *b = tb;
  return nsv;
}

/* this cuts up the training set into training & validation */
int al_svm_test_wrapper(bow_wv **docs, int *yvect, double *weights, double *b, 
			bow_wv **W, int ndocs, int do_ts, int do_random_learning) {
  struct al_test_data altd;
  int      max_iter;
  bow_wv **train_docs;
  int     *train_y;
  int      ntrain;
  int      nsv;
  int     *permute_table;
  int      tp, tn;
  int  i,j,k;

  ntrain = altd.ntest = 0;

  if (svm_random_seed == 0) {
    svm_random_seed = (int) time(NULL);
    printf("random seed to chop test/train split: %d\n",svm_random_seed);
    fprintf(stderr,"random seed to chop test/train split: %d\n",svm_random_seed);
  }

  srandom(svm_random_seed);

  permute_table = (int *) malloc(sizeof(int)*ndocs);

  svm_permute_data(permute_table, docs, yvect, ndocs);

  /* lets try to bring some lesser determinism back... */
  srandom((int) time(NULL));

  ntrain = ndocs/2;
  altd.ntest = ndocs - ntrain;

  train_docs = docs;
  train_y = yvect;
  altd.test_docs = &(docs[ntrain]);
  altd.test_yvect = &(yvect[ntrain]);

  max_iter = ((ntrain+svm_al_qsize-1) / svm_al_qsize) + 1;
  
  altd.apvect = (int *) malloc(sizeof(int)*max_iter);
  altd.anvect = (int *) malloc(sizeof(int)*max_iter);
  altd.nsv_vect = (int *) malloc(sizeof(int)*max_iter);
  altd.nbsv_vect = (int *) malloc(sizeof(int)*ntrain);
  altd.prb = (double *) malloc(sizeof(double)*max_iter);
  altd.nkce_vect = (int *) malloc(sizeof(int)*max_iter);
  altd.time_vect = (int *) malloc(sizeof(int)*max_iter);
  
  if (do_ts) {
    altd.test_scores = (double **) malloc(sizeof(double *)*max_iter);
    
    for (i=0; i<max_iter; i++) {
      altd.test_scores[i] = (double *) malloc(sizeof(double)*altd.ntest);
    }
  } else {
    altd.test_scores = NULL;
  }

  altd.npos_added = (int *) malloc(sizeof(int)*max_iter+1);
  altd.nneg_added = (int *) malloc(sizeof(int)*max_iter+1);
  altd.docs_added = (int *) malloc(sizeof(int)*ntrain);
  altd.scores_added = (double *) malloc(sizeof(double)*ntrain);

  for (i=0; i<ntrain; i++) {
    altd.scores_added[i] = 0.0;
  }

  memset(altd.apvect, -1, max_iter*sizeof(int));
  memset(altd.anvect, -1, max_iter*sizeof(int));

  altd.sv_dim_sat_vect = (int **) malloc(NDIM_INSPECTED*sizeof(int *));
  altd.train_dim_sat_vect = (int **) malloc(NDIM_INSPECTED*sizeof(int *));
  for(i=0; i<NDIM_INSPECTED; i++) {
    altd.sv_dim_sat_vect[i] = (int *) malloc(sizeof(int)*max_iter);
    altd.train_dim_sat_vect[i] = (int *) malloc(sizeof(int)*max_iter);
  }

  nsv = al_svm_guts(train_docs, train_y, weights, b, W, ntrain, 
		    &altd, do_random_learning);

  for (i=tp=tn=0; i<altd.ntest; i++) {
    if (altd.test_yvect[i] == 1) {
      tp ++;
    } else {
      tn ++;
    }
  }


  printf("%d positive test documents, %d negative test documents.\npositive accuracy vector: ",tp,tn);
  for (i=0; (altd.apvect[i]>=0) && i < max_iter; i++) {
    printf("  %d", altd.apvect[i]);
  }
  printf("\nnegative accuracy vector: ");
  for (j=0; j<i; j++) {
    printf("  %d", altd.anvect[j]);
  }
  printf("\nprecision/recall breakeven vector: ");
  for (j=0; j<i; j++) {
    printf("  %f", altd.prb[j]);
  }
  {
    int k;
    int start_index= MIN(ntrain, svm_init_al_tset);
    printf("\n\"Real\" document indices when added: ");

    printf("0(%d",permute_table[altd.docs_added[0]]);
    for (k=1; k<start_index; k++) {
      printf(",%d",permute_table[altd.docs_added[k]]);
    }
    printf(") ");

    for (j=0; j<i-1; j++) {
      printf("%d(%d",j+1,permute_table[altd.docs_added[j*svm_al_qsize+start_index]]);
      for (k=1; k<svm_al_qsize && k+j*svm_al_qsize+start_index<ntrain; k++) {
	printf(",%d",permute_table[altd.docs_added[j*svm_al_qsize+start_index+k]]);
      }
      printf(") ");
    }
    printf("\nminimum scores of documents when added: ");
    for (j=0; j<i-1; j++) {
      printf("  %f", altd.scores_added[j*svm_al_qsize+svm_init_al_tset]);
    }
    printf("\naverage scores of documents when added: ");
    for (j=0; j<i-1; j++) {
      double avg = 0.0;
      for (k=0; k<svm_al_qsize && k+j*svm_al_qsize+svm_init_al_tset<ntrain; k++) {
	avg += altd.scores_added[j*svm_al_qsize+k+svm_init_al_tset];
      }
      printf("  %f", avg/k);
    }
  }
  printf("\nnumber of positive documents inspected: ");
  for (j=0; j<i; j++) {
    printf(" %d", altd.npos_added[j]);
  }
  printf("\nnumber of negative documents inspected: ");
  for (j=0; j<i; j++) {
    printf(" %d", altd.nneg_added[j]);
  }
  printf("\nnumber of support vectors: ");
  for (j=0; j<i; j++) {
    printf("  %d", altd.nsv_vect[j]);
  }
  printf("\nnumber of bounded support vectors: ");
  for (j=0; j<i; j++) {
    printf("  %d", altd.nbsv_vect[j]);
  }
  printf("\nrunning times: ");
  for (j=0; j<i; j++) {
    printf("  %d", altd.time_vect[j]);
  }
  printf("\nkernel_cache calls: ");
  for (j=0; j<i; j++) {
    printf(" %d", altd.nkce_vect[j]);
  }
  for (k=0; k<NDIM_INSPECTED; k++) {
    /* following is only good if the 0'th # of dimensions == 1 */
    int num_words = altd.train_dim_sat_vect[0][i-1];
    printf("\nnumber of SV dimensions with more than %d elements (%d total dimensions): ", dim_map(k), num_words);
    for (j=0; j<i; j++) {
      printf("  %d", altd.sv_dim_sat_vect[k][j]);
    }
  }
  for (k=0; k<NDIM_INSPECTED; k++) {
    int num_words = altd.train_dim_sat_vect[0][i-1];
    printf("\nnumber of train dimensions with more than %d elements (%d total dimensions): ", dim_map(k), num_words);
    for (j=0; j<i; j++) {
      printf("  %d", altd.train_dim_sat_vect[k][j]);
    }
  }
  if (do_ts) {
    printf("\nbegin score matrix:");
    for (j=0; j<i; j++) {
      int k;
      printf("\n");
      for (k=0; k<altd.ntest; k++) {
	printf(" %.3f", altd.test_scores[j][k]);
      }
    }
    printf("\nend score matrix\n");
    for (i=0; i<max_iter; i++) {
      free(altd.test_scores[i]);
    }
    free(altd.test_scores);
  } else {
    printf("\n");
  }

  free(altd.apvect);
  free(altd.anvect);
  free(altd.prb);
  free(altd.nsv_vect);
  free(altd.nbsv_vect);
  free(altd.time_vect);
  free(altd.sv_dim_sat_vect);
  free(altd.train_dim_sat_vect);
  free(altd.nkce_vect);

  svm_unpermute_data(permute_table, docs, yvect, ndocs);

  free(permute_table);

  return nsv;
}

int al_svm(bow_wv **docs, int *yvect, double *weights, double *b, bow_wv **W, 
	   int ndocs, int do_rlearn) {
  struct al_test_data altd;

  bzero(&altd,sizeof(struct al_test_data));

  return (al_svm_guts(docs, yvect, weights, b, W, ndocs, &altd, do_rlearn));
}
