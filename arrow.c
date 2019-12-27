/* arrow - a document retreival front-end to libbow. */

/* Copyright (C) 1997 Andrew McCallum

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
#include <argp.h>
#include <errno.h>		/* needed on DEC Alpha's */

/* The version number of this program. */
#define ARROW_MAJOR_VERSION 0
#define ARROW_MINOR_VERSION 2

/* Definitions for using argp command-line processing */

const char *argp_program_version =
"arrow " STRINGIFY(ARROW_MAJOR_VERSION) "." STRINGIFY(ARROW_MINOR_VERSION);

const char *argp_program_bug_address = "<mccallum@cs.cmu.edu>";

static char arrow_argp_doc[] =
"Arrow -- a document retrieval front-end to libbow";

static char arrow_argp_args_doc[] = "[ARG...]";

#define PRINT_IDF_KEY 3000
static struct argp_option arrow_options[] =
{
  {0, 0, 0, 0,
   "For building data structures from text files:", 1},
  {"index", 'i', 0, 0,
   "tokenize training documents found under ARG..., build weight vectors, "
   "and save them to disk"},

  {0, 0, 0, 0,
   "For doing document retreival using the data structures built with -i:", 2},
  {"query", 'q', "FILE", OPTION_ARG_OPTIONAL, 
   "tokenize input from stdin [or FILE], then print document most like it"},
  {"num-hits-to-show", 'n', "N", 0,
   "Show the N documents that are most similar to the query text "
   "(default N=1)"},
  {"compare", 'c', "FILE", 0,
   "Print the TFIDF cosine similarity metric of the query with this FILE."},

  {0, 0, 0, 0,
   "Diagnostics", 3},
  {"print-idf", PRINT_IDF_KEY, 0, 0,
   "Print, in unsorted order the IDF of all words in the model's vocabulary"},

  { 0 }
};

struct arrow_arg_state
{
  /* Is this invocation of arrow to do indexing or querying? */
  enum {
    arrow_indexing, 
    arrow_querying,
    arrow_comparing,
    arrow_printing_idf
  } what_doing;
  int non_option_argi;
  /* Where to find query text, or if NULL get query text from stdin */
  const char *query_filename;
  const char *compare_filename;
  /* number of closest-matching docs to print */
  int num_hits_to_show;
} arrow_arg_state;

static error_t
arrow_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'q':
      arrow_arg_state.what_doing = arrow_querying;
      arrow_arg_state.query_filename = arg;
      break;
    case 'i':
      arrow_arg_state.what_doing = arrow_indexing;
      break;
    case 'n':
      arrow_arg_state.num_hits_to_show = atoi (arg);
      break;
    case 'c':
      arrow_arg_state.what_doing = arrow_comparing;
      arrow_arg_state.compare_filename = arg;
      break;
    case PRINT_IDF_KEY:
      arrow_arg_state.what_doing = arrow_printing_idf;

    case ARGP_KEY_ARG:
      /* Now we consume all the rest of the arguments.  STATE->next is the
	 index in STATE->argv of the next argument to be parsed, which is the
	 first STRING we're interested in, so we can just use
	 `&state->argv[state->next]' as the value for RAINBOW_ARG_STATE->ARGS.
	 IN ADDITION, by setting STATE->next to the end of the arguments, we
	 can force argp to stop parsing here and return.  */
      arrow_arg_state.non_option_argi = state->next - 1;
      if (arrow_arg_state.what_doing == arrow_indexing
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

static struct argp arrow_argp = 
{ arrow_options, arrow_parse_opt, arrow_argp_args_doc,
  arrow_argp_doc, bow_argp_children};


/* The structures that hold the data necessary for answering a query. */

bow_barrel *arrow_barrel;	/* the stats about words and documents */
/* The static structure in bow/int4word.c is also used. */


/* Writing and reading the word/document stats to disk. */

#define VOCABULARY_FILENAME "vocabulary"
#define BARREL_FILENAME "barrel"

/* Write the stats in the directory DATA_DIRNAME. */
void
arrow_archive ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  char *fnp;
  FILE *fp;

  strcpy (filename, bow_data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, VOCABULARY_FILENAME);
  fp = bow_fopen (filename, "w");
  bow_words_write (fp);
  fclose (fp);

  strcpy (fnp, BARREL_FILENAME);
  fp = bow_fopen (filename, "w");
  bow_barrel_write (arrow_barrel, fp);
  fclose (fp);
}

