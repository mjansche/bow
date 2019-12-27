/* rainbow - a document classification front-end to libbow. */

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
#include <argp.h>

#include <errno.h>		/* needed on DEC Alpha's */
#include <unistd.h>		/* for getopt(), maybe */
#include <stdlib.h>		/* for atoi() */
#include <string.h>		/* for strrchr() */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

static int rainbow_sockfd;

/* The version number of this program. */
#define RAINBOW_MAJOR_VERSION 0
#define RAINBOW_MINOR_VERSION 2

#define rainbow_default_method (&bow_method_naivebayes)

/* Definitions for using argp command-line processing */

const char *argp_program_version =
"rainbow " 
STRINGIFY(RAINBOW_MAJOR_VERSION) "." STRINGIFY(RAINBOW_MINOR_VERSION);

const char *argp_program_bug_address = "<mccallum@cs.cmu.edu>";

static char rainbow_argp_doc[] =
"Rainbow -- a document classification front-end to libbow";

static char rainbow_argp_args_doc[] = "[ARG...]";

enum {
  PRINT_COUNTS_FOR_WORD_KEY = 10000,
  INFOGAIN_PAIR_VECTOR_KEY,
  USE_VOCAB_IN_FILE_KEY,
  NO_LISP_SCORE_TRUNCATION_KEY,
  SERVER_KEY,
  PRINT_DOC_NAMES_KEY,
  PRINT_WORD_PROBABILITIES_KEY,
  INDEX_PRINTED_BARREL_KEY,
  HIDE_VOCAB_IN_FILE_KEY,
  HIDE_VOCAB_INDICES_IN_FILE_KEY,
  TEST_ON_TRAINING_KEY,
};

static struct argp_option rainbow_options[] =
{
  {0, 0, 0, 0,
   "For building data structures from text files:", 20},
  {"index", 'i', 0, 0,
   "Tokenize training documents found under directories ARG... "
   "(where each ARG directory contains documents of a different class), "
   "build weight vectors, and save them to disk."},
  {"index-printed-barrel", INDEX_PRINTED_BARREL_KEY, "FORMAT", 0,
   "Read document/word statistics from a file in the format produced by "
   "--print-barrel=FORMAT.  See --print-barrel for details about FORMAT."},

  {0, 0, 0, 0,
   "For doing document classification using the data structures "
   "built with -i:", 21},
  {"query", 'q', "FILE", OPTION_ARG_OPTIONAL, 
   "Tokenize input from stdin [or FILE], then print classification scores."},
  {"output-text", 'o', "FILE", OPTION_HIDDEN,
   "Intead of outputing the classnames, output the contents of FILE in the "
   "data directory of the winning class, (for use as email auto-answer)."},
  {"repeat", 'r', 0, 0,
   "Prompt for repeated queries."},
  {"query-server", SERVER_KEY, "PORTNUM", 0,
   "Run rainbow in server mode."},

  {0, 0, 0, 0,
   "Rainbow specific vocabulary options:", 22},
  {"use-vocab-in-file", USE_VOCAB_IN_FILE_KEY, "FILE", 0,
   "Limit vocabulary to just those words read as space-separated strings "
   "from FILE."},
  {"hide-vocab-in-file", HIDE_VOCAB_IN_FILE_KEY, "FILE", 0,
   "Hide from the vocabulary all words read as space-separated strings "
   "from FILE."},
  {"hide-vocab-indices-in-file", HIDE_VOCAB_INDICES_IN_FILE_KEY, "FILE", 0,
   "Hide from the vocabulary all words read as space-separated word "
   "index indices from FILE."},


  {0, 0, 0, 0,
   "Testing documents that were indexed with `-i':", 23},
  {"test", 't', "N", 0,
   "Perform N test/train splits of the indexed documents, and output "
   "classifications of all test documents each time."},
  {"test-on-training", TEST_ON_TRAINING_KEY, "N", 0,
   "Like `--test', but instead of classifing the held-out test documents "
   "classify the training data in leave-one-out fashion.  Perform N trials."},
  {"no-lisp-score-truncation", NO_LISP_SCORE_TRUNCATION_KEY, 0, 0,
   "Normally scores that are lower than 1e-35 are printed as 0, "
   "because our LISP reader can't handle floating point numbers smaller "
   "than 1e-35.  This option turns off that truncation."},
  {0, 0, 0, 0,
   "Testing documents that are specified on the command line:", 5},
  {"test-files", 'x', 0, 0,
   "In same format as `-t', output classifications of documents in "
   "the directory ARG  The ARG must have the same subdir names as the "
   "ARG's specified when --index'ing."},
  {"test-files-loo", 'X', 0, 0,
   "Same as --test-files, but evaulate the files assuming that they "
   "were part of the training data, and doing leave-one-out "
   "cross-validation."},

  {0, 0, 0, 0,
   "Diagnostics:", 24},
  {"print-word-infogain", 'I', "N", 0,
   "Print the N words with the highest information gain."},
  {"print-word-pair-infogain", INFOGAIN_PAIR_VECTOR_KEY, "N", 0,
   "Print the N word-pairs, which when co-occuring in a document, have "
   "the highest information gain.  (Unfinished; ignores N.)"},
  {"print-word-weights", 'W', "CLASSNAME", 0,
   "Print the word/weight vector for CLASSNAME, "
   "sorted with high weights first."},
  {"print-word-probabilities", PRINT_WORD_PROBABILITIES_KEY, "CLASS", 0,
   "Print P(w|CLASS), the probability in class CLASS "
   "of each word in the vocabulary."},
  {"print-foilgain", 'F', "CLASSNAME", 0,
   "Print the word/foilgain vector for CLASSNAME."},
  {"print-barrel", 'B', "FORMAT", OPTION_ARG_OPTIONAL,
   "Print the word/document count matrix in an awk- or perl-accessible "
   "format.  Format is specified by the following letters:\n"
   "print all vocab or just words in document:\n"
   "  a=all OR s=sparse\n"
   "print counts as ints or binary:\n"
   "  b=binary OR i=integer\n"
   "print word as:\n  "
   "  n=integer index OR w=string OR e=empty OR c=combination\n"
   "The default is the last in each list"},
  {"print-word-counts", PRINT_COUNTS_FOR_WORD_KEY, "WORD", 0,
   "Print the number of times WORD occurs in each class."},
  {"print-counts-for-word", PRINT_COUNTS_FOR_WORD_KEY, "WORD", 
   OPTION_ALIAS | OPTION_HIDDEN},
  {"print-doc-names", PRINT_DOC_NAMES_KEY, 0, 0,
   "Print the list of filenames on which the model was built."},

  { 0 }
};

