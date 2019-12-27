/* rainbow - a document classification front-end to libbow. */

#include "libbow.h"
#include <errno.h>		/* needed on DEC Alpha's */
#include <string.h>
#include <unistd.h>		/* for getopt(), maybe */
#include <stdlib.h>		/* for atoi() */
#include <string.h>		/* for strrchr() */
/* For mkdir() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if !HAVE_GETOPT_H
extern int optind;
extern char *optarg;
#endif /* !HAVE_GETOPT_H */

/* The version number of this program. */
#define RAINBOW_MAJOR_VERSION 0
#define RAINBOW_MINOR_VERSION 1


/* These global variables hold the results of parsing the command line
   switches. */

/* The directory in which we write/read the stats */
const char *data_dirname = NULL;

/* Where to find query text, or if NULL get query text from stdin */
const char *query_filename = NULL;

/* Name of file to find in each class directory; output the contents
   of this file instead of the classname. */
const char *output_filename = NULL;

/* Are we doing test trials?  If so, how many trials? */
int num_trials = 0;

/* What percentage of the data should be test data? */
int test_percentage = 30;

/* What weight-setting method to use? */
bow_method method = bow_method_naivebayes;

/* Are we printing info gain stats?  If so, how many of the top words? */
int infogain_words_to_print = 0;

/* Don't use words with occurrence counts less than this. */
int remove_words_with_occurrences_less_than = 2;

/* Reduce the vocabulary to this size, choosing the words with highest
   information gain.  Zero means don't reduce vocabulary by info gain. */
int num_top_words_to_keep = 0;

/* If this is non-zero, don't bother parsing, just unarchive and use
   the saved barrel in the datadir.  Use its counts, but reset the
   weights from scratch */
int reuse_archived_barrel_counts = 0;

/* The lexer to use for tokenizing the document. */
bow_lexer_gram rainbow_lexer;
bow_lexer_simple rainbow_underlying_lexer;
bow_lexer_indirect rainbow_html_lexer;


/* The structures that hold the data necessary for answering a query. */

bow_barrel *rainbow_doc_barrel;     /* the stats about words and documents */
bow_barrel *rainbow_class_barrel;   /* the stats about words and classes */
const char **rainbow_classnames;
/* The static structure in bow/int4word.c is also used. */


/* Writing and reading the word/document stats to disk. */

#define VOCABULARY_FILENAME "vocabulary"
#define DOC_BARREL_FILENAME "doc-barrel"
#define CLASS_BARREL_FILENAME "class-barrel"
#define OUTPUTNAME_FILENAME "outfile"

/* Write the stats in the directory DATA_DIRNAME. */
void
rainbow_archive ()
{
  char filename[BOW_MAX_WORD_LENGTH];
  char *fnp;
  FILE *fp;

  strcpy (filename, data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, OUTPUTNAME_FILENAME);
  fp = bow_fopen (filename, "w");
  if (output_filename)
    fprintf (fp, "%s\n", output_filename);
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
  int i;

  bow_verbosify (bow_progress, "Loading data files...\n");

  strcpy (filename, data_dirname);
  strcat (filename, "/");
  fnp = filename + strlen (filename);

  strcpy (fnp, OUTPUTNAME_FILENAME);
  fp = bow_fopen (filename, "r");
  buf[0] = '\0';
  fscanf (fp, "%s", buf);
  output_filename = strdup (buf);
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

  /* Extract the CLASSNAMES from the class barrel. */
  rainbow_classnames = bow_malloc (rainbow_class_barrel->cdocs->length
				   * sizeof (char*));
  for (i = 0; i < rainbow_class_barrel->cdocs->length; i++)
    {
      bow_cdoc *cdoc = 
	bow_array_entry_at_index (rainbow_class_barrel->cdocs, i);
      assert (cdoc->filename);
      rainbow_classnames[i] = strdup (cdoc->filename);
      assert (rainbow_classnames[i]);
    }
}

/* Given a fully-specified file path name (all the way from `/'),
   return just the last filename part of it. */
static inline const char *
filename_to_classname (const char *filename)
{
  return strrchr (filename, '/') + 1;
}



/* Building the word/document stats. */

