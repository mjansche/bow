/* Implementation of a one-to-one mapping of string->int, and int->string. */

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
#include <string.h>
#include <assert.h>

/* The magic-string written at the beginning of archive files, so that
   we can verify we are in the right place for when reading. */
#define HEADER_STRING "bow_int4str\n"

/* The default initial size of map->STR_ARRAY, unless otherwise requested
   by calling bow_int4str_initialize */
#define DEFAULT_INITIAL_CAPACITY 1024

/* The value of map->STR_HASH entries that are emtpy. */
#define HASH_EMPTY -1

/* Returns an initial index for ID at which to begin searching for an entry. */
#define HASH(map, id) ((id) % (map)->str_hash_size)

/* Returns subsequent indices for ID, given the previous one. */
#define REHASH(map, id, h) (((id) + (h)) % (map)->str_hash_size)

/* This function is defined in bow/primes.c. */
extern int _bow_nextprime (unsigned n);

/* Initialize the string->int and int->string map.  The parameter
   CAPACITY is used as a hint about the number of words to expect; if
   you don't know or don't care about a CAPACITY value, pass 0, and a
   default value will be used. */
void
bow_int4str_init (bow_int4str *map, int capacity)
{
  int i;

  if (capacity == 0)
    capacity = DEFAULT_INITIAL_CAPACITY;
  map->str_array_size = capacity;
  map->str_array = bow_malloc (map->str_array_size * sizeof (char*));
  map->str_array_length = 0;
  map->str_hash_size = _bow_nextprime (map->str_array_size * 2);
  map->str_hash = bow_malloc (map->str_hash_size * sizeof (int));
  for (i = 0; i < map->str_hash_size; i++)
    map->str_hash[i] = HASH_EMPTY;
}

/* Allocate, initialize and return a new int/string mapping structure.
   The parameter CAPACITY is used as a hint about the number of words
   to expect; if you don't know or don't care about a CAPACITY value,
   pass 0, and a default value will be used. */
bow_int4str *
bow_int4str_new (int capacity)
{
  bow_int4str *ret;
  ret = bow_malloc (sizeof (bow_int4str));
  bow_int4str_init (ret, capacity);
  return ret;
}

/* Return the string corresponding to the integer INDEX. */
const char *
bow_int2str (bow_int4str *map, int index)
{
  assert (index < map->str_array_length);
  return map->str_array[index];
}


/* Extract and return a (non-unique) integer `id' from string S. */
static int
_str2id (const char *s)
{
  int h = 0;
  int c = 0;

  while (*s != '\0')
    {
      h ^= *s << c;
      s++;
      c++;
    }
  if (h == 0)
    {
      /* Never return 0, otherwise _str_hash_add() will infinite-loop */
      h = 1;
    }
  else
    {
      if (h < 0)
	h = -h;
      /* If return value is too big then REHASH() can make it go negative,
	 so here we use modulo to keep it a little small. */
      h = h % (INT_MAX / 4);
    }
  return h;
}


/* Look up STRING in the MAP->STR_HASH; or, more precisely: Return the
   index to the location in MAP->STR_HASH that contains the index to
   the location in MAP->STR_ARRAY that contains a (char*) with
   contents matching STRING.  The second argument ID must be the value
   returned by _STR2ID(STRING).  If the string was found, then the
   return value will be different from HASH_EMPTY, and *STRDIFF will
   be zero. */
static int
_str_hash_lookup (bow_int4str *map, const char *string, int id, int *strdiffp)
{
  int h;
  int firsth = -1;		/* the first value of H */

  assert (id >= 0);
  assert (map->str_hash[0] >= -1);
  assert (id == _str2id (string));
  /* Keep looking at STR_HASH locations until we either (1) find the
     string, or (2) find an empty spot, or (3) "modulo-loop" around to
     the same spot we began the search.  In the third case, we know
     that we will have to grow the STR_HASH before we can add the
     string corresponding to ID. */
  *strdiffp = 1;
  for (h = HASH(map, id);
       h != firsth
	 && map->str_hash[h] != HASH_EMPTY 
	 && (*strdiffp = strcmp (string, map->str_array[map->str_hash[h]]));
       h = REHASH(map, id, h))
    {
      assert (h >= 0);
      if (firsth == -1)
	firsth = h;
    }
  return h;
}

