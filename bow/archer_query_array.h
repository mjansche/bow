#ifndef ARCHER_QUERY_ARRAY_H_
#define ARCHER_QUERY_ARRAY_H_
#include "archer_query_execute.h"

void
  archer_query_array_free_wo (archer_query_word_occurence * wo);

void
  archer_query_array_free_result (archer_query_result * r);

void
  archer_query_array_append(bow_array *onto, bow_array *from);

bow_array *
  archer_query_array_intersection (bow_array * a, bow_array * b);

bow_array *
  archer_query_array_union (bow_array * a, bow_array * b);

bow_array *
  archer_query_array_subtract (bow_array * a, bow_array * b);

bow_boolean
archer_query_array_contains (bow_array * a, int di);

bow_array *
  bow_array_duplicate (bow_array * array);

bow_array *
  bow_array_duplicate_wo (bow_array * array);

#endif