struct rainbow_arg_state
{
  /* Is this invocation of rainbow to do indexing or querying? */
  enum {
    rainbow_indexing, 
    rainbow_querying,
    rainbow_query_serving,
    rainbow_testing,		/* many queries, from train/test split */
    rainbow_file_testing,	/* many queries, from a directory */
    rainbow_infogain_printing,
    rainbow_infogain_pair_printing,
    rainbow_weight_vector_printing,
    rainbow_foilgain_printing,
    rainbow_barrel_printing,
    rainbow_word_count_printing,
    rainbow_doc_name_printing,
    rainbow_printing_word_probabilities
  } what_doing;
  /* Where to find query text, or if NULL get query text from stdin */
  const char *query_filename;
  /* Name of file to find in each class directory; output the contents
     of this file instead of the classname. */
  const char *output_filename;
  /* If we are doing test, how many test are we doing? */
  int num_trials;
  /* If we are printing info gain stats, how many of the top words? */
  int infogain_words_to_print;
  /* Used for selecting the class for which the weight-vector will be
     printed. */
  const char *printing_class;
  /* Index into argv of the non-option args at the end (i.e. for -i
     classnames or -x filenames, etc). */
  int non_option_argi;
  int repeat_query;
  bow_int4str *vocab_map;
  bow_int4str *hide_vocab_map;
  int use_lisp_score_truncation;
  int loo_cv;
  const char *server_port_num;
  const char *barrel_printing_format;
  const char *hide_vocab_indices_filename;
  int test_on_training;
} rainbow_arg_state;

static error_t
rainbow_parse_opt (int key, char *arg, struct argp_state *state)
{
  switch (key)
    {
    case 'q':
      rainbow_arg_state.what_doing = rainbow_querying;
      rainbow_arg_state.query_filename = arg;
      break;
    case SERVER_KEY:
      rainbow_arg_state.what_doing = rainbow_query_serving;
      rainbow_arg_state.server_port_num = arg;
      bow_default_lexer->document_end_pattern = "\n.\r\n";
      break;
    case 'i':
      rainbow_arg_state.what_doing = rainbow_indexing;
      break;
    case INDEX_PRINTED_BARREL_KEY:
      rainbow_arg_state.what_doing = rainbow_indexing;
      rainbow_arg_state.barrel_printing_format = arg;
      break;
    case 'r':
      rainbow_arg_state.repeat_query = 1;
      break;

    case USE_VOCAB_IN_FILE_KEY:
      rainbow_arg_state.vocab_map = bow_int4str_new_from_text_file (arg);
      bow_verbosify (bow_progress,
		     "Using vocab with %d words from file `%s'\n",
		     rainbow_arg_state.vocab_map->str_array_length, arg);
      break;

    case HIDE_VOCAB_IN_FILE_KEY:
      rainbow_arg_state.hide_vocab_map = bow_int4str_new_from_text_file(arg);
      break;
    case HIDE_VOCAB_INDICES_IN_FILE_KEY:
      rainbow_arg_state.hide_vocab_indices_filename = arg;
      break;

    /* Switches for testing */
    case 't':
      rainbow_arg_state.what_doing = rainbow_testing;
      rainbow_arg_state.num_trials = atoi (arg);
      break;
    case TEST_ON_TRAINING_KEY:
      rainbow_arg_state.what_doing = rainbow_testing;
      rainbow_arg_state.test_on_training = 1;
      rainbow_arg_state.num_trials = atoi (arg);
      break;
    case NO_LISP_SCORE_TRUNCATION_KEY:
      rainbow_arg_state.use_lisp_score_truncation = 0;
      break;

      /* Switches for file testing */
    case 'X':
      rainbow_arg_state.loo_cv = 1;
    case 'x':
      rainbow_arg_state.what_doing = rainbow_file_testing;
      break;

      /* Switches for diagnostics */
    case 'I':
      /* Print out ARG number of vocab words ranked by infogain. */
      rainbow_arg_state.what_doing = rainbow_infogain_printing;
      rainbow_arg_state.infogain_words_to_print = atoi (arg);
      break;
    case 'W':
      /* Print the weight-vector for the named class */
      rainbow_arg_state.what_doing = rainbow_weight_vector_printing;
      rainbow_arg_state.printing_class = arg;
      break;
    case 'F':
      /* Print the foil gain for the named class */
      rainbow_arg_state.what_doing = rainbow_foilgain_printing;
      rainbow_arg_state.printing_class = arg;
      break;
    case 'P':
      /* Print the contribution of each word to each class during 
	 scoring. */ 
      bow_print_word_scores = 1;
      break;
    case 'B':
      /* Print the barrel in awk-accessible form to stdout. */
      rainbow_arg_state.what_doing = rainbow_barrel_printing;
      rainbow_arg_state.barrel_printing_format = arg;
      break;
    case PRINT_COUNTS_FOR_WORD_KEY:
      rainbow_arg_state.what_doing = rainbow_word_count_printing;
      rainbow_arg_state.printing_class = arg;
      break;
    case INFOGAIN_PAIR_VECTOR_KEY:
      rainbow_arg_state.what_doing = rainbow_infogain_pair_printing;
      rainbow_arg_state.infogain_words_to_print = atoi (arg);
      break;
    case PRINT_DOC_NAMES_KEY:
      rainbow_arg_state.what_doing = rainbow_doc_name_printing;
      break;
    case PRINT_WORD_PROBABILITIES_KEY:
      rainbow_arg_state.what_doing = rainbow_printing_word_probabilities;
      rainbow_arg_state.printing_class = arg;
      break;

#if 0
    case ARGP_KEY_NO_ARGS:
      argp_usage (state);
#endif

    case ARGP_KEY_ARG:
      /* Now we consume all the rest of the arguments.  STATE->next is the
	 index in STATE->argv of the next argument to be parsed, which is the
	 first STRING we're interested in, so we can just use
	 `&state->argv[state->next]' as the value for RAINBOW_ARG_STATE->ARGS.
	 IN ADDITION, by setting STATE->next to the end of the arguments, we
	 can force argp to stop parsing here and return.  */
      rainbow_arg_state.non_option_argi = state->next - 1;
      if (rainbow_arg_state.what_doing == rainbow_indexing
	  && rainbow_arg_state.barrel_printing_format == NULL
	  && state->next == state->argc)
	{
	  /* Only one classname is not enough. */
	  fprintf (stderr, "Need data from more than one class.\n");
	  argp_usage (state);
	}
      state->next = state->argc;
      break;

    case ARGP_KEY_END:
      /* Here we know that STATE->arg_num == 0, since we force argument
	 parsing to end before any more arguments can get here.  */
      if (rainbow_arg_state.what_doing == rainbow_indexing
	  || rainbow_arg_state.what_doing == rainbow_file_testing)
	{
	  if (state->arg_num == 0)
	    {
	      /* Too few arguments.  */
	      fprintf (stderr, "No non-option arguments needed.\n");
	      argp_usage (state);
	    }
	}
      else if (state->arg_num != 0)
	{
	  /* Too many arguments.  */
	  fprintf (stderr, "No non-option arguments needed.\n");
	  argp_usage (state);
	}
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp rainbow_argp = 
{ rainbow_options, rainbow_parse_opt, rainbow_argp_args_doc,
  rainbow_argp_doc, bow_argp_children};


/* The structures that hold the data necessary for answering a query. */

bow_barrel *rainbow_doc_barrel;     /* the stats about words and documents */
bow_barrel *rainbow_class_barrel=NULL;  /* the stats about words and classes */
/* The static structure in bow/int4word.c is also used. */

/* Given a fully-specified file path name (all the way from `/'),
   return just the last filename part of it. */
static inline const char *
filename_to_classname (const char *filename)
{
#if 0
  /* Don't bother stripping off the directory name from the classname.
     This way, even when we give rainbow multi-directory
     specifications for where to find the data for each class, and
     some of the lowest-level directories have the same name, we can
     still distinguish between them. */
  return filename;
#else
  const char *ret;
  ret = strrchr (filename, '/');
  if (ret)
    return ret + 1;
  return filename;
#endif
}




/* Writing and reading the word/document stats to disk. */

#define VOCABULARY_FILENAME "vocabulary"
#define DOC_BARREL_FILENAME "doc-barrel"
#define CLASS_BARREL_FILENAME "class-barrel"
#define OUTPUTNAME_FILENAME "outfile"
#define FORMAT_VERSION_FILENAME "format-version"

/* Write the stats in the directory DATA_DIRNAME. */
void
rainbow_archive ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  char *fnp;
  FILE *fp;

  strcpy (filename, bow_data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, FORMAT_VERSION_FILENAME);
  bow_write_format_version_to_file (filename);

  strcpy (fnp, OUTPUTNAME_FILENAME);
  fp = bow_fopen (filename, "w");
  if (rainbow_arg_state.output_filename)
    fprintf (fp, "%s\n", rainbow_arg_state.output_filename);
  fclose (fp);

  strcpy (fnp, VOCABULARY_FILENAME);
  fp = bow_fopen (filename, "w");
  bow_words_write (fp);
  fclose (fp);

  strcpy (fnp, CLASS_BARREL_FILENAME);
  fp = bow_fopen (filename, "w");
  bow_barrel_write (rainbow_class_barrel, fp);
  fclose (fp);

  strcpy (fnp, DOC_BARREL_FILENAME);
  fp = bow_fopen (filename, "w");
  bow_barrel_write (rainbow_doc_barrel, fp);
  fclose (fp);
}

/* Read the stats from the directory DATA_DIRNAME. */
void
rainbow_unarchive ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  char *fnp;
  FILE *fp;
  char buf[1024];
  struct stat st;
  int e;
  
  if (rainbow_arg_state.what_doing != rainbow_query_serving)
    bow_verbosify (bow_progress, "Loading data files...\n");

  strcpy (filename, bow_data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, FORMAT_VERSION_FILENAME);
  e = stat (filename, &st);
  if (e != 0)
    {
      /* Assume this means the file doesn't exist, and this archive
	 was created before BOW_DEFAULT_FORMAT_VERSION was added to
	 the library.  The version number before
	 BOW_DEFAULT_FORMAT_VERSION was added to the library was 3. */
      bow_file_format_version = 3;
    }
  else
    {
      bow_read_format_version_from_file (filename);
    }

  strcpy (fnp, OUTPUTNAME_FILENAME);
  fp = bow_fopen (filename, "r");
  buf[0] = '\0';
  fscanf (fp, "%s", buf);
  rainbow_arg_state.output_filename = strdup (buf);
  fclose (fp);

  strcpy (fnp, VOCABULARY_FILENAME);
  fp = bow_fopen (filename, "r");
  bow_words_read_from_fp (fp);
  fclose (fp);

  strcpy (fnp, CLASS_BARREL_FILENAME);
  fp = bow_fopen (filename, "r");
  rainbow_class_barrel = bow_barrel_new_from_data_fp (fp);
  /* Don't close it because bow_wi2dvf_dv will still need to read it. */

  strcpy (fnp, DOC_BARREL_FILENAME);
  fp = bow_fopen (filename, "r");
  rainbow_doc_barrel = bow_barrel_new_from_data_fp (fp);
  /* Don't close it because bow_wi2dvf_dv will still need to read it. */

  if (rainbow_doc_barrel->classnames == NULL)
    {
      int i;
      bow_cdoc *cdoc;
      rainbow_doc_barrel->classnames = bow_int4str_new (0);
      rainbow_class_barrel->classnames = bow_int4str_new (0);
      for (i = 0; i < rainbow_class_barrel->cdocs->length; i++)
	{
	  cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, i);
	  bow_str2int (rainbow_doc_barrel->classnames, 
		       filename_to_classname (cdoc->filename));
	  bow_str2int (rainbow_class_barrel->classnames, 
		       filename_to_classname (cdoc->filename));
	}
    }

  if (bow_uniform_class_priors)
    bow_barrel_set_cdoc_priors_to_class_uniform (rainbow_doc_barrel);
}



