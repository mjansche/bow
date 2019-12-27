/* archer.h - public declartions for IR frontend to libbow.
   Copyright (C) 1998 Andrew McCallum

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

#ifndef __archer_h_INCLUDE
#define __archer_h_INCLUDE

#include <bow/libbow.h>


/* The version number of this program. */
#define ARCHER_MAJOR_VERSION 0
#define ARCHER_MINOR_VERSION 0

#define BOW_MAX_WORD_LABELS 100

/* The variables that are set by command-line options. */
struct archer_arg_state_s
{
  /* What this invocation of archer to do? */
  void (*what_doing)();
  int non_option_argi;
  int num_hits_to_print;
  FILE *query_out_fp;
  const char *dirname;
  const char *query_string;
  const char *server_port_num;
  int serve_with_forking;
  int score_is_raw_count;
};

typedef union {
  /* When we write it to disk, it just looks like a `char' */
  unsigned char byte;
  /* The first byte not only must tell us if we must read more bytes
     in order to get all the bits of the offset, but also must tell us
     if this is a "document index", "label index" or a "position
     index". "label indices" will be each non-first index in each
     sequence of indices with is_di true; the first index in each such
     sequence will be a valid (non-zero) "document index" or a
     placeholder zero value. However many label parameters there are are
     stored immediately afterwards the label index. */
  struct _bow_pe {
    unsigned int is_more:1;
    unsigned int is_di:1;
    unsigned int index:6;
  } bits;
  /* The bytes following the first don't need to tell us if they are a
     "document index", "label index" or "position index" because the
     first byte already told us. */
  struct _bow_pe_more {
    unsigned int is_more:1;
    unsigned int index:7;
  } bits_more;
} bow_pe;

typedef struct _bow_pv {
  int count;			/* total number of word occurrences in PV */
  int seek_start;		/* disk position where this PV starts */
  int read_seek_end;		/* disk position from which to read next */
  int read_last_di;		/* doc index last read */
  int read_last_pi;		/* position index last read */
  int read_segment_bytes_remaining;
  int write_seek_end;
  int write_last_di;
  int write_last_pi;
  int write_segment_bytes;
  int write_segment_bytes_remaining;
} bow_pv;

typedef struct _bow_wi2pv {
  const char *pv_filename;   /* filename where pv/di/pi matrix is stored */
  FILE *fp;                  /* file-pointer for the same */
  int num_words;             /* number of used wi entries */
  int next_word;             /* greatest wi entry on disk, plus 1.
				0 if no used entries */
  int entry_count;           /* number of wi entries allocated
				(entry_count >= next_word >= num_words) */
  bow_pv *entry;           
  FILE *inc_fp;              /* file-pointer for the wi2pv mapping */
  long entry_start;          /* position in the above file where entries 
				start. 
				- equal to the length of the header,
				which includes the pv_filename, whose
				length isn't known at compile-time */
} bow_wi2pv;

typedef struct archer_doc {
  bow_doc_type tag;
  int word_count;
  int di;
} archer_doc;

/* document annotations */
typedef struct annotation {
  int count;
  int size;
  char **feats;
  char **vals;
} annotation;

typedef struct archer_label {
  int word_count;
  int li;
} archer_label;

bow_wi2pv *bow_wi2pv_new (int capacity, const char *pv_filename, const char *inc_filename);
void bow_wi2pv_free (bow_wi2pv *wi2pv);
void bow_wi2pv_add_wi_di_pi (bow_wi2pv *wi2pv, int wi, int di, int pi); /* deprecated */
void bow_wi2pv_add_wi_di_li_pi (bow_wi2pv *wi2pv, int wi, int di, int li[],
				int ln, int pi);
void bow_wi2pv_rewind (bow_wi2pv *wi2pv);
void bow_wi2pv_wi_next_di_pi (bow_wi2pv *wi2pv, int wi, int *di, int *pi); /* deprecated */
void bow_wi2pv_wi_next_di_li_pi(bow_wi2pv *wi2pv, int wi, int *di,
				int li[], int *ln, int *pi);
