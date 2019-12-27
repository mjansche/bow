/* archer - a document retreival front-end to libbow. */

/* Copyright (C) 1998, 1999 Andrew McCallum

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

#include <pwd.h>
#include <crypt.h>
#include <unistd.h>
#include <bow/libbow.h>
#include <argp.h>
#include <bow/archer.h>
#include <errno.h>		/* needed on DEC Alpha's */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef ARCHER_USE_MCHECK
#include <signum.h>
#include <mcheck.h>
#endif
 

/* Global variables */

/* The directory to index */
char *archer_source_directory = NULL;

/* The extraction directory: contains a mirror file, with markup, for each
   file in the directory to index */
char *archer_extraction_directory = NULL;

/* The extraction file: Holds the name of the marked-up copy of file
   to be indexed */
char *archer_extraction_filename = NULL;

/* The document/word/position matrix */
bow_wi2pv *archer_wi2pv = NULL;

/* The document/label/position matrix */
bow_wi2pv *archer_li2pv = NULL;

/* The list of documents. */
bow_sarray *archer_docs = NULL;

/* The labels */
bow_sarray *archer_labels = NULL;

/* The file-pointer for the vocabulary */
FILE *archer_vocabulary_fp = NULL;

/* File pointers for the list of documents */
FILE *archer_docs_i4k_fp = NULL;
FILE *archer_docs_array_fp = NULL;

/* File pointers for the labels */
FILE *archer_labels_i4k_fp = NULL;
FILE *archer_labels_array_fp = NULL;

/* Are we doing incremental writes? 
   This will be true for the query server,
   false if we're doing one big indexing run. */
/* Can this be inferred from archer_arg_stat? */
int archer_index_inc = 1;

/* Doing one big run, but only indexing files not in index */
int archer_incremental_index = 0;

/* Password protection */
char archer_password[14] = {0};

/* The variables that are set by command-line options. */
struct archer_arg_state_s archer_arg_state;

/* Host restrictions: default = 255.255.255.255 (allow all connections) */
struct in_addr archer_ip_spec = { 0xffffffff };


/* Functions for creating, reading, writing a archer_doc */

int
archer_label_write (archer_label *label, FILE *fp)
{
  int ret;

  ret = bow_fwrite_int(label->word_count, fp);
  ret += bow_fwrite_int(label->li, fp);

  return ret;
}

int
archer_label_read (archer_label *label, FILE *fp)
{
  int ret;

  ret = bow_fread_int (&(label->word_count), fp);
  ret += bow_fread_int (&(label->li), fp);
 
  return ret;
}

void
archer_label_free (archer_label *label)
{
}

int
archer_doc_write (archer_doc *doc, FILE *fp)
{
  int ret;

  ret = bow_fwrite_int (doc->tag, fp);
  ret += bow_fwrite_int (doc->word_count, fp);
  ret += bow_fwrite_int (doc->di, fp);
  return ret;
}

int
archer_doc_read (archer_doc *doc, FILE *fp)
{
  int ret;
  int tag;

  ret = bow_fread_int (&tag, fp);
  doc->tag = tag;
  ret += bow_fread_int (&(doc->word_count), fp);
  ret += bow_fread_int (&(doc->di), fp);
  return ret;
}

void
archer_doc_free (archer_doc *doc)
{
}



/* Writing and reading the word/document stats to disk. */

/* Write the stats in the directory DATA_DIRNAME. */
void
archer_archive ()
{
  if (archer_index_inc) 
  {
      fflush (archer_wi2pv->fp);
      fflush(archer_li2pv->fp);
  }
  else 
  {
      bow_wi2pv_write(archer_wi2pv);
      bow_wi2pv_write(archer_li2pv);
  }
  fflush (archer_docs_i4k_fp);
  fflush (archer_docs_array_fp);  
  fflush (archer_labels_i4k_fp);
  fflush (archer_labels_array_fp);
  fflush (archer_vocabulary_fp);
}

/* Read the stats from the directory DATA_DIRNAME. */
void
archer_unarchive ()
{
  char filename[BOW_MAX_WORD_LENGTH];

  bow_verbosify (bow_progress, "Loading data files...");

  sprintf (filename, "%s/vocabulary", bow_data_dirname);
  archer_vocabulary_fp = bow_fopen (filename, "rb+");
  bow_words_read_from_fp_inc (archer_vocabulary_fp);

  sprintf (filename, "%s/wi2pv", bow_data_dirname);
  archer_wi2pv = bow_wi2pv_new_from_filename (filename);

  sprintf (filename, "%s/li2pv", bow_data_dirname);
  archer_li2pv = bow_wi2pv_new_from_filename (filename);

  sprintf (filename, "%s/docs.i4k", bow_data_dirname);
  archer_docs_i4k_fp = bow_fopen (filename, "rb+");
  sprintf (filename, "%s/docs.array", bow_data_dirname);
  archer_docs_array_fp = bow_fopen (filename, "rb+");
  archer_docs = 
    bow_sarray_new_from_data_fps_inc ((int(*)(void*,FILE*))archer_doc_read, 
				      archer_doc_free, archer_docs_i4k_fp, archer_docs_array_fp);
 
  sprintf (filename, "%s/labels.i4k", bow_data_dirname);
  archer_labels_i4k_fp = bow_fopen (filename, "rb+");
  sprintf (filename, "%s/labels.array", bow_data_dirname);
  archer_labels_array_fp = bow_fopen (filename, "rb+");
  archer_labels = 
    bow_sarray_new_from_data_fps_inc ((int(*)(void*,FILE*))archer_label_read, 
				      archer_label_free, archer_labels_i4k_fp, archer_labels_array_fp);

  bow_verbosify (bow_progress, "\n");
}

