/* A clustering front-end to libbow, unfinished and non-functional. */

/* Copyright (C) 1997 Andrew McCallum and Rahul Sukthankar

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>
   and Rahul Sukthankar <rahuls@jprc.com>

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

typedef struct _dendogram_node
{
  struct _dendogram_node *child1;
  struct _dendogram_node *child2;
  int document_index;		/* This is -1 for all non-leaf nodes */
} dendogram_node;

typedef struct _crossbow_doc
{
  const char *filename;
  bow_wv *wv;
} crossbow_doc;

void crossbow_doc_free (crossbow_doc *doc)
{
  free ((char*)doc->filename);
  bow_wv_free (doc->wv);
}

double
crossbow_wv_distance (bow_wv *wv1, bow_wv *wv2)
{
  /* e.g. wv1->entry[0].weight; */
  return 0.0;
}

int
main (int argc, char *argv[])
{
  /* argv[i] a directory name containing all files to be clustered */

  bow_array *documents;
  float *wi2df;
  int di, wvi, di1, di2;
  double dist;

  int add_filename_to_documents (const char *filename, void *context)
    {
      FILE *fp;
      crossbow_doc doc;
      
      fp = bow_fopen (filename, "r");
      doc.wv = bow_wv_new_from_text_fp (fp);
      fclose (fp);
      if (doc.wv == NULL)
	return 0;
      doc.filename = strdup (filename);
      bow_array_append (documents, &doc);
      bow_verbosify (bow_progress,
		     "\b\b\b\b\b\b%6d", documents->length);
      return 1;
    }

  documents = bow_array_new (32, sizeof (bow_cdoc), crossbow_doc_free);
  bow_verbosify (bow_progress, "Indexing documents...        ");
  bow_map_filenames_from_dir (add_filename_to_documents, NULL, 
			      argv[1], "");
  bow_verbosify (bow_progress, "\n");

  /* Set weights in the word vectors. */
  /* Get the Document Frequencies. */
  wi2df = bow_malloc (sizeof (float) * bow_num_words ());
  for (di = 0; di < documents->length; di++)
    {
      crossbow_doc *doc = bow_array_entry_at_index (documents, di);
      for (wvi = 0; wvi < doc->wv->num_entries; wvi++)
	{
	  (wi2df[doc->wv->entry[wvi].wi])++;
	}
    }
  /* Set the weights. */
  for (di = 0; di < documents->length; di++)
    {
      crossbow_doc *doc = bow_array_entry_at_index (documents, di);
      double normalizer = 0;
      for (wvi = 0; wvi < doc->wv->num_entries; wvi++)
	{
	  doc->wv->entry[wvi].weight = 
	    (doc->wv->entry[wvi].count / wi2df[doc->wv->entry[wvi].wi]);
	  normalizer += 
	    (doc->wv->entry[wvi].weight * doc->wv->entry[wvi].weight);
	}
      normalizer = 1.0 / sqrt (normalizer);
      for (wvi = 0; wvi < doc->wv->num_entries; wvi++)
	doc->wv->entry[wvi].weight *= normalizer;
    }

  /* Loop over all documents, and calculate for each distance to all 
     other documents, put the results in a 2d array */
  for (di1 = 0; di1 < documents->length; di1++)
    {
      crossbow_doc *crossbow_doc1 = 
	bow_array_entry_at_index (documents, di1);
      for (di2 = di+1; di2 < documents->length; di2++)
	{
	  crossbow_doc *crossbow_doc2 =
	    bow_array_entry_at_index (documents, di1);
	  /* Calculate the distance between the two word vectors. */
	  dist = crossbow_wv_distance (crossbow_doc1->wv, crossbow_doc2->wv);
	}
    }

  /* Run the clustering algorithm. */

  /* Print the results in ASCII with space-indentation. */

  exit (0);
}
