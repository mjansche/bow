/* libbow.h - public declarations for the Bag-Of-Words Library, libbow.
   Copyright (C) 1996, 1997 Andrew McCallum

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>
   Created: September 1996

   This file is part of the Bag-Of-Words Library, `libbow'.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/ 

/* Pronounciation guide: "libbow" rhymes with "lib-low", not "lib-cow". */

#ifndef __bow_h_INCLUDE
#define __bow_h_INCLUDE

/* These next two macros are automatically maintained by the Makefile,
   in conjunction with the file ./Version. */
#define BOW_MAJOR_VERSION 0
#define BOW_MINOR_VERSION 5
#define BOW_VERSION BOW_MAJOR_VERSION.BOW_MINOR_VERSION

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>		/* for netinet/in.h on SunOS */
#include <netinet/in.h>		/* for machine-independent byte-order */
#include <malloc.h>		/* for malloc() and friends. */
#include <stdlib.h>             /* For malloc() etc. on DEC Alpha */
#include <string.h>		/* for strlen() on DEC Alpha */
#include <limits.h>		/* for PATH_MAX */

#if !defined(MIN)
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if !defined(MAX)
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#if !PATH_MAX			/* for SunOS */
#define PATH_MAX 255
#endif

#if !defined SEEK_SET || !defined(SEEK_CUR) /* for SunOS */
#define SEEK_SET 0
#define SEEK_CUR 1
#endif


/* Lexing words from a file. */

#define BOW_MAX_WORD_LENGTH 1024

/* A structure for maintaining the context of a lexer.  (If you need
   to create a lexer that uses more context than this, define a new
   structure that includes this structure as its first element;
   BOW_LEX_GRAM, defined below is an example of this.)  */
typedef struct _bow_lex {
  char *document;
  int document_length;
  int document_position;
} bow_lex;

/* A lexer is represented by a pointer to a structure of this type. */
typedef struct _bow_lexer {
  int sizeof_lex;		/* The size of this structure */
  /* Pointers to functions for opening, closing and getting words. */
  bow_lex* (*open_text_fp) (struct _bow_lexer *self, FILE *fp);
  int (*get_word) (struct _bow_lexer *self, bow_lex *lex, 
		   char *buf, int buflen);
  void (*close) (struct _bow_lexer *self, bow_lex *lex);
  /* How to recognize the beginning and end of a document. */
  const char *document_start_pattern;
  const char *document_end_pattern;
  /* NULL pattern means don't scan forward at all.
     "" pattern means scan forward to EOF. */
} bow_lexer;

/* This is an augmented version of BOW_LEXER that works for simple,
   context-free lexers. */
typedef struct _bow_lexer_simple {
  /* The basic lexer. */
  bow_lexer lexer;
  /* Parameters of the simple, context-free lexing. */
  int (*true_to_start)(int character);          /* non-zero on char to start */
  int (*false_to_end)(int character);           /* zero on char to end */
  int (*stoplist_func)(const char *);           /* one on token in stoplist */
  int (*stem_func)(char *);	                /* modify arg by stemming */
  int case_sensitive;		                /* boolean */
  int strip_non_alphas_from_end;                /* boolean */
  int toss_words_containing_non_alphas;	        /* boolean */
  int toss_words_containing_this_many_digits;
} bow_lexer_simple;

/* Get the raw token from the document buffer by scanning forward
   until we get a start character, and filling the buffer until we get
   an ending character.  The resulting token in the buffer is
   NULL-terminated.  Return the length of the token. */
int bow_lexer_simple_get_raw_word (bow_lexer_simple *self, bow_lex *lex, 
				   char *buf, int buflen);

/* Perform all the necessary postprocessing after the initial token
   boundaries have been found: strip non-alphas from end, toss words
   containing non-alphas, toss words containing certaing many digits,
   toss words appearing in the stop list, stem the word, check the
   stoplist again, toss words of length one.  If the word is tossed,
   return zero, otherwise return the length of the word. */
int bow_lexer_simple_postprocess_word (bow_lexer_simple *self, bow_lex *lex, 
				       char *buf, int buflen);

/* Create and return a BOW_LEX, filling the document buffer from
   characters in FP, starting after the START_PATTERN, and ending with
   the END_PATTERN. */
bow_lex *bow_lexer_simple_open_text_fp (bow_lexer *self, FILE *fp);

/* Close the LEX buffer, freeing the memory held by it. */
void bow_lexer_simple_close (bow_lexer *self, bow_lex *lex);

/* Scan a single token from the LEX buffer, placing it in BUF, and
   returning the length of the token.  BUFLEN is the maximum number of
   characters that will fit in BUF.  If the token won't fit in BUF,
   an error is raised. */
int bow_lexer_simple_get_word (bow_lexer *self, bow_lex *lex, 
			       char *buf, int buflen);


/* Here are some simple, ready-to-use lexers that are implemented in
   lex-simple.c */

/* A lexer that throws out all space-delimited strings that have any
   non-alphabetical characters.  For example, the string `obtained
   from http://www.cs.cmu.edu' will result in the tokens `obtained'
   and `from', but the URL will be skipped. */
extern const bow_lexer_simple *bow_alpha_only_lexer;

/* A lexer that keeps all alphabetic strings, delimited by
   non-alphabetic characters.  For example, the string
   `http://www.cs.cmu.edu' will result in the tokens `http', `www',
   `cs', `cmu', `edu'. */
extern const bow_lexer_simple *bow_alpha_lexer;

/* A lexer that keeps all strings that begin and end with alphabetic
   characters, delimited by white-space.  For example,
   the string `http://www.cs.cmu.edu' will be a single token. */
extern const bow_lexer_simple *bow_white_lexer;

/* Some declarations for a generic indirect lexer.  See lex-indirect.c */

typedef struct _bow_lexer_indirect {
  bow_lexer lexer;
  bow_lexer *underlying_lexer;
} bow_lexer_indirect;