int
archer_index_filename_old_lex (const char *filename, void *unused, int di)
{
  FILE *fp;
  bow_lex *lex;
  char word[BOW_MAX_WORD_LENGTH];
  int wi;
  int pi = 0;

  fp = fopen (filename, "r");
  if (fp == NULL)
    {
      perror ("bow_fopen");
      return 0;
    }
  /* NOTE: This will read just the first document from the file. */
  lex = bow_default_lexer->open_text_fp (bow_default_lexer, fp, filename);
  if (lex == NULL)
    {
      fclose (fp);
      return 0;
    }
  while (bow_default_lexer->get_word (bow_default_lexer,
				      lex, word, BOW_MAX_WORD_LENGTH))
    {
      wi = bow_word2int_inc (word, archer_vocabulary_fp);
      
      if (wi < 0)
	continue;

      bow_wi2pv_add_wi_di_pi (archer_wi2pv, wi, di, pi); 

      if (archer_index_inc) 
	bow_wi2pv_write_entry (archer_wi2pv, wi);

      pi++;
    }
  bow_default_lexer->close (bow_default_lexer, lex);
  fclose (fp);

  if (archer_index_inc)
    bow_wi2pv_write_header(archer_wi2pv); 

  return pi;
}


static FILE *
archer_get_fp_from_filename(const char *filename)
{
  FILE *fp;

  if (archer_extraction_filename)
  {
    fp = fopen(archer_extraction_filename, "r");
  }
  if (archer_extraction_directory)
  {
    int len;
    char buf[1000];

    len = strlen(archer_source_directory);
    if (archer_source_directory[len - 1] != '/')
      ++len;
    sprintf(buf, "%s/%s", archer_extraction_directory, &filename[len]);
    fp = fopen(buf, "r");
  }
  else
    fp = fopen (filename, "r");

  return fp;
}


static int waitli[1000], waitln;


static inline void flush_labels(int di, int pi)
{
  int waiti;

  for (waiti = 0; waiti < waitln; ++waiti)
  {
    bow_wi2pv_add_wi_di_li_pi(archer_li2pv,waitli[waiti], di, NULL, 0, pi);
    if (archer_index_inc) bow_wi2pv_write_entry(archer_li2pv, waitli[waiti]);
  }
  waitln = 0;
}


static void
archer_index_term(char *word, int di, int pi)
{
  char *cp, label[BOW_MAX_WORD_LENGTH];
  int ln, li[BOW_MAX_WORD_LABELS], wi;
  archer_label *label_ptr;

  flush_labels(di, pi);
      
  for (cp = bow_first_label(label, BOW_MAX_WORD_LENGTH), ln = 0; 
       cp;
       cp = bow_next_label(label, BOW_MAX_WORD_LENGTH))
  {
    int i;

    for (i = 0; label[i]; ++i) label[i] = tolower(label[i]);
    label_ptr = bow_sarray_entry_at_keystr(archer_labels, label);
    if (!label_ptr)
      bow_error("Missed a label (%s)", label);
    label_ptr->word_count++;

    /* Update information on disk */
    bow_array_write_entry_inc (
		       archer_labels->array, 
		       bow_sarray_index_at_keystr(archer_labels, label), 
		       (int(*)(void*,FILE*))archer_label_write, 
		       archer_labels_array_fp);

    li[ln++] = label_ptr->li;
  }

  wi = bow_word2int_inc (word, archer_vocabulary_fp);
  if (wi < 0)
    return;

  bow_wi2pv_add_wi_di_li_pi (archer_wi2pv, wi, di, li, ln, pi);
      
  if (archer_index_inc)
    bow_wi2pv_write_entry (archer_wi2pv, wi);
}


static void
archer_index_label(char *word, int di, int pi)
{
  int wi, waiti;
  archer_label *label_ptr, labelobj;

  label_ptr = bow_sarray_entry_at_keystr(archer_labels, word);
  if (!label_ptr)
  {
    labelobj.word_count = 0;
    labelobj.li = archer_labels->array->length;
    bow_sarray_add_entry_with_keystr_inc (
				 archer_labels, &labelobj, word,
				 (int(*)(void*,FILE*))archer_label_write,
				 archer_labels_i4k_fp,
				 archer_labels_array_fp);
    label_ptr = bow_sarray_entry_at_keystr(archer_labels, word);
  }
  wi = label_ptr->li;

  /* We can't just go ahead and write it.  Reason: 
     This might mean writing two identical labels one after the other
     with the same pi.  For obscure reasons, archer's indexing
     scheme disallows this.  Instead, we'll hold onto
     the label.  If we see an identical one immediately, we'll wait 
     until we see the next closing label. Note, this also takes care
     of the potential problem of fields containing no indexable text */
 
  for (waiti = 0; waiti < waitln; ++waiti)
    if (waitli[waiti] == wi)    /* String fields together */
    {
      while (waiti + 1 < waitln)
      {
	waitli[waiti] = waitli[waiti + 1];
	++waiti;
      }
      waitln--;
      waiti = -1;
      break;
    }
  if (waiti != -1)              /* Didn't find in wait list */
    waitli[waitln++] = wi;
}


static int 
archer_index_filename_flex(const char *filename, 
                           archer_doc *doc_ptr, int di)
{
  FILE *fp;
  char word[BOW_MAX_WORD_LENGTH];
  int skip = 0;
  int pi = 0;
  int wtype;
  int (*get_word)(char buf[], int bufsz) = NULL;
  void (*set_fp)(FILE *fp, const char * name) = NULL;

  switch (bow_flex_option)
    {
    case USE_MAIL_FLEXER :
      set_fp = flex_mail_open;
      get_word = flex_mail_get_word;
      break;
    case USE_TAGGED_FLEXER :
      set_fp = tagged_lex_open;
      get_word = tagged_lex_get_word;
      break;
    default :
      bow_error("Unrecognized bow_flex_option=%d\n", bow_flex_option);
    }

  fp = archer_get_fp_from_filename(filename);
  if (fp == NULL)
  {
    bow_verbosify (bow_progress, "Not indexing %s\n", filename);
    return 0;
  }

  set_fp(fp, filename);
  waitln = 0;
  while ((wtype = get_word(word, BOW_MAX_WORD_LENGTH)))
  {
    if (wtype == 1)
    {
      if (!skip)
	archer_index_term(word, di, pi);
      ++pi;
    }
         /* Start label -- End label */
    else if (wtype == 2 || wtype == 3)
    {
      if (strcmp(word, "skip") == 0)
      {
	skip += wtype == 2 ? 1 : -1;
	assert(skip >= 0);
      }
      else 
	archer_index_label(word, di, pi);
    }
  }

  flush_labels(di, pi);

  fclose (fp);

  if (archer_index_inc)
  {
    bow_wi2pv_write_header (archer_wi2pv); 
    bow_wi2pv_write_header(archer_li2pv);
  }

  return pi;
}