/* Given the char-pointer STRING, return its integer index.  If STRING
   is not yet in the mapping, return -1. */
int
bow_str2int_no_add (bow_int4str *map, const char *string)
{
  int strdiff;
  int h;

  h = _str_hash_lookup (map, string, _str2id (string), &strdiff);
  if (strdiff == 0)
    return map->str_hash[h];
  return -1;
}

/* Add the char* STRING to the string hash table MAP->STR_HASH.  The
   second argument H must be the value returned by
   _STR_HASH_LOOKUP(STRING).  Don't call this function with a string
   that has already been added to the hashtable!  The duplicate index
   would get added, and cause many bugs. */
static void
_str_hash_add (bow_int4str *map, 
	       const char *string, int id, int h, int str_array_index)
{
  assert (h >= 0);
  assert (str_array_index >= 0 && str_array_index < map->str_array_length);
  if (map->str_hash[h] == HASH_EMPTY)
    {
      /* str_hash doesn't have to grow; just drop it in place.
	 STR_ARRAY_INDEX is the index at which we can find STRING in
	 the STR_ARRAY. */
      map->str_hash[h] = str_array_index;
    }
  else
    {
      /* str_hash must grow in order to accomodate new entry. */
      int sd;

      int i;
      int *old_str_hash, *entry;
      int old_str_hash_size;

      /* Create a new, empty str_hash. */
      old_str_hash = map->str_hash;
      old_str_hash_size = map->str_hash_size;
      map->str_hash_size = _bow_nextprime (2 * old_str_hash_size);
      map->str_hash = bow_malloc (map->str_hash_size * sizeof(int));
      for (i = map->str_hash_size, entry = map->str_hash; i > 0; i--, entry++)
	*entry = HASH_EMPTY;

      /* Fill the new str_hash with the values from the old str_hash. */
      {
	for (i = 0; i < old_str_hash_size; i++)
	  if (old_str_hash[i] != HASH_EMPTY)
	    {
	      const char *old_string = map->str_array[old_str_hash[i]];
	      int old_id = _str2id (old_string);
	      _str_hash_add (map, old_string, old_id,
			     _str_hash_lookup (map, old_string, old_id, &sd),
			     old_str_hash[i]);
	      assert (sd);      /* All these strings should be unique! */
	    }
      }

      /* Free the old hash memory */
      bow_free (old_str_hash);

      /* Finally, add new string. */
      _str_hash_add (map, string, id,
		     _str_hash_lookup (map, string, id, &sd),
		     str_array_index);
    }
}


/* Given the char-pointer STRING, return its integer index.  If this is 
   the first time we're seeing STRING, add it to the tables, assign
   it a new index, and return the new index. */
int
bow_str2int (bow_int4str *map, const char *string)
{
  int id;			/* the integer extracted from STRING */
  int h;			/* ID, truncated to fit in STR_HASH */
  int strdiff;			/* gets 0 if we found STRING in STR_HASH */

  id = _str2id (string);

  /* Search STR_HASH for the string, or an empty space.  */
  h = _str_hash_lookup (map, string, id, &strdiff);
  
  if (!strdiff)
    /* Found the string; return its index. */
    return map->str_hash[h];

  /* Didn't find the string in our mapping, so add it. */

  /* Make our own malloc()'ed copy of it. */
  string = strdup (string);
  if (!string)
    bow_error ("Memory exhausted.");

  /* Add it to str_array. */
  if (map->str_array_length > map->str_array_size-2)
    {
      /* str_array must grow in order to accomodate new entry. */
      map->str_array_size *= 2;
      map->str_array = bow_realloc (map->str_array, 
				    map->str_array_size * sizeof (char*));
    }
  map->str_array[map->str_array_length] = string;

  /* The STR_ARRAY has one more element in it now, so increment its length. */
  map->str_array_length++;

  /* Add it to str_hash. */
  _str_hash_add (map, string, id, h, (map->str_array_length)-1);

  /* Return the index at which it was added.  */
  return (map->str_array_length)-1;
}

