/* Managing the connection between document-vectors (wi2dvf's) and cdocs
   Copyright (C) 1997 Andrew McCallum

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>

   This file is part of the Bag-Of-Words Library, `libbow'.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation, version 2.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA */

#include <bow/libbow.h>

static int _bow_barrel_version = -1;
#define BOW_DEFAULT_BARREL_VERSION 3


/* Old, deprecated identifiers for methods. */
/* Identifiers for methods */
typedef enum _bow_method_id {
  bow_method_id_tfidf_words,	/* TFIDF/Rocchio */
  bow_method_id_tfidf_log_words,
  bow_method_id_tfidf_log_occur,
  bow_method_id_tfidf_prtfidf,
  bow_method_id_naivebayes,	/* Naive Bayes */
  bow_method_id_prind,		/* Fuhr's Probabilistic Indexing */
  bow_method_id_max
} bow_method_id;
bow_method* _old_bow_methods[bow_method_id_max] = 
{
  [bow_method_id_tfidf_words] = &bow_method_tfidf_words,
  [bow_method_id_tfidf_log_words] = &bow_method_tfidf_log_words,
  [bow_method_id_tfidf_log_occur] = &bow_method_tfidf_log_occur,
  [bow_method_id_naivebayes] = &bow_method_naivebayes,
  [bow_method_id_prind] = &bow_method_prind
};


/* Create a new, empty `bow_barrel', with cdoc's of size ENTRY_SIZE
   and cdoc free function FREE_FUNC.*/
bow_barrel *
bow_barrel_new (int word_capacity, 
		int class_capacity, int entry_size, void (*free_func)())
{
  bow_barrel *ret;

  ret = bow_malloc (sizeof (bow_barrel));
  ret->cdocs = bow_array_new (class_capacity, entry_size, free_func);
  ret->wi2dvf = bow_wi2dvf_new (word_capacity);
  ret->method = (bow_argp_method 
		 ? : bow_method_at_name (bow_default_method_name));
  return ret;
}

static void
_bow_barrel_cdoc_free (bow_cdoc *cdoc)
{
  if (cdoc->filename)
    free ((void*)(cdoc->filename));
}

/* Add statistics about the document described by CDOC and WV to the
   BARREL. */
int
bow_barrel_add_document (bow_barrel *barrel,
			 bow_cdoc *cdoc,
			 bow_wv *wv)
{
  int di;

  /* Add the CDOC.  (This makes a new copy of CDOC in the array.) */
  di = bow_array_append (barrel->cdocs, cdoc);
  /* Add the words in WV. */
  bow_wi2dvf_add_di_wv (&(barrel->wi2dvf), di, wv);

  return di;
}

/* Add statistics to the barrel BARREL by indexing all the documents
   found when recursively decending directory DIRNAME.  Return the number
   of additional documents indexed. */
int
bow_barrel_add_from_text_dir (bow_barrel *barrel, 
			      const char *dirname, 
			      const char *except_name,
			      int class)
{
  int text_file_count, binary_file_count;

  int barrel_index_file (const char *filename, void *context)
    {
      FILE *fp;
      bow_cdoc cdoc;
      int di;			/* a document index */

      /* If the filename matches the exception name, return immediately. */
      if (except_name && !strcmp (filename, except_name))
	return 0;

      if (!(fp = fopen (filename, "r")))
	bow_error ("Couldn't open file `%s' for reading.", filename);
      if (bow_fp_is_text (fp))
	{
	  /* The file contains text; snarf the words and put them in
	     the WI2DVF map. */
	  cdoc.type = model;
	  cdoc.class = class;
	  cdoc.prior = 1.0f;
	  assert (cdoc.class >= 0);
	  cdoc.filename = strdup (filename);
	  assert (cdoc.filename);
	  /* Add the CDOC to CDOCS, and determine the "index" of this
             document. */
	  di = bow_array_append (barrel->cdocs, &cdoc);
	  /* Add all the words in this document. */
	  bow_wi2dvf_add_di_text_fp (&(barrel->wi2dvf), di, fp);
	  text_file_count++;
	}
      else
	{
	  bow_verbosify (bow_progress,
			 "\nFile `%s' skipped because not text\n",
			 filename);
	  binary_file_count++;
	}
      fclose (fp);
      bow_verbosify (bow_progress,
		     "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		     "%6d : %6d", 
		     text_file_count, bow_num_words ());
      return 1;
    }

  bow_verbosify (bow_progress,
		 "Gathering stats... files : unique-words :: "
		 "                 ");
  text_file_count = binary_file_count = 0;
  bow_map_filenames_from_dir (barrel_index_file, 0, dirname, "");
  bow_verbosify (bow_progress, "\n");
  if (binary_file_count > text_file_count)
    bow_verbosify (bow_quiet,
		   "Found mostly binary files, which were ignored.\n");
  return text_file_count;
}