int
archer_index_filename (const char *filename, void *unused)
{
  int di, count;
  archer_doc doc, *doc_ptr;

  /* Make sure this file isn't already in the index.  If it is just
     return (after undeleting it, if necessary. */
  doc_ptr = bow_sarray_entry_at_keystr (archer_docs, filename);
  if (doc_ptr)
    {
      if (doc_ptr->word_count < 0)
      {
	doc_ptr->word_count = -(doc_ptr->word_count);

	/* Update information on disk */
	bow_array_write_entry_inc (archer_docs->array, 
				   bow_sarray_index_at_keystr(archer_docs, filename), 
				   (int(*)(void*,FILE*))archer_doc_write, archer_docs_array_fp);
      }

      return 1;
    }

  /* The index of this new document is the next available index in the
     array of documents. */
  di = archer_docs->array->length;
  count = bow_flex_option ?
    archer_index_filename_flex (filename, doc_ptr, di) :
    archer_index_filename_old_lex (filename, NULL, di);
  if (count == 0)
    return 0;

  doc.tag = bow_doc_train;
  doc.word_count = count;
  doc.di = di;

  /* Update document list on disk */
  /* Do this -regardless- of archer_index_inc; writing once per
     word is expensive, but once per document cheap.
     Doing this makes things in archer_archive less complicated.
  */
  bow_sarray_add_entry_with_keystr_inc (archer_docs, &doc, filename,
					(int(*)(void*,FILE*))archer_doc_write,
					archer_docs_i4k_fp,
					archer_docs_array_fp);
      
  if (di % 10 == 0)
    bow_verbosify (bow_progress, "\b\b\b\b\b\b\b\b%8d", di);
  
  di++;
  return count;
}

void
archer_index ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  int count;

  if (!archer_docs_i4k_fp)
  {
    sprintf (filename, "%s/docs.i4k", bow_data_dirname);
    archer_docs_i4k_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_docs_array_fp)
  {
    sprintf (filename, "%s/docs.array", bow_data_dirname);
    archer_docs_array_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_docs)
  {
    archer_docs = bow_sarray_new (0, sizeof (archer_doc), archer_doc_free);
    bow_array_write_header_inc (archer_docs->array, archer_docs_array_fp);
  }
  
  if (!archer_labels_i4k_fp)
  {
    sprintf (filename, "%s/labels.i4k", bow_data_dirname);
    archer_labels_i4k_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_labels_array_fp)
  {
    sprintf (filename, "%s/labels.array", bow_data_dirname);
    archer_labels_array_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_labels)
  {
    archer_labels = bow_sarray_new (0, sizeof (archer_label), archer_label_free);
    bow_array_write_header_inc (archer_labels->array, archer_labels_array_fp);
  }

  if (!archer_wi2pv)
    archer_wi2pv = bow_wi2pv_new (0, "pv", "wi2pv");
  if (!archer_li2pv)
    archer_li2pv = bow_wi2pv_new (0, "lipv", "li2pv");

  if (!archer_vocabulary_fp)
  {
    sprintf (filename, "%s/vocabulary", bow_data_dirname);
    archer_vocabulary_fp = bow_fopen (filename, "wb+");
  }

  /* Do NOT write to disk every word */
  archer_index_inc = 0;

  count = archer_docs->array->length;
  bow_verbosify (bow_progress, "Indexing files:              ");
  bow_map_filenames_from_dir (archer_index_filename, NULL,
			      archer_arg_state.dirname, "");
  bow_verbosify (bow_progress, "\nIndexed %d documents\n", 
		 archer_docs->array->length - count);

  archer_archive ();

  /* Close the wi2pv and pv files */
  bow_wi2pv_free (archer_wi2pv);
  bow_wi2pv_free(archer_li2pv);
}

/* Index each line of ARCHER_ARG_STATE.DIRNAME as if it were a
   separate file, named after the line number. Does not deal with labels.*/
void
archer_index_lines ()
{
  static const int max_line_length = 2048;
  char buf[max_line_length];
  FILE *fp;
  archer_doc doc;
  bow_lex *lex;
  char word[BOW_MAX_WORD_LENGTH];
  int wi, di, pi;
  char filename[BOW_MAX_WORD_LENGTH];

  /*
    archer_docs = bow_sarray_new (0, sizeof (archer_doc), archer_doc_free);
    archer_wi2pv = bow_wi2pv_new (0, "pv", "wi2pv");
  */

  if (!archer_docs_i4k_fp)
  {
    sprintf (filename, "%s/docs.i4k", bow_data_dirname);
    archer_docs_i4k_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_docs_array_fp)
  {
    sprintf (filename, "%s/docs.array", bow_data_dirname);
    archer_docs_array_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_docs)
  {
    archer_docs = bow_sarray_new (0, sizeof (archer_doc), archer_doc_free);
    bow_array_write_header_inc (archer_docs->array, archer_docs_array_fp);
  }
  
  if (!archer_labels_i4k_fp)
  {
    sprintf (filename, "%s/labels.i4k", bow_data_dirname);
    archer_labels_i4k_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_labels_array_fp)
  {
    sprintf (filename, "%s/labels.array", bow_data_dirname);
    archer_labels_array_fp =  bow_fopen (filename, "wb+");
  }
  if (!archer_labels)
  {
    archer_labels = bow_sarray_new (0, sizeof (archer_label), archer_label_free);
    bow_array_write_header_inc (archer_labels->array, archer_labels_array_fp);
  }

  if (!archer_wi2pv)
    archer_wi2pv = bow_wi2pv_new (0, "pv", "wi2pv");
  if (!archer_li2pv)
    archer_li2pv = bow_wi2pv_new (0, "lipv", "li2pv");

  if (!archer_vocabulary_fp)
  {
    sprintf (filename, "%s/vocabulary", bow_data_dirname);
    archer_vocabulary_fp = bow_fopen (filename, "wb+");
  }

  /* Do NOT write to disk every word */
  archer_index_inc = 1;

  fp = bow_fopen (archer_arg_state.dirname, "r");
  bow_verbosify (bow_progress, "Indexing lines:              ");
  while (fgets (buf, max_line_length, fp))
    {
      lex = bow_default_lexer->open_str (bow_default_lexer, buf);
      if (lex == NULL)
	continue;
      di = archer_docs->array->length;
      sprintf (filename, "%08d", di);
      pi = 0;
      while (bow_default_lexer->get_word (bow_default_lexer,
					  lex, word, BOW_MAX_WORD_LENGTH))
	{
	  wi = bow_word2int_inc (word, archer_vocabulary_fp);
	  if (wi < 0)
	    continue;
	  bow_wi2pv_add_wi_di_pi (archer_wi2pv, wi, di, pi);
	  if (archer_index_inc) 
	    bow_wi2pv_write_entry (archer_wi2pv, wi);
	  pi++;
	}
      bow_default_lexer->close (bow_default_lexer, lex);
      doc.tag = bow_doc_train;
      doc.word_count = pi;
      doc.di = di;
      bow_sarray_add_entry_with_keystr_inc (archer_docs, &doc, filename,
					    (int(*)(void*,FILE*))archer_doc_write,
					    archer_docs_i4k_fp,
					    archer_docs_array_fp);
      pi++;
    }
  fclose (fp);
  bow_verbosify (bow_progress, "\n");

  archer_archive ();

  /* Close the wi2pv and pv files */
  bow_wi2pv_free (archer_wi2pv);
  bow_wi2pv_free (archer_li2pv);
}

