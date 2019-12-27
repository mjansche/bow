#include <stdlib.h>
#include <string.h>
#include <bow/archer_query.h>

archer_query_info *archer_query_tree = NULL;
char *archer_query_errorstr = NULL;

void
archer_query_error (char *s)
{
  if (archer_query_errorstr)
    free (archer_query_errorstr);
  archer_query_errorstr = strdup (s);
}

archer_query_string_list *
archer_query_string_list_new (char *string)
{
  archer_query_string_list *ret;

  ret = (archer_query_string_list *) malloc (sizeof (archer_query_string_list));
  ret->string = string;
  ret->next = NULL;

  return ret;
}
archer_query_string_list *
archer_query_string_list_prepend (char *string, archer_query_string_list * list)
{
  archer_query_string_list *ret;

  ret = archer_query_string_list_new (string);
  ret->next = list;

  return ret;
}

archer_query_term *
archer_query_term_new ()
{
  archer_query_term *ret;

  ret = (archer_query_term *) malloc (sizeof (archer_query_term));
  ret->word = ret->range_start = ret->range_end = NULL;
  ret->weight = 1;
  ret->df = 0;
  ret->idf = -1.0;
  ret->next = NULL;
  ret->head = NULL;
  ret->proximity = NULL;
  ret->labels = NULL;

  return ret;
}

archer_query_pterm *
archer_query_pterm_new ()
{
  archer_query_pterm *ret;

  ret = (archer_query_pterm *) malloc (sizeof (archer_query_pterm));
  ret->term = NULL;

  return ret;
}

archer_query_info *
archer_query_info_new ()
{
  archer_query_info *ret;

  ret = (archer_query_info *) malloc (sizeof (archer_query_info));
  ret->ranking = NULL;
  ret->inclusion = NULL;
  ret->exclusion = NULL;

  return ret;
}


void
archer_query_string_list_free(archer_query_string_list *list)
{
  if (list->string) free(list->string);
  if (list->next) archer_query_string_list_free(list->next);
  free(list);
}

void
archer_query_pterm_free(archer_query_pterm *pterm)
{
  void archer_query_term_free(archer_query_term *term);
  if (pterm->term) archer_query_term_free(pterm->term);
  free(pterm);
}

void
archer_query_term_free(archer_query_term *term)
{
  if (term->word) free(term->word);
  if (term->labels) archer_query_string_list_free(term->labels);
  if (term->range_start) free(term->range_start);
  if (term->range_end) free(term->range_end);
  if (term->next) archer_query_term_free(term->next);
  if (term->proximity) archer_query_pterm_free(term->proximity);
  free(term);
}

int 
archer_query_term_length(archer_query_term *term)
{
  int length = 0;

  while (term)
  {
    ++length;
    term = term->next;
  }

  return length;
}

void
archer_query_info_free(archer_query_info *query)
{
  if (query->exclusion) archer_query_term_free(query->exclusion);
  if (query->inclusion) archer_query_term_free(query->inclusion);
  if (query->ranking) archer_query_term_free(query->ranking);
  free(query);
}

