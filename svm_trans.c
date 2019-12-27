/* Copyright (C) 1999 Greg Schohn - gcs@jprc.com */

/* ********************* svm_trans.c **********************
 * Code for transductive SVM's */
 
#include <bow/svm.h>

int transduce_svm(bow_wv **docs, int *yvect, double *weights, double *b, 
		  bow_wv **W_wv, int ndocs, int ntrans) {
  int  nlabeled;
  int *permute_table;



  nlabeled = ndocs-ntrans;
  permute_table = malloc(sizeof(int)*ndocs);

  /* permute each part, but don't mudge them together, because the 
   * solvers are going to expect all unlabeled data (data with a 
   * different C* to be in the latter half) */
  svm_permute_data(permute_table, docs, yvect, nlabeled);
  svm_permute_data(&(permute_table[nlabeled]), &(docs[nlabeled]), &(yvect[nlabeled]), ntrans);

  return 0;
}