/* Set the special flag in FILENAME's doc structure indicating that
   this document has been removed from the index.  Return zero on
   success, non-zero on failure. */
int
archer_delete_filename (const char *filename)
{
  archer_doc *doc;

  doc = bow_sarray_entry_at_keystr (archer_docs, filename);
  if (doc)
    {
      doc->word_count = -(doc->word_count);

      /* Update information on disk */
      bow_array_write_entry_inc (archer_docs->array, 
				 bow_sarray_index_at_keystr(archer_docs, filename), 
				 (int(*)(void*,FILE*))archer_doc_write, archer_docs_array_fp);
      return 0;
    }
  return 1;
}

bow_wa *
archer_query_hits_matching_wi (int wi, int fld, int *occurrence_count)
{
  int count = 0;
  int di, pi, li[100], ln;
  int i;
  bow_wa *wa;

  if (wi >= archer_wi2pv->entry_count && archer_wi2pv->entry[wi].count <= 0)
    return NULL;
  wa = bow_wa_new (0);
  bow_pv_rewind (&(archer_wi2pv->entry[wi]), archer_wi2pv->fp);
  ln = 100;
  bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, wi, &di, li, &ln, &pi);
  while (di != -1)
    {
      if (fld >= 0)
	{
	  for (i = 0; i < ln; ++i) 
	    {
	      if (li[i] == fld)
		{
		  bow_wa_add_to_end (wa, di, 1);
		  break;
		}
	    }
	}
      else
	bow_wa_add_to_end (wa, di, 1);
      count++;
      ln = 100;
      bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, wi, &di, li, &ln, &pi);
    }
  *occurrence_count = count;
  return wa;
}

/* Temporary constant.  Fix this soon! */
#define MAX_QUERY_WORDS 50

bow_wa*
archer_query_hits_matching_sequence (const char *query_string,
				     const char *suffix_string)
{
  int query[MAX_QUERY_WORDS];		/* WI's in the query */
  int di[MAX_QUERY_WORDS];
  int pi[MAX_QUERY_WORDS];
  int li = -1;
  int query_len;
  int max_di, max_pi;
  int wi, i;
  bow_lex *lex;
  char word[BOW_MAX_WORD_LENGTH];
  int sequence_occurrence_count = 0;
  int something_was_greater_than_max;
  bow_wa *wa;
  float scaler;
  archer_doc *doc;
  archer_label *label;

  if (bow_flex_option) 
    {
      if (suffix_string[0])
	{
	  label = bow_sarray_entry_at_keystr(archer_labels, suffix_string);
	  
	  /* No words occur in label */
	  if (!label)
	    return NULL;
	  
	  li = label->li;
	}
    }

  /* Parse the query */
  lex = bow_default_lexer->open_str (bow_default_lexer, (char*)query_string);
  if (lex == NULL)
    return NULL;
  query_len = 0;
  while (bow_default_lexer->get_word (bow_default_lexer, lex,
				      word, BOW_MAX_WORD_LENGTH))
    {
      /* Add the field-restricting suffix string, e.g. "xxxtitle" */
      if (!bow_flex_option) 
	{
	  if (suffix_string[0])
	    {
	      strcat (word, "xxx");
	      strcat (word, suffix_string);
	      assert (strlen (word) < BOW_MAX_WORD_LENGTH);
	    }
	}
      wi = bow_word2int_no_add (word);
      if (wi >= 0)
	{
	  di[query_len] = pi[query_len] = -300;
	  query[query_len++] = wi;
	}
      else if ((bow_lexer_stoplist_func
		&& !(*bow_lexer_stoplist_func) (word))
	       || (!bow_lexer_stoplist_func
		   && strlen (word) < 2))
	{
	  /* If a query term wasn't present, and its not because it
	     was in the stoplist or the word is a single char, then
	     return no hits. */
	  query_len = 0;
	  break;
	}
      /* If we have no more room for more query terms, just don't use
         the rest of them. */
      if (query_len >= MAX_QUERY_WORDS)
	break;
    }
  bow_default_lexer->close (bow_default_lexer, lex);

  if (query_len == 0)
    return NULL;

  if (query_len == 1)
    {
      wa = archer_query_hits_matching_wi (query[0], li,
					  &sequence_occurrence_count);
      goto search_done;
    }

  /* Initialize the array of document scores */
  wa = bow_wa_new (0);

  /* Search for documents containing the query words in the same order
     as the query. */
  bow_wi2pv_rewind (archer_wi2pv);
  max_di = max_pi = -200;
  /* Loop while we look for matches.  We'll break out of this loop when
     all query words are at the end of their PV's. */
  for (;;)
    {
      /* Keep reading DI and PI from one or more of the query-word PVs
	 until none of the DIs or PIs is greater than the MAX_DI or
	 MAX_PI.  At this point the DIs and PI's should all be equal,
	 indicating a match.  Break out of this loop if all PVs are
	 at the end, (at which piont they return -1 for both DI and
	 PI). */
      do
	{
	  something_was_greater_than_max = 0;
	  for (i = 0; i < query_len; i++)
	    {
	      int lia[100];
	      int ln;

	      while (di[i] != -1
		  && (di[i] < max_di
		      || (di[i] <= max_di && pi[i] < max_pi)))
		{
		  ln = 100;
		  bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, query[i],
					      &(di[i]), lia, &ln, &(pi[i]));

		  /* If any of the query words is at the end of their
		     PV, then we're not going to find any more
		     matches, and we're done setting the scores.  Go
		     print the matching documents. */
		  if (di[i] == -1)
		    goto search_done;

		  /* Make it so that all PI's will be equal if the words
		     are in order. */
		  pi[i] -= i;
		  bow_verbosify (bow_verbose, "%20s %10d %10d %10d %10d\n", 
				 bow_int2word (query[i]), 
				 di[i], pi[i], max_di, max_pi);
		}
	      if (di[i] > max_di) 
		{
		  max_di = di[i];
		  max_pi = pi[i];
		  something_was_greater_than_max = 1;
		}
	      else if (pi[i] > max_pi && di[i] == max_di) 
		{
		  max_pi = pi[i];
		  something_was_greater_than_max = 1;
		}
	      else if ((di[i] == max_di) && (pi[i] == max_pi))
		{
		  int j;

		  for (j = 0; j < ln; ++j) if (lia[j] == li) break;
		  if((ln != 0) && (j == ln)) something_was_greater_than_max = 1;
		}
	    }
	}
      while (something_was_greater_than_max);
      bow_verbosify (bow_verbose, 
		     "something_was_greater_than_max di=%d\n", di[0]);
      for (i = 1; i < query_len; i++)
	assert (di[i] == di[0] && pi[i] == pi[0]);
      
      /* Make sure this DI'th document hasn't been deleted.  If it
         hasn't then add this DI to the WA---the list of hits */
      doc = bow_sarray_entry_at_index (archer_docs, di[0]);
      if (doc->word_count > 0)
	{
	  bow_wa_add_to_end (wa, di[0], 1);
	  sequence_occurrence_count++;
	}

      /* Set up so that next time through we'll read the next words
         from each PV. */
      for (i = 0; i < query_len; i++)
	{
	  if (di[i] != -1)
	    di[i] = -300;
	  if (pi[i] != -1)
	    pi[i] = -300;
	}
    }
 search_done:

  /* Scale the scores by the log of the occurrence count of this sequence,
     and take the log of the count (shifted) to encourage documents that
     have all query term to be ranked above documents that have many 
     repetitions of a few terms. */
  scaler = 1.0 / log (5 + sequence_occurrence_count);
  for (i = 0; i < wa->length; i++)
    wa->entry[i].weight = scaler * log (5 + wa->entry[i].weight);

  if (wa->length == 0)
    {
      bow_wa_free (wa);
      return NULL;
    }
  return wa;
}

