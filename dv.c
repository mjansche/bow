/* Functions for manipulating "document vectors". */

#include "libbow.h"
#include <netinet/in.h>		/* for machine-independent byte-order */
#include <assert.h>

unsigned int bow_dv_default_capacity = 2;

/* The number of "document vectors" current in existance. */
unsigned int bow_dv_count = 0;

/* Create a new, empty "document vector". */
bow_dv *
bow_dv_new (int capacity)
{
  bow_dv *ret;

  if (capacity == 0)
    capacity = bow_dv_default_capacity;
  ret = bow_malloc (sizeof (bow_dv) + (sizeof (bow_de) * capacity));
  ret->length = 0;
  ret->idf = 0.0f;
  ret->size = capacity;
  bow_dv_count++;
  return ret;
}

/* Return the index into (*DV)->entries[] at which the "document
   entry" for the document with index DI can be found.  If necessary,
   allocate more space, and/or shift other document entries around
   in order to make room for it. */
static int
_bow_dv_index_for_di (bow_dv **dv, int di, int error_on_creation)
{
  int dv_index;			/* The "document vector" index at
				   which we are looking for. */
  static inline void grow_if_necessary ()
    {
      if (error_on_creation)
	bow_error ("Shouldn't be creating new entry for a weight.");
      if ((*dv)->length == (*dv)->size)
	{
	  /* The DV must grow to accommodate another entry. */
	  (*dv)->size *= 3;
	  (*dv)->size /= 2;
	  (*dv) = bow_realloc ((*dv), (sizeof (bow_dv) 
				       + sizeof (bow_de) * (*dv)->size));
	}
    }
  static inline void initialize_dv_index (int dvi)
    {
      (*dv)->entry[dvi].di = di;
      (*dv)->entry[dvi].count = 0;
      (*dv)->entry[dvi].weight = 0.0f;
    }

  assert ((*dv)->length <= (*dv)->size);
  if ((*dv)->length == 0)
    {
      /* The DV is empty. */
      assert ((*dv)->size);
      ((*dv)->length)++;
      initialize_dv_index (0);
      return 0;
    }
  else if (di == (*dv)->entry[(dv_index = (*dv)->length - 1)].di)
    {
      /* An entry already exists for this DI; it's at the end. */
      return dv_index;
    }
  else if (di > (*dv)->entry[dv_index].di)
    {
      /* The entry does not already exist, and the entry belongs at
	 the end of the current DV. */
      dv_index = (*dv)->length;
      ((*dv)->length)++;
      grow_if_necessary ();
      initialize_dv_index (dv_index);
      return dv_index;
    }
  else
    {
      /* Search for the entry in the middle of the list. */
      for (dv_index = 0; 
	   (((*dv)->entry[dv_index].di < di)
	    && (dv_index < (*dv)->length));
	   dv_index++)
	{
	  if ((*dv)->entry[dv_index].di == di)
	    break;
	}
      if ((*dv)->entry[dv_index].di == di)
	{
	  /* The entry already exists in the middle of the DV. */
	  return dv_index;
	}
      else
	{
	  /* The entry should be in the middle of the DV, but it isn't 
	     there now; we'll have to push some aside to make room. */
	  int dvi;
	  assert (dv_index < (*dv)->length);
	  ((*dv)->length)++;
	  grow_if_necessary ();
	  /* Scoot some "document entries" up to make room */
	  for (dvi = dv_index; dvi < (*dv)->length; dvi++)
	    memcpy (&((*dv)->entry[dvi+1]), &((*dv)->entry[dvi]), 
		    sizeof (bow_de));
	  initialize_dv_index (dv_index);
	  return dv_index;
	}
    }
}

/* Sum the COUNT into the document vector DV at document index DI,
   creating a new entry in the document vector if necessary. */
void
bow_dv_add_di_count_weight (bow_dv **dv, int di, int count, float weight)
{
  int dv_index;			/* The "document vector" index at
				   which we are adding COUNT. */

  dv_index = _bow_dv_index_for_di (dv, di, 0);
  (*dv)->entry[dv_index].count += count;
  assert ((*dv)->entry[dv_index].count > 0);
  (*dv)->entry[dv_index].weight += weight;
}

/* Return the number of bytes required for writing the "document vector" DV. */
int
bow_dv_write_size (bow_dv *dv)
{
  if (dv == NULL)
    return sizeof (int);
  return (sizeof (int)		       /* length */
	  + sizeof (float)	       /* idf */
	  + (dv->length		       /* for each entry */
	     * (sizeof (short)	        /* di */
		+ sizeof (short)        /* count */
		+ sizeof (float))));    /* weight */
}

/* Write "document vector" DV to the stream FP. */
void
bow_dv_write (bow_dv *dv, FILE *fp)
{
  int i;

  if (dv == NULL)
    {
      bow_fwrite_int (0, fp);
      return;
    }

  bow_fwrite_int (dv->length, fp);
  bow_fwrite_float (dv->idf, fp);
  assert (dv->idf == dv->idf);	/* testing for NaN */

  for (i = 0; i < dv->length; i++)
    {
      bow_fwrite_short (dv->entry[i].di, fp);
      bow_fwrite_short (dv->entry[i].count, fp);
      bow_fwrite_float (dv->entry[i].weight, fp);
    }
}

/* Return a new "document vector" read from a pointer into a data file, FP. */
bow_dv *
bow_dv_new_from_data_fp (FILE *fp)
{
  int i;
  int len;
  bow_dv *ret;

  assert (feof (fp) != -1);	/* Help make sure FP hasn't been closed. */
  bow_fread_int (&len, fp);
  
  if (len == 0)
    return NULL;

  ret = bow_dv_new (len);
  bow_fread_float (&(ret->idf), fp);
  assert (ret->idf == ret->idf);	/* testing for NaN */
  ret->length = len;

  for (i = 0; i < len; i++)
    {
      bow_fread_short (&(ret->entry[i].di), fp);
      bow_fread_short (&(ret->entry[i].count), fp);
      bow_fread_float (&(ret->entry[i].weight), fp);
    }
  return ret;
}

void
bow_dv_free (bow_dv *dv)
{
  bow_dv_count--;
  bow_free (dv);
}