void bow_wi2pv_wi_unnext (bow_wi2pv *wi2pv, int wi);
int bow_wi2pv_wi_count (bow_wi2pv *wi2pv, int wi);
void bow_wi2pv_write_header (bow_wi2pv *wi2pv);
void bow_wi2pv_write_entry (bow_wi2pv *wi2pv, int wi);
void bow_wi2pv_write (bow_wi2pv *wi2pv);
bow_wi2pv *bow_wi2pv_new_from_filename (const char *filename);
void bow_wi2pv_print_stats (bow_wi2pv *wi2pv);


/* Fill in PV with the correct initial values, and write the first
   segment header to disk.  What this function does must match what
   bow_pv_add_di_pi() does when it adds a new segment. */
void bow_pv_init (bow_pv *pv, FILE *fp);

/* Add "document index" DI and "position index" PI to PV by writing
   the correct information to FP.  Does not assume that FP is already
   seek'ed to the correct position.  Will add a new PV segment on disk
   if necessary.  Assumes that both DI and PI are greater than or
   equal to the last DI and PI written, respectively. */
void bow_pv_add_di_pi (bow_pv *pv, int di, int pi, FILE *fp);
void bow_pv_add_di_li_pi (bow_pv *pv, int di, int li[], int ln, int pi,
			  FILE *fp);

/* Read the next "document index" DI and "position index" PI.  Does
   not assume that FP is already seek'ed to the correct position.
   Will jump to a new PV segment on disk if necessary. */
void bow_pv_next_di_pi (bow_pv *pv, int *di, int *pi, FILE *fp);
void bow_pv_next_di_li_pi (bow_pv *pv, int *di, int li[], int *ln, int *pi,
			   FILE *fp);

/* Undo the effect of the last call to bow_pv_next_di_pi().  That is,
   make the next call to bow_pv_next_di_pi() return the same DI and PI
   as the last call did.  This function may not be called multiple
   times in a row without calling bow_pv_next_di_pi() in between. */
void bow_pv_unnext (bow_pv *pv);

/* Rewind the read position to the beginning of the PV */
void bow_pv_rewind (bow_pv *pv, FILE *fp);

/* Write the in-memory portion of PV to FP */
void bow_pv_write (bow_pv *pv, FILE *fp);

/* Read the in-memory portion of PV from FP */
void bow_pv_read (bow_pv *pv, FILE *fp);

/* Close and re-open WI2PV's FILE* for its PV's.  This should be done
   after a fork(), since the parent and child will share lseek()
   positions otherwise. */
void bow_wi2pv_reopen_pv (bow_wi2pv *wi2pv);

/* Label handling code */
const char *bow_last_label(void);
void bow_push_label(const char *label);
char *bow_pop_label(char buf[], int bufsz);
char *bow_first_label(char buf[], int bufsz);
char *bow_next_label(char buf[], int bufsz);
void bow_reset_labels(void);

/* Lexer interfaces */
void flex_mail_open(FILE *fp, const char * name);
int flex_mail_get_word(char buf[], int bufsz);
int flex_mail_get_word_extended(char buf[], int bufsz, long* start, long* end);
void tagged_lex_open(FILE* fp, const char * name);
void tagged_lex_open_dont_parse_tags(FILE* fp, const char * name);
int tagged_lex_get_word(char buf[], int bufsz);
int tagged_lex_get_word_extended(char buf[], int bufsz, long* start, long* end);

/* server code */
void archer_query_serve();

/* annotations */
bow_sarray *annotation_sarray_new(void);
annotation *annotation_new(void);
void annotation_add_fval(annotation *a, char *feat, char *val);
void annotation_sarray_write(bow_sarray *sa, char *fname);
bow_sarray *annotation_sarray_read(const char *fname);
bow_sarray *annotation_sarray_reread(bow_sarray *sa, const char *fname);
int annotation_count(annotation *a);
char *annotation_feat(annotation *a, int index);
char *annotation_val(annotation *a, int index);
annotation *annotation_sarray_entry_at_keystr(bow_sarray *a, const char
					      *keystr);


#endif /* __archer_h_INCLUDE */