/* Open the underlying lexer. */
bow_lex *bow_lexer_indirect_open_text_fp (bow_lexer *self, FILE *fp);

/* Close the underlying lexer. */
void bow_lexer_indirect_close (bow_lexer *self, bow_lex *lex);


/* Some declarations for a simple N-gram lexer.  See lex-gram.c */

/* An augmented version of BOW_LEXER that provides N-grams */
typedef struct _bow_lexer_gram {
  bow_lexer_indirect indirect_lexer;
  int gram_size;
} bow_lexer_gram;

/* An augmented version of BOW_LEX that works for N-grams */
typedef struct _bow_lex_gram {
  bow_lex lex;
  int gram_size_this_time;
} bow_lex_gram;

/* A lexer that returns N-gram tokens using BOW_ALPHA_ONLY_LEXER.
   It actually returns all 1-grams, 2-grams ... N-grams, where N is 
   specified by GRAM_SIZE.  */
extern const bow_lexer_gram *bow_gram_lexer;


/* A lexer that ignores all HTML directives, ignoring all characters
   between angled brackets: < and >. */
extern const bow_lexer_indirect *bow_html_lexer;


/* The default lexer that will be used by various library functions
   like BOW_WV_NEW_FROM_TEXT_FP().  You should set this variable to
   point at whichever lexer you desire.  If you do not set it, it
   will point at bow_alpha_lexer. */
extern bow_lexer *bow_default_lexer;



/* Functions that may be useful in writing a lexer. */

/* Apply the Porter stemming algorithm to modify WORD.  Return 0 on success. */
int bow_stem_porter (char *word);

/* A function wrapper around POSIX's `isalpha' macro. */
int bow_isalpha (int character);

/* A function wrapper around POSIX's `isgraph' macro. */
int bow_isgraph (int character);

/* Return non-zero if WORD is on the stoplist. */
int bow_stoplist_present (const char *word);

/* Add to the stoplist the white-space delineated words from FILENAME.
   Return the number of words added.  If the file could not be opened,
   return -1. */
int bow_stoplist_add_from_file (const char *filename);


/* Arrays of C struct's that can grow. */

typedef struct _bow_array {
  int length;			/* number of elements in the array */
  int size;			/* number of elts for which alloc'ed space */
  int entry_size;		/* number of bytes in each entry */
  void (*free_func)(void*);	/* call this with each entry when freeing */
  int growth_factor;		/* mult, then divide by 1-this when realloc */
  void *entries;		/* the malloc'ed space for the entries */
} bow_array;

extern int bow_array_default_capacity;
extern int bow_array_default_growth_factor;

/* Allocate, initialize and return a new array structure. */
bow_array *bow_array_new (int capacity, int entry_size, void (*free_func)());

/* Initialize an already allocated array structure. */
void bow_array_init (bow_array *array, int capacity, 
		     int entry_size, void (*free_func)());

/* Append an entry to the array.  Return its index. */
int bow_array_append (bow_array *array, void *entry);

/* Return a pointer to the array entry at index INDEX. */
void *bow_array_entry_at_index (bow_array *array, int index);

/* Write the array ARRAY to the file-pointer FP, using the function
   WRITE_FUNC to write each of the entries in ARRAY. */
void bow_array_write (bow_array *array, int (*write_func)(void*,FILE*), 
		      FILE *fp);

/* Return a new array, created by reading file-pointer FP, and using
   the function READ_FUNC to read each of the array entries.  The
   returned array will have entry-freeing-function FREE_FUNC. */
bow_array *
bow_array_new_from_data_fp (int (*read_func)(void*,FILE*), 
			    void (*free_func)(),
			    FILE *fp);

/* Free the memory held by the array ARRAY. */
void bow_array_free (bow_array *array);


/* Managing int->string and string->int mappings. */

typedef struct _bow_int4str {
  const char **str_array;
  int str_array_length;
  int str_array_size;
  int *str_hash;
  int str_hash_size;
} bow_int4str;

/* Allocate, initialize and return a new int/string mapping structure.
   The parameter CAPACITY is used as a hint about the number of words
   to expect; if you don't know or don't care about a CAPACITY value,
   pass 0, and a default value will be used. */
bow_int4str *bow_int4str_new (int capacity);

/* Given a integer INDEX, return its corresponding string. */
const char *bow_int2str (bow_int4str *map, int index);

/* Given the char-pointer STRING, return its integer index.  If this is 
   the first time we're seeing STRING, add it to the mapping, assign
   it a new index, and return the new index. */
int bow_str2int (bow_int4str *map, const char *string);

/* Given the char-pointer STRING, return its integer index.  If STRING
   is not yet in the mapping, return -1. */
int bow_str2int_no_add (bow_int4str *map, const char *string);

/* Write the int-str mapping to file-pointer FP. */
void bow_int4str_write (bow_int4str *map, FILE *fp);

/* Return a new int-str mapping, created by reading file-pointer FP. */
bow_int4str *bow_int4str_new_from_fp (FILE *fp);

/* Return a new int-str mapping, created by reading FILENAME. */
bow_int4str *bow_int4str_new_from_file (const char *filename);

/* Free the memory held by the int-word mapping MAP. */
void bow_int4str_free (bow_int4str *map);



/* Arrays of C struct's that can grow.  Entries can be retrieved
   either by integer index, or by string key. */

typedef struct _bow_sarray {
  bow_array *array;
  bow_int4str *i4k;
} bow_sarray;

extern int bow_sarray_default_capacity;

/* Allocate, initialize and return a new sarray structure. */
bow_sarray *bow_sarray_new (int capacity, int entry_size, void (*free_func)());

/* Initialize a newly allocated sarray structure. */
void bow_sarray_init (bow_sarray *sa, int capacity,
		      int entry_size, void (*free_func)());

/* Append a new entry to the array.  Also make the entry accessible by
   the string KEYSTR.  Returns the index of the new entry. */
int bow_sarray_add_entry_with_keystr (bow_sarray *sa, void *entry,
				      const char *keystr);

