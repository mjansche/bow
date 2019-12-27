#include <bow/libbow.h>
#include <bow/archer_query.h>
#include <bow/archer_query_array.h>

/* free function for passing to bow_array_new() for word_occurences */
void
archer_query_array_free_wo (archer_query_word_occurence * wo)
{
  /* printf(". freeing word_occurence %p\n", wo); */
  bow_array_free (wo->pi);
}

/* free function for passing to bow_array_new() for results */
void
archer_query_array_free_result (archer_query_result * r)
{
  /* printf(". freeing result %p with di %d and wo %p\n", r, r->di, r->wo); */
  bow_array_free (r->wo);
}

/* merge two bow_arrays of ints --- union in other words */
static bow_array *
merge_int (bow_array * a, bow_array * b)
{
  bow_array *ret;
  int i, j;

  ret = bow_array_new (0, sizeof (int), NULL);

  j = 0;
  for (i = 0; i < a->length; i++)
    {
      while ((j < b->length) && (*(int *) bow_array_entry_at_index (a, i) >
				 *(int *) bow_array_entry_at_index (b, j)))
	{
	  bow_array_append (ret, (int *) bow_array_entry_at_index (b, j));
	  j++;
	}

      if ((j < b->length) && (*(int *) bow_array_entry_at_index (a, i) ==
			      *(int *) bow_array_entry_at_index (b, j)))
	j++;

      bow_array_append (ret, (int *) bow_array_entry_at_index (a, i));
    }

  for (; j < b->length; j++)
    bow_array_append (ret, (int *) bow_array_entry_at_index (b, j));

  return ret;
}

/* add a duplicate of a word_occurence to a bow_array */
static void
dup_add_wo (bow_array * a, archer_query_word_occurence * wo)
{
  archer_query_word_occurence new_word_occurence;

  new_word_occurence.is_li = wo->is_li;
  new_word_occurence.wi = wo->wi;
  new_word_occurence.pi = bow_array_duplicate (wo->pi);
  bow_array_append (a, &new_word_occurence);
}

/* add a new word_occurence to a bow_array that contains a pi list that is
   the union of the two word_occurences passed */
static void
dup_add_merge_wo (bow_array * a, archer_query_word_occurence * wo1,
		  archer_query_word_occurence * wo2)
{
  archer_query_word_occurence new_word_occurence;

  new_word_occurence.is_li = wo1->is_li;
  new_word_occurence.wi = wo1->wi;
  new_word_occurence.pi = merge_int (wo1->pi, wo2->pi);
  bow_array_append (a, &new_word_occurence);
}


static inline int
wo_cmp(bow_array *a, int ai, bow_array *b, int bi)
{
  archer_query_word_occurence 
    *awo = (archer_query_word_occurence *)bow_array_entry_at_index(a, ai),
    *bwo = (archer_query_word_occurence *)bow_array_entry_at_index(b, bi);

  if (awo->wi < bwo->wi)
    return -1;
  else if (awo->wi > bwo->wi)
    return +1;
  else if (awo->is_li && !bwo->is_li)
    return -1;
  else if (!awo->is_li && bwo->is_li)
    return +1;
  else
    return 0;
}

/* merge two arrays of word_occurences (union) */
static bow_array *
merge_wo (bow_array * a, bow_array * b)
{
  bow_array *ret;
  int i, j, cmp;

  ret = bow_array_new (0, sizeof (archer_query_word_occurence),
		       archer_query_array_free_wo);
  j = 0;

  for (i = 0; i < a->length; i++)
  {
    while (j < b->length)
    {
      cmp = wo_cmp(a, i, b, j);
      if (cmp <= 0) break;
      dup_add_wo (ret, (archer_query_word_occurence *)
		       bow_array_entry_at_index (b, j));
      j++;
    }

    if (j < b->length && cmp == 0)
    {
      dup_add_merge_wo (ret,
			(archer_query_word_occurence *)
			bow_array_entry_at_index (a, i),
			(archer_query_word_occurence *)
			bow_array_entry_at_index (b, j));
      j++;
    }
    else
      dup_add_wo (ret, (archer_query_word_occurence *)
		       bow_array_entry_at_index (a, i));
  }

  for (; j < b->length; j++)
    dup_add_wo (ret, (archer_query_word_occurence *)
		bow_array_entry_at_index (b, j));

  return ret;
}

/* add a new result to a bow_array that has a word_occurence list that is the
   union of the two passed */
static void
dup_add_merge_result (bow_array * a, archer_query_result * r1,
		      archer_query_result * r2)
{
  archer_query_result new_result;

  new_result.di = r1->di;
  new_result.wo = merge_wo (r1->wo, r2->wo);
  bow_array_append (a, &new_result);
}

/* add a duplicate of a result to a bow_array */
static void
dup_add_result (bow_array * a, archer_query_result * r)
{
  archer_query_result new_result;

  new_result.di = r->di;
  new_result.wo = bow_array_duplicate_wo (r->wo);
  bow_array_append (a, &new_result);
}

/* the actual code for doing unions and intersections of bow_arrays of results.
 */
