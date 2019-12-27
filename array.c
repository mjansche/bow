/* Implementation of arrays that can grow. */

/* Copyright (C) 1997, 1998 Andrew McCallum

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
#include <assert.h>

/* The magic-string written at the beginning of archive files, so that
   we can verify we are in the right place for when reading. */
#define HEADER_STRING "bow_array\n"

int bow_array_default_capacity = 1024;
int bow_array_default_growth_factor = 2;

/* Allocate, initialize and return a new array structure. */
bow_array *
bow_array_new (int capacity, int entry_size, void (*free_func)())
{
  bow_array *ret;

  ret = bow_malloc (sizeof (bow_array));
  bow_array_init (ret, capacity, entry_size, free_func);
  return ret;
}

/* Initialize an already allocated array structure. */
void
bow_array_init (bow_array *array, int capacity, 
		int entry_size, void (*free_func)())
{
  if (capacity == 0)
    capacity = bow_array_default_capacity;
  array->size = capacity;
  array->length = 0;
  array->entry_size = entry_size;
  array->free_func = free_func;
  array->growth_factor = 2;
  array->entries = bow_malloc (array->size * entry_size);
}

#define ADDR_ENTRY_AT_INDEX(ARRAY, INDEX)	\
(((char*)((ARRAY)->entries)) + (INDEX * (ARRAY)->entry_size))

/* Append an entry to the array.  Return its index. */
int
bow_array_append (bow_array *array, void *entry)
{
  if (array->length >= array->size)
    {
      /* The array must grow to accommodate the new entry. */
      array->size *= array->growth_factor;
      array->size /= array->growth_factor - 1;
      array->entries = bow_realloc (array->entries, 
				    array->size * array->entry_size);
    }
  memcpy (ADDR_ENTRY_AT_INDEX (array, array->length), entry,
	  array->entry_size);
  return (array->length)++;
}

/* Return what will be the index of the next entry to be appended */
int
bow_array_next_index (bow_array *array)
{
  return array->length;
}

/* Return a pointer to the array entry at index INDEX. */
void *
bow_array_entry_at_index (bow_array *array, int index)
{
  assert (index < array->length);
  return ADDR_ENTRY_AT_INDEX (array, index);
}

/* Write the array ARRAY to the file-pointer FP, using the function
   WRITE_FUNC to write each of the entries in ARRAY. */
void
bow_array_write (bow_array *array, int (*write_func)(void*,FILE*), FILE *fp)
{
  int i;

  fprintf (fp, HEADER_STRING);
  bow_fwrite_int (array->length, fp);
  bow_fwrite_int (array->entry_size, fp);
  for (i = 0; i < array->length; i++)
    (*write_func) (ADDR_ENTRY_AT_INDEX (array, i), fp);
}

/* Return a new array, created by reading file-pointer FP, and using
   the function READ_FUNC to read each of the array entries.  The
   returned array will have entry-freeing-function FREE_FUNC. */
bow_array *
bow_array_new_from_data_fp (int (*read_func)(void*,FILE*), 
			    void (*free_func)(),
			    FILE *fp)
{
  int length;
  int entry_size;
  bow_array *ret;
  int i;

  /* Make sure the FP is positioned corrected to read a bow_int4str.
     Look for the magic string we are expecting. */
  {
    const char *magic = HEADER_STRING;
    while (*magic)
      {
	if (*magic != fgetc (fp))
	  bow_error ("Proper header not found in file.");
	magic++;
      }
  }

  bow_fread_int (&length, fp);
  bow_fread_int (&entry_size, fp);
  ret = bow_array_new (length, entry_size, free_func);
  for (i = 0; i < length; i++)
    (*read_func) (ADDR_ENTRY_AT_INDEX (ret, i), fp);
  ret->length = length;
  return ret;
}

/* Free the memory held by the array ARRAY. */
void
bow_array_free (bow_array *array)
{
  if (array->free_func)
    {
      while ((array->length)--)
	(*(array->free_func)) (ADDR_ENTRY_AT_INDEX (array, array->length));
    }
  bow_free (array->entries);
  bow_free (array);
}