/* Return a pointer to the entry at index INDEX. */
void *bow_sarray_entry_at_index (bow_sarray *sa, int index);

/* Return a pointer to the entry associated with string KEYSTR. */
void *bow_sarray_entry_at_keystr (bow_sarray *sa, const char *keystr);

/* Return the string KEYSTR associated with the entry at index INDEX. */
const char *bow_sarray_keystr_at_index (bow_sarray *sa, int index);

/* Return the index of the entry associated with the string KEYSTR. */
int bow_sarray_index_at_keystr (bow_sarray *sa, const char *keystr);

/* Write the sarray SARRAY to the file-pointer FP, using the function
   WRITE_FUNC to write each of the entries in SARRAY. */
void bow_sarray_write (bow_sarray *sarray, int (*write_func)(void*,FILE*), 
		       FILE *fp);

/* Return a new sarray, created by reading file-pointer FP, and using
   the function READ_FUNC to read each of the sarray entries.  The
   returned sarray will have entry-freeing-function FREE_FUNC. */
bow_sarray *bow_sarray_new_from_data_fp (int (*read_func)(void*,FILE*), 
					 void (*free_func)(),
					 FILE *fp);

/* Free the memory held by the bow_sarray SA. */
void bow_sarray_free (bow_sarray *sa);



/* Bit vectors, indexed by multiple dimensions.  They can grow
   automatically in the last dimension. */

typedef struct _bow_bitvec {
  int num_dimensions;		/* the number of dimensions by which indexed */
  int *dimension_sizes;		/* the sizes of each index dimension */
  int vector_size;		/* size of VECTOR in bytes */
  unsigned char *vector;	/* the memory for the bit vector */
} bow_bitvec;

/* Allocate, initialize and return a new "bit vector" that is indexed
   by NUM_DIMENSIONS different dimensions.  The size of each dimension
   is given in DIMENSION_SIZES.  The size of the last dimension is
   used as hint for allocating initial memory for the vector, but in
   practice, higher indices for the last dimension can be used later,
   and the bit vector will grow automatically.  Initially, the bit
   vector contains all zeros. */
bow_bitvec *bow_bitvec_new (int num_dimensions, int *dimension_sizes);

/* Set all the bits in the bit vector BV to 0 if value is zero, to 1
   otherwise. */
void bow_bitvec_set_all_to_value (bow_bitvec *bv, int value);

/* If VALUE is non-zero, set the bit at indices INDICES to 1,
   otherwise set it to zero.  Returns the previous value of that
   bit. */
int bow_bitvec_set (bow_bitvec *bv, int *indices, int value);

/* Return the value of the bit at indicies INDICIES. */
int bow_bitvec_value (bow_bitvec *bv, int *indices);

/* Free the memory held by the "bit vector" BV. */
void bow_bitvec_free (bow_bitvec *bv);


/* A convient interface to a specific instance of the above int/string
   mapping; this one is intended for all the words encountered in all
   documents. */

/* Given a "word index" WI, return its WORD, according to the global
   word-int mapping. */
const char *bow_int2word (int wi);

/* Given a WORD, return its "word index", WI, according to the global
   word-int mapping; if it's not yet in the mapping, add it. */
int bow_word2int (const char *word);

/* Like bow_word2int(), except it also increments the occurrence count 
   associated with WORD. */
int bow_word2int_add_occurrence (const char *word);

/* If this is non-zero, then bow_word2int() will return -1 when asked
   for the index of a word that is not already in the mapping.
   Essentially, setting this to non-zero makes bow_word2int() and
   bow_word2int_add_occurrence() behave like bow_str2int_no_add(). */
extern int bow_word2int_do_not_add;

/* Add to the word occurrence counts by recursively decending directory 
   DIRNAME and lexing all the text files; skip any files matching
   EXCEPTION_NAME. */
int bow_words_add_occurrences_from_text_dir (const char *dirname,
					     const char *exception_name);

/* Return the number of times bow_word2int_add_occurrence() was
   called with the word whose index is WI. */
int bow_words_occurrences_for_wi (int wi);

/* Replace the current word/int mapping with MAP. */
void bow_words_set_map (bow_int4str *map);

/* Modify the int/word mapping by removing all words that occurred 
   less than OCCUR number of times.  WARNING: This totally changes
   the word/int mapping; any WV's, WI2DVF's or BARREL's you build
   with the old mapping will have bogus WI's afterward. */
void bow_words_remove_occurrences_less_than (int occur);

/* Return the total number of unique words in the int/word map. */
int bow_num_words ();

/* Save the int/word map to file-pointer FP. */
void bow_words_write (FILE *fp);

/* Read the int/word map from file-pointer FP. */
void bow_words_read_from_fp (FILE *fp);



/* Word vectors.  A "word vector" is sorted array of words, with count
   information attached to each word.  Typically, there would be one
   "word vector" associated with a document, or with a concept. */

/* A "word entry"; these are the elements of a "word vector" */
typedef struct _bow_we {
  int wi;
  int count;
  float weight;
} bow_we;

/* A "word vector", containing an array of words with their statistics */
typedef struct _bow_wv {
  int num_entries;		/* the number of unique words in the vector */
  float normalizer;		/* multiply weights by this for normalizing */
  bow_we entry[0];
} bow_wv;

/* Create and return a new "word vector" from a file. */
bow_wv *bow_wv_new_from_text_fp (FILE *fp);

/* Create and return a new "word vector" from a document buffer LEX. */
bow_wv *bow_wv_new_from_lex (bow_lex *lex);

/* Create and return a new "word vector" that is the sum of all the
   "word vectors" in WV_ARRAY.  The second parameter, WV_ARRAY_LENGTH,
   is the number of "word vectors" in WV_ARRAY. */
bow_wv *bow_wv_add (bow_wv **wv_array, int wv_array_length);

/* Create and return a new "word vector" with uninitialized contents. */
bow_wv *bow_wv_new (int capacity);

