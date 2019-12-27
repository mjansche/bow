/* arrow - a document retreival front-end to libbow. */

#include "libbow.h"
#include <errno.h>		/* needed on DEC Alpha's */

#if HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif /* HAVE_GETOPT_H */

/* For mkdir() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* The version number of this program. */
#define ARROW_MAJOR_VERSION 0
#define ARROW_MINOR_VERSION 1


/* These global variables hold the results of parsing the command line
   switches. */

/* How many matching documents to print. */
int num_hits_to_show = 1;
/* The directory in which we write/read the stats */
const char *data_dirname = NULL;
/* Where to find query text, or if NULL get query text from stdin */
const char *query_filename = NULL;
/* Where to find the text files to index. */
const char *text_dirname = NULL;


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

  strcpy (filename, data_dirname);
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

  strcpy (filename, data_dirname);
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

/* Traverse the directory TEXT_DIRNAME, gathering word/document stats. 
   Return the number of documents indexed. */
int
arrow_index ()
{
  arrow_barrel = bow_barrel_new (0, 0, sizeof (bow_cdoc), 0);
  bow_barrel_add_from_text_dir (arrow_barrel, text_dirname, 0, 0);
  bow_barrel_set_weights (arrow_barrel);
  bow_barrel_set_weight_normalizers (arrow_barrel);
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
   NUM_HITS_TO_SHOW.  If QUERY_FILENAME is non-null, the query text
   will be obtained from that file; otherwise it will be prompted for
   and read from stdin. */
int
arrow_query ()
{
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

  bow_wv_set_weights (query_wv, arrow_barrel->method);
  bow_wv_set_weight_normalizer (query_wv, arrow_barrel->method);

  /* Get the best matching documents. */
  actual_num_hits = bow_get_best_matches (arrow_barrel, query_wv,
					  hits, num_hits_to_show);

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



/* The main() function. */

volatile void
arrow_print_usage (const char *progname)
{
  fprintf (stderr,
	   "usage:\n"
	   "  %s [-d <data_dir>] [-v <verbosity_level>]\n"
	   "then, for indexing\n"
	   "  -i <source_dir>\n"
	   "or, for querying\n"
	   "  -q [<file_containing_query>] [-n <N>]\n"
	   "  (where `-n <N>' prints the N best-matching documents)\n",
	   progname);
  exit (-1);
}

int
main (int argc, char *argv[])
{
  int i_or_q = 1;		/* Are we indexing or querying, 0=indexing,
				   1=querying. */

  /* A feable attempt to see if we are running under gdb; if so, don't
     print backspaces. */
  if (strlen (argv[0]) > 10)
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

  /* Parse the command-line arguments. */
  while (1)
    {
      int c;			/* command-line switch returned by getopt() */

      c = getopt (argc, argv, "Vqivn:d:f:");
      if (c == -1)
	break;			/* no more options */
      switch (c)
	{
	case 'q':
	  i_or_q = 1;
	  break;
	case 'i':
	  i_or_q = 0;
	  break;
	case 'v':
	  bow_verbosity_level = atoi (optarg);
	  break;
	case 'd':
	  data_dirname = optarg;
	  break;
	case 'n':
	  num_hits_to_show = 0;
	  break;
	case 'V':
	  printf ("Arrow Version %d.%d\n"
		  "Bag-Of-Words Library Version %d.%d\n"
		  "Send bug reports to <mccallum@cs.cmu.edu>\n",
		  ARROW_MAJOR_VERSION, ARROW_MINOR_VERSION, 
		  BOW_MAJOR_VERSION, BOW_MINOR_VERSION);
	  exit (0);
	default:
	  arrow_print_usage (argv[0]);
	}
    }

  /* Build the default data directory name, if it wasn't specified on
     the command line. */
  if (!data_dirname)
    {
      static const char default_datadir[] = "/.arrow";
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
	bow_error ("Missing <text_dirname> argument");
      text_dirname = argv[optind++];
      if (arrow_index ())
	arrow_archive ();
      else
	bow_error ("No text documents found.");
    }
  else
    {
      if (optind < argc)
	query_filename = argv[optind];
      arrow_unarchive ();
      arrow_query ();
    }

  exit (0);
}
