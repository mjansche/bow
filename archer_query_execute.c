#include <bow/libbow.h>
#include <bow/archer.h>
#include <bow/archer_query.h>
#include <bow/archer_query_array.h>
#include <bow/archer_query_execute.h>
#include <bow/archer_query_index.h>
#include <bow/archer_query_table.h>

extern bow_sarray *archer_docs;
archer_query_info *archer_query_last_query = NULL;
int archer_query_doc_restriction = -1;

static int
list_length (archer_query_term * term)
{
  int ret;

  ret = 0;
  while (term && term->proximity)
    {
      ret++;
      term = term->proximity->term;
    }

  if (term)
    ret++;

  return ret;
}

/* moves the index file pointers from where they are to the next di that is
   >= target_di for term `term', and places the final di in result_di. if this
   is not possible (i.e. no more dis that meet that critereon exist) then the
   return value is 1; success returns 0. */
static int
scan_to_di (bow_index * index, archer_query_term * term, int target_di)
{
  int ret, pi;

  archer_query_index_current_di (index, term, &ret, &pi);
  while ((ret != -1) && 
	 ((archer_query_doc_restriction > -1 && 
	   ret != archer_query_doc_restriction) || 
	  (ret < target_di)))
    archer_query_index_next_di (index, term, &ret);

  return ret;
}

/* Insert into pi_array, maintaining sortedness */
static void
insert_pi_into_pi_array(int pi, bow_array *pi_array)
{
  void *ptr;
  int i, opi, len = pi_array->length;

  for (i = 0; i < len; ++i)
  {
    opi = *(int *)bow_array_entry_at_index(pi_array, i);
    if (pi == opi) 
      return;
    if (opi > pi)
      break;
  }

  /* i is now the appropriate index for pi */

  if (i == len)
    bow_array_append(pi_array, &pi);
  else
  {
    /* Make space for a new entry */
    opi = -1;
    bow_array_append(pi_array, &opi);

    /* Shift contents up one space */
    ptr = bow_array_entry_at_index(pi_array, i);
    memmove(ptr + sizeof(int), ptr, sizeof(int) * (len - i));

    /* Insert new pi */
    *((int *)ptr) = pi;
  }
}


static int
insert_new_pi_existing_wo(int pi, archer_query_word_occurence *wop, 
			  bow_array *wo_array)
{
  int i;
  archer_query_word_occurence *wop2 = NULL;

  for (i = 0; i < wo_array->length; ++i)
  {
    wop2 =(archer_query_word_occurence *)bow_array_entry_at_index(wo_array, i);
    if (wop->wi == wop2->wi)
    {
      if (wop->is_li == wop2->is_li)
	break;
      else
	return 0;
    }
    if (wop->wi < wop2->wi)
      return 0;
  }

  if (i == wo_array->length)
    return 0;

  insert_pi_into_pi_array(pi, wop2->pi);
  return 1;
}


static void
insert_new_wo(archer_query_word_occurence *wop, bow_array *wo_array)
{
  void *ptr;
  int i, len = wo_array->length;
  int wosz = sizeof(archer_query_word_occurence);
  archer_query_word_occurence *wop2;

  for (i = 0; i < len; ++i)
  {
    wop2 = (archer_query_word_occurence*)bow_array_entry_at_index(wo_array, i);
    if (wop2->wi > wop->wi || (wop2->wi == wop->wi && !wop2->is_li))
      break;
  }

  /* i is now the appropriate index for wop */

  if (i == len)
    bow_array_append(wo_array, wop);
  else
  {
    /* Make space for a new entry */
    bow_array_append(wo_array, wop);

    /* Shift contents up one space */
    ptr = bow_array_entry_at_index(wo_array, i);
    memmove(ptr + wosz, ptr, wosz * (len - i));

    /* Insert new pi */
    memcpy(ptr, wop, wosz);
  }
}


static void
single_term_result(archer_query_term *term, int di, int pi, bow_array **table)
{
  archer_query_word_occurence wo;

  if (!table[di])
    table[di] = bow_array_new(8, sizeof(archer_query_word_occurence), 
			      archer_query_array_free_wo);

  if (term->word)
  {
    wo.is_li = 0;
    wo.wi = bow_word2int_no_add (term->word);
  }
  else 
  {
    archer_label *lp = 
      bow_sarray_entry_at_keystr(archer_labels, term->labels->string);
    wo.is_li = 1;
    wo.wi = lp->li;
  }

  wo.weight = term->weight;
  wo.term = term;

  if (insert_new_pi_existing_wo(pi, &wo, table[di]))
    return;

  wo.pi = bow_array_new(32, sizeof(int), NULL);
  bow_array_append(wo.pi, &pi);

  insert_new_wo(&wo, table[di]);
}


/* recursively assembles the bow_array of results that satisfies all the
   proximity constraints of the linked list `term' passed. pi_arrays should
   contain the positions of the terms (e.g. pi_arrays[0] contains the
   positions of term->proximity->term, pi_arrays[0] contains those of
   term->proximity->term->proximity->term, etc.)

   possibly some kind of DP approach would be better here; this is going to
   make a lot of redundant calls i think */