/* Return a pointer to the "word entry" with index WI in "word vector WV */
bow_we *bow_wv_entry_for_wi (bow_wv *wv, int wi);

/* Return the count entry of "word" with index WI in "word vector" WV */
int bow_wv_count_for_wi (bow_wv *wv, int wi);

/* Print "word vector" WV on stream FP in a human-readable format. */
void bow_wv_fprintf (FILE *fp, bow_wv *wv);

/* Return the number of bytes required for writing the "word vector" WV. */
int bow_wv_write_size (bow_wv *wv);

/* Write "word vector" DV to the stream FP. */
void bow_wv_write (bow_wv *wv, FILE *fp);

/* Return a new "word vector" read from a pointer into a data file, FP. */
bow_wv *bow_wv_new_from_data_fp (FILE *fp);

/* Free the memory held by the "word vector" WV. */
void bow_wv_free (bow_wv *wv);

/* Collections of "word vectors. */

/* An array that maps "document indices" to "word vectors" */
typedef struct _bow_di2wv {
  int length;
  int size;
  bow_wv *entry[0];
} bow_di2wv;



/* Documents */  

/* We want a nice way of saying this is a training or test document, or do
   we ignore it for now. */
typedef enum {model, test, ignore} bow_doc_type;

/* A "document" entry useful for standard classification tasks. */
typedef struct _bow_cdoc {
  bow_doc_type type;		/* Is this document part of the model to be
				   built, a test document, or to be ignored */
  float normalizer;		/* Multiply weights by this for normalizing */
  float prior;			/* Prior probability of this class/doc */
  const char *filename;		/* Where to find the original document */
  short class;			/* A classification label. */
} bow_cdoc;

/* A convenient interface to bow_array that is specific to bow_cdoc. */
#define bow_cdocs bow_array
#define bow_cdocs_new(CAPACITY) bow_array_new (CAPACITY, sizeof (bow_cdoc), 0)
#define bow_cdocs_register_doc(CDOCS,CDOC) bow_array_append (CDOCS, CDOC)
#define bow_cdocs_di2doc(CDOCS, DI) bow_array_entry_at_index (CDOCS, DI)


/* Traversing directories to get filenames. */

/* A list of document names. */
/* xxx We might change this someday to allow for multiple documents
   per file, e.g. for "mbox" files containing many email messages. */
typedef struct _bow_doc_list {
  struct _bow_doc_list *next;
  char filename[0];
} bow_doc_list;

/* Return a non-zero value if the file FP contains mostly text. */
int bow_fp_is_text (FILE *fp);

/* Calls the function CALLBACK for each of the filenames encountered
   when recursively descending the directory named DIRNAME.  CALLBACK
   should be a pointer to function that takes a filename char-pointer,
   and a void-pointer as arguments and returns an integer.  Currently
   the return value is ignored, but it may be used in the future to
   cut short, causing bow_map_filesnames_from_dir to return
   immediately.  The value CONTEXT will be passed as the second
   argument to the CALLBACK function; it provides you with a way to
   transfer context you may need inside the implementation of the
   callback function.  EXCLUDE_PATTERNS is currently ignored. */
int
bow_map_filenames_from_dir (int (*callback)(const char *filename, 
					    void *context),
			    void *context,
			    const char *dirname,
			    const char *exclude_patterns);

/* Create a linked list of filenames, and append the file list pointed
   to by FL to it; return the new concatenated lists in *FL.  The
   function returns the total number of filenames.  When creating the
   list, look for files (and recursively descend directories) among
   those matching INCLUDE_PATTERNS, but don't include those matching
   EXCLUDE_PATTERNS; don't include files that aren't text files. */
/* xxx For now, this only works with a single directory name in
   INCLUDE_PATTERNS, and it ignores EXCLUDE_PATTERNS. */
int bow_doc_list_append (bow_doc_list **list, 
			 const char *include_patterns,
			 const char *exclude_patterns);

/* Print the file list FL to the output stream FP. */
void bow_doc_list_fprintf (FILE *fp, bow_doc_list *fl);

/* Return the number of entries in the "docname list" DL. */
int bow_doc_list_length (bow_doc_list *dl);

/* Free the memory held by the file list FL. */
void bow_doc_list_free (bow_doc_list *fl);



/* A convient interface to a specific instance of the above int/string
   mapping; this one is intended for all the documents encountered. */

/* Given a "word index" WI, return its WORD, according to the global
   word-int mapping. */
const char *bow_int2docname (int wi);

/* Given a WORD, return its "word index", WI, according to the global
   word-int mapping; if it's not yet in the mapping, add it. */
int bow_docname2int (const char *word);

/* Return the total number of unique words in the int/word map. */
int bow_num_docnames ();

/* Save the docname map to file-pointer FP. */
void bow_docnames_write (FILE *fp);

/* Read the docname from file-pointer FP. */
void bow_docnames_read_from_fp (FILE *fp);



/* xxx Perhaps the name should be changed from "dv" to "cv", for
   "class vector", or "concept vector", or "codebook vector". */
/* Document vectors.  A "document vector" is a sorted array of
   documents, with count information attached to each document.
   Typically, there would be one "document vector" associated with a
   word.  If "word vectors" are the rows of a large matrix, "document
   vectors" are the columns.  It can be more efficient to search just
   the docment vectors of the words in the query document, than it is
   to search the word vectors of all documents. */

/* A "document entry"; these are the elements of a "document vector". */
typedef struct _bow_de {
  short di;			/* a "document index" */
  short count;			/* number of times X appears in document DI */
  float weight;
} bow_de;

/* A "document vector" */ 
typedef struct _bow_dv {
  int length;			/* xxx Rename this to num_entries */
  int size;
  float idf;                    /* The idf factor for this word. */
  bow_de entry[0];
} bow_dv;

/* Create a new, empty "document vector". */
bow_dv *bow_dv_new (int capacity);