/* Call this on a vector-per-document barrel to set the CDOC->PRIOR's
   so that the CDOC->PRIOR's for all documents of the same class sum
   to 1. */
void
bow_barrel_set_cdoc_priors_to_class_uniform (bow_barrel *barrel)
{
  int *ci2dc;			/* class index 2 document count */
  int ci2dc_size = 100;
  int ci;
  int di;
  bow_cdoc *cdoc;
  int total_model_docs = 0;
  int num_non_zero_ci2dc_entries = 0;
  
  ci2dc = bow_malloc (sizeof (int) * ci2dc_size);
  for (ci = 0; ci < ci2dc_size; ci++)
    ci2dc[ci] = 0;
  for (di = 0; di < barrel->cdocs->length; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      if (cdoc->class >= ci2dc_size)
	{
	  /* CI2DC must grow to accommodate larger "class index" */
	  int old_size = ci2dc_size;
	  ci2dc_size *= 2;
	  ci2dc = bow_realloc (ci2dc, sizeof (int) * ci2dc_size);
	  for ( ; old_size < ci2dc_size; old_size++)
	    ci2dc[old_size] = 0;
	}
      if (cdoc->type == model)
	{
	  if (ci2dc[cdoc->class] == 0)
	    num_non_zero_ci2dc_entries++;
	  ci2dc[cdoc->class]++;
	  total_model_docs++;
	}
    }

  for (di = 0; di < barrel->cdocs->length; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      if (cdoc->type == model)
	{
	  cdoc->prior = (1.0 /
			 (num_non_zero_ci2dc_entries * ci2dc[cdoc->class]));
	  assert (cdoc->prior >= 0);
	}
    }
#if 0
  fprintf (stderr, "Infogain post-prior-setting\n");
  bow_infogain_per_wi_print (stderr, barrel, num_non_zero_ci2dc_entries, 5);
#endif

  /* Do some sanity checks. */
  {
    float prior_total = 0;
    for (di = 0; di < barrel->cdocs->length; di++)
      {
	cdoc = bow_array_entry_at_index (barrel->cdocs, di);
	if (cdoc->type == model)
	  prior_total += cdoc->prior;
      }
    assert (prior_total < 1.1 && prior_total > 0.9);
  }

  free (ci2dc);
}

/* Modify the BARREL by removing those entries for words that are not
   in the int/str mapping MAP. */
void
bow_barrel_prune_words_not_in_map (bow_barrel *barrel,
				   bow_int4str *map)
{
  int wi;
  int max_wi = MIN (barrel->wi2dvf->size, bow_num_words());
  
  assert (max_wi);
  /* For each word in MAP. */
  for (wi = 0; wi < max_wi; wi++)
    {
      if (bow_str2int_no_add (map, bow_int2word (wi)) == -1)
	{
	  /* Word WI is not in MAP.  Remove it from the BARREL. */
	  bow_wi2dvf_remove_wi (barrel->wi2dvf, wi);
	}
    }
}

/* Modify the BARREL by removing those entries for words that are not
   among the NUM_WORDS_TO_KEEP top words, by information gain.  This
   function is similar to BOW_WORDS_KEEP_TOP_BY_INFOGAIN(), but this
   one doesn't change the word-int mapping. */