void
rainbow_index (int num_classes, const char *classdir_names[],
	       const char *exception_name)
{
  int class_index;

  void do_indexing ()
    {
      if (rainbow_doc_barrel)
	bow_barrel_free (rainbow_doc_barrel);
      /* Index all the documents. */
      rainbow_doc_barrel = bow_barrel_new (0, 0, sizeof (bow_cdoc), NULL);
      rainbow_doc_barrel->method = method;
      for (class_index = 0; class_index < num_classes; class_index++)
	{
	  bow_verbosify (bow_progress, "Class `%s'\n  ", 
			 strrchr(classdir_names[class_index], '/')+1);
	  /* This function traverses the directory class directory
	     gathering word/document stats.  Return the number of
	     documents indexed.  This gathers stats on individual
	     documents; we have yet to "sum together the word vectors
	     of all documents for each particular class". */
	  if (bow_barrel_add_from_text_dir (rainbow_doc_barrel, 
					    classdir_names[class_index],
					    exception_name,
					    class_index)
	      == 0)
	    bow_error ("No text files found in directory `%s'", 
		       classdir_names[class_index]);
	}
    }

  if (reuse_archived_barrel_counts)
    {
      /* Get the barrel with word counts by unarchiving data files. */
      rainbow_unarchive ();
    }
  else
    {
      /* Do all the parsing to build a barrel with word counts. */
      if (remove_words_with_occurrences_less_than)
	{
	  /* Parse all the documents to get word occurrence counts. */
	  for (class_index = 0; class_index < num_classes; class_index++)
	    {
	      bow_verbosify (bow_progress,
			     "Class `%s'\n  ", 
			     filename_to_classname
			     (classdir_names[class_index]));
	      bow_words_add_occurrences_from_text_dir
		(classdir_names[class_index], "");
	    }
	  bow_words_remove_occurrences_less_than
	    (remove_words_with_occurrences_less_than);
	  /* Now insist that future calls to bow_word2int*() will not
	     register new words. */
	  bow_word2int_do_not_add = 1;
	}

      do_indexing ();
    }

  if (num_top_words_to_keep)
    /* Change barrel by removing words with small information gain. */
    bow_barrel_keep_top_words_by_infogain (num_top_words_to_keep, 
					   rainbow_doc_barrel, num_classes);

  /* Combine the documents into class statistics. */
  rainbow_class_barrel = 
    bow_barrel_new_vpc_with_weights (rainbow_doc_barrel, classdir_names);
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
rainbow_query ()
{
  /* Show as many hits as there are classes. */
  int num_hits_to_show = rainbow_class_barrel->cdocs->length;
  bow_doc_score *hits;
  int actual_num_hits;
  int i;
  bow_wv *query_wv;

  hits = alloca (sizeof (bow_doc_score) * num_hits_to_show);

  /* Get the query text, and create a "word vector" from the query text. */
  if (query_filename)
    {
      FILE *fp;
      fp = bow_fopen (query_filename, "r");
      query_wv = bow_wv_new_from_text_fp (fp);
      fclose (fp);
    }
  else
    {
      bow_verbosify (bow_quiet, 
		     "Type your query text now.  End with a Control-D.\n");
      query_wv = bow_wv_new_from_text_fp (stdin);
    }

  if (query_wv->num_entries == 0)
    {
      bow_verbosify (bow_quiet, "No query text found.\n");
      return 0;
    }

  /* Get the best matching documents. */
  bow_wv_set_weights (query_wv, method);
  bow_wv_set_weight_normalizer (query_wv, method);
  actual_num_hits = bow_get_best_matches (rainbow_class_barrel, query_wv,
					  hits, num_hits_to_show);

  /* Print them. */
  printf ("\n");
  for (i = 0; i < actual_num_hits; i++)
    {
      bow_cdoc *cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, 
						 hits[i].di);
      if (strlen (output_filename))
	{
	  char buf[1024];
	  strcpy (buf, cdoc->filename);
	  strcat (buf, "/");
	  strcat (buf, output_filename);
	  print_file (buf);
	}
      else
	{
	  printf ("%s %g\n", cdoc->filename, hits[i].weight);
	}
    }

  return actual_num_hits;
}


/* Run test trials, outputing results to TEST_FP.  The results are
   indended to be read and processed by the Perl script
   ./rainbow-stats. */
