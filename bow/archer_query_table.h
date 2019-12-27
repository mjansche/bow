#ifndef ARCHER_QUERY_TABLE_H_
#define ARCHER_QUERY_TABLE_H_

void archer_query_table_invert(bow_array **table);
void archer_query_table_empty(bow_array **table);
bow_array *archer_query_table_to_bow_array_with_freeing(bow_array **table);
bow_array **archer_query_table_new(void);
void archer_query_table_free(bow_array **table);
void archer_query_table_add(bow_array **table, int index, bow_array *wo);
bow_array **archer_query_table_copy(bow_array **table);

#endif
