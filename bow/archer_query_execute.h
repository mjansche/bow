#ifndef ARCHER_QUERY_EXECUTE_H_
#define ARCHER_QUERY_EXECUTE_H_

#include "bow/libbow.h"
#include "bow/archer.h"
#include "archer_query.h"
#include "archer_query_index.h"

typedef struct
  {
    int is_li;                  /* Is this a label? */
    int wi;                     /* ID of word or label */
    float weight;               /* User-provided weight on term */
    archer_query_term *term;    /* Query term to which this corresponds */
    bow_array *pi;		/* position indices */
  }
archer_query_word_occurence;

typedef struct
  {
    int di;
    double score;
    bow_array *wo;		/* archer_query_word_occurences */
  }
archer_query_result;

bow_array *
  archer_query_execute (bow_index * index, archer_query_info * query);
bow_array *
  archer_query_repeat_for_document (bow_index * index, int di);

#endif