/* The default capacity used when 0 is passed for CAPACITY above. */
extern unsigned int bow_dv_default_capacity; 

/* Add a new entry to the "document vector" *DV. */
void bow_dv_add_di_count_weight (bow_dv **dv, int di, int count, float weight);

/* Sum the WEIGHT into the document vector DV at document index DI,
   creating a new entry in the document vector if necessary. */
void bow_dv_add_di_weight (bow_dv **dv, int di, float weight);

/* Write "document vector" DV to the stream FP. */
void bow_dv_write (bow_dv *dv, FILE *fp);

/* Return the number of bytes required for writing the "document vector" DV. */
int bow_dv_write_size (bow_dv *dv);

/* Return a new "document vector" read from a pointer into a data file, FP. */
bow_dv *bow_dv_new_from_data_fp (FILE *fp);

/* Free the memory held by the "document vector" DV. */
void bow_dv_free (bow_dv *dv);

/* A "document vector with file info (file storage information)" */
typedef struct _bow_dvf {
  int seek_start;
  bow_dv *dv;
} bow_dvf;


/* xxx Perhaps these should be generalized and renamed to `bow_i2v'. */
/* An array that maps "word indices" to "document vectors with file info" */
typedef struct _bow_wi2dvf {
  int size;
  FILE *fp;
  bow_dvf entry[0];
} bow_wi2dvf;

/* Create an empty `wi2dvf' */
bow_wi2dvf *bow_wi2dvf_new (int capacity);

/* The default capacity used when 0 is passed for CAPACITY above. */
extern unsigned int bow_wi2dvf_default_capacity;

/* Create a `wi2dvf' by reading data from file-pointer FP.  This
   doesn't actually read in all the "document vectors"; it only reads
   in the DVF information, and lazily loads the actually "document
   vectors". */
bow_wi2dvf *bow_wi2dvf_new_from_data_fp (FILE *fp);

/* Create a `wi2dvf' by reading data from a file.  This doesn't actually 
   read in all the "document vectors"; it only reads in the DVF 
   information, and lazily loads the actually "document vectors". */
bow_wi2dvf *bow_wi2dvf_new_from_data_file (const char *filename);

/* Return the "document vector" corresponding to "word index" WI.  If
   is hasn't been read already, this function will read the "document
   vector" out of the file passed to bow_wi2dvf_new_from_data_file(). */
bow_dv *bow_wi2dvf_dv (bow_wi2dvf *wi2dvf, int wi);

/* Read all the words from file pointer FP, and add them to the map
   WI2DVF, such that they are associated with document index DI. */
void bow_wi2dvf_add_di_text_fp (bow_wi2dvf **wi2dvf, int di, FILE *fp);

/* Add a "word vector" WV, associated with "document index" DI, to 
   the map WI2DVF. */ 
void bow_wi2dvf_add_di_wv (bow_wi2dvf **wi2dvf, int di, bow_wv *wv);

/* Write WI2DVF to file-pointer FP, in a machine-independent format.
   This is the format expected by bow_wi2dvf_new_from_fp(). */
void bow_wi2dvf_add_wi_di_count_weight (bow_wi2dvf **wi2dvf, int wi,
					int di, int count, float weight);

/* Remove the word with index WI from the vocabulary of the map WI2DVF */
void bow_wi2dvf_remove_wi (bow_wi2dvf *wi2dvf, int wi);

/* Write WI2DVF to file-pointer FP, in a machine-independent format.
   This is the format expected by bow_wi2dvf_new_from_fp(). */
void bow_wi2dvf_write (bow_wi2dvf *wi2dvf, FILE *fp);

/* Write WI2DVF to a file, in a machine-independent format.  This
   is the format expected by bow_wi2dvf_new_from_file(). */
void bow_wi2dvf_write_data_file (bow_wi2dvf *wi2dvf, const char *filename);

/* Compare two maps, and return 0 if they are equal.  This function was
   written for debugging. */
int bow_wi2dvf_compare (bow_wi2dvf *map1, bow_wi2dvf *map2);

/* Print statistics about the WI2DVF map to STDOUT. */
void bow_wi2dvf_print_stats (bow_wi2dvf *map);

/* Free the memory held by the map WI2DVF. */
void bow_wi2dvf_free (bow_wi2dvf *wi2dvf);

typedef enum {
  bow_method_tfidf_words,	/* TFIDF with DF=`word-count' */
  bow_method_tfidf_log_words,	/* TFIDF with DF=`log-word-count' */
  bow_method_tfidf_log_occur,	/* TFIDF with DF=`log-occurances' */
  bow_method_tfidf_prtfidf,	/* Joachim's PrTFIDF */
  bow_method_naivebayes,	/* Naive Bayes */
  bow_method_prind,		/* Fuhr's Probabilistic Indexing */
} bow_method;

/* If this is non-zero, use uniform class priors. */
extern int bow_prind_uniform_priors;

#define bow_str2method(METHODNAME)					     \
(!strcmp (METHODNAME, "tfidf_words")					     \
 ? bow_method_tfidf_words						     \
 : (!strcmp (METHODNAME, "tfidf_log_words")				     \
    ? bow_method_tfidf_log_words					     \
    : (!strcmp (METHODNAME, "tfidf_log_occur")				     \
       || !strcmp (METHODNAME, "tfidf")					     \
       ? bow_method_tfidf_log_occur					     \
       : (!strcmp (METHODNAME, "tfidf_prtfidf")				     \
	  ? bow_method_tfidf_prtfidf					     \
	  : (!strcmp (METHODNAME, "naivebayes")				     \
	     ? bow_method_naivebayes					     \
	     : (!strcmp (METHODNAME, "prind")				     \
		? bow_method_prind					     \
		: (bow_error ("Bad method name `%s'", METHODNAME), -1)))))))