/* Building the word/document stats. */

/* Traverse the directories CLASSDIR_NAMES, gathering word/document
   stats, and write the stats to disk in BOW_DATA_DIRNAME. */
void
rainbow_index (int num_classes, const char *classdir_names[],
	       const char *exception_name)
{
  int class_index;

  void do_indexing ()
    {
      if (rainbow_doc_barrel)
	bow_free_barrel (rainbow_doc_barrel);
      /* Index all the documents. */
      rainbow_doc_barrel = bow_barrel_new (0, 0, sizeof (bow_cdoc), NULL);
      if (bow_argp_method)
	rainbow_doc_barrel->method = bow_argp_method;
      else
	rainbow_doc_barrel->method = rainbow_default_method;
      for (class_index = 0; class_index < num_classes; class_index++)
	{
	  bow_verbosify (bow_progress, "Class `%s'\n  ", 
			 filename_to_classname (classdir_names[class_index]));
	  if (bow_hdb)
	    {
	      /* Gathers stats on all documents in HDB database */
	      if (bow_barrel_add_from_hdb
		  (rainbow_doc_barrel,
		   classdir_names[class_index],
		   exception_name,
		   filename_to_classname (classdir_names[class_index]))
		  == 0)
		bow_verbosify (bow_quiet,
			       "No text files found in database `%s'\n", 
			       classdir_names[class_index]);
	    }
	  else
	    /* This function traverses the class directory
	       gathering word/document stats.  Return the number of
	       documents indexed.  This gathers stats on individual
	       documents; we have yet to "sum together the word vectors
	       of all documents for each particular class". */
	    if (bow_barrel_add_from_text_dir
		(rainbow_doc_barrel, 
		 classdir_names[class_index],
		 exception_name,
		 filename_to_classname (classdir_names[class_index]))
		== 0)
	      bow_verbosify (bow_quiet,
			     "No text files found in directory `%s'\n", 
			     classdir_names[class_index]);
	}
      if (bow_uniform_class_priors)
	bow_barrel_set_cdoc_priors_to_class_uniform (rainbow_doc_barrel);
    }

  /* Do all the parsing to build a barrel with word counts. */
  if (bow_prune_vocab_by_occur_count_n)
    {
      /* Parse all the documents to get word occurrence counts. */
      for (class_index = 0; class_index < num_classes; class_index++)
	{
	  bow_verbosify (bow_progress,
			 "Class `%s'\n  ", 
			 filename_to_classname
			 (classdir_names[class_index]));
	  if (bow_hdb)
	    bow_words_add_occurrences_from_hdb
	      (classdir_names[class_index], "");
	  else
	    bow_words_add_occurrences_from_text_dir
	      (classdir_names[class_index], "");
	}
      /* xxx This should be (bow_prune_vocab_by_occur_count_n+1) !! */
      bow_words_remove_occurrences_less_than
	(bow_prune_vocab_by_occur_count_n);
      /* Now insist that future calls to bow_word2int*() will not
	 register new words. */
      bow_word2int_do_not_add = 1;
    }
  
  do_indexing ();

  if (bow_prune_vocab_by_infogain_n
      || bow_prune_words_by_doc_count_n)
    {
      if (0)
	{
	  /* Change barrel by removing words with small information gain. */
	  bow_barrel_keep_top_words_by_infogain
	    (bow_prune_vocab_by_infogain_n, rainbow_doc_barrel, num_classes);
	}
      else
	{
	  /* Change vocabulary to remove words with small information gain */
	  bow_wi2dvf_hide_words_by_doc_count (rainbow_doc_barrel->wi2dvf,
					      bow_prune_words_by_doc_count_n);
	  /* The doc count pruning must be before the infogain pruning,
	     because this function below is the one that re-assigns
	     the word-indices. */
	  bow_words_keep_top_by_infogain (bow_prune_vocab_by_infogain_n,
					  rainbow_doc_barrel,
					  num_classes);
	  /* Now insist that future calls to bow_word2int*() will not
	     register new words. */
	  bow_word2int_do_not_add = 1;
	  do_indexing ();
	}
    }

  /* Combine the documents into class statistics. */
  rainbow_class_barrel = 
    bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);
}

