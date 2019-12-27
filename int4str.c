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
/* All elements of the formula must be of type unsigned to get unsigned arithmetic */
#define REHASH(map, id, h) (((id) + (h)) % (map)->str_hash_size)

/* This function is defined in bow/primelist.c. */
extern int _bow_primedouble (unsigned n);

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
  //map->hash_id_array = bow_malloc (map->str_array_size * sizeof (unsigned));
  map->str_array_length = 0;
  map->str_hash_size = _bow_primedouble (map->str_array_size);
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
static unsigned
_str2id (const unsigned char *s)
{
  unsigned h;

#if 0
  static const unsigned bigprime = 1931045213L;
  static const unsigned copying = 0x1111U;
  unsigned c = 0;
  unsigned x;
  int i;
#endif

  for (h = 0; *s; s++)
    h = 131*h + *s;

#if 0
  while (*s++ != '\0')
    {
      x = (*s) * copying;
      for (i = 0; i < c; i++)
	x = (x & 0x1) ? ((x << 1) | 0x1) : (x << 1);
      h ^= x;
      c++;
    }
#endif

#if 0
  while (*s++ != '\0')
    h ^= (((unsigned)*s) * copying) << (c++ % 7);
#endif

#if 0
  while (*s++ != '\0')
    h ^= (((unsigned)*s) * bigprime) << (c++ % 7);
    //h ^= (((unsigned)*s) * bigprime);
#endif

  /* Never return 0, otherwise _str_hash_add() will infinite-loop */
  //assert (h != 0);
  //if (h == 0) h = 1;

  return h;
}

#if 0
int my_strcmp (const char *s1, const char *s2)
{
  return strcmp (s1, s2);
}
#endif

#if 0
/* Look up STRING in the MAP->STR_HASH; or, more precisely: Return the
   index to the location in MAP->STR_HASH that contains the index to
   the location in MAP->STR_ARRAY that contains a (char*) with
   contents matching STRING.  The second argument ID must be the value
   returned by _STR2ID(STRING).  If the string was found, then the
   return value will be different from HASH_EMPTY, and *STRDIFF will
   be zero. */