static bow_array *
union_or_intersection (bow_array * a, bow_array * b, int unionn)
{
  bow_array *ret;
  int i, j;

  ret = bow_array_new (0, sizeof (archer_query_result),
		       archer_query_array_free_result);
  j = 0;

  for (i = 0; i < a->length; i++)
    {
      while ((j < b->length) &&
	     (((archer_query_result *) bow_array_entry_at_index (a, i))->di >
	      ((archer_query_result *) bow_array_entry_at_index (b, j))->di))
	{
	  if (unionn)
	    dup_add_result (ret, (archer_query_result *)
			    bow_array_entry_at_index (b, j));
	  j++;
	}

      if ((j < b->length) &&
	  (((archer_query_result *) bow_array_entry_at_index (a, i))->di ==
	   ((archer_query_result *) bow_array_entry_at_index (b, j))->di))
	{
	  dup_add_merge_result (ret,
		      (archer_query_result *) bow_array_entry_at_index (a, i),
		     (archer_query_result *) bow_array_entry_at_index (b, j));
	  j++;
	}

      if (unionn)
	dup_add_result (ret, (archer_query_result *)
			bow_array_entry_at_index (a, i));
    }

  if (unionn)
    for (; j < b->length; j++)
      dup_add_result (ret, (archer_query_result *)
		      bow_array_entry_at_index (b, j));

  return ret;
}

/* Add the contents of "from" to the end of "onto".  Trash "from" */
void archer_query_array_append(bow_array *onto, bow_array *from)
{
    int i, len, last_di = -1;
    bow_array *tmp;
    archer_query_result *aqrp, *last = NULL;

    len = onto->length;
    if (len)
    {
      last = (archer_query_result *)bow_array_entry_at_index(onto, len - 1);
      last_di = last->di;
    }

    for (i = 0; i < from->length; ++i)
    {
	aqrp = (archer_query_result *)bow_array_entry_at_index(from, i);
	if (aqrp->di == last_di)
	{
	  tmp = merge_wo(last->wo, aqrp->wo);
	  bow_array_free(last->wo);
	  last->wo = tmp;
	}
	else
	{
	  last = (archer_query_result *)
	    bow_array_entry_at_index(onto, bow_array_append(onto, aqrp));
	  last->wo = bow_array_duplicate_wo(last->wo);
	  last_di = aqrp->di;
	}
    }

    bow_array_free(from);
}

/* assumes they're bow_arrays of results and sorted by di. returns a
   new array containing duplicates of all elements of a that have a di
   equal to that of an element of b, merging the pi lists of each
   result. */
bow_array *
archer_query_array_intersection (bow_array * a, bow_array * b)
{
  return union_or_intersection (a, b, 0);
}

/* assumes they're bow_arrays of results and sorted by di. returns a
   new array containing duplicates of all elements of a and all
   elements of b such that any element of a with the same di as an
   element of b has a word_occurrence array that is the union of the 
   two corresponding arrays. phew. */
bow_array *
archer_query_array_union (bow_array * a, bow_array * b)
{
  return union_or_intersection (a, b, 1);
}

/* assumes they're bow_arrays of results, sorted, and b is a subset of
   a. returns a new array containing duplicates of all elements of a
   that did not have a di equal to that of an element of b. */
bow_array *
archer_query_array_subtract (bow_array * a, bow_array * b)
{
  int i, j;
  bow_array *ret;

  assert (a->length > b->length);

  ret = bow_array_new (a->length - b->length, sizeof (archer_query_result),
		       archer_query_array_free_result);

  j = 0;
  for (i = 0; i < b->length; i++)
    {
      archer_query_result *aj;
      archer_query_result *bi;

      bi = (archer_query_result *) bow_array_entry_at_index (b, i);
      aj = (archer_query_result *) bow_array_entry_at_index (a, j);

      while (aj->di < bi->di)
      {
	dup_add_result (ret, aj);
	aj = (archer_query_result *) bow_array_entry_at_index(a, ++j);
      }

      assert (aj->di == bi->di);

      ++j;
    }

  return ret;
}

/* duplicates the array entirely, regardless of its content */
bow_array *
bow_array_duplicate (bow_array * array)
{
  bow_array *ret;

  ret = (bow_array *) malloc (sizeof (bow_array));
  memcpy (ret, array, sizeof (bow_array));
  ret->entries = calloc (ret->size, ret->entry_size);
  memcpy (ret->entries, array->entries, ret->size * ret->entry_size);

  return ret;
}

/* duplicates the array entirely, regardless of its content
   has to do a little extra because of some nested dynamically alloc'd mem
 */
bow_array *
bow_array_duplicate_wo (bow_array * wo_array)
{
  int i;
  bow_array *ret;
  archer_query_word_occurence *wop;

  ret = (bow_array *) malloc (sizeof (bow_array));
  memcpy (ret, wo_array, sizeof (bow_array));
  ret->entries = calloc (ret->size, ret->entry_size);
  memcpy (ret->entries, wo_array->entries, ret->size * ret->entry_size);

  for (i = 0; i < ret->length; ++i)
  {
    wop = (archer_query_word_occurence *)bow_array_entry_at_index(ret, i);
    wop->pi = bow_array_duplicate(wop->pi);
  }

  return ret;
}

bow_boolean
archer_query_array_contains (bow_array * a, int di)
{
  int i;
  bow_boolean found;

  found = bow_no;

  for (i = 0; (i < a->length) && !found; i++)
    if (((archer_query_result *) bow_array_entry_at_index (a, i))->di == di)
      found = bow_yes;

  return found;
}