void
rainbow_index_printed_barrel (const char *filename)
{
  rainbow_doc_barrel = 
    bow_barrel_new_from_printed_barrel_file
    (filename, rainbow_arg_state.barrel_printing_format);

  /* Combine the documents into class statistics. */
  rainbow_class_barrel = 
    bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);
}



/* Perform a query. */

/* Print the contents of file FILENAME to stdout. */
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
   NUM_HITS_TO_SHOW.  If QUERY_FILENAME is non-null, the query text
   will be obtained from that file; otherwise it will be prompted for
   and read from stdin. */
int
rainbow_query (FILE *in, FILE *out)
{
  /* Show as many hits as there are classes. */
  int num_hits_to_show = bow_barrel_num_classes (rainbow_class_barrel);
  bow_score *hits;
  int actual_num_hits;
  int i;
  bow_wv *query_wv;

  hits = alloca (sizeof (bow_score) * num_hits_to_show);

  /* Get the query text, and create a "word vector" from the query text. */
  if (rainbow_arg_state.query_filename)
    {
      FILE *fp;
      fp = bow_fopen (rainbow_arg_state.query_filename, "r");
      query_wv = bow_wv_new_from_text_fp (fp, 
					  rainbow_arg_state.query_filename);
      fclose (fp);
    }
  else
    {
    query_again:
      if (rainbow_arg_state.what_doing != rainbow_query_serving)
	bow_verbosify (bow_quiet, 
		       "Type your query text now.  End with a Control-D.\n");
      if (feof (in))
	clearerr (in);
      query_wv = bow_wv_new_from_text_fp (in, NULL);
    }

  if (query_wv == NULL || query_wv->num_entries == 0)
    {
      if (rainbow_arg_state.query_filename)
	bow_verbosify (bow_quiet, "No query text found in `%s'.\n", 
		       rainbow_arg_state.query_filename);
      else
	if (rainbow_arg_state.what_doing != rainbow_query_serving)
	  bow_verbosify (bow_quiet, "No query text found.");
	else
	  {
	    fprintf(out, ".\n");
	    fflush(out);
	  }
      if (rainbow_arg_state.repeat_query)
	bow_verbosify (bow_progress, "  Stopping query repeat\n");
      return 0;
    }

  /* (Re)set the weight-setting method, if requested with a `-m' on
     the command line. */
  if (bow_argp_method)
    rainbow_doc_barrel->method = bow_argp_method;
  else
    rainbow_doc_barrel->method = rainbow_default_method;

  if (bow_prune_vocab_by_infogain_n)
    {
      /* Change barrel by removing words with small information gain. */
      bow_barrel_keep_top_words_by_infogain
	(bow_prune_vocab_by_infogain_n, rainbow_doc_barrel, 
	 bow_barrel_num_classes (rainbow_class_barrel));
    }
  /* Infogain pruning must be done before this vocab_map pruning, because
     infogain pruning first unhides all words! */
  if (rainbow_arg_state.vocab_map)
    {
      /* Remove words not in the VOCAB_MAP. */
      bow_barrel_prune_words_not_in_map (rainbow_doc_barrel,
					 rainbow_arg_state.vocab_map);
    }
  if (rainbow_arg_state.hide_vocab_map)
    {
      bow_barrel_prune_words_in_map (rainbow_doc_barrel,
				     rainbow_arg_state.hide_vocab_map);
    }

  /* Re-build the rainbow_class_barrel, if necessary */
  if (rainbow_doc_barrel->method != rainbow_class_barrel->method
      || rainbow_arg_state.vocab_map
      || rainbow_arg_state.hide_vocab_map
      || bow_prune_vocab_by_infogain_n)
    {
      bow_free_barrel (rainbow_class_barrel);
      rainbow_class_barrel = 
	bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);
    }

  /* Get the best matching documents. */
  bow_wv_set_weights (query_wv, rainbow_doc_barrel);
  bow_wv_normalize_weights (query_wv, rainbow_doc_barrel);
  actual_num_hits = bow_barrel_score  (rainbow_class_barrel, query_wv,
				       hits, num_hits_to_show, -1);

  /* Print them. */
  if (rainbow_arg_state.what_doing != rainbow_query_serving)
    fprintf (out, "\n");
  for (i = 0; i < actual_num_hits; i++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, 
						 hits[i].di);
      if (strlen (rainbow_arg_state.output_filename))
	{
	  char buf[1024];
	  strcpy (buf, cdoc->filename);
	  strcat (buf, "/");
	  strcat (buf, rainbow_arg_state.output_filename);
	  print_file (buf);
	}
      else
	{
	  /* For the sake CommonLisp, don't print numbers smaller than
	     1e-35, because it can't `(read)' them. */
	  if (rainbow_arg_state.use_lisp_score_truncation
	      && hits[i].weight < 1e-35
	      && hits[i].weight > 0)
	    hits[i].weight = 0;
	  fprintf (out, "%s %.*g\n", 
		   cdoc->filename, bow_score_print_precision, 
		   hits[i].weight);
	}
    }
  if (rainbow_arg_state.what_doing == rainbow_query_serving)
    fprintf(out, ".\n");
  fflush(out);

  if (rainbow_arg_state.repeat_query)
    goto query_again;
  return actual_num_hits;
}

void
rainbow_socket_init (const char *socket_name, int use_unix_socket)
{
  int servlen, type, bind_ret;
  struct sockaddr_un un_addr;
  struct sockaddr_in in_addr;
  struct sockaddr *sap;

  type = use_unix_socket ? AF_UNIX : AF_INET;
   
  rainbow_sockfd = socket(type, SOCK_STREAM, 0);
  assert(rainbow_sockfd >= 0);

  if (type == AF_UNIX)
    {
      sap = (struct sockaddr *)&un_addr;
      bzero((char *)sap, sizeof(un_addr));
      strcpy(un_addr.sun_path, socket_name);
      servlen = strlen(un_addr.sun_path) + sizeof(un_addr.sun_family) + 1;
    }
  else
    {
      sap = (struct sockaddr *)&in_addr;
      bzero((char *)sap, sizeof(in_addr));
      in_addr.sin_port = htons(atoi(socket_name));
      in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      servlen = sizeof(in_addr);
    }

  sap->sa_family = type;     

  bind_ret = bind(rainbow_sockfd, sap, servlen);
  assert(bind_ret >= 0);

  listen(rainbow_sockfd, 5);
}