static int
search_recursive (archer_query_term * term, int di, int pi,
		  bow_array ** pi_arrays, bow_array **table)
{
  int i, opi, prox, ret, good = 0;

  if (term == NULL)
    return 1;

  /* no recursion needed; just return a single-element bow_array for term */
  else if (term->proximity == NULL)
  {
    if (table)
      single_term_result(term, di, pi, table);
    return 1;
  }

  else
  {
    ret = 0;

    for (i = 0; i < pi_arrays[0]->length; i++)
    {
      opi = *((int *)bow_array_entry_at_index(pi_arrays[0], i));
      prox = term->proximity->proximity;

      switch (term->proximity->position)
      {
      case ARCHER_QUERY_PTERM_BEFORE :
	good = opi > pi && opi - pi <= prox;
	break;
      case ARCHER_QUERY_PTERM_AFTER :
	good = pi > opi && pi - opi <= prox;
	break;
      case ARCHER_QUERY_PTERM_WITHIN :
	good = ABS(pi - opi) <= prox;
	break;
      }

      if (good && 
	  search_recursive(term->proximity->term, di, opi, &pi_arrays[1], 
			   table))
      {
	if (table)
	{
	  ret = 1;
	  single_term_result(term, di, pi, table);
	}
	else
	  return 1;
      }
    }
  }

  return ret;
}


#define good_di(table, di, exclude) \
                  ((table) == NULL || \
		   ((table[di]) && !(exclude)) || \
		   (!(table[di]) && (exclude)))


/* next_term_di advances to next di in which all term components co-occur.
   Does not check that proximity constraints are satisfied. The third 
   argument, if provided, is a shortlist; exclude tells whether to use it
   as an exclusion or inclusion list */

static int 
next_term_di(int current_di, bow_index *index, bow_array **table, int exclude,
	     archer_query_term *term)
{
  archer_query_term *cterm = term;

  ++current_di;

  while (1)
    {
      while (cterm)
	{
	  int di = scan_to_di(index, cterm, current_di);

	  if (di == -1)
	    return -1;

	  if (di == current_di)
	    {
	      /* move on to the next term, recording this term's file pos */
	      cterm = cterm->proximity ? cterm->proximity->term : NULL;
	    }
	  else  /* Try the next di */
	    {
	      current_di = di == current_di ? di + 1 : di;
	      cterm = term;
	    }
	}

      /* 
	 This is a little inaccurate, since the terms may co-occur
	 without satisfying proximity constraints.  Done this way to
	 avoid the cost of verifying prox constraints unnecessarily.
      */
      term->head->df++;

      if (good_di(table, current_di, exclude))
	break;

      ++current_di;
      cterm = term;
    }

  return current_di;
}


static int
satisfies_proximity_constraints(int di, bow_index *index, archer_query_term *term)
{
  int i, j, pi, satisfies;
  int num_terms = list_length(term);
  bow_array *pi_arrays[num_terms];
  archer_query_term *cterm;

  for (cterm = term, i = 0; 
       cterm; 
       cterm = cterm->proximity ? cterm->proximity->term : NULL, ++i)
    pi_arrays[i] = archer_query_index_current_pis(index, cterm);
  
  satisfies = 0;
  for (j = 0; j < pi_arrays[0]->length; ++j)
  {
    pi = *((int *)bow_array_entry_at_index(pi_arrays[0], j));
    if (search_recursive(term, di, pi, &pi_arrays[1], NULL))
    {
      satisfies = 1;
      break;
    }
  }

  while (--i >= 0) bow_array_free(pi_arrays[i]); 

  return satisfies;
}


static int
add_if_satisfies_proximity_constraints(int di, bow_index *index, 
				       archer_query_term *term, 
				       bow_array **table)
{
  int i, j, pi, ret = 0;
  int num_terms = list_length(term);
  bow_array *pi_arrays[num_terms];
  archer_query_term *cterm;

  for (cterm = term, i = 0; 
       cterm; 
       cterm = cterm->proximity ? cterm->proximity->term : NULL, ++i)
    pi_arrays[i] = archer_query_index_current_pis(index, cterm);
  
  for (j = 0; j < pi_arrays[0]->length; ++j)
  {
    pi = *((int *)bow_array_entry_at_index(pi_arrays[0], j));
    if (search_recursive(term, di, pi, &pi_arrays[1], table))
      ret = 1;
  }

  while (--i >= 0) bow_array_free(pi_arrays[i]); 

  return ret;
}


static inline void
delete_intervening_entries(bow_array **table, int lastdi, int di)
{
  int i;
  for (i = lastdi + 1; i < di; ++i)
    if (table[i])
    {
      bow_array_free(table[i]);
      table[i] = NULL;
    }
}