/* Create a new int-str mapping words fscanf'ed from FILE using %s. */
bow_int4str *
bow_int4str_new_from_string_file (const char *filename)
{
  FILE *fp;
  bow_int4str *map;
  const static int BUFLEN = 1024;
  char buf[BUFLEN];
  int reading_numbers = 0;

  map = bow_int4str_new (0);

  fp = bow_fopen (filename, "r");

  while (fscanf (fp, "%s", buf) == 1)
    {
      assert (strlen (buf) < BUFLEN);
      if (reading_numbers == -1)
	{
	  /* Say that we are WI reading numbers instead of word strings
	     if the first word consists of nothing but digits. */
	  if (strspn (buf, "0123456789") == strlen (buf))
	    {
	      reading_numbers = 1;
	      bow_verbosify (bow_progress, 
			     "Reading words from file `%s' as indices\n",
			     filename);
	    }
	  else
	    reading_numbers = 0;
	}
      if (reading_numbers)
	bow_str2int (map, bow_int2word (atoi (buf)));
      else
	bow_str2int (map, buf);
    }
  fclose (fp);
  return map;
}

/* Create a new int-str mapping by lexing words from FILE. */
bow_int4str *
bow_int4str_new_from_text_file (const char *filename)
{
  bow_int4str *map;
  FILE *fp;
  int text_document_count;
  char word[BOW_MAX_WORD_LENGTH];
  int wi;
  bow_lex *lex;

  map = bow_int4str_new (0);
  text_document_count = 0;
  
  fp = bow_fopen (filename, "r");
  if (bow_fp_is_text (fp))
    {
      /* Loop once for each document in this file. */
      while ((lex = bow_default_lexer->open_text_fp
	      (bow_default_lexer, fp, filename)))
	{
	  /* Loop once for each lexical token in this document. */
	  while (bow_default_lexer->get_word (bow_default_lexer, 
					      lex, word, 
					      BOW_MAX_WORD_LENGTH))
	    {
	      /* Increment the word's occurrence count. */
	      wi = bow_str2int (map, word);
	      if (wi >= 0)
		{
		  /* Show total word count */
		  bow_verbosify (bow_progress,
				 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
				 "%6d : %6d", 
				 text_document_count, wi);
		}
	    }
	  bow_default_lexer->close (bow_default_lexer, lex);
	  text_document_count++;
	}
    }
  fclose (fp);
  return map;
}


/* Write the int-str mapping to file-pointer FP. */
void
bow_int4str_write (bow_int4str *map, FILE *fp)
{
  int i;

  fprintf (fp, HEADER_STRING);
  fprintf (fp, "%d\n", map->str_array_length);
  for (i = 0; i < map->str_array_length; i++)
    {
      if (strchr (map->str_array[i], '\n') != 0)
	bow_error ("Not allowed to write string containing a newline");
      fprintf (fp, "%s\n", map->str_array[i]);
    }
}


bow_int4str *
bow_int4str_new_from_fp (FILE *fp)
{
  const char *magic = HEADER_STRING;
  int num_words, i;
  int len;
  char buf[BOW_MAX_WORD_LENGTH];
  bow_int4str *ret;

  /* Make sure the FP is positioned corrected to read a bow_int4str.
     Look for the magic string we are expecting. */
  while (*magic)
    {
      if (*magic != fgetc (fp))
	bow_error ("Proper header not found in file.");
      magic++;
    }

  /* Get the number of words in the list, and initialize mapping
     structures large enough. */

  fscanf (fp, "%d\n", &num_words);
  ret = bow_int4str_new (num_words);

  for (i = 0; i < num_words; i++)
    {
      /* Read the string from the file. */
      if (fgets (buf, BOW_MAX_WORD_LENGTH, fp) == 0)
	bow_error ("Error reading data file.");
      len = strlen (buf);
      if (buf[len-1] == '\n')
	buf[len-1] = '\0';
      /* Add it to the mappings. */
      bow_str2int (ret, buf);
    }
  return ret;
}

/* Return a new int-str mapping, created by reading FILENAME. */
bow_int4str *
bow_int4str_new_from_file (const char *filename)
{
  FILE *fp;
  bow_int4str *ret;

  fp = fopen (filename, "r");
  if (!fp)
    bow_error ("Couldn't open file `%s' for reading\n", filename);
  ret = bow_int4str_new_from_fp (fp);
  fclose (fp);
  return ret;
}

void
bow_int4str_free_contents (bow_int4str *map)
{
  bow_free (map->str_array);
  bow_free (map->str_hash);
}

void
bow_int4str_free (bow_int4str *map)
{
  bow_int4str_free_contents (map);
  bow_free (map);
}