static int
old_str_hash_lookup (bow_int4str *map, const char *string, unsigned id, int *strdiffp)
{
  unsigned h;
  int firsth = -1;		/* the first value of H */

  assert (map->str_hash[0] >= -1);
  //assert (id == _str2id (string));
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



static int
old2_str_hash_lookup (bow_int4str *map, const char *string, unsigned id, int *strdiffp)
{
  unsigned h;
  int num_hops = 0;
  register int local_strdiffp = 1;
  static int num_hops_sum = 0;
  static int num_calls = 0;
  static int max_num_hops = 0;
  /* Make INCR be relatively prime to the STR_HASH_SIZE */
  unsigned incr = 1 + (id % (map->str_hash_size - 1));

  //assert (map->str_hash[0] >= -1);
  //assert (id == _str2id (string));
  /* Keep looking at STR_HASH locations until we either (1) find the
     string, or (2) find an empty spot, or (3) "modulo-loop" around to
     the same spot we began the search.  In the third case, we know
     that we will have to grow the STR_HASH before we can add the
     string corresponding to ID.  We don't need to check to stop when
     H == the first H because we are now growing the table whenever it gets
     half full, and so we can never loop around completely.  */
  //assert (h > 0);
  num_hops++;
  for (h = HASH(map, id);
       map->str_hash[h] != HASH_EMPTY 
	 && (//id != map->hash_id_array[map->str_hash[h]] ||
	     (local_strdiffp = strcmp (string, map->str_array[map->str_hash[h]])));
       h = (h + incr) % map->str_hash_size)
    {
      num_hops++;
      //assert (h > 0);
    }
  *strdiffp = local_strdiffp;
#if 1
  num_calls++;
  num_hops_sum += num_hops;
  if (num_hops > max_num_hops)
    max_num_hops = num_hops;
  if (num_calls % 10000 == 0)
    {
      bow_verbosify (bow_progress,
		     "0x%x Average num hops = %f\n "
		     "Max num hops = %d\n "
		     "Hash size = %d  Num entries = %d\n",
		     map,
		     ((float)num_hops_sum)/num_calls, max_num_hops,
		     map->str_hash_size, map->str_array_length);
      num_calls = num_hops_sum = max_num_hops = 0;
    }
#endif
  return h;
}

static int
old3_str_hash_lookup (bow_int4str *map, const char *string, unsigned id, int *strdiffp)
{
  register unsigned h;
  /* Make INCR be relatively prime to the STR_HASH_SIZE */
  unsigned incr = 1 + (id % (map->str_hash_size - 1));

  /* Keep looking at STR_HASH locations until we either (1) find the
     string, or (2) find an empty spot.  We don't need to check to
     stop when H == the first H because we are now growing the table
     whenever it gets half full, and so we can never loop around
     completely.  */
  for (h = id % map->str_hash_size;
       map->str_hash[h] != HASH_EMPTY 
	 && (*strdiffp = strcmp (string, map->str_array[map->str_hash[h]]));
       h = (h + incr) % map->str_hash_size)
    ;
  return h;
}
#endif /* 0 */

int
_bow_str_hash_lookup (bow_int4str *map, const char *string, unsigned id, 
		  int *strdiffp)
{
  register unsigned h;
  register const char **str_array = map->str_array;
  
  *strdiffp = 1;
  h = id % map->str_hash_size;
  if (map->str_hash[h] == HASH_EMPTY
      || (!(*strdiffp = strcmp (string, str_array[map->str_hash[h]]))))
    return h;

  {
    unsigned incr = 1 + (id % (map->str_hash_size - 1));
    for (;;)
      {
	/* Incrementing INCR is not among the recommendation of
	   Corman, Lieiserson & Rivest, page 236, but without this,
	   the problem below happens.  I'm mystified. */
	//h = (h + incr++) % map->str_hash_size;
	h = (h + incr) % map->str_hash_size;
	if (map->str_hash[h] == HASH_EMPTY
	    || (!(*strdiffp =strcmp(string, str_array[map->str_hash[h]]))))
	  return h;
      }
  }
}

/* Problem:
56911565 65517 %p
42809
1-p
42808
56911565 65516 %p
43677
42809 43678 + 65517 %p
20970
43678 + 65517 %p
64648
43678 + 65517 %p
42809
43678 + 65517 %p
20970

65517 2*p
131034
43678 3*p
131034

*/

int
_bow_str_hash_lookup2 (bow_int4str *map, const char *string, unsigned id)
{
  register unsigned h;
  register const char **str_array = map->str_array;

#define HOP_REPORTING 0
#if HOP_REPORTING
  static int num_hops = 0;
  static int num_calls = 0;
  static int hops_count = 0;
  static int max_hops_count = 0;
  static int max2_hops_count = 0;
  if (num_calls % 100000 == 0)
    {
      fprintf (stderr, "num_hops=%d, num_calls=%d, ratio=%f, max_hops=%d,%d\n",
	       num_hops, num_calls, ((float)num_hops)/num_calls, 
	       max_hops_count, max2_hops_count);
      max_hops_count, hops_count = num_hops = num_calls = 0;
    }
#endif

#if HOP_REPORTING
  num_calls++;
  hops_count = 0;
#endif
  h = id % map->str_hash_size;
  if (map->str_hash[h] == HASH_EMPTY
      || (! strcmp (string, str_array[map->str_hash[h]])))
    return h;

  {
    unsigned incr = 1 + (id % (map->str_hash_size - 1));
    for (;;)
      {
#if HOP_REPORTING
	num_hops++;
	hops_count++;
	if (hops_count > max_hops_count)
	  max_hops_count = hops_count;
	if (hops_count != max_hops_count && hops_count > max2_hops_count)
	  max2_hops_count = hops_count;
#endif
	h = (h + incr) % map->str_hash_size;
	if (map->str_hash[h] == HASH_EMPTY
	    || (! strcmp(string, str_array[map->str_hash[h]])))
	  return h;
      }
  }
}


/* Given the char-pointer STRING, return its integer index.  If STRING
   is not yet in the mapping, return -1. */
int
bow_str2int_no_add (bow_int4str *map, const char *string)
{
  int strdiff;
  int h;

  h = _bow_str_hash_lookup (map, string, _str2id (string), &strdiff);
  if (strdiff == 0)
    return map->str_hash[h];
  return -1;
}

/* Add the char* STRING to the string hash table MAP->STR_HASH.  The
   second argument H must be the value returned by
   _BOW_STR_HASH_LOOKUP(STRING).  Don't call this function with a string
   that has already been added to the hashtable!  The duplicate index
   would get added, and cause many bugs. */
static void
_str_hash_add (bow_int4str *map, 
	       const char *string, unsigned id, int h, int str_array_index)
{
  assert (h >= 0);
  assert (str_array_index >= 0 && str_array_index < map->str_array_length);
  /* if (map->str_hash[h] == HASH_EMPTY) */
  if (map->str_hash_size > map->str_array_length * 2)
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
      map->str_hash_size = _bow_primedouble (old_str_hash_size);
#if 0
      bow_verbosify (bow_progress,
		     "Growing hash table to %d\n", map->str_hash_size);
#endif
      map->str_hash = bow_malloc (map->str_hash_size * sizeof(int));
      for (i = map->str_hash_size, entry = map->str_hash; i > 0; i--, entry++)
	*entry = HASH_EMPTY;

      /* Fill the new str_hash with the values from the old str_hash. */
      {
	for (i = 0; i < old_str_hash_size; i++)
	  if (old_str_hash[i] != HASH_EMPTY)
	    {
	      const char *old_string = map->str_array[old_str_hash[i]];
	      unsigned old_id = _str2id (old_string);
	      _str_hash_add (map, old_string, old_id,
			     _bow_str_hash_lookup (map, old_string, old_id, 
						   &sd),
			     old_str_hash[i]);
	      assert (sd);      /* All these strings should be unique! */
	    }
      }

      /* Free the old hash memory */
      bow_free (old_str_hash);

      /* Finally, add new string. */
      _str_hash_add (map, string, id,
		     _bow_str_hash_lookup (map, string, id, &sd),
		     str_array_index);
    }
}