/* A temporary hack.  Also, does not work for queries containing
   repeated words */
void
archer_query ()
{
  int i;
  int num_hits_to_print;
#define NUM_FLAGS 3
  enum {pos = 0,
	reg,
	neg,
	num_flags};
  struct _word_hit {
    const char *term;
    bow_wa *wa;
    int flag;
  } word_hits[num_flags][MAX_QUERY_WORDS];
  int word_hits_count[num_flags];
  int current_wai[num_flags][MAX_QUERY_WORDS];
  struct _doc_hit {
    int di;
    float score;
    const char **terms;
    int terms_count;
  } *doc_hits;
  int doc_hits_count;
  int doc_hits_size;
  bow_wa *term_wa;
  int current_di, h, f, min_di;
  int something_was_greater_than_max;
  char *query_copy, *query_remaining, *end;
  char query_string[BOW_MAX_WORD_LENGTH];
  char suffix_string[BOW_MAX_WORD_LENGTH];
  int found_flag, flag, length;

  /* For sorting the combined list of document hits */
  int compare_doc_hits (struct _doc_hit *hit1, struct _doc_hit *hit2)
    {
      if (hit1->score < hit2->score)
	return 1;
      else if (hit1->score == hit2->score)
	return 0;
      else
	return -1;
    }

  /* Initialize the list of target documents associated with each term */
  for (i = 0; i < num_flags; i++)
    word_hits_count[i] = 0;

  /* Initialize the combined list of target documents */
  doc_hits_size = 1000;
  doc_hits_count = 0;
  doc_hits = bow_malloc (doc_hits_size * sizeof (struct _doc_hit));

  /* Process each term in the query.  Quoted sections count as one
     term here. */
  query_remaining = query_copy = strdup (archer_arg_state.query_string);
  assert (query_copy);
  /* Chop any trailing newline or carriage return. */
  end = strpbrk (query_remaining, "\n\r");
  if (end)
    *end = '\0';
  while (*query_remaining)
    {
      /* Find the beginning of the next query term, and record +/- flags */
      while (*query_remaining 
	     && (!isalnum (*query_remaining)
		 && *query_remaining != ':'
		 && *query_remaining != '+'
		 && *query_remaining != '-'
		 && *query_remaining != '"'))
	query_remaining++;
      flag = reg;
      found_flag = 0;
      if (*query_remaining == '\0')
	{
	  break;
	}
      if (*query_remaining == '+')
	{
	  query_remaining++;
	  flag = pos;
	}
      else if (*query_remaining == '-')
	{
	  query_remaining++;
	  flag = neg;
	}

      /* See if there is a field-restricting tag here, and if so, deal
         with it */
      if ((end = strpbrk (query_remaining, ": \"\t"))
	  && *end == ':')
	{
	  /* The above condition ensures that a ':' appears before any
	     term-delimiters */
	  /* Find the end of the field-restricting suffix */
	  length = end - query_remaining;
	  assert (length < BOW_MAX_WORD_LENGTH);
	  /* Remember the suffix, and move ahead the QUERY_REMAINING */
	  memcpy (suffix_string, query_remaining, length);
	  suffix_string[length] = '\0';
	  query_remaining = end + 1;
	}
      else
	suffix_string[0] = '\0';

      /* Find the end of the next query term. */
      if (*query_remaining == '"')
	{
	  query_remaining++;
	  end = strchr (query_remaining, '"');
	}
      else
	{
	  end = strchr (query_remaining, ' ');
	}
      if (end == NULL)
	end = strchr (query_remaining, '\0');

      /* Put the next query term into QUERY_STRING and increment
         QUERY_REMAINING */
      length = end - query_remaining;
      length = MIN (length, BOW_MAX_WORD_LENGTH-1);
      memcpy (query_string, query_remaining, length);
      query_string[length] = '\0';
      if (*end == '"')
	query_remaining = end + 1;
      else
	query_remaining = end;
      if (length == 0)
	continue;
      /* printf ("%d %s\n", flag, query_string); */

      /* Get the list of documents matching the term */
      term_wa = archer_query_hits_matching_sequence (query_string, 
						     suffix_string);
      if (!term_wa)
	{
	  if (flag == pos)
	    /* A required term didn't appear anywhere.  Print nothing */
	    goto hit_combination_done;
	  else
	    continue;
	}

      word_hits[flag][word_hits_count[flag]].term = strdup (query_string);
      word_hits[flag][word_hits_count[flag]].wa = term_wa;
      word_hits[flag][word_hits_count[flag]].flag = flag;
      word_hits_count[flag]++;
      assert (word_hits_count[flag] < MAX_QUERY_WORDS);
      bow_verbosify (bow_progress, "%8d %s\n", term_wa->length, query_string);
    }

  /* Bring together the WORD_HITS[*], following the correct +/-
     semantics */
  current_di = 0;
  for (f = 0; f < num_flags; f++)
    for (h = 0; h < word_hits_count[f]; h++)
      current_wai[f][h] = 0;

 next_current_di:
  if (word_hits_count[pos] == 0)
    {
      /* Find a document in which a regular term appears, and align the
	 CURRENT_WAI[REG][*] to point to the document if exists in that list */
      min_di = INT_MAX;
      for (h = 0; h < word_hits_count[reg]; h++)
	{
	  if (current_wai[reg][h] != -1
	      && (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		  < current_di))
	    {
	      if (current_wai[reg][h] < word_hits[reg][h].wa->length - 1)
		current_wai[reg][h]++;
	      else
		current_wai[reg][h] = -1;
	    }
	  assert (current_wai[reg][h] == -1
		  || (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		      >= current_di));
	  if (current_wai[reg][h] != -1
	      && word_hits[reg][h].wa->entry[current_wai[reg][h]].wi < min_di)
	    min_di = word_hits[reg][h].wa->entry[current_wai[reg][h]].wi;
	}
      if (min_di == INT_MAX)
	goto hit_combination_done;
	
      current_di = min_di;
    }
  else
    {
      /* Find a document index in which all the +terms appear */
      /* Loop until current_wai[pos][*] all point to the same document index */
      do
	{
	  something_was_greater_than_max = 0;
	  for (h = 0; h < word_hits_count[pos]; h++)
	    {
	      while (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi
		     < current_di)
		{
		  if (current_wai[pos][h] < word_hits[pos][h].wa->length - 1)
		    current_wai[pos][h]++;
		  else
		    /* We are at the end of a + list, and thus are done. */
		    goto hit_combination_done;
		}
	      if (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi 
		  > current_di)
		{
		  current_di = 
		    word_hits[pos][h].wa->entry[current_wai[pos][h]].wi;
		  something_was_greater_than_max = 1;
		}
	    }
	}
      while (something_was_greater_than_max);
      /* At this point all the CURRENT_WAI[pos][*] should be pointing to the
	 same document.  Verify this. */
      for (h = 1; h < word_hits_count[pos]; h++)
	assert (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi
		== word_hits[pos][0].wa->entry[current_wai[pos][0]].wi);
    }

  /* Make sure the CURRENT_DI doesn't appear in any of the -term lists. */
  for (h = 0; h < word_hits_count[neg]; h++)
    {
      /* Loop until we might have found the CURRENT_DI in this neg list */
      while (current_wai[neg][h] != -1
	     && (word_hits[neg][h].wa->entry[current_wai[neg][h]].wi
		 < current_di))
	{
	  if (current_wai[neg][h] < word_hits[neg][h].wa->length - 1)
	    current_wai[neg][h]++;
	  else
	    current_wai[neg][h] = -1;
	}
      if (word_hits[neg][h].wa->entry[current_wai[neg][h]].wi == current_di)
	{
	  current_di++;
	  goto next_current_di;
	}
    }

  /* Add this CURRENT_DI to the combinted list of hits in DOC_HITS */
  assert (current_di < archer_docs->array->length);
  doc_hits[doc_hits_count].di = current_di;
  doc_hits[doc_hits_count].score = 0;
  for (h = 0; h < word_hits_count[pos]; h++)
    doc_hits[doc_hits_count].score += 
      word_hits[pos][h].wa->entry[current_wai[pos][h]].weight;
  doc_hits[doc_hits_count].terms_count = 0;
  doc_hits[doc_hits_count].terms = bow_malloc (MAX_QUERY_WORDS*sizeof (char*));

  /* Add score value from the regular terms, if CURRENT_DI appears there */
  for (h = 0; h < word_hits_count[reg]; h++)
    {
      if (word_hits_count[pos] != 0)
	{
	  while (current_wai[reg][h] != -1
		 && (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		     < current_di))
	    {
	      if (current_wai[reg][h] < word_hits[reg][h].wa->length - 1)
		current_wai[reg][h]++;
	      else
		current_wai[reg][h] = -1;
	    }
	}
      if (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
	  == current_di)
	{
	  doc_hits[doc_hits_count].score += 
	    word_hits[reg][h].wa->entry[current_wai[reg][h]].weight;
	  doc_hits[doc_hits_count].
	    terms[doc_hits[doc_hits_count].terms_count]
	    = word_hits[reg][h].term;
	  doc_hits[doc_hits_count].terms_count++;
	}
    }

  doc_hits_count++;
  if (doc_hits_count >= doc_hits_size)
    {
      doc_hits_size *= 2;
      doc_hits = bow_realloc (doc_hits, (doc_hits_size
					 * sizeof (struct _doc_hit)));
    }

  current_di++;
  goto next_current_di;

 hit_combination_done:

  if (doc_hits_count)
    {
      /* Sort the DOC_HITS list */
      qsort (doc_hits, doc_hits_count, sizeof (struct _doc_hit), 
	     (int(*)(const void*,const void*))compare_doc_hits);

      fprintf (archer_arg_state.query_out_fp, ",HITCOUNT %d\n", 
	       doc_hits_count);
      num_hits_to_print = MIN (doc_hits_count, 
			       archer_arg_state.num_hits_to_print);
      for (i = 0; i < num_hits_to_print; i++)
	{
	  fprintf (archer_arg_state.query_out_fp,
		   "%s %f ", bow_sarray_keystr_at_index (archer_docs, doc_hits[i].di), 
		   doc_hits[i].score);
	  for (h = 0; h < word_hits_count[pos]; h++)
	    fprintf (archer_arg_state.query_out_fp, 
		     "%s, ", word_hits[pos][h].term);
	  for (h = 0; h < doc_hits[i].terms_count-1; h++)
	    fprintf (archer_arg_state.query_out_fp, 
		     "%s, ", doc_hits[i].terms[h]);
	  h = doc_hits[i].terms_count - 1;
	  if (h >= 0)
	    fprintf (archer_arg_state.query_out_fp, 
		     "%s", doc_hits[i].terms[h]);
	  fprintf (archer_arg_state.query_out_fp, "\n");
	}
    }
  fprintf (archer_arg_state.query_out_fp, ".\n");
  fflush (archer_arg_state.query_out_fp);

  /* Free all the junk we malloc'ed */
  for (f = 0; f < num_flags; f++)
    for (h = 0; h < word_hits_count[f]; h++)
      bow_free ((char*)word_hits[f][h].term);
  for (h = 0; h < doc_hits_count; h++)
    bow_free (doc_hits[h].terms);
  bow_free (doc_hits);
  bow_free (query_copy);
}