void
rainbow_test (FILE *test_fp)
{
  int tn;			/* trial number */
  int num_test_docs;		/* how many doc's will be for testing */
  bow_dv_heap *test_heap;	/* we'll extract test WV's from here */
  bow_wv *query_wv;
  int di;			/* a document index */
  bow_doc_score *hits;
  int num_hits_to_retrieve = rainbow_class_barrel->cdocs->length;
  int actual_num_hits;
  int hi;			/* hit index */
  bow_cdoc *doc_cdoc;
  bow_cdoc *class_cdoc;

  hits = alloca (sizeof (bow_doc_score) * num_hits_to_retrieve);

  /* Calculate the number of testing documents according to TEST_PERCENTAGE. */
  num_test_docs = (rainbow_doc_barrel->cdocs->length * test_percentage) / 100;

  /* Loop once for each trial. */
  for (tn = 0; tn < num_trials; tn++)
    {
      fprintf (test_fp, "#%d\n", tn);

      /* Randomly set which doc's are for training and which are testing. */
      bow_test_split (rainbow_doc_barrel, num_test_docs);

      /* Re-create the vector-per-class barrel in accordance with the
	 new train/test settings. */
      bow_barrel_free (rainbow_class_barrel);
      rainbow_class_barrel = 
	bow_barrel_new_vpc_with_weights (rainbow_doc_barrel, 
					 rainbow_classnames);

      /* Create the heap from which we'll get WV's. */
      test_heap = bow_test_new_heap (rainbow_doc_barrel);

      /* Initialize QUERY_WV so BOW_TEST_NEXT_WV() knows not to try to free */
      query_wv = NULL;

      /* Loop once for each test document. */
      while ((di = bow_test_next_wv (test_heap, rainbow_doc_barrel, &query_wv))
	     != -1)
	{
	  doc_cdoc = bow_array_entry_at_index (rainbow_doc_barrel->cdocs, 
					       di);
	  class_cdoc = bow_array_entry_at_index (rainbow_class_barrel->cdocs, 
						 doc_cdoc->class);
	  fprintf (test_fp, "%s %s ", 
		   doc_cdoc->filename, 
		   filename_to_classname(class_cdoc->filename));
	  bow_wv_set_weights (query_wv, method);
	  bow_wv_set_weight_normalizer (query_wv, method);
	  actual_num_hits = bow_get_best_matches (rainbow_class_barrel, 
						  query_wv, hits,
						  num_hits_to_retrieve);
	  for (hi = 0; hi < actual_num_hits; hi++)
	    {
	      class_cdoc = 
		bow_array_entry_at_index (rainbow_class_barrel->cdocs,
					  hits[hi].di);
	      fprintf (test_fp, "%s:%g ", 
		       filename_to_classname (class_cdoc->filename),
		       hits[hi].weight);
	    }
	  fprintf (test_fp, "\n");
	}
    }
}


/* The main() function. */

volatile void
rainbow_print_usage (const char *progname)
{
  fprintf
    (stderr,
     "usage:\n"
     " %s [-d datadir] [-v <verbosity_level>]\n"
     "  (where\n"
     "   `datadir' is the directory in which to read/write data\n"
     "   `verbosity_level' is 0=silent, 1=quiet, 2=show-progress, ... 5=max)\n"
     "lexing options\n"
     "  [-s]  don't use the stoplist (i.e. don't prune frequent words)\n"
     "  [-S]  turn off stemming of the tokens\n"
     "  [-H]  ignore HTML tokens\n"
     "  [-g <N>]  set N for N-gram lexer (default=1)\n"
     "  [-h]  skip over email or news header\n"
     "then, for indexing and setting weights\n"
     " -i <class1_dir> <class2_dir> ...\n"
     "  [-L]  don't lex to get word counts, instead read archived barrel\n"
     "  [-f <file>]  prints file contents instead of class_dir at query time\n"
     "  [-T <N>]  prune away top N words by information gain\n"
     "  [-R <N>]  remove words with occurrence counts less than N\n"
     "  [-V]  print version information and exit\n"
     "  [-m <mname>]  set method to <mname> (eg. naivebayes, tfidf, prind)\n"
     "  [-U]  use non-uniform prior probabilities for the prind method\n"
     "or, for querying\n"
     " -q [<file_containing_query>]\n"
     "  [-n <N>]  prints the N best-matching documents to the query\n"
     "  [-I <N>]  prints the top N words with highest information gain\n"
     "  [-t <N>]  perform N testing trials\n"
     "  [-p <N>]  (with -t) Use N%% of the documents as test instances\n"
     , progname);
  exit (-1);
}