#if 0
/* The parameters of weighting and scoring in barrel's. */
typedef struct _bow_method {
  /* Numerical label for this method */
  bow_method_id id;
  /* Functions for implementing parts of the method. */
  void (*set_weights)();
  bow_barrel* (*vpc_with_weights)();
  void (*normalize_weights)();
  int (*score)();
  /* Parameters of the method. */
  union {
    struct _tfidf_params {
      /* The parameters of TFIDF-like weight settings. */
      enum { words, occurrences } df_counts;
      enum { log, straight } df_transform;
    } tfidf;
    struct _naivebayes_params {
      /* The parameters of NaiveBayes-like weight settings. */
      enum { no, yes } uniform_priors;
    } naivebayes;
    struct _prind_params {
      enum { no, yes } uniform_priors;
    } prind;
  } params;
} bow_method;
#endif


/* A wrapper around a wi2dvf/cdocs combination. */
typedef struct _bow_barrel {
  int is_vpc;			/* non-zero if each `document' is a `class' */
  bow_method method;		/* TFIDF, NaiveBayes, PrInd, or whatever. */
  bow_array *cdocs;		/* The documents */
  bow_wi2dvf *wi2dvf;		/* The matrix of words vs documents */
} bow_barrel;

/* Create a new, empty `bow_barrel', with cdoc's of size ENTRY_SIZE
   and cdoc free function FREE_FUNC.  WORD_CAPACITY and CLASS_CAPACITY
   are just hints. */
bow_barrel *bow_barrel_new (int word_capacity, 
			    int class_capacity,
			    int entry_size, void (*free_func)());

/* Create a BARREL by indexing all the documents found when
   recursively decending directory DIRNAME, but skip files matching
   EXCEPTION_NAME. */
int bow_barrel_add_from_text_dir (bow_barrel *barrel,
				  const char *dirname, 
				  const char *except_name, 
				  int class);

/* Given a barrel of documents, create and return another barrel with
   only one vector per class. The classes will be represented as
   "documents" in this new barrel.  CLASSNAMES is an array of strings
   that maps class indices to class names. */
bow_barrel *bow_barrel_new_vpc (bow_barrel *barrel, const char **classnames);

/* Like bow_barrel_new_vpc(), but it also sets and normalizes the
   weights appropriately. */
bow_barrel *bow_barrel_new_vpc_with_weights (bow_barrel *doc_barrel, 
					     const char **classnames);

/* Modify the BARREL by removing those entries for words that are not
   among the NUM_WORDS_TO_KEEP top words, by information gain.  This
   function is similar to BOW_WORDS_KEEP_TOP_BY_INFOGAIN(), but this
   one doesn't change the word-int mapping. */
void bow_barrel_keep_top_words_by_infogain (int num_words_to_keep, 
					    bow_barrel *barrel,
					    int num_classes);

/* Write BARREL to the file-pointer FP in a machine independent format. */
void bow_barrel_write (bow_barrel *barrel, FILE *fp);

/* Create and return a `barrel' by reading data from the file-pointer FP. */
bow_barrel *bow_barrel_new_from_data_fp (FILE *fp);

/* Free the memory held by BARREL. */
void bow_barrel_free (bow_barrel *barrel);


/* Parsing headers from email messages. */
/* xxx Eventually all these will be replaced by use of a regular
   expression library. */

/* Read in BUF the characters inside the `<>' of the `Message-Id:'
   field of the email message contain in the file pointer FP.  Return
   the number of characters placed in BUF.  Signal an error if more
   than BUFLEN characters are necessary.  Return -1 if no matching
   field is found. */
int bow_email_get_msgid (FILE *fp, char *buf, int buflen);

/* Read in BUF the characters between the `Received: from ' and the
   following space, and the characters between the ` id ' and the
   following `;' in the file pointer FP.  Return the number of
   characters placed in BUF.  Signal an error if more than BUFLEN
   characters are necessary.  Return -1 if no matching field is
   found. */
int bow_email_get_receivedid (FILE *fp, char *buf, int buflen);

/* Read in BUF the characters inside the `<>' of the `In-Reply-To:'
   field of the email message contain in the file pointer FP.  Return
   the number of characters placed in BUF.  Signal an error if more than
   than BUFLEN characters are necessary.  Return -1 if no matching
   field is found. */
int bow_email_get_replyid (FILE *fp, char *buf, int buflen);

/* Read in BUF the characters inside the `<>' of the `References:'
   field of the news message contain in the file pointer FP.  Return
   the number of characters placed in BUF.  Signal an error if more
   than BUFLEN characters are necessary.  Return -1 if no matching
   field is found. */
int bow_email_get_references (FILE *fp, char *buf, int buflen);

/* Read in BUF the characters inside the `<>' of the
   `Resent-Message-Id:' field of the email message contain in the file
   pointer FP.  Return the number of characters placed in BUF.  Signal
   an error if more than BUFLEN characters are necessary.  Return -1
   if no matching field is found. */
int bow_email_get_resent_msgid (FILE *fp, char *buf, int buflen);

/* Read into BUF the characters inside the `<>' of the `From:' field
   of the email message contain in the file pointer FP.  Return the
   number of characters placed in BUF.  Signal an error if more than
   BUFLEN characters are necessary.  Return -1 if no matching field is
   found. */
int bow_email_get_sender (FILE *fp, char *buf, int buflen);

/* Read into BUF the characters inside the `<>' of the `To:' field
   of the email message contain in the file pointer FP.  Return the
   number of characters placed in BUF.  Signal an error if more than
   BUFLEN characters are necessary.  Return -1 if no matching field is
   found. */
int bow_email_get_recipient (FILE *fp, char *buf, int buflen);

/* Read into BUF the day, month and year of the `Date:' field of the
   email message contain in the file pointer FP.  The format is
   something like `21 Jul 1996'.  Return the number of characters
   placed in BUF.  Signal an error if more than BUFLEN characters are
   necessary.  Return -1 if no matching field is found. */
int bow_email_get_date (FILE *fp, char *buf, int buflen);



/* Progress and error reporting. */