void
archer_print_all ()
{
  int wi;
  int di;
  int pi;
  int li;
  int labels[BOW_MAX_WORD_LABELS];
  int i;

  bow_wi2pv_rewind (archer_wi2pv);
  for (wi = 0; wi < bow_num_words (); wi++)
    {
      for (;;)
	{
	  li = BOW_MAX_WORD_LABELS;
	  bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, wi, &di, labels, &li, &pi);
	  if (di == -1)
	    break;
	  printf ("%010d %010d %s: ", di, pi, bow_int2word (wi));
	  for (i = 0; i < li; i++)
	    printf ("%s ", bow_sarray_keystr_at_index (archer_labels, labels[i]));
	  printf("\n");
	}
    }
}

void
archer_print_word_stats ()
{
  bow_wi2pv_print_stats (archer_wi2pv);
}


/* Definitions for using argp command-line processing */

const char *argp_program_version =
"archer " STRINGIFY(ARCHER_MAJOR_VERSION) "." STRINGIFY(ARCHER_MINOR_VERSION);

const char *argp_program_bug_address = "<mccallum@cs.cmu.edu>";

#if !defined(DART) && !defined(FDART) && !defined(IDART)

static char archer_argp_doc[] =
"Archer -- a document retrieval front-end to libbow";

static char archer_argp_args_doc[] = "[ARG...]";