int
main (int argc, const char *argv[])
{
  int i_or_q = 1;		/* Are we indexing or querying, 0=indexing,
				   1=querying. */

  /* A feable attempt to see if we are running under gdb; if so, don't
     print backspaces. */
  if (strlen (argv[0]) > 39)
    bow_verbosity_use_backspace = 0;

  /* If the file ./.bow-stopwords exists, load the extra words into
     the stoplist. */
  bow_stoplist_add_from_file ("./.bow-stopwords");

  /* If the file ~/.bow-stopwords exists, load the extra words into
     the stoplist. */
  {
    const char sw_fn[] = "/.bow-stopwords";
    const char *home = getenv ("HOME");
    char filename[strlen (home) + strlen (sw_fn) + 1];
    strcpy (filename, home);
    strcat (filename, sw_fn);
    bow_stoplist_add_from_file (filename);    
  }
 
  /* Make our own local copy of the N-gram lexer, modify it, and
     make it the default lexer. */
  rainbow_lexer = *bow_gram_lexer;
  rainbow_lexer.gram_size = 1;
  rainbow_underlying_lexer = *bow_alpha_lexer;
  /* Just in case we need it. */
  rainbow_html_lexer = *bow_html_lexer;

  rainbow_lexer.indirect_lexer.underlying_lexer =
    (bow_lexer*) &rainbow_underlying_lexer;
  bow_default_lexer = (bow_lexer*) &rainbow_lexer;

  /* Parse the command-line arguments. */
  while (1)
    {
      int c;			/* command-line switch returned by getopt() */

      c = getopt (argc, (char**)argv, "VqihSHUL?v:I:T:R:n:d:f:t:p:g:m:");
      if (c == -1)
	break;			/* no more options */
      switch (c)
	{
	  /* Switches for both indexing and querying */
	case 'v':
	  bow_verbosity_level = atoi (optarg);
	  break;
	case 'd':
	  data_dirname = optarg;
	  break;
	case 'V':
	  printf ("Rainbow Version %d.%d\n"
		  "Bag-Of-Words Library Version %d.%d\n"
		  "Send bug reports to <mccallum@cs.cmu.edu>\n",
		  RAINBOW_MAJOR_VERSION, RAINBOW_MINOR_VERSION, 
		  BOW_MAJOR_VERSION, BOW_MINOR_VERSION);
	  exit (0);

	  /* Switches for lexing (for both indexing and querying) */
	case 's':
	  /* Turn off use of the stoplist */
	  rainbow_underlying_lexer.stoplist_func = NULL;
	  break;
	case 'S':
	  /* Turn off stemming */
	  rainbow_underlying_lexer.stem_func = NULL;
	  break;
	case 'H':
	  /* Ignore HTML tokens (everythin inside <...>) */
	  rainbow_lexer.indirect_lexer.underlying_lexer =
	    (bow_lexer*) &rainbow_html_lexer;
	  rainbow_html_lexer.underlying_lexer =
	    (bow_lexer*) &rainbow_underlying_lexer;
	  break;
	case 'g':
	  /* Set the N-gram size. */
	  rainbow_lexer.gram_size = atoi (optarg);
	  break;
	case 'h':
	  /* Skip over the email or news header. */
	  bow_default_lexer->document_start_pattern = "\n\n";
	  break;

	  /* Switches for indexing */
	case 'i':
	  i_or_q = 0;
	  break;
	case 'L':
	  /* Don't do lexing to build barrel of word counts; instead,
	     read in archived data files. */
	  reuse_archived_barrel_counts = 1;
	  break;
	case 'f':
	  output_filename = optarg;
	  break;
	case 'T':
	  /* Prune away top N words by information gain */
	  num_top_words_to_keep = atoi (optarg);
	  break;
	case 'R':
	  remove_words_with_occurrences_less_than = atoi (optarg);
	  break;

	  /* Switches for querying */
	case 'q':
	  i_or_q = 1;
	  break;
	case 'I':
	  /* Print out OPTARG number of vocab words ranked by infogain. */
	  infogain_words_to_print = atoi (optarg);
	  break;
	case 'm':
	  method = bow_str2method (optarg);
	  break;
	case 'U':
	  /* Don't have PrTFIDF use uniform class prior probabilities */
	  bow_prind_uniform_priors = 0;
	  break;
	case 't':
	  num_trials = atoi (optarg);
	  break;
	case 'p':
	  test_percentage = atoi (optarg);
	  break;

	case '?':
	default:
	  rainbow_print_usage (argv[0]);
	}
    }

  /* Build the default data directory name, if it wasn't specified on
     the command line. */
  if (!data_dirname)
    {
      static const char default_datadir[] = "/.rainbow";
      char *homedir = getenv ("HOME");
      char *dirname = bow_malloc (strlen (homedir) 
				  + strlen (default_datadir) + 1);
      strcpy (dirname, homedir);
      strcat (dirname, default_datadir);
      data_dirname = dirname;

    }
  /* Create the data directory, if it doesn't exist already. */
  if (mkdir (data_dirname, 0777) == 0)
    bow_verbosify (bow_quiet, "Created directory `%s'.\n", data_dirname);
  else if (errno != EEXIST)
    bow_error ("Couldn't create default data directory `%s'", data_dirname);

  if (i_or_q == 0)
    {
      if (optind >= argc)
	bow_error ("Missing <class1_dir>... arguments");
      rainbow_classnames = argv + optind;
      rainbow_index (argc - optind, rainbow_classnames, output_filename);
      if (bow_num_words ())
	rainbow_archive ();
      else
	bow_error ("No text documents found.");
    }
  else
    {
      if (optind < argc)
	query_filename = argv[optind];
      rainbow_unarchive ();
      if (num_trials)
	{
	  /* We are doing test trials, and making output for Perl. */
	  rainbow_test (stdout);
	}
      else if (infogain_words_to_print)
	{
	  bow_infogain_per_wi_print (rainbow_doc_barrel,
				     rainbow_class_barrel->cdocs->length,
				     infogain_words_to_print);
	}
      else
	{
	  /* We aren't doing test trials; we are doing an individual query. */
	  rainbow_query ();
	}
    }

  exit (0);
}
