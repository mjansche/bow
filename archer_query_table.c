#include <bow/libbow.h>
#include <bow/archer.h>
#include <bow/archer_query.h>
#include <bow/archer_query_array.h>

extern bow_sarray *archer_docs;


void archer_query_table_invert(bow_array **table)
{
  int i;
  int len = archer_docs->array->length;

  for (i = 0; i < len; ++i)
    if (table[i])
    {
      bow_array_free(table[i]);
      table[i] = NULL;
    }
    else
      table[i] = bow_array_new(0, sizeof(archer_query_word_occurence), 
			       archer_query_array_free_wo);
}

void archer_query_table_empty(bow_array **table)
{
  int i;
  int len = archer_docs->array->length;

  for (i = 0; i < len; ++i)
    if (table[i])
    {
      bow_array_free(table[i]);
      table[i] = NULL;
    }
}

bow_array *archer_query_table_to_bow_array_with_freeing(bow_array **table)
{
  int i;
  int len = archer_docs->array->length;
  archer_query_result aqr;
  bow_array *ret = bow_array_new(0, sizeof(archer_query_result), 
				 archer_query_array_free_result);

  for (i = 0; i < len; ++i)
    if (table[i])
    {
      aqr.di = i;
      aqr.wo = table[i];
      bow_array_append(ret, &aqr);
    }

  free(table); 

  return ret;
}

bow_array **archer_query_table_new(void)
{
  int len = archer_docs->array->length;
  int space = len * sizeof(bow_array *);
  bow_array **new = (bow_array **) bow_malloc(space);
  
  memset(new, 0, space);
  return new;
}

void archer_query_table_free(bow_array **table)
{
  int i;
  int len = archer_docs->array->length;

  for (i = 0; i < len; ++i)
    if (table[i])
      bow_array_free(table[i]);

  free(table);
}

static bow_array *archer_query_table_copy_wo(bow_array *wo_array)
{
    int i, j;
    int len = wo_array->length;
    archer_query_word_occurence *wo, new;
    bow_array *ret = bow_array_new(len, sizeof(archer_query_word_occurence), 
				   archer_query_array_free_wo);

    for (i = 0; i < wo_array->length; ++i)
    {
	wo = bow_array_entry_at_index(wo_array, i);
	new = *wo;

	new.pi = bow_array_new(wo->pi->length, sizeof(int), NULL);
	for (j = 0; j < wo->pi->length; ++j)
	    bow_array_append(new.pi, bow_array_entry_at_index(wo->pi, j));
	
	bow_array_append(ret, &new);
    }

    return ret;
}

bow_array **archer_query_table_copy(bow_array **table)
{
    int i;
    int len = archer_docs->array->length;
    bow_array **new = archer_query_table_new();
    
    for (i = 0; i < len; ++i)
	if (table[i])
	    new[i] = archer_query_table_copy_wo(table[i]);

    return new;
}

/*
void archer_query_table_add(bow_array **table, int index, bow_array *wo)
{
  int len = archer_docs->array->length;
  bow_array *old;

  assert(index >= 0 && index < len);
  old = table[index];

  if (old)
  {
    table[index] = merge_wo(old, wo);
    bow_array_free(wo);
    bow_array_free(old);
  }
  else
    table[index] = wo;
}
*/