void
rainbow_serve ()
{
  int newsockfd, clilen;
  struct sockaddr cli_addr;
  FILE *in, *out;

  clilen = sizeof(cli_addr);
  newsockfd = accept(rainbow_sockfd, &cli_addr, &clilen);

  assert(newsockfd >= 0);

  in = fdopen(newsockfd, "r");
  out = fdopen(newsockfd, "w");

  while (!feof(in))
    rainbow_query(in, out);

  fclose(in);
  fclose(out);

  close(newsockfd);
}

#if RAINBOW_LISP

/* Setup rainbow so that we can do our lisp interface. */
void
rainbow_lisp_setup (char *datadirname)
{
  /* Defined in deflexer.c */
  extern void _bow_default_lexer_init ();
  /* Defined in naivebayes.c */
  extern void _register_method_crossentropy ();
  extern void _register_method_naivebayes ();
  /* Defined in tfidf.c */
  extern void _register_method_tfidf_words ();
  extern void _register_method_tfidf_log_words ();
  extern void _register_method_tfidf_log_occur ();
  /* Defined in prind.c */
  extern void _register_method_prind ();

  char *dirname = bow_malloc (strlen (datadirname) + 1);
  int argc;
  static char *argv[] = {
    "rainbow-lisp-interface",
    "-q",
    "-H",
    "-h",
    "-s",
    "-b",
    "-m", "kl",
/*    "--lex-pipe-command", "/afs/cs/project/theo-9/webkb/univ4.rainbow/tag-digits.pl", */
    "-d", 0,
    0};

  for (argc = 0; argv[argc]; argc++);
  strcpy (dirname, datadirname);
  argv[argc] = dirname;
  for (argc = 0; argv[argc]; argc++);

  /* Since this was dynamically loaded, the __attribute__((constructor))
     functions weren't called.  Call them now. */
  _bow_default_lexer_init ();
  _register_method_crossentropy ();
  _register_method_naivebayes ();
  _register_method_tfidf_words ();
  _register_method_tfidf_log_words ();
  _register_method_tfidf_log_occur ();
  _register_method_prind ();
  _register_method_kl ();
  _register_method_evi ();

  /* Default command-line argument values */
  rainbow_arg_state.what_doing = rainbow_indexing;
  rainbow_arg_state.query_filename = NULL;
  rainbow_arg_state.output_filename = NULL;
  rainbow_arg_state.num_trials = 0;
  rainbow_arg_state.infogain_words_to_print = 10;
  rainbow_arg_state.printing_class = 0;
  rainbow_arg_state.non_option_argi = 0;
  rainbow_arg_state.repeat_query = 0;
  rainbow_arg_state.vocab_map = NULL;
  rainbow_arg_state.hide_vocab_map = NULL;
  rainbow_arg_state.use_lisp_score_truncation = 1;
  rainbow_arg_state.loo_cv = 0;

  argp_parse (&rainbow_argp, argc, argv, 0, 0, &rainbow_arg_state);

  rainbow_unarchive ();

  if (bow_argp_method)
    rainbow_doc_barrel->method = bow_argp_method;
  else
    rainbow_doc_barrel->method = rainbow_default_method;

  /*  if (rainbow_doc_barrel->method != rainbow_class_barrel->method)
    { */
      bow_free_barrel (rainbow_class_barrel);
      rainbow_class_barrel = 
	bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);
      /*    } */

}

/* Classify the text in the file QUERY_FILE, and return the 
   class scores (in sorted order) in SCORES.  NUM_SCORES indicates
   the maximum number of slots for which space is allocated in SCORES. */
int
rainbow_lisp_query (const char *query_file,
		    bow_score *scores, int num_scores)
{
  /* Show as many hits as there are classes. */
  int actual_num_scores;
  bow_wv *query_wv;

  /* Get the query text, and create a "word vector" from the query text. */
  if (query_file)
    {
      FILE *fp;
      fp = bow_fopen (query_file, "r");
      query_wv = bow_wv_new_from_text_fp (fp, NULL);
      fclose (fp);
    }
  else
    {
      bow_verbosify (bow_quiet, 
		     "Type your query text now.  End with a Control-D.\n");
      query_wv = bow_wv_new_from_text_fp (stdin, NULL);
    }

  if (query_wv == NULL || query_wv->num_entries == 0)
    {
      return 0;
    }

  /* Get the best matching documents. */
  bow_wv_set_weights (query_wv, rainbow_class_barrel);
  bow_wv_normalize_weights (query_wv, rainbow_class_barrel);
  actual_num_scores = bow_barrel_score (rainbow_class_barrel, query_wv,
					scores, num_scores, -1);

  bow_wv_free (query_wv);
  return actual_num_scores;
}

#endif /* RAINBOW_LISP */


/* Run test trials, outputing results to TEST_FP.  The results are
   indended to be read and processed by the Perl script
   ./rainbow-stats. */
