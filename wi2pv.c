/* mapping a word index to a "position vector" */

/* Copyright (C) 1998 Andrew McCallum

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
#include <bow/archer.h>

bow_wi2pv *
bow_wi2pv_new (int capacity, const char *pv_filename, const char *inc_filename)
{
  char pathname[BOW_MAX_WORD_LENGTH];
  bow_wi2pv *wi2pv;
  int i;

  wi2pv = bow_malloc (sizeof (bow_wi2pv));
  assert (strchr (pv_filename, '/') == NULL);
  wi2pv->pv_filename = strdup (pv_filename);
  assert (wi2pv->pv_filename);
  sprintf (pathname, "%s/%s", bow_data_dirname, pv_filename);
  /* Truncate the file at PV_FILENAME and open for reading and writing */
  wi2pv->fp = bow_fopen (pathname, "wb+");
  if (capacity)
    wi2pv->entry_count = capacity;
  else
    wi2pv->entry_count = 1024;
  wi2pv->num_words = 0;
  wi2pv->next_word = 0;
  wi2pv->entry = bow_malloc (wi2pv->entry_count * sizeof (bow_pv));
  /* Initialize the entries with a special COUNT that means "stub" */
  for (i = 0; i < wi2pv->entry_count; i++)
    wi2pv->entry[i].count = -1;

  /* Truncate the file at INC_FILENAME and open for reading and writing */
  sprintf (pathname, "%s/%s", bow_data_dirname, inc_filename);
  wi2pv->inc_fp = bow_fopen (pathname, "wb+");

  /* Write wi2pv header to disk */
  bow_wi2pv_write_header (wi2pv);

  return wi2pv;
}

void
bow_wi2pv_free (bow_wi2pv *wi2pv)
{
  fclose (wi2pv->fp);
  fclose (wi2pv->inc_fp);
  bow_free (wi2pv->entry);
  bow_free (wi2pv);
}

void
bow_wi2pv_add_wi_di_pi (bow_wi2pv *wi2pv, int wi, int di, int pi)
{
  /* If WI is so large that there isn't an entry for it, enlarge
     the array of PV's so that there is a place for it. */
  if (wi >= wi2pv->entry_count)
    {
      int i, old_entry_count = wi2pv->entry_count;
      do
	wi2pv->entry_count *= 2;
      while (wi > wi2pv->entry_count);
      wi2pv->entry = bow_realloc (wi2pv->entry, 
				  wi2pv->entry_count * sizeof (bow_pv));
      /* Initialize the entries with a special COUNT that means "stub" */
      for (i = old_entry_count; i < wi2pv->entry_count; i++)
	wi2pv->entry[i].count = -1;
    }

  /* If WI's entry is just a stub, then initialize it. */
  if (wi2pv->entry[wi].count < 0)
    {
      bow_pv_init (&(wi2pv->entry[wi]), wi2pv->fp);
      wi2pv->num_words++;
    }

  /* Add the DI and PI */
  bow_pv_add_di_pi (&(wi2pv->entry[wi]), di, pi, wi2pv->fp);
}

void
bow_wi2pv_add_wi_di_li_pi (bow_wi2pv *wi2pv, int wi, int di, int li[], int ln, 
			   int pi)
{

  assert (ln <= BOW_MAX_WORD_LABELS);

  /* If WI is so large that there isn't an entry for it, enlarge
     the array of PV's so that there is a place for it. */
  if (wi >= wi2pv->entry_count)
    {
      int i, old_entry_count = wi2pv->entry_count;
      do
	wi2pv->entry_count *= 2;
      while (wi > wi2pv->entry_count);
      wi2pv->entry = bow_realloc (wi2pv->entry, 
				  wi2pv->entry_count * sizeof (bow_pv));
      /* Initialize the entries with a special COUNT that means "stub" */
      for (i = old_entry_count; i < wi2pv->entry_count; i++)
	wi2pv->entry[i].count = -1;
    }

  /* If WI's entry is just a stub, then initialize it. */
  if (wi2pv->entry[wi].count < 0)
    {
      bow_pv_init (&(wi2pv->entry[wi]), wi2pv->fp);
      wi2pv->num_words++;
    }

  assert (di >= wi2pv->entry[wi].write_last_di);
  /* Add the DI and PI */
  bow_pv_add_di_li_pi (&(wi2pv->entry[wi]), di, li, ln, pi, wi2pv->fp);
}

