#include "libbow.h"

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
  ret->method = bow_method_tfidf_log_occur;
  return ret;
}

static void
_bow_barrel_cdoc_free (bow_cdoc *cdoc)
{
  if (cdoc->filename)
    free ((void*)(cdoc->filename));
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
	binary_file_count++;
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
  float min_ig;
  int wi, min_ig_wi;
  int num_words_to_remove;

  wi2ig = bow_infogain_per_wi_new (barrel, num_classes, &wi2ig_size);

  /* Remove NUM_WORDS_TO_REMOVE words from BARREL. */
  num_words_to_remove = wi2ig_size - num_words_to_keep;
  while (num_words_to_remove--)
    {
      min_ig = 2.0f;		/* 1.0 is the highest possible infogain */
      /* Find the word with the smallest info gain. */
      for (wi = 0; wi < wi2ig_size; wi++)
	{
	  assert ((wi2ig[wi] >= 0 && wi2ig[wi] <= 1.0f)
		  || wi2ig[wi] == 2.0f);
	  if (wi2ig[wi] < min_ig)
	    {
	      min_ig = wi2ig[wi];
	      min_ig_wi = wi;
	    }
	}
      assert (min_ig >= 0);
      /* Remove WI from BARREL. */
      bow_wi2dvf_remove_wi (barrel->wi2dvf, min_ig_wi);
      /* Push WI's info gain to the sky so we can find the next lowest. */
      wi2ig[min_ig_wi] = 2.0f;
    }
}


int
_bow_barrel_cdoc_write (bow_cdoc *cdoc, FILE *fp)
{
  int ret;

  ret = bow_fwrite_int (cdoc->type, fp);
  ret += bow_fwrite_float (cdoc->normalizer, fp);
  ret += bow_fwrite_float (cdoc->prior, fp);
  ret += bow_fwrite_string (cdoc->filename, fp);
  ret += bow_fwrite_short (cdoc->class, fp);
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
  ret += bow_fread_string ((char**)&(cdoc->filename), fp);
  ret += bow_fread_short (&(cdoc->class), fp);
  return ret;
}

/* Create and return a `barrel' by reading data from the file-pointer FP. */
bow_barrel *
bow_barrel_new_from_data_fp (FILE *fp)
{
  bow_barrel *ret;
  int method;

  ret = bow_malloc (sizeof (bow_barrel));
  bow_fread_int (&method, fp);
  ret->method = method;
  ret->cdocs = 
    bow_array_new_from_data_fp ((int(*)(void*,FILE*))_bow_barrel_cdoc_read,
				 _bow_barrel_cdoc_free, fp);
  ret->wi2dvf = bow_wi2dvf_new_from_data_fp (fp);
  return ret;
}

/* Write BARREL to the file-pointer FP in a machine independent format. */
void
bow_barrel_write (bow_barrel *barrel, FILE *fp)
{
  bow_fwrite_int (barrel->method, fp);
  bow_array_write (barrel->cdocs,
		   (int(*)(void*,FILE*))_bow_barrel_cdoc_write, fp);
  bow_wi2dvf_write (barrel->wi2dvf, fp);
}

/* Free the memory held by BARREL. */
void
bow_barrel_free (bow_barrel *barrel)
{
  bow_wi2dvf_free (barrel->wi2dvf);
  bow_array_free (barrel->cdocs);
  bow_free (barrel);
}
