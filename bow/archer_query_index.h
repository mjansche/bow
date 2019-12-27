#ifndef ARCHER_QUERY_INDEX_H_
#define ARCHER_QUERY_INDEX_H_

#include "archer_query.h"

typedef struct
  {
    bow_wi2pv *wi2pv;
    bow_wi2pv *li2pv;
    /*
       other indices, which do not current exist:
       bow_di2pv * di2pv;
     */
  }
bow_index;

extern bow_sarray *archer_labels;

void
  archer_query_index_reset (bow_index * index);

void
  archer_query_index_next_di_pi (bow_index * index, archer_query_term * term,
				 int *di, int *pi);

void
  archer_query_index_next_di (bow_index * index, archer_query_term * term,
			      int *di);

void
  archer_query_index_current_di (bow_index * index, archer_query_term * term,
				 int *di, int *pi);

bow_array *
  archer_query_index_current_pis (bow_index * index, archer_query_term * term);

#endif