/* Read the stats from the directory DATA_DIRNAME. */
void
arrow_unarchive ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  char *fnp;
  FILE *fp;

  bow_verbosify (bow_progress, "Loading data files...");

  strcpy (filename, bow_data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, VOCABULARY_FILENAME);
  fp = bow_fopen (filename, "r");
  bow_words_read_from_fp (fp);
  fclose (fp);

  strcpy (fnp, BARREL_FILENAME);
  fp = bow_fopen (filename, "r");
  arrow_barrel = bow_barrel_new_from_data_fp (fp);
  /* Don't close this FP because we still need to read individual DV's. */

  bow_verbosify (bow_progress, "\n");
}



/* Building the word/document stats. */

/* Traverse the directories ARGV[ARROW_ARG_STATE.NON_OPTION_ARGI...],
   gathering word/document stats.  Return the number of documents
   indexed. */
int
arrow_index (int argc, char *argv[])
{
  int argi;

  /* Do all the parsing to build a barrel with word counts. */
  if (bow_prune_vocab_by_occur_count_n)
    {
      /* Parse all the documents to get word occurrence counts. */
      for (argi = arrow_arg_state.non_option_argi; argi < argc; argi++)
	bow_words_add_occurrences_from_text_dir (argv[argi], "");
      bow_words_remove_occurrences_less_than
	(bow_prune_vocab_by_occur_count_n);
      /* Now insist that future calls to bow_word2int*() will not
	 register new words. */
      bow_word2int_do_not_add = 1;
    }

  arrow_barrel = bow_barrel_new (0, 0, sizeof (bow_cdoc), 0);
  for (argi = arrow_arg_state.non_option_argi; argi < argc; argi++)
    bow_barrel_add_from_text_dir (arrow_barrel, argv[argi], 0, 0);
  if (bow_argp_method)
    arrow_barrel->method = bow_argp_method;
  else
    arrow_barrel->method = &bow_method_prind;
  bow_barrel_set_weights (arrow_barrel);
  bow_barrel_normalize_weights (arrow_barrel);
  return arrow_barrel->cdocs->length;
}



/* Perform a query. */

/* Print the contents of file FILENAME. */
static inline void
print_file (const char *filename)
{
  FILE *fp;
  int byte;

  if ((fp = fopen (filename, "r")) == NULL)
    bow_error ("Couldn't open file `%s' for reading", filename);
  while ((byte = fgetc (fp)) != EOF)
    fputc (byte, stdout);
  fclose (fp);
}

/* Get some query text, and print its best-matching documents among
   those previously indexed.  The number of matching documents is
   ARROW_ARG_STATE.NUM_HITS_TO_SHOW.  If
   ARROW_ARG_STATE.QUERY_FILENAME is non-null, the query text will be
   obtained from that file; otherwise it will be prompted for and read
   from stdin. */
int
arrow_query ()
{
  bow_score *hits;
  int actual_num_hits;
  int i;
  bow_wv *query_wv;

  hits = alloca (sizeof (bow_score) * arrow_arg_state.num_hits_to_show);

  /* Get the query text, and create a "word vector" from the query text. */
  if (arrow_arg_state.query_filename)
    {
      FILE *fp;
      fp = bow_fopen (arrow_arg_state.query_filename, "r");
      query_wv = bow_wv_new_from_text_fp (fp);
      fclose (fp);
    }
  else
    {
      bow_verbosify (bow_quiet, 
		     "Type your query text now.  End with a Control-D.\n");
      query_wv = bow_wv_new_from_text_fp (stdin);
    }

  bow_wv_set_weights (query_wv, arrow_barrel);
  bow_wv_normalize_weights (query_wv, arrow_barrel);

  /* Get the best matching documents. */
  actual_num_hits = bow_barrel_score (arrow_barrel, query_wv,
				      hits, arrow_arg_state.num_hits_to_show,
				      -1);

  /* Print them. */
  for (i = 0; i < actual_num_hits; i++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (arrow_barrel->cdocs, 
						 hits[i].di);
      printf ("\nHit number %d, with score %g\n", i, hits[i].weight);
      print_file (cdoc->filename);
    }

  return actual_num_hits;
}