enum {
  QUERY_SERVER_KEY = 3000,
  QUERY_FORK_SERVER_KEY,
  INDEX_LINES_KEY,
  SCORE_IS_RAW_COUNT_KEY,
};

static struct argp_option archer_options[] =
{
  {0, 0, 0, 0,
   "For building data structures from text files:", 1},
  {"index", 'i', "DIRNAME", 0,
   "Tokenize training documents found under DIRNAME, "
   "and save them to disk"},
  {"index-lines", INDEX_LINES_KEY, "FILENAME", 0,
   "Like --index, except index each line of FILENAME as if it were a "
   "separate document.  Documents are named after sequential line numbers."},

  {0, 0, 0, 0,
   "For doing document retreival using the data structures built with -i:", 2},
  {"query", 'q', "WORDS", 0, 
   "tokenize input from stdin [or FILE], then print document most like it"},
  {"query-server", QUERY_SERVER_KEY, "PORTNUM", 0,
   "Run archer in socket server mode."},
  {"query-forking-server", QUERY_FORK_SERVER_KEY, "PORTNUM", 0,
   "Run archer in socket server mode, forking a new process with every "
   "connection.  Allows multiple simultaneous connections."},
  {"num-hits-to-show", 'n', "N", 0,
   "Show the N documents that are most similar to the query text "
   "(default N=1)"},
  {"score-is-raw-count", SCORE_IS_RAW_COUNT_KEY, 0, 0,
   "Instead of using a weighted sum of logs, the score of a document "
   "will be simply the number of terms in both the query and the document."},

  {0, 0, 0, 0,
   "Diagnostics", 3},
  {"print-all", 'p', 0, 0,
   "Print, in unsorted order, all the document indices, positions and words"},
  {"print-word-stats", 's', 0, 0,
   "Print the number of times each word occurs."},

  { 0 }
};

static error_t
archer_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'q':
      archer_arg_state.what_doing = archer_query;
      archer_arg_state.query_string = arg;
      break;
    case 'i':
      archer_arg_state.what_doing = archer_index;
      archer_arg_state.dirname = arg;
      break;
    case INDEX_LINES_KEY:
      archer_arg_state.what_doing = archer_index_lines;
      archer_arg_state.dirname = arg;
      break;
    case 'p':
      archer_arg_state.what_doing = archer_print_all;
      break;
    case 'n':
      archer_arg_state.num_hits_to_print = atoi (arg);
      break;
    case 's':
      archer_arg_state.what_doing = archer_print_word_stats;
      break;
    case SCORE_IS_RAW_COUNT_KEY:
      archer_arg_state.score_is_raw_count = 1;
      break;
    case QUERY_FORK_SERVER_KEY:
      archer_arg_state.serve_with_forking = 1;
    case QUERY_SERVER_KEY:
      archer_arg_state.what_doing = archer_query_serve;
      archer_arg_state.server_port_num = arg;
      break;

    case ARGP_KEY_ARG:
      /* Now we consume all the rest of the arguments.  STATE->next is the
	 index in STATE->argv of the next argument to be parsed, which is the
	 first STRING we're interested in, so we can just use
	 `&state->argv[state->next]' as the value for ARCHER_ARG_STATE->ARGS.
	 IN ADDITION, by setting STATE->next to the end of the arguments, we
	 can force argp to stop parsing here and return.  */
      archer_arg_state.non_option_argi = state->next - 1;
      if (archer_arg_state.what_doing == archer_index
	  && state->next > state->argc)
	{
	  /* Zero directory names is not enough. */
	  fprintf (stderr, "Need at least one directory to index.\n");
	  argp_usage (state);
	}
      state->next = state->argc;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