void
bow_barrel_keep_top_words_by_infogain (int num_words_to_keep, 
				       bow_barrel *barrel, int num_classes)
{
  float *wi2ig;
  int wi2ig_size;
  int wi, i;
  struct wiig_list_entry {
    float ig;
    int wi;
  } *wiig_list;
  /* For sorting the above entries. */
  int compare_wiig_list_entry (const void *e1, const void *e2)
    {
      if (((struct wiig_list_entry*)e1)->ig >
	  ((struct wiig_list_entry*)e2)->ig)
	return -1;
      else if (((struct wiig_list_entry*)e1)->ig ==
	  ((struct wiig_list_entry*)e2)->ig)
	return 0;
      else return 1;
    }

  if (num_words_to_keep == 0)
    return;

  /* Unhide "document vectors" for all WI's */
  bow_wi2dvf_unhide_all_wi (barrel->wi2dvf);

  /* Get the information gain of all the words. */
  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  /* Make a list of the info gain numbers paired with their WI's,
     in prepartion for sorting. */
  wiig_list = alloca (sizeof (struct wiig_list_entry) * wi2ig_size);
  for (wi = 0; wi < wi2ig_size; wi++)
    {
      wiig_list[wi].wi = wi;
      wiig_list[wi].ig = wi2ig[wi];
    }
  /* Sort the list */
  qsort (wiig_list, wi2ig_size, sizeof (struct wiig_list_entry), 
	 compare_wiig_list_entry);

#if 1
  for (i = 0; i < 5; i++)
    fprintf (stderr,
	     "%20.10f %s\n", wiig_list[i].ig, bow_int2word (wiig_list[i].wi));
#endif

  bow_verbosify (bow_progress, 
		 "Removing words by information gain:          ");

  /* Hide words from the BARREL. */
  for (i = num_words_to_keep; i < wi2ig_size; i++)
    {
      /* Hide the WI from BARREL. */
      bow_wi2dvf_hide_wi (barrel->wi2dvf, wiig_list[i].wi);
      if (i % 100 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b\b\b\b%9d", wi2ig_size - i); 
    }    

  bow_verbosify (bow_progress, "\n");
}

int
_bow_barrel_cdoc_write (bow_cdoc *cdoc, FILE *fp)
{
  int ret;

  ret = bow_fwrite_int (cdoc->type, fp);
  ret += bow_fwrite_float (cdoc->normalizer, fp);
  ret += bow_fwrite_float (cdoc->prior, fp);
  ret += bow_fwrite_int (cdoc->word_count, fp);
  ret += bow_fwrite_string (cdoc->filename, fp);
  if (bow_file_format_version < 5)
    ret += bow_fwrite_short (cdoc->class, fp);
  else
    ret += bow_fwrite_int (cdoc->class, fp);
  return ret;
}

int
_bow_barrel_cdoc_read (bow_cdoc *cdoc, FILE *fp)
{
  int ret;
  int type;

  ret = bow_fread_int (&type, fp);
  cdoc->type = type;
  ret += bow_fread_float (&(cdoc->normalizer), fp);
  ret += bow_fread_float (&(cdoc->prior), fp);
  ret += bow_fread_int (&(cdoc->word_count), fp);
  ret += bow_fread_string ((char**)&(cdoc->filename), fp);
  if (bow_file_format_version < 5)
    {
      short s;
      ret += bow_fread_short (&s, fp);
      cdoc->class = s;
    }
  else
    ret += bow_fread_int (&(cdoc->class), fp);
  return ret;
}

/* Create and return a `barrel' by reading data from the file-pointer FP. */
bow_barrel *
bow_barrel_new_from_data_fp (FILE *fp)
{
  bow_barrel *ret;
  int version_tag;
  int method_id;

  version_tag = fgetc (fp);
  /* xxx assert (version_tag >= 0); */
  if (version_tag <= 0)
    return NULL;
  if (_bow_barrel_version != -1 && _bow_barrel_version != version_tag)
    bow_error ("Trying to read bow_barrel's with different version numbers");
  _bow_barrel_version = version_tag;
  ret = bow_malloc (sizeof (bow_barrel));
  if (_bow_barrel_version < 3)
    {
      bow_fread_int (&method_id, fp);
      ret->method = _old_bow_methods[method_id];
    }
  else
    {
      char *method_string;
      bow_fread_string (&method_string, fp);
      ret->method = bow_method_at_name (method_string);
      free (method_string);
    }
  ret->cdocs = 
    bow_array_new_from_data_fp ((int(*)(void*,FILE*))_bow_barrel_cdoc_read,
				 _bow_barrel_cdoc_free, fp);
  assert (ret->cdocs->length);
  ret->wi2dvf = bow_wi2dvf_new_from_data_fp (fp);
  assert (ret->wi2dvf->num_words);
  return ret;
}

/* Decide whether to keep this or not.  Currently it it used by
   rainbow-h.c. */
bow_barrel *
bow_barrel_new_from_data_file (const char *filename)
{
  FILE *fp;
  bow_barrel *ret_barrel;
  int wi;
  bow_dv *dv;
  int dv_count = 0;

  fp = bow_fopen (filename, "r");
  ret_barrel = bow_barrel_new_from_data_fp (fp);

  if (ret_barrel)
    {
      /* Read in all the dvf's so that we can close the FP. */
      for (wi = 0; wi < ret_barrel->wi2dvf->size; wi++)
	{
	  dv = bow_wi2dvf_dv (ret_barrel->wi2dvf, wi);
	  if (dv)
	    dv_count++;
	}
      ret_barrel->wi2dvf->fp = NULL;
      assert (dv_count);
    }
  fclose (fp);
  return ret_barrel;
}

/* Write BARREL to the file-pointer FP in a machine independent format. */
void
bow_barrel_write (bow_barrel *barrel, FILE *fp)
{
  if (!barrel)
    {
      fputc (0, fp);		/* 0 version_tag means NULL barrel */
      return;
    }
  fputc (BOW_DEFAULT_BARREL_VERSION, fp);
  _bow_barrel_version = BOW_DEFAULT_BARREL_VERSION;
  bow_fwrite_string (barrel->method->name, fp);
  bow_array_write (barrel->cdocs,
		   (int(*)(void*,FILE*))_bow_barrel_cdoc_write, fp);
  bow_wi2dvf_write (barrel->wi2dvf, fp);
}

/* Print barrel to FP in human-readable and awk-accessible format. */
void
bow_barrel_printf (bow_barrel *barrel, FILE *fp, const char *format)
{
  
  bow_dv_heap *heap;		/* a heap of "document vectors" */
  int current_di;
  bow_cdoc *cdoc;

  bow_verbosify (bow_progress, "Printing barrel:          ");
  heap = bow_make_dv_heap_from_wi2dvf (barrel->wi2dvf);

  /* Keep going until the heap is empty */
  while (heap->length > 0)
    {
      /* Set the current document we're working on */
      current_di = heap->entry[0].current_di;
      assert (heap->entry[0].dv->idf == heap->entry[0].dv->idf);  /* NaN */

      if (current_di % 10 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", current_di);

      /* Here we should check if this di is part of some training set and
	 move on if it isn't. */
    
      /* Get the document */
      cdoc = bow_cdocs_di2doc (barrel->cdocs, current_di);

#if 0
      /* If it's not a model document, then move on to next one */
      if (cdoc->type != model)
	{
	  do 
	    {
	      bow_dv_heap_update (heap);
	    }
	  while ((current_di == heap->entry[0].current_di)
		 && (heap->length > 0));
	  
	  /* Try again */
	  continue;
	}
#endif

      fprintf (fp, "%s", cdoc->filename);
    
      /* Loop over all words in this document, printing out the
         FORMAT-requested statistics. */
      do 
	{
#if 0
	  int wi;
	  for (wi = 0; heap->entry[0].wi > wi; wi++)
	    fprintf (fp, " 0");
#endif
	  fprintf (fp, "  %s %d %d", 
		   bow_int2word (heap->entry[0].wi),
		   heap->entry[0].wi,
		   heap->entry[0].dv->entry[heap->entry[0].index].count);

	  /* Update the heap, we are done with this di, move it to its
	     new position */
	  bow_dv_heap_update (heap);
#if 0
	  for (; heap->entry[0].wi > wi; wi++)
	    fprintf (fp, " 0");
#endif
	} 
      while ((current_di == heap->entry[0].current_di)
	     && (heap->length > 0));
      fprintf (fp, "\n");
    }

  bow_free (heap);
  bow_verbosify (bow_progress, "\n"); 
}

/* Print barrel to FP in human-readable and awk-accessible format.
   Step through each CDOC in BARREL->CDOCS instead of using a heap.  
   This way we even print out the documents that have zero words. 
   This function runs much more slowly than the one above. */
void
bow_new_slow_barrel_printf (bow_barrel *barrel, FILE *fp, const char *format)
{
  int di;
  bow_cdoc *cdoc;
  bow_de *de;
  int wi, max_wi;

  bow_verbosify (bow_progress, "Printing barrel:          ");
  max_wi = barrel->wi2dvf->size;
  for (di = 0; di < barrel->cdocs->length; di++)
    {
      if (barrel->cdocs->length - di % 10 == 0)
	bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", 
		       barrel->cdocs->length - di);

      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      fprintf (fp, "%s", cdoc->filename);
      for (wi = 0; wi < max_wi; wi++)
	{
	  de = bow_wi2dvf_entry_at_wi_di (barrel->wi2dvf, wi, di);
	  if (de)
	    fprintf (fp, "  %s %d %d", 
		     bow_int2word (wi),
		     wi,
		     de->count);
	}
      fprintf (fp, "\n");
    }
  bow_verbosify (bow_progress, "\n"); 
}

/* Print on stdout the number of times WORD occurs in the various
   docs/classes of BARREL. */
void
bow_barrel_print_word_count (bow_barrel *barrel, const char *word)
{
  int wi;
  bow_dv *dv;
  int dvi;
  bow_cdoc *cdoc;
  
  wi = bow_word2int (word);
  if (wi == -1)
    {
      fprintf (stderr, "No such word `%s' in dictionary\n", word);
      exit (-1);
    }
  dv = bow_wi2dvf_dv (barrel->wi2dvf, wi);
  if (!dv)
    {
      fprintf (stderr, "No document vector for word `%s'\n", word);
      return;
    }
  for (dvi = 0; dvi < dv->length; dvi++) 
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, 
				       dv->entry[dvi].di);
      printf ("%9d / %9d  (%9.5f) %s\n", 
	      dv->entry[dvi].count, 
	      cdoc->word_count,
	      ((float)dv->entry[dvi].count / cdoc->word_count),
	      cdoc->filename);
    }
}


/* Free the memory held by BARREL. */
void
bow_barrel_free (bow_barrel *barrel)
{
  if (barrel->wi2dvf)
    bow_wi2dvf_free (barrel->wi2dvf);
  if (barrel->cdocs)
    bow_array_free (barrel->cdocs);
  bow_free (barrel);
}