void
rainbow_test (FILE *test_fp)
{
  int tn;			/* trial number */
  bow_dv_heap *test_heap;	/* we'll extract test WV's from here */
  bow_wv *query_wv;
  int di;			/* a document index */
  bow_score *hits = NULL;
  int num_hits_to_retrieve=0;
  int actual_num_hits;
  int hi;			/* hit index */
  bow_cdoc *doc_cdoc;
  bow_cdoc *class_cdoc;
  int (*classify_cdoc_p)(bow_cdoc*);

  /* (Re)set the weight-setting method, if requested with `-m' argument. */
  if (bow_argp_method)
    rainbow_doc_barrel->method = bow_argp_method;

  hits = NULL;

  /* Loop once for each trial. */
  for (tn = 0; tn < rainbow_arg_state.num_trials; tn++)
    {

      set_doc_types (rainbow_doc_barrel);

      if (bow_uniform_class_priors)
	bow_barrel_set_cdoc_priors_to_class_uniform (rainbow_doc_barrel);

      if (bow_prune_vocab_by_infogain_n)
	{
	  /* Change barrel by removing words with small information gain. */
	  bow_barrel_keep_top_words_by_infogain
	    (bow_prune_vocab_by_infogain_n, rainbow_doc_barrel, 
	     bow_barrel_num_classes (rainbow_class_barrel));
	}
      /* Infogain pruning must be done before this vocab_map pruning,
	 because infogain pruning first unhides all words! */
      if (rainbow_arg_state.vocab_map)
	{
	  bow_barrel_prune_words_not_in_map (rainbow_doc_barrel,
					     rainbow_arg_state.vocab_map);
	}
      if (rainbow_arg_state.hide_vocab_map)
	{
	  bow_barrel_prune_words_in_map (rainbow_doc_barrel,
					 rainbow_arg_state.hide_vocab_map);
	}
      if (rainbow_arg_state.hide_vocab_indices_filename)
	{
	  FILE *fp = 
	    bow_fopen (rainbow_arg_state.hide_vocab_indices_filename, "r");
	  int wi;
	  int num_hidden = 0;
	  while (fscanf (fp, "%d", &wi) == 1)
	    {
	      bow_wi2dvf_hide_wi (rainbow_doc_barrel->wi2dvf, wi);
	      num_hidden++;
	    }
	  fclose (fp);
	  bow_verbosify (bow_progress, "%d words hidden by index\n", 
			 num_hidden);
	}

      /* Re-create the vector-per-class barrel in accordance with the
	 new train/test settings. */
      bow_free_barrel (rainbow_class_barrel);
      rainbow_class_barrel = 
	bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);

      /* do this late for --em-multi-hump-neg */
      if (!hits)
	{
	  num_hits_to_retrieve = bow_barrel_num_classes (rainbow_class_barrel);
	  assert (num_hits_to_retrieve);
	  hits = alloca (sizeof (bow_score) * num_hits_to_retrieve);
	}

      fprintf (test_fp, "#%d\n", tn);


      /* Create the heap from which we'll get WV's. */
      test_heap = bow_test_new_heap (rainbow_doc_barrel);

      /* Initialize QUERY_WV so BOW_TEST_NEXT_WV() knows not to try to free */
      query_wv = NULL;

      /* Determine if we are classifying the testing documents or the
	 training documents. */
      if (rainbow_arg_state.test_on_training)
	{
	  classify_cdoc_p = bow_cdoc_is_model;
	  assert (rainbow_arg_state.num_trials == 1);
	}
      else
	{
	  classify_cdoc_p = bow_cdoc_is_test;
	}

      /* Loop once for each test document.  NOTE: This will skip documents
	 that don't have any words that are in the vocabulary. */

      while ((di = bow_heap_next_wv (test_heap, rainbow_doc_barrel, &query_wv,
				     classify_cdoc_p)) != -1)
	{
	  doc_cdoc = bow_array_entry_at_index (rainbow_doc_barrel->cdocs, 
					       di);

	  class_cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, 
						 doc_cdoc->class);
	  bow_wv_set_weights (query_wv, rainbow_class_barrel);
	  bow_wv_normalize_weights (query_wv, rainbow_class_barrel);
	  actual_num_hits = 
	    bow_barrel_score (rainbow_class_barrel, 
			      query_wv, hits,
			      num_hits_to_retrieve, 
			      (rainbow_arg_state.test_on_training
			       ? doc_cdoc->class
			       : -1));
	  assert (actual_num_hits == num_hits_to_retrieve);
#if 0
	  printf ("%8.6f %d %8.6f %8.6f %d ",
		  class_cdoc->normalizer, 
		  class_cdoc->word_count, 
		  class_cdoc->normalizer / class_cdoc->word_count, 
		  class_cdoc->prior,
		  doc_cdoc->class);
	  if (hits[0].di == doc_cdoc->class)
	    printf ("1\n");
	  else
	    printf ("0\n");
#endif
	  fprintf (test_fp, "%s %s ", 
		   doc_cdoc->filename, 
		   bow_barrel_classname_at_index (rainbow_doc_barrel,
						  doc_cdoc->class));
	  for (hi = 0; hi < actual_num_hits; hi++)
	    {
	      class_cdoc = 
		bow_array_entry_at_index (rainbow_class_barrel->cdocs,
					  hits[hi].di);
	      /* For the sake CommonLisp, don't print numbers smaller than
		 1e-35, because it can't `(read)' them. */
	      if (rainbow_arg_state.use_lisp_score_truncation
		  && hits[hi].weight < 1e-35
		  && hits[hi].weight > 0)
		hits[hi].weight = 0;
	      fprintf (test_fp, "%s:%.*g ", 
		       bow_barrel_classname_at_index
		       (rainbow_class_barrel, hits[hi].di),
		       bow_score_print_precision,
		       hits[hi].weight);
	    }
	  fprintf (test_fp, "\n");
	}
      /* Don't free the heap here because bow_test_next_wv() does it
	 for us. */
    }
}



/* Run test trials, outputing results to TEST_FP.  The results are
   indended to be read and processed by the Perl script
   ./rainbow-stats.  The test documents come from files inside the
   directories that are named in argv[].  */
void
rainbow_test_files (FILE *out_fp, const char *test_dirname)
{
  bow_score *hits;
  /* int num_test_docs; */
  int num_hits_to_retrieve = bow_barrel_num_classes (rainbow_class_barrel);
  int actual_num_hits;
  int hi;			/* hit index */
  const char *current_class;
  int current_ci;
  int ci;
  unsigned int dirlen = 1024;
  char dir[dirlen];

  /* Deals with the word vector once it has been taken from the file
     or HDB database.  Called by test_file and test_hdb_file. (see below) */
  int process_wv (const char *filename, bow_wv *query_wv, void *context) 
    {
      bow_cdoc *class_cdoc;

      if (!query_wv)
	{
	  bow_verbosify (bow_progress, "%s found to be empty.\n", filename);
	  return 0;
	}
    
      fprintf (out_fp, "%s %s ", 
	       filename,	/* This test instance */
	       current_class); /* The name of the correct class */

      bow_wv_set_weights (query_wv, rainbow_class_barrel);
      bow_wv_normalize_weights (query_wv, rainbow_class_barrel);
      actual_num_hits = 
	bow_barrel_score (rainbow_class_barrel, 
			  query_wv, hits,
			  num_hits_to_retrieve,
			  (rainbow_arg_state.loo_cv
			   ? current_ci
			   : -1));
      for (hi = 0; hi < actual_num_hits; hi++)
	{
	  class_cdoc = 
	    bow_array_entry_at_index (rainbow_class_barrel->cdocs,
				      hits[hi].di);
	  /* For the sake CommonLisp, don't print numbers smaller than
	     1e-35, because it can't `(read)' them. */
	  if (rainbow_arg_state.use_lisp_score_truncation
	      && hits[hi].weight < 1e-35
	      && hits[hi].weight > 0)
	    hits[hi].weight = 0;
	  fprintf (out_fp, "%s:%.*g ", 
		   filename_to_classname (class_cdoc->filename),
		   bow_score_print_precision, hits[hi].weight);
	}
      fprintf (out_fp, "\n");
      return 0;
    }

  /* This nested function is called once for each test document. */
  int test_file (const char *filename, void *context)
    {
      bow_wv *query_wv = NULL;
      FILE *fp;

      fp = bow_fopen (filename, "r");
      /* Must test to see if text here because this was done when the
	 barrel was build in barrel.c:bow_barrel_add_from_text_dir().
	 Otherwise we may read a document that was not included in the
	 original barrel, and get negative word occurrence counts
	 since we subtract to do leave-one-out processing here. */
      if (bow_fp_is_text (fp))
	query_wv = bow_wv_new_from_text_fp (fp, filename);
      fclose (fp);
      return process_wv (filename, query_wv, context);
    }

  /* This is used for the case that we are dealing with HDB files.
     At this point, the fulltext of the file has already been retrieved
     and is passed in as DATA. */
  int test_hdb_file (const char *filename, char *data, void *context)
    {
      bow_wv *query_wv = NULL;
      bow_lex lex;

      lex.document = data;
      lex.document_length = strlen (data);
      lex.document_position = 0;

      if (bow_str_is_text (data))
	query_wv = bow_wv_new_from_lex (&lex);
      return process_wv (filename, query_wv, context);
    }

  hits = alloca (sizeof (bow_score) * num_hits_to_retrieve);

  fprintf (out_fp, "#0\n");
  for (ci = 0; ci < bow_barrel_num_classes (rainbow_doc_barrel); ci++)
    {
      /* Build a string containing the name of this directory. */
      bow_cdoc *class_cdoc;

      strcpy (dir, test_dirname);
      class_cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, ci);
      strcat (dir, "/");
      strcat (dir, filename_to_classname (class_cdoc->filename));
      assert (strlen (dir) < dirlen);

      /* Remember which classname this comes from, so, above, we know
	 the correct class */
      current_ci = class_cdoc->class;
      current_class = bow_barrel_classname_at_index (rainbow_doc_barrel, ci);
      /* Test each document in that diretory. */
      if (bow_hdb)
	bow_map_filenames_from_hdb (test_hdb_file, 0, dir, "");
      else
	bow_map_filenames_from_dir (test_file, 0, dir, "");
    }
}