#endif

#ifdef ARCHER_USE_MCHECK
void mem_error (enum mcheck_status status)
{
  switch (status)
    {
    case MCHECK_DISABLED:
      printf ("mem_error: `mcheck' was not called before the first\n"
	      "allocation. No consistency checking can be done.\n");
      break;
    case MCHECK_OK:
      break;
    case MCHECK_HEAD:
      printf ("mem_error: The data immediately before the block was\n"
	      "modified. This commonly happens when an array index or\n"
	      "pointer is decremented too far.\n");
      break;
    case MCHECK_TAIL:
      printf ("mem_error: The data immediately after the block was modified.\n"
	      "This commonly happens when an array index or pointer is\n"
	      "incremented too far.\n");
      break;
    case MCHECK_FREE:
      printf ("mem_error: The block was already freed.\n");
      break;
    }

  if (status != MCHECK_OK)
    raise (SIGSEGV);
}
#endif


/* The main() function. */

#if defined(DART) || defined(FDART)

static void dart_usage(char **argv)
{
  fprintf(stderr, 
	  "USAGE: %s [-a<ip-spec>] [-p] <port> <data-dir> [<annotations>]\n", 
	  argv[0]);
  exit(1);
}

static int dart_arg_parse(int argc, char **argv)
{
  char *cp;
  int argoff;

  if (argc < 3 || argc > 6)
    dart_usage(argv);

  for (argoff = 1; argoff < argc && argv[argoff][0] == '-'; ++argoff)
  {
    switch (argv[1][1])
    {
    case 'a' :
      cp = argv[1] + 2;
      if (!inet_aton(cp, &archer_ip_spec))
	dart_usage(argv);
      break;
    case 'p' :
      {  
	char *pw = getpass("Password: ");
	if (pw[0] == 0)
	  fprintf(stderr, "No password supplied: queries unrestricted\n");
	else
	  strcpy(archer_password, crypt(pw, "t5"));
      }
      break;
    default :
      dart_usage(argv);
    }
  }

  return argoff;
}


int
main(int argc, char **argv)
{
  extern const char *bow_annotation_filename;
  int argoff;

  /* Prevents zombie children in System V environments */
  signal (SIGCHLD, SIG_IGN);

  /* Default command-line argument values */
  archer_arg_state.what_doing = archer_query_serve;
  archer_arg_state.num_hits_to_print = 10;
  archer_arg_state.dirname = NULL;
  archer_arg_state.query_string = NULL;
#if defined(FDART)
  archer_arg_state.serve_with_forking = 1; 
#endif
  archer_arg_state.query_out_fp = stdout;
  archer_arg_state.score_is_raw_count = 0;

  argoff = dart_arg_parse(argc, argv);

  archer_arg_state.server_port_num = argv[argoff];
  bow_data_dirname = argv[argoff + 1];
  if (argc == argoff + 3)
    bow_annotation_filename = argv[argoff + 2];
  bow_flex_option = USE_TAGGED_FLEXER;
  bow_lexer_stoplist_func = NULL;

  archer_unarchive ();

  (*archer_arg_state.what_doing) ();

  exit (0);
}

#elif defined(IDART)

int
main (int argc, char *argv[])
{
  /* Prevents zombie children in System V environments */
  signal (SIGCHLD, SIG_IGN);

  /* Default command-line argument values */
  archer_arg_state.what_doing = archer_index;
  archer_arg_state.dirname = NULL;
  archer_arg_state.serve_with_forking = 0;
  archer_arg_state.query_out_fp = stdout;
  archer_arg_state.score_is_raw_count = 0;

  if (argc < 3 || argc > 5)
  {
    fprintf(stderr, 
	    "USAGE: %s [-i] <dir-to-index> <dir-to-write-to> [<extraction-dir>]\n",
	    argv[0]);
    exit(1);
  }

  if (strcmp(argv[1], "-i") == 0)
    archer_incremental_index = 1;

  archer_source_directory = archer_arg_state.dirname 
                          = argv[archer_incremental_index + 1];
  bow_data_dirname = argv[archer_incremental_index + 2];
  if (argc == 4 + archer_incremental_index)
    archer_extraction_directory = argv[archer_incremental_index + 3];

  bow_flex_option = USE_TAGGED_FLEXER;
  bow_lexer_stoplist_func = NULL;

  if (archer_incremental_index)
      archer_unarchive();

  (*archer_arg_state.what_doing) ();

  exit (0);
}

#else

static struct argp archer_argp = 
{ archer_options, archer_parse_opt, archer_argp_args_doc,
  archer_argp_doc, bow_argp_children};

int
main (int argc, char *argv[])
{
  /* Prevents zombie children in System V environments */
  signal (SIGCHLD, SIG_IGN);

#ifdef ARCHER_USE_MCHECK
  mcheck (mem_error);
#endif

  /* Default command-line argument values */
  archer_arg_state.what_doing = NULL;
  archer_arg_state.num_hits_to_print = 10;
  archer_arg_state.dirname = NULL;
  archer_arg_state.query_string = NULL;
  archer_arg_state.serve_with_forking = 0;
  archer_arg_state.query_out_fp = stdout;
  archer_arg_state.score_is_raw_count = 0;

  /* Parse the command-line arguments. */
  argp_parse (&archer_argp, argc, argv, 0, 0, &archer_arg_state);

  if (archer_arg_state.what_doing == NULL)
    bow_error ("No action specified on command-line.");
  if (*archer_arg_state.what_doing != archer_index
      && *archer_arg_state.what_doing != archer_index_lines)
    archer_unarchive ();

  (*archer_arg_state.what_doing) ();

  exit (0);
}

#endif