void
bow_wi2pv_wi_next_di_li_pi (bow_wi2pv *wi2pv, int wi, int *di, 
			    int li[], int *ln, int *pi)
{
  if (wi >= wi2pv->entry_count || wi2pv->entry[wi].count < 0)
    {
      *di = -1;
      *pi = -1;
      return;
    }
  else
    {
      bow_pv_next_di_li_pi (&(wi2pv->entry[wi]), di, li, ln, pi, 
				   wi2pv->fp);
    }
}

void
bow_wi2pv_rewind (bow_wi2pv *wi2pv)
{
  int wi;
  for (wi = 0; wi < wi2pv->entry_count; wi++)
    {
      /* Don't rewind if it is a stub (== -1) and if it has no words
         in it (== 0) */
      if (wi2pv->entry[wi].count > 0)
	bow_pv_rewind (&(wi2pv->entry[wi]), wi2pv->fp);
    }
}

void
bow_wi2pv_wi_next_di_pi (bow_wi2pv *wi2pv, int wi, int *di, int *pi)
{
  if (wi >= wi2pv->entry_count || wi2pv->entry[wi].count < 0)
    {
      *di = -1;
      *pi = -1;
    }
  else
    {
      bow_pv_next_di_pi (&(wi2pv->entry[wi]), di, pi, wi2pv->fp);
    }
}

/* note that a subsequent call to bow_wi2pv_wi_next_di_li_pi() will
   not return labels correctly (although di and pi should be ok) */
void
bow_wi2pv_wi_unnext (bow_wi2pv *wi2pv, int wi)
{
  if (wi < wi2pv->entry_count && wi2pv->entry[wi].count >= 0)
    bow_pv_unnext (&(wi2pv->entry[wi]));
}

int
bow_wi2pv_wi_count (bow_wi2pv *wi2pv, int wi)
{
  if (wi < wi2pv->entry_count && wi2pv->entry[wi].count >= 0)
    return wi2pv->entry[wi].count;
  return 0;
}

/* Write wi2pv header to disk. */
void
bow_wi2pv_write_header (bow_wi2pv *wi2pv)
{
  fseek (wi2pv->inc_fp, 0, SEEK_SET);
  bow_fwrite_int (wi2pv->entry_count, wi2pv->inc_fp);
  bow_fwrite_int (wi2pv->next_word, wi2pv->inc_fp);
  bow_fwrite_int (wi2pv->num_words, wi2pv->inc_fp);
  bow_fwrite_string (wi2pv->pv_filename, wi2pv->inc_fp);
  /* Remember where the entries start */
  wi2pv->entry_start = ftell (wi2pv->inc_fp);
  fflush (wi2pv->inc_fp);
}

/* Write pv seek-location information associated with wi
   to disk. If wi is not the successor of the greatest previously
   written wi, then we write all the entries in between,
   too. (they will just be stubs)
   
   wi must already correspond to an entry; specifically,
   wi < wi2pv->entry_count must hold. */
void
bow_wi2pv_write_entry (bow_wi2pv *wi2pv, int wi)
{
  if (wi >= wi2pv->next_word) /* if entry not yet on disk */
    {
      fseek (wi2pv->inc_fp, wi2pv->entry_start + wi2pv->next_word * sizeof (bow_pv), SEEK_SET);
      while (wi > wi2pv->next_word++) { 
	bow_pv_write (&(wi2pv->entry[wi2pv->next_word - 1]), wi2pv->inc_fp);
      }
    }
  else 
    {
      fseek (wi2pv->inc_fp, wi2pv->entry_start + wi * sizeof (bow_pv), SEEK_SET);
    }


  bow_pv_write (&(wi2pv->entry[wi]), wi2pv->inc_fp);
  fflush (wi2pv->inc_fp);
  fflush (wi2pv->fp); /* Is this necessary? */
}

