/* An array of word indices, each associated with a floating point number. 
   Useful for lists of words by information gain, etc. */

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
#include <stdlib.h>		/* for qsort() */

/* Create a new, empty array of word/score entries, with CAPACITY entries. */
bow_wa *
bow_wa_new (int capacity)
{
  bow_wa *ret;

  if (capacity == 0)
    capacity = 8;		/* default */
  ret = bow_malloc (sizeof (bow_wa));
  ret->entry = bow_malloc (sizeof (bow_ws) * capacity);
  ret->size = capacity;
  ret->length = 0;
  return ret;
}

/* Add a new word and score to the array */
int
bow_wa_append (bow_wa *wa, int wi, float score)
{
  if (wa->length + 1 >= wa->size)
    {
      wa->size *= 2;
      wa->entry = bow_realloc (wa->entry, wa->size * sizeof (bow_ws));
    }
  wa->entry[wa->length].wi = wi;
  wa->entry[wa->length].weight = score;
  wa->length++;
  assert (wa->length < wa->size);
  return wa->length;
}

static int
compare_wa_high_first (const void *ws1, const void *ws2)
{
  if (((bow_ws*)ws1)->weight > ((bow_ws*)ws2)->weight)
    return -1;
  else if (((bow_ws*)ws1)->weight == ((bow_ws*)ws2)->weight)
    return 0;
  else
    return 1;
}

static int
compare_wa_high_last (const void *ws1, const void *ws2)
{
  if (((bow_ws*)ws1)->weight < ((bow_ws*)ws2)->weight)
    return -1;
  else if (((bow_ws*)ws1)->weight == ((bow_ws*)ws2)->weight)
    return 0;
  else
    return 1;
}

/* Sort the word array. */
void
bow_wa_sort (bow_wa *wa)
{
  qsort (wa->entry, wa->length, sizeof (bow_ws), compare_wa_high_first);
}

void
bow_wa_sort_reverse (bow_wa *wa)
{
  qsort (wa->entry, wa->length, sizeof (bow_ws), compare_wa_high_last);
}

/* Print the first N entries of the word array WA to stream FP. */
void
bow_wa_fprintf (bow_wa *wa, FILE *fp, int n)
{
  int i;

  if (n > wa->length || n < 0)
    n = wa->length;
  for (i = 0; i < n; i++)
    fprintf (fp, "%20.10f %s\n",
	     wa->entry[i].weight,
	     bow_int2word (wa->entry[i].wi));
}

/* Free the word array */
void
bow_wa_free (bow_wa *wa)
{
  bow_free (wa->entry);
  bow_free (wa);
}