void
rainbow_print_weight_vector (const char *classname)
{
  int ci;			/* The `class index' of CLASSNAME */
  bow_cdoc *cdoc;
  int wi, max_wi;		/* a word index */
  bow_dv *dv;			/* a class vector */
  int dvi;			/* an index into DV */

  /* Find the `class index' of the class with name CLASSNAME */
  for (ci = 0; ci < bow_barrel_num_classes (rainbow_class_barrel); ci++)
    {
      cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, ci);
      if (!strcmp (filename_to_classname (cdoc->filename), classname))
	break;
    }
  if (ci == bow_barrel_num_classes (rainbow_class_barrel))
    bow_error ("No class named `%s'\n", classname);

  /* Get the CDOC for this class, so we can use its NORMALIZER. */
  cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, ci);

  /* Print the `weight' for each word in the class */
  max_wi = MIN (bow_num_words (), rainbow_class_barrel->wi2dvf->size);
  for (wi = 0; wi < max_wi; wi++)
    {
      dv = bow_wi2dvf_dv (rainbow_class_barrel->wi2dvf, wi);
      if (dv == NULL)
	continue;
      /* Find the DVI with the DI matching CI */
      for (dvi = 0; dvi < dv->length && dv->entry[dvi].di < ci; dvi++);
      if (!(dv && dvi < dv->length && dv->entry[dvi].di == ci))
	continue;
      /* This is an attempt for a test to see if the weights need to
	 be "normalized" before being used. */
      if (rainbow_class_barrel->method->normalize_weights)
	printf ("%20.10f %s\n",
		dv->entry[dvi].weight * cdoc->normalizer,
		bow_int2word (wi));
      else
	printf ("%20.10f %s\n",
		dv->entry[dvi].weight,
		bow_int2word (wi));
    }
}

void
rainbow_print_foilgain (const char *classname)
{
  int ci;			/* The `class index' of CLASSNAME */
  int wi;
  bow_cdoc *cdoc;
  float **fig_per_wi_ci;
  int fig_num_wi;

  /* Find the `class index' of the class with name CLASSNAME */
  for (ci = 0; ci < bow_barrel_num_classes (rainbow_class_barrel); ci++)
    {
      cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, ci);
      if (!strcmp (filename_to_classname (cdoc->filename), classname))
	break;
    }
  if (ci == bow_barrel_num_classes (rainbow_class_barrel))
    bow_error ("No class named `%s'\n", classname);

  /* Get the foilgains. */
  fig_per_wi_ci = 
    bow_foilgain_per_wi_ci_new (rainbow_doc_barrel,
				bow_barrel_num_classes (rainbow_class_barrel),
				&fig_num_wi);

  /* Print the `foilgain' for each word in the class */
  for (wi = 0; wi < fig_num_wi; wi++)
    {
      printf ("%20.6f %s\n", 
	      fig_per_wi_ci[wi][ci], bow_int2word (wi));
    }

  bow_foilgain_free (fig_per_wi_ci, fig_num_wi);
}


/* The main() function. */