/* Write the whole thing out to disk */
void
bow_wi2pv_write (bow_wi2pv *wi2pv)
{
  int wi;

  /* write the header once to fix the
     start-position of the entries */
  bow_wi2pv_write_header(wi2pv);
  /* write all the entries. 
     XXX: This will probably break if there are gaps! */
  for(wi = 0; wi < wi2pv->num_words; wi++) 
      bow_wi2pv_write_entry(wi2pv, wi);
  /* write it again to update num_words,
     next_word, et alia */
  bow_wi2pv_write_header(wi2pv);
}

bow_wi2pv *
bow_wi2pv_new_from_filename (const char *filename)
{
  FILE *fp;
  bow_wi2pv *wi2pv;
  int wi;
  char *foo;
  char pv_pathname[BOW_MAX_WORD_LENGTH];

  fp = bow_fopen (filename, "rb+");
  
  wi2pv = bow_malloc (sizeof (bow_wi2pv));
  bow_fread_int (&(wi2pv->entry_count), fp);
  bow_fread_int (&(wi2pv->next_word), fp);
  bow_fread_int (&(wi2pv->num_words), fp);
  bow_fread_string (&foo, fp);
  wi2pv->entry_start = ftell (fp);
  wi2pv->pv_filename = foo;
  wi2pv->entry = bow_malloc (wi2pv->entry_count * sizeof (bow_pv));
  for (wi = 0; wi < wi2pv->next_word; wi++)
    bow_pv_read (&(wi2pv->entry[wi]), fp);
  for (; wi < wi2pv->entry_count; wi++) 
    wi2pv->entry[wi].count = -1;

  wi2pv->inc_fp = fp;

  /* Open the PV_FILENAME for reading and writing, but do not truncate */
  if (strchr (wi2pv->pv_filename, '/'))
    sprintf (pv_pathname, "%s/pv", bow_data_dirname);
  else
    sprintf (pv_pathname, "%s/%s", bow_data_dirname, wi2pv->pv_filename);
  wi2pv->fp = bow_fopen (pv_pathname, "rb+");
  return wi2pv;
}

/* Close and re-open WI2PV's FILE* for its PV's.  This should be done
   after a fork(), since the parent and child will share lseek()
   positions otherwise. */
void
bow_wi2pv_reopen_pv (bow_wi2pv *wi2pv)
{
  char pv_pathname[BOW_MAX_WORD_LENGTH];

  fclose (wi2pv->fp);
  if (strchr (wi2pv->pv_filename, '/'))
    sprintf (pv_pathname, "%s/pv", bow_data_dirname);
  else
    sprintf (pv_pathname, "%s/%s", bow_data_dirname, wi2pv->pv_filename);
  wi2pv->fp = bow_fopen (pv_pathname, "rb");
}

void
bow_wi2pv_print_stats (bow_wi2pv *wi2pv)
{
  int wi, i;
  int histogram_size = 100;
  int word_count_histogram[histogram_size];
  int count;

  for (i = 0; i < histogram_size; i++)
    word_count_histogram[i] = 0;
  for (wi = 0; wi < wi2pv->entry_count; wi++)
    {
      count = bow_wi2pv_wi_count (wi2pv, wi);
      if (wi2pv->entry[wi].count >= 0)
	{
	  printf ("%10d %-30s \n", count, bow_int2word (wi));
	  if (count < histogram_size-1)
	    word_count_histogram[count]++;
	  else
	    word_count_histogram[histogram_size-1]++;
	}
    }
  /* Print the number of unique words associated with each occurrence count */
  for (i = 1; i < histogram_size; i++)
    printf ("histogram %5d %10d\n", i, word_count_histogram[i]);
}