enum bow_verbosity_levels {
  bow_silent = 0,		/* never print anything */
  bow_quiet,			/* only warnings and errors */
  bow_progress,			/* minimal # lines to show progress, use \b */
  bow_verbose,			/* give more status info */
  bow_chatty,			/* stuff most users wouldn't care about */
  bow_screaming			/* every little nit */
};

/* Examined by bow_verbosify() to determine whether to print the message.
   Default is bow_progress. */
extern int bow_verbosity_level;

/* If this is 0, and the message passed to bow_verbosify() contains
   backspaces, then the message will not be printed.  It is useful to
   turn this off when debugging inside an emacs window.  The default
   value is on. */
extern int bow_verbosity_use_backspace;

/* Print the printf-style FORMAT string and ARGS on STDERR, only if
   the BOW_VERBOSITY_LEVEL is equal or greater than the argument 
   VERBOSITY_LEVEL. */
int bow_verbosify (int verbosity_level, const char *format, ...);

/* Print the printf-style FORMAT string and ARGS on STDERR, and abort.
   This function appends a newline to the printed message. */
#define bow_error(FORMAT, ARGS...)			\
({if (bow_verbosity_level > bow_silent)			\
  {							\
    fprintf (stderr, "%s: ", __PRETTY_FUNCTION__);	\
    _bow_error (FORMAT , ## ARGS);			\
  }							\
 else							\
  {							\
    abort ();						\
  }}) 
volatile void _bow_error (const char *format, ...);



/* Memory allocation with error checking. */

/* These "extern inline" functions in this .h file will only be taken
   from here if gcc is optimizing, otherwise, they will be taken from
   identical copies defined in io.c */

#if ! defined (_BOW_MALLOC_INLINE_EXTERN)
#define _BOW_MALLOC_INLINE_EXTERN inline extern
#endif

_BOW_MALLOC_INLINE_EXTERN void *
bow_malloc (size_t s)
{
  void *ret;
  ret = malloc (s);
  if (!ret)
    bow_error ("Memory exhausted.");
  return ret;
}

_BOW_MALLOC_INLINE_EXTERN void * 
bow_realloc (void *ptr, size_t s)
{
  void *ret;
  ret = realloc (ptr, s);
  if (!ret)
    bow_error ("Memory exhausted.");
  return ret;
}

_BOW_MALLOC_INLINE_EXTERN void
bow_free (void *ptr)
{
  free (ptr);
}



/* Conveniences for writing and reading. */

/* Open a file using fopen(), with the same parameters.  Check the
   return value, and raise an error if the open failed.  The caller
   should close the returned file-pointer with fclose(). */
#define bow_fopen(FILENAME, MODE)					\
({									\
  FILE *ret;								\
  ret = fopen (FILENAME, MODE);						\
  if (ret == NULL)							\
    {									\
      if (*MODE == 'r')							\
        {								\
	  perror ("bow_fopen");						\
	  bow_error ("Couldn't open file `%s' for reading", FILENAME);	\
        }								\
      else								\
        {								\
          perror ("bow_fopen");						\
	  bow_error ("Couldn't open file `%s' for writing", FILENAME);	\
        }								\
    }									\
  ret;									\
})

/* These "extern inline" functions in this .h file will only be taken
   from here if gcc is optimizing, otherwise, they will be taken from
   identical copies defined in io.c */

#if ! defined (_BOW_IO_INLINE_EXTERN)
#define _BOW_IO_INLINE_EXTERN inline extern
#endif

/* Write a (int) value to the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fwrite_int (int n, FILE *fp)
{
  int num_written;
  n = htonl (n);
  num_written = fwrite (&n, sizeof (int), 1, fp);
  assert (num_written == 1);
  return num_written * sizeof (int);
}

/* Read a (long) value from the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fread_int (int *np, FILE *fp)
{
  int num_read;
  num_read = fread (np, sizeof (int), 1, fp);
  assert (num_read == 1);
  *np = ntohl (*np);
  return num_read * sizeof (int);
}

/* Write a (short) value to the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fwrite_short (short n, FILE *fp)
{
  int num_written;
  n = htons (n);
  num_written = fwrite (&n, sizeof (short), 1, fp);
  assert (num_written == 1);
  return num_written * sizeof (short);
}

/* Read a (long) value from the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fread_short (short *np, FILE *fp)
{
  int num_read;
  num_read = fread (np, sizeof (short), 1, fp);
  assert (num_read == 1);
  *np = ntohs (*np);
  return num_read * sizeof (short);
}

/* Write a "char*"-string value to the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fwrite_string (const char *s, FILE *fp)
{
  int len;
  int ret;

  if (s)
    len = strlen (s);
  else
    len = 0;
  ret = bow_fwrite_short (len, fp);
  if (len)
    ret += fwrite (s, sizeof (char), len, fp);
  assert (ret == sizeof (short) + len);
  return ret;
}

/* Read a "char*"-string value from the stream FP.  The memory for the
   string will be allocated using bow_malloc(). */
_BOW_IO_INLINE_EXTERN int
bow_fread_string (char **s, FILE *fp)
{
  short len;
  int ret;

  ret = bow_fread_short (&len, fp);
  *s = bow_malloc (len+1);
  if (len)
    ret += fread (*s, sizeof (char), len, fp);
  assert (ret = sizeof (short) + len);
  (*s)[len] = '\0';
  return ret;
}

/* Write a (float) value to the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fwrite_float (float n, FILE *fp)
{
  /* xxx This is not machine-independent! */
  int num_written;
  num_written = fwrite (&n, sizeof (float), 1, fp);
  assert (num_written == 1);
  return num_written * sizeof (float);
}

/* Read a (float) value from the stream FP. */
_BOW_IO_INLINE_EXTERN int
bow_fread_float (float *np, FILE *fp)
{
  /* xxx This is not machine-independent! */
  int num_written;
  num_written = fread (np, sizeof (float), 1, fp);
  assert (num_written == 1);
  return num_written * sizeof (float);
}



/* Manipulating a heap of documents */