#if !RAINBOW_LISP
int
main (int argc, char *argv[])
{
  /* Default command-line argument values */
  rainbow_arg_state.what_doing = rainbow_indexing;
  rainbow_arg_state.query_filename = NULL;
  rainbow_arg_state.output_filename = NULL;
  rainbow_arg_state.num_trials = 0;
  rainbow_arg_state.infogain_words_to_print = 10;
  rainbow_arg_state.printing_class = 0;
  rainbow_arg_state.non_option_argi = 0;
  rainbow_arg_state.repeat_query = 0;
  rainbow_arg_state.vocab_map = NULL;
  rainbow_arg_state.hide_vocab_map = NULL;
  rainbow_arg_state.use_lisp_score_truncation = 1;
  rainbow_arg_state.loo_cv = 0;
  rainbow_arg_state.barrel_printing_format = NULL;
  rainbow_arg_state.hide_vocab_indices_filename = NULL;
  rainbow_arg_state.test_on_training = 0;
  
  /* Parse the command-line arguments. */
  argp_parse (&rainbow_argp, argc, argv, 0, 0, &rainbow_arg_state);

  if (rainbow_arg_state.what_doing == rainbow_indexing)
    {
      /* Strip any trailing `/'s from the classnames, so we can find the 
	 classname later using FILENAME_TO_CLASSNAME. */
      int argi, len;
      const char **rainbow_classnames;

      if (rainbow_arg_state.barrel_printing_format)
	{
	  rainbow_index_printed_barrel
	    (argv[rainbow_arg_state.non_option_argi]);
	}
      else
	{
	  for (argi = rainbow_arg_state.non_option_argi; argi < argc; argi++)
	    {
	      len = strlen (argv[argi]);
	      if (argv[argi][len-1] == '/')
		argv[argi][len-1] = '\0';
	    }

	  rainbow_classnames = 
	    (const char **)(argv + rainbow_arg_state.non_option_argi);
	  /* Index text in the directories. */
	  rainbow_index (argc - rainbow_arg_state.non_option_argi,
			 rainbow_classnames, 
			 rainbow_arg_state.output_filename);
	}
      if (bow_num_words ())
	rainbow_archive ();
      else
	bow_error ("No text documents found.");
      exit (0);
    }

  /* We are using an already built model.  Get it from disk. */
  rainbow_unarchive ();

  if (rainbow_arg_state.hide_vocab_indices_filename)
    {
      FILE *fp = 
	bow_fopen (rainbow_arg_state.hide_vocab_indices_filename, "r");
      int wi;
      while (fscanf (fp, "%d", &wi) == 1)
	bow_wi2dvf_hide_wi (rainbow_doc_barrel->wi2dvf, wi);
      fclose (fp);
    }

  /* (Re)set the weight-setting method, if requested with a `-m' on
     the command line. */
  if (bow_argp_method)
    rainbow_doc_barrel->method = bow_argp_method;

  /* Make the test/train split */
  if (rainbow_arg_state.what_doing != rainbow_testing)
    set_doc_types(rainbow_doc_barrel);

  /* Do things that update their own class/word weights. */

#if 0
  /* Compute the number of word pairs that co-occur in documents more
     than 0 times.  Did this for Jeff Schneider. */
  if (1)
    {
      static const int max_vocab_size = 10000;
      int vocab_sizes[] = {max_vocab_size, max_vocab_size};
      bow_bitvec *co_occurrences = bow_bitvec_new (2, vocab_sizes);
      int wi_pair[2];
      int wvi1, wvi2;
      bow_dv_heap *heap;
      bow_wv *doc_wv;
      int di;
      int num_co_occurrences;

      /* Make vocabulary size manageable. */
      bow_barrel_keep_top_words_by_infogain
	(max_vocab_size-1, rainbow_doc_barrel,
	 bow_barrel_num_classes (rainbow_class_barrel));

      /* Step through each document, setting bit for each word-pair 
	 co-occurrence. */
      heap = bow_test_new_heap (rainbow_doc_barrel);
      doc_wv = NULL;
      while ((di = bow_model_next_wv (heap, rainbow_doc_barrel, &doc_wv))
	     != -1)
	{
	  for (wvi1 = 0; wvi1 < doc_wv->num_entries; wvi1++)
	    {
	      for (wvi2 = 0; wvi2 < doc_wv->num_entries; wvi2++)
		{
		  wi_pair[0] = doc_wv->entry[wvi1].wi;
		  wi_pair[1] = doc_wv->entry[wvi2].wi;
		  bow_bitvec_set (co_occurrences, wi_pair, 1);
		}
	    }
	}
      /* Don't free the heap here because bow_model_next_wv() does it
	 for us. */
      
      /* Count the number of co-occurrences. */
      num_co_occurrences = 0;
      for (wvi1 = 0; wvi1 < max_vocab_size; wvi1++)
	{
	  for (wvi2 = 0; wvi2 < max_vocab_size; wvi2++)
	    {
	      wi_pair[0] = wvi1;
	      wi_pair[1] = wvi2;
	      if (bow_bitvec_value (co_occurrences, wi_pair))
		num_co_occurrences++;
	    }
	}

      printf ("Num co-occurrences = %d\n", num_co_occurrences);
      exit (0);
    }
#endif

  if (rainbow_arg_state.what_doing == rainbow_query_serving)
    {
      rainbow_socket_init (rainbow_arg_state.server_port_num, 0);
      while (1)
	{
	  rainbow_serve();
	}
    }

  /* Do things that don't require the class/word weights to be updated. */

  if (rainbow_arg_state.what_doing == rainbow_testing)
    {
      /* We are doing test trials, and making output for Perl. */
      rainbow_test (stdout);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_infogain_printing)
    {
      bow_infogain_per_wi_print
	(stdout, rainbow_doc_barrel,
	 bow_barrel_num_classes (rainbow_class_barrel),
	 rainbow_arg_state.infogain_words_to_print);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_foilgain_printing)
    {
      rainbow_print_foilgain (rainbow_arg_state.printing_class);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_infogain_pair_printing)
    {
      int s;
      bow_infogain_per_wi_new_using_pairs
	(rainbow_doc_barrel,
	 bow_barrel_num_classes (rainbow_class_barrel),
	 &s);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_doc_name_printing)
    {
      int di;
      bow_cdoc *cdoc;
      for (di = 0; di < rainbow_doc_barrel->cdocs->length; di++)
	{
	  cdoc = bow_array_entry_at_index (rainbow_doc_barrel->cdocs, di);
	  printf ("%s\n", cdoc->filename);
	}
      exit (0);
    }


  /* Do things necessary to update the class/word weights for the 
     command-line options. */

  /* Reduce vocabulary size by low info-gain words, if requested. */
  if (bow_prune_vocab_by_infogain_n)
    {
      /* Change barrel by removing words with small info gain. */
      bow_barrel_keep_top_words_by_infogain
	(bow_prune_vocab_by_infogain_n, rainbow_doc_barrel, 
	 bow_barrel_num_classes (rainbow_class_barrel));
    }
  /* Infogain pruning must be done before this vocab_map pruning, because
     infogain pruning first unhides all words! */
  /* Reduce vocabulary size by removing words not in a file listed
     on the command line. */
  if (rainbow_arg_state.vocab_map)
    {
      bow_barrel_prune_words_not_in_map (rainbow_doc_barrel,
					 rainbow_arg_state.vocab_map);
    }
  if (rainbow_arg_state.hide_vocab_map)
    {
      bow_barrel_prune_words_in_map (rainbow_doc_barrel,
				     rainbow_arg_state.hide_vocab_map);
    }
  if (bow_prune_words_by_doc_count_n)
    {
      bow_wi2dvf_hide_words_by_doc_count (rainbow_doc_barrel->wi2dvf,
					  bow_prune_words_by_doc_count_n);
    }
  if (bow_prune_vocab_by_occur_count_n)
    {
      bow_wi2dvf_hide_words_by_occur_count (rainbow_doc_barrel->wi2dvf,
					    bow_prune_vocab_by_occur_count_n);
    }

  /* Re-build the rainbow_class_barrel, if necessary */
  if (rainbow_doc_barrel->method != rainbow_class_barrel->method
      || rainbow_arg_state.vocab_map
      || rainbow_arg_state.hide_vocab_map
      || bow_prune_vocab_by_infogain_n
      || bow_prune_words_by_doc_count_n
      || bow_prune_vocab_by_occur_count_n
      || 1)
    {
      bow_free_barrel (rainbow_class_barrel);
      rainbow_class_barrel = 
	bow_barrel_new_vpc_with_weights (rainbow_doc_barrel);
    }
  
  /* Do things that require the vocabulary or class/word weights to
     have been updated. */
  
  if (rainbow_arg_state.what_doing == rainbow_word_count_printing)
    {
      bow_barrel_print_word_count (rainbow_class_barrel,
				   rainbow_arg_state.printing_class);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_barrel_printing)
    {
      bow_barrel_printf (rainbow_doc_barrel, stdout,
			 rainbow_arg_state.barrel_printing_format);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_printing_word_probabilities)
    {
      bow_naivebayes_print_word_probabilities_for_class
	(rainbow_class_barrel, rainbow_arg_state.printing_class);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_file_testing)
    {
      int argi;
      assert (rainbow_arg_state.non_option_argi < argc);
      for (argi = rainbow_arg_state.non_option_argi; argi < argc; argi++)
	rainbow_test_files (stdout, argv[argi]);
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_weight_vector_printing)
    {
#if 1
      bow_naivebayes_print_odds_ratio_for_class
	(rainbow_class_barrel, rainbow_arg_state.printing_class);
#else
      rainbow_print_weight_vector (rainbow_arg_state.printing_class);
#endif
      exit (0);
    }

  if (rainbow_arg_state.what_doing == rainbow_querying)
    {
      rainbow_query (stdin, stdout);
      exit (0);
    }

  exit (0);
}
#endif /* !RAINBOW_LISP */
