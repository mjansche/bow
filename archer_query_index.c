#include <bow/libbow.h>
#include <bow/archer.h>
#include <bow/archer_query.h>
#include <bow/archer_query_array.h>
#include <bow/archer_query_index.h>

#define archer_query_remember_pointer_vars \
        long start; long remain; int read_last_di; int read_last_pi;

#define archer_query_remember_pointer(pv) \
  start = (pv)->read_seek_end; \
  remain = (pv)->read_segment_bytes_remaining; \
  read_last_di = (pv)->read_last_di; \
  read_last_pi = (pv)->read_last_pi; 

#define archer_query_recall_pointer(pv) \
  (pv)->read_seek_end = start; \
  (pv)->read_segment_bytes_remaining = remain; \
  (pv)->read_last_di = read_last_di; \
  (pv)->read_last_pi = read_last_pi; 


void
archer_query_index_reset (bow_index * index)
{
  bow_wi2pv_rewind (index->wi2pv);
  bow_wi2pv_rewind(index->li2pv);
}

static inline int
archer_query_term_id(archer_query_term *term)
{
  archer_label *lp;

  if (term->word)
    return bow_word2int_no_add(term->word);
  else
  {  
    /* Assume just a single label for now */
    lp = bow_sarray_entry_at_keystr(archer_labels, term->labels->string);
    if (!lp) return -1;
    return lp->li;
  }
}


static inline int
archer_query_bare_label(archer_query_term *term)
{
  return !(term->word);
}


static inline int
archer_query_prolog(archer_query_term *term, bow_index *index,
		    int *islabel, bow_pv **pv, FILE **fp)
{
  int id = archer_query_term_id(term);

  if (id == -1) return -1;

  *islabel = archer_query_bare_label(term);

  if (*islabel)
  {
    *pv = &index->li2pv->entry[id];
    *fp = index->li2pv->fp;
  }
  else
  {
    *pv = &index->wi2pv->entry[id];
    *fp = index->wi2pv->fp;
  }

  return id;
}


/* scan forward from the current record for the next (di, pi) that
   matches `term'. di = -1 on error (i.e. end of pv); otherwise, pi is
   guaranteed to be different from the previous call to this and di is
   not. File pointer points to beginning of record */
void
archer_query_index_next_di_pi (bow_index * index, archer_query_term * term,
			       int *di, int *pi)
{
  int li[BOW_MAX_WORD_LABELS], ln = 0, match;
  int islabel;
  bow_pv *pv;
  FILE *fp;
  int id = archer_query_prolog(term, index, &islabel, &pv, &fp);
  archer_query_string_list *label;
  int i;
  archer_query_remember_pointer_vars;

  if (id == -1)
  {
    *di = -1;
    return;
  }

  archer_query_remember_pointer(pv);

  if (islabel)
    bow_pv_next_di_li_pi(pv, di, NULL, &ln, pi, fp);

  else           /* Regular term */
  {
    match = 0;
    *di = 0;

    while (!match && *di != -1)
    {
      archer_query_remember_pointer(pv);

      ln = BOW_MAX_WORD_LABELS;
      bow_pv_next_di_li_pi (pv, di, li, &ln, pi, fp);

      if (*di != -1)
      {
	label = term->labels;
	match = 1;

	while (label && match)
	{
	  match = 0;
	  for (i = 0; (i < ln) && !match; i++)
	    if (li[i] == bow_sarray_index_at_keystr (archer_labels,
						     label->string))
	    {
	      match = 1;
	      break;
	    }
	  label = label->next;
	}
      }
    }
  }
  
  archer_query_recall_pointer(pv);
}

/* scan forward for the next di that matches `term'. di is -1 on error (i.e.
   end of pv); otherwise is guaranteed to be different from the last call.
   leaves file pointer at the beginning of the record returned */
void
archer_query_index_next_di (bow_index *index, archer_query_term *term, int *di)
{
  int ln = 0;
  int current_di;
  int islabel, pi;
  bow_pv *pv;
  FILE *fp;
  int id = archer_query_prolog(term, index, &islabel, &pv, &fp);

  if (id == -1)
  {
    *di = -1;
    return;
  }

  ln = 0;
  bow_pv_next_di_li_pi(pv, di, NULL, &ln, &pi, fp);
  current_di = *di;

  while (*di != -1 && *di == current_di)
  {
    archer_query_index_next_di_pi(index, term, di, &pi);
    if (*di == current_di)
    {
      ln = 0;
      bow_pv_next_di_li_pi(pv, di, NULL, &ln, &pi, fp);
    }
  }
}


/* returns the current di for term, or -1 on error (no more dis). leaves 
   file pointer at the beginning of the record returned.  
*/
void
archer_query_index_current_di (bow_index * index, archer_query_term * term,
			       int *di, int *piarg)
{
  int ln = 0;
  int islabel;
  bow_pv *pv;
  FILE *fp;
  int id = archer_query_prolog(term, index, &islabel, &pv, &fp);
  archer_query_remember_pointer_vars;

  if (id == -1)
  {
    *di = -1;
    return;
  }

  /* if this is the first access for this wi, scan for the first valid di */
  if (pv->read_last_di == -1)
    archer_query_index_next_di_pi (index, term, di, piarg);

  archer_query_remember_pointer(pv);
  ln = 0;
  bow_pv_next_di_li_pi(pv, di, NULL, &ln, piarg, fp);
  archer_query_recall_pointer(pv);
}


/* returns a bow_array of all the valid pis for the current di. leaves the
   file pointer where it was. */
bow_array *
archer_query_index_current_pis (bow_index * index, archer_query_term * term)
{
  int ln = 0;
  int islabel;
  int current_di, di, pi, npi, j;
  bow_pv *pv;
  FILE *fp;
  int id = archer_query_prolog(term, index, &islabel, &pv, &fp);
  bow_array *ret = bow_array_new (0, sizeof (int), NULL);
  archer_query_remember_pointer_vars;

  if (id == -1)
    return ret;

  /* if this is the first access for this wi, scan for the first valid di */
  if (pv->read_last_di == -1)
    archer_query_index_next_di_pi (index, term, &di, &pi);

  archer_query_index_current_di(index, term, &current_di, &pi);
  archer_query_remember_pointer(pv);

  di = current_di;

  if (current_di != -1)
    do
    {
      if (di == current_di)
      {
	if (islabel)
	{
	  ln = 0;
	  bow_pv_next_di_li_pi(pv, &di, NULL, &ln, &npi, fp);
	  archer_query_index_next_di_pi(index, term, &di, &npi);

	  /* Should have the terminating index for this field */
	  assert(di == current_di && npi > pi);
	  for (j = pi; j < npi; ++j)
	  {
	    /* printf("%d: %d\n", di, j); */
	    bow_array_append(ret, &j);
	  }
	}
	else
	{
	  /* printf("%d: %d\n", di, pi); */
	  bow_array_append(ret, &pi);
	}
      }
	
      ln = 0;
      bow_pv_next_di_li_pi(pv, &di, NULL, &ln, &pi, fp);
      archer_query_index_next_di_pi(index, term, &di, &pi);

    } while (di == current_di);

  archer_query_recall_pointer(pv);

  return ret;
}