/* Just like BOW_STR2INT, except assume that the STRING's ID has
   already been calculated. */
int
_bow_str2int (bow_int4str *map, const char *string, unsigned id)
{
  int h;			/* ID, truncated to fit in STR_HASH */

  /* Search STR_HASH for the string, or an empty space.  */
  h = _bow_str_hash_lookup2 (map, string, id);
  
  if (map->str_hash[h] != HASH_EMPTY)
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
      assert (map->str_array_size < 1768448882);
      map->str_array = bow_realloc (map->str_array, 
				    map->str_array_size * sizeof (char*));
      //map->hash_id_array = bow_realloc (map->hash_id_array, 
      //map->str_array_size * sizeof (unsigned));
    }
  map->str_array[map->str_array_length] = string;
  //map->hash_id_array[map->str_array_length] = id;

  /* The STR_ARRAY has one more element in it now, so increment its length. */
  map->str_array_length++;

  /* Add it to str_hash. */
  _str_hash_add (map, string, id, h, (map->str_array_length)-1);

  /* Return the index at which it was added.  */
  return (map->str_array_length)-1;
}

/* Given the char-pointer STRING, return its integer index.  If this is 
   the first time we're seeing STRING, add it to the tables, assign
   it a new index, and return the new index. */
int
bow_str2int (bow_int4str *map, const char *string)
{
  return _bow_str2int (map, string, _str2id (string));
}

/* Create a new int-str mapping words fscanf'ed from FILE using %s. */
bow_int4str *
bow_int4str_new_from_string_file (const char *filename)
{
  FILE *fp;
  bow_int4str *map;
  static const int BUFLEN = 1024;
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

bow_int4str *
bow_int4str_new_from_fp_inc (FILE *fp)
{
  int len;
  char buf[BOW_MAX_WORD_LENGTH];
  bow_int4str *ret;

   /* Initialize mapping structures to default size. */

  ret = bow_int4str_new (0);

  while (fgets (buf, BOW_MAX_WORD_LENGTH, fp))
    {
      /* Read the string from the file. */
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