/* Compare two documents by cosine similarity with TFIDF weights. */
void
arrow_compare (bow_wv *wv1, bow_wv *wv2)
{
  float score = 0;
  float score_increment;
  int wvi1, wvi2;
  bow_dv *dv;
  float idf;

#if 0
  bow_wv_set_weights_to_count_times_idf (wv1, arrow_barrel);
  bow_wv_set_weights_to_count_times_idf (wv2, arrow_barrel);
#else
  bow_wv_set_weights_to_count (wv1);
  bow_wv_set_weights_to_count (wv2);
#endif
  bow_wv_normalize_weights_by_vector_length (wv1);
  bow_wv_normalize_weights_by_vector_length (wv2);

  /* Loop over all words in WV1, summing the score. */
  for (wvi1 = 0, wvi2 = 0; wvi1 < wv1->num_entries; wvi1++)
    {
      /* Find the WV index of this word in WV2. */
      while (wv2->entry[wvi2].wi < wv1->entry[wvi1].wi
	     && wvi2 < wv2->num_entries)
	wvi2++;
      
      /* If we found the word, add the produce to the score. */
      if (wv1->entry[wvi1].wi == wv2->entry[wvi2].wi
	  && wvi2 < wv2->num_entries)
	{
	  dv = bow_wi2dvf_dv (arrow_barrel->wi2dvf, wv1->entry[wvi1].wi);
	  if (dv)
	    idf = dv->idf;
	  else
	    idf = 0;
	  score_increment = ((wv1->entry[wvi1].weight * wv1->normalizer)
			     * (wv2->entry[wvi2].weight * wv2->normalizer));
	  score += score_increment;
	  if (bow_print_word_scores)
	    fprintf (stderr, "%10.8f   (%3d,%3d, %8.4f)  %-30s   %10.8f\n", 
		     score_increment,
		     wv1->entry[wvi1].count,
		     wv2->entry[wvi2].count,
		     idf,
		     bow_int2word (wv1->entry[wvi1].wi),
		     score);
	}
    }
  printf ("%g\n", score);
}


/* The main() function. */

int
main (int argc, char *argv[])
{
  /* Default command-line argument values */
  arrow_arg_state.num_hits_to_show = 1;
  arrow_arg_state.what_doing = arrow_indexing;
  arrow_arg_state.query_filename = NULL;

  /* Parse the command-line arguments. */
  argp_parse (&arrow_argp, argc, argv, 0, 0, &arrow_arg_state);

  if (arrow_arg_state.what_doing == arrow_indexing)
    {
      if (arrow_index (argc, argv))
	arrow_archive ();
      else
	bow_error ("No text documents found.");
    }
  else
    {
      arrow_unarchive ();
      if (arrow_arg_state.what_doing == arrow_querying)
	{
	  arrow_query ();
	}
      else if (arrow_arg_state.what_doing == arrow_comparing)
	{
	  bow_wv *query_wv;
	  bow_wv *compare_wv;
	  FILE *fp;

	  /* The user must specify the query filename on the command line.
	     In this case it is not optional. */
	  assert (arrow_arg_state.query_filename);

	  /* Make word vectors from the files. */
	  fp = bow_fopen (arrow_arg_state.query_filename, "r");
	  query_wv = bow_wv_new_from_text_fp (fp);
	  fclose (fp);
	  fp = bow_fopen (arrow_arg_state.compare_filename, "r");
	  compare_wv = bow_wv_new_from_text_fp (fp);
	  fclose (fp);

	  arrow_compare (query_wv, compare_wv);
	}
      else if (arrow_arg_state.what_doing == arrow_printing_idf)
	{
	  int wi;
	  int max_wi = MIN (arrow_barrel->wi2dvf->size, bow_num_words());
	  bow_dv *dv;

	  for (wi = 0; wi < max_wi; wi++)
	    {
	      dv = bow_wi2dvf_dv (arrow_barrel->wi2dvf, wi);
	      if (dv)
		printf ("%9f %s\n", dv->idf, bow_int2word (wi));
	    }
	}
      else
	bow_error ("Internal error");
    }

  exit (0);
}