/* Elements of the heap. */
typedef struct _bow_dv_heap_element {
  bow_dv *dv;                   /* The document vector */
  int wi;                       /* The id of this word */
  int index;                    /* Where we are in the vector at the mo. */
  int current_di;               /* Might as well keep the key here. */
} bow_dv_heap_element;

/* The heap itself */
typedef struct _bow_dv_heap {
  int length;                   /* How many items in the heap */
  bow_dv_heap_element entry[0];	/* The heap */
} bow_dv_heap;


/* Turn an array of bow_dv_heap_elements into a proper heap. The
   heapify function starts working at position i and works down the
   heap.  The heap is indexed from position 1. */
void bow_heapify (bow_dv_heap *wi2dvf, int i);

/* Function to take the top element of the heap - move it's index
   along and place it back in the heap. */
void bow_dv_heap_update (bow_dv_heap *heap);

/* Function to make a heap from all the vectors of documents in the big
   data structure we've built - I hope it all fits.... */
bow_dv_heap *bow_make_dv_heap_from_wi2dvf (bow_wi2dvf *wi2dvf);

/* Function to create a heap of the vectors of documents associated
   with each word in the word vector. */
bow_dv_heap *bow_make_dv_heap_from_wv (bow_wi2dvf *wi2dvf, bow_wv *wv);


/* Classes for classification.  In some cases each document will
   be in its own class. */

typedef struct _bow_class {
  short class;
  float length;
} bow_class;


/* Finding documents that best match a query. */

/* An array of these is filled in by bow_get_best_matches(). */
typedef struct _bow_doc_score {
  int di;			/* The "document index" for this document */
  float weight;			/* Its score */
} bow_doc_score;

/* Search the corpus map WI2DVF to find the closest matches to the
   "word vector" WV.  Fill the array SCORES with SCORES_LEN number of
   results, which will be sorted in decreasing order of their score.
   There may not be optimal results if, during the search, we need
   more space to hold same-scoring documents as the final document in
   this list.  The number of elements placed in the SCORES array is
   returned. */
int
bow_get_best_matches (bow_barrel *barrel,
		      bow_wv *wv, bow_doc_score *scores, int best);


/* Assigning weights to documents and calculating vector lengths */

/* Assign TFIDF weights to each element of each document vector. If the
   third argument is NULL, then all the documents are used
   for calculating weights. Otherwise the function should be passed a 
   bow_array of bow_cdoc entries. The function will then only use the
   documents whose type field is 'model' to calculate the weights. */ 
void bow_barrel_set_weights (bow_barrel *barrel);

/* Calculate the length of each word vector. Ideally, we can store
   this length in the document info and not have to scale all the
   elements by it (which would have neccessitated going through the
   whole structure twice). */
void bow_barrel_set_weight_normalizers (bow_barrel *barrel);

/* Assign the values of the "word vector entry's" WEIGHT field,
   according to the COUNT. */
void bow_wv_set_weights (bow_wv *wv, bow_method method);

/* Assign a value to the "word vector's" NORMALIZER field, according
   to the counts of all words.  Return the value of the NORMALIZER
   field. */
void bow_wv_set_weight_normalizer (bow_wv *wv, bow_method method);



/* Creating and working with test sets. */
/* This takes a bow_array of bow_cdoc's and first sets them all to be in the
   model. It then randomly choses 'no_test' bow_cdoc's to be in the test set
   and sets their type to be test. */
void bow_test_split (bow_barrel *barrel, int num_test);

/* This function sets up the data structure so we can step through the word
   vectors for each test document easily. */
bow_dv_heap *bow_test_new_heap (bow_barrel *barrel);

typedef struct _bow_test_wv {
  int di;                          /* The di of this test document. */
  bow_wv wv;                       /* It's associated wv */
} bow_test_wv;

/* This function takes the heap returned by bow_initialise_test_set and
   creates a word vector corresponding to the next document in the test set.
   The index of the test document is returned. If the test set is empty, 0
   is returned and *wv == NULL. This can't really deal with
   vectors which are all zero, since they are not represented explicitly
   in our data structure. Not sure what we should/can do. */
int
bow_test_next_wv (bow_dv_heap *heap, bow_barrel *barrel, bow_wv **wv);



/* Functions for information gain */

/* Return a malloc()'ed array containing an infomation-gain score for
   each word index; it is the caller's responsibility to free this
   array.  NUM_CLASSES should be the total number of classes in the
   BARREL.  The number of entries in the returned array will be found
   in SIZE. */
float *bow_infogain_per_wi_new (bow_barrel *barrel, int num_classes, 
				int *size);

/* Print to stdout the sorted results of bow_infogain_per_wi_new().
   It will print the NUM_TO_PRINT words with the highest infogain. */
void bow_infogain_per_wi_print (bow_barrel *barrel, int num_classes, 
				int num_to_print);

/* Function to calculate the information gain for each word in the corpus
   (looking only at documents in the model) and multiply each weight by the
   gain. */
void bow_barrel_scale_by_info_gain (bow_barrel *barrel, int no_classes);

/* Modify the int/word mapping by removing all words except the
   NUM_WORDS_TO_KEEP number of words that have the top information
   gain. */
void bow_words_keep_top_by_infogain (int num_words_to_keep, 
				     bow_barrel *barrel, int num_classes);


/* Parsing news article headers */

/* Function which takes a freshly opened file and reads in the lines up to
   the first blank line, parsing them into header/contents. An sarray is
   returned with the header lines (e.g.Subject) as keys and the entries are
   strings containing the contents. This function _will_ do bad things if not
   used on a news article. */
bow_sarray *bow_parse_news_headers (FILE *fp);

/* Function to take the headers bow_sarray and return a bow_array of strings
   corresponding to the newsgroups. */
bow_array *
bow_headers2newsgroups(bow_sarray *headers);
bow_sarray *bow_parse_news_headers (FILE *fp);

#endif /* __bow_h_INCLUDE */