static void
search_restrict(bow_index *index, bow_array **table, archer_query_term *term, 
		int exclude)
{
  int di, lastdi;
  int len = archer_docs->array->length;

  archer_query_index_reset (index);

  di = next_term_di(-1, index, (exclude ? table : NULL), 0, term);
  lastdi = -1;
  while (di != -1)
  {
    if (satisfies_proximity_constraints(di, index, term))
    {
      if (exclude)
      {
	assert(table[di]);
	bow_array_free(table[di]);
	table[di] = NULL;
      }
      else   /* Delete all intervening docs that didn't match */
      {
	delete_intervening_entries(table, lastdi, di);
	if (table[di])
	  add_if_satisfies_proximity_constraints(di, index, term, table);
	lastdi = di;
      }
    }

    di = next_term_di(di, index, (exclude ? table : NULL), 0, term);
  }

  delete_intervening_entries(table, lastdi, len);
}


static void
search (bow_index *index, bow_array **table, archer_query_term *term,
	bow_array **shortlist, int exclude)
{
  int len, di;

  len = archer_docs->array->length;
  archer_query_index_reset(index);
  di = next_term_di(-1, index, shortlist, exclude, term);
  while (di != -1)
  {
    add_if_satisfies_proximity_constraints(di, index, term, table);
    di = next_term_di(di, index, shortlist, exclude, term);
  }
}


/* 
   fill in the `score' elements of a bow_array of results.
   Score used: tfidf(w) = tf(w) * log(|D| / df(w))
*/
static void
calculate_tfidf (bow_index * index, bow_array * array)
{
  extern bow_sarray *archer_docs;
  int doccount = archer_docs->array->length;
  int i;

  for (i = 0; i < array->length; i++)
    {
      archer_query_result *current;
      int j;

      current = (archer_query_result *) bow_array_entry_at_index (array, i);
      current->score = 0.0;

      for (j = 0; j < current->wo->length; j++)
	{
	  archer_query_word_occurence *current_wo;

	  current_wo = (archer_query_word_occurence *)
	    bow_array_entry_at_index (current->wo, j);

	  if (current_wo->term->head->idf < 0.0)
	    current_wo->term->head->idf = 
	      log(((double)doccount) / ((double) current_wo->term->head->df));

	  current->score += 
	    current_wo->pi->length * 
	    current_wo->weight * 
	    current_wo->term->head->idf;
	}
    }
}


/*
  Set all member terms in a proximity list to point to the first
  (for DF calculation).
*/
static void
archer_query_thread(archer_query_term *term)
{
  archer_query_term *pterm;

  while (term)
    {
      for (pterm = term; 
	   pterm; 
	   pterm = pterm->proximity ? pterm->proximity->term : NULL)
	pterm->head = term;
      term = term->next;
    }
}


bow_array *
archer_query_execute (bow_index * index, archer_query_info * query)
{
  int exclude = 0;
  archer_query_term *term;
  bow_array **table = NULL, **shortlist = NULL, *ranking_results = NULL;

  if (query)
  {
    archer_query_doc_restriction = -1;
    archer_query_last_query = query;
  }
  else
    query = archer_query_last_query;

  archer_query_thread(query->inclusion);
  archer_query_thread(query->exclusion);
  archer_query_thread(query->ranking);

  for (term = query->inclusion; term; term = term->next)
  {
    if (table)
      /* Delete any items in table that _don't_ match term */
      search_restrict(index, table, term, 0); 
    else
    {
      /* Do an unconstrained search for matching docs */
      table = archer_query_table_new();
      search(index, table, term, NULL, 0);
    }
  }

  /* If table is non-NULL, we have a short list */

  if (query->exclusion)
  {
    if (query->inclusion)
    {
      /* We have a short list of documents; delete any with exclusion terms */
      for (term = query->exclusion; term; term = term->next)
	search_restrict(index, table, term, 1);
    }
    else
    {
      /* No short list: Create an exclusion table */
      exclude = 1;
      table = archer_query_table_new();
      for (term = query->exclusion; term; term = term->next)
	search(index, table, term, NULL, 0);
    }
  }

  /* If table is non-NULL, it is meant to restrict search in some way.
     If exclude = 1, it contains a list of docs to exclude */

  if (query->ranking)
  {
    shortlist = table;
    table = (exclude || !shortlist) ? 
	      archer_query_table_new() : 
	      archer_query_table_copy(shortlist);

    for (term = query->ranking; term; term = term->next)
      search(index, table, term, shortlist, exclude);

    if (shortlist)
      archer_query_table_free(shortlist);
  }

  /* This happens when only exclusion terms are given.  The right thing
     to do in such a case is just to return an empty list */
  else if (exclude)
  {
    /* archer_query_table_invert(table); */
    archer_query_table_empty(table);
  }

  ranking_results = archer_query_table_to_bow_array_with_freeing(table);
  calculate_tfidf (index, ranking_results);

  return ranking_results;
}



bow_array *
archer_query_repeat_for_document(bow_index *index, int di)
{
  if (!archer_query_last_query)
    return NULL;
  archer_query_doc_restriction = di;
  return archer_query_execute(index, NULL);
}



