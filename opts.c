/* Handling command-line options that apply across the whole of libbow. */

#include <argp.h>
#include <bow/libbow.h>

/* For mkdir() and stat() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


/* Global variables whose value is set by bow_argp functions, but
   which must be examined by some other function (called later) in
   order to have any effect. */

/* Remove all but the top N words by selecting words with highest
   information gain */
int bow_prune_vocab_by_infogain_n = 0;

/* Remove words that occur less than N times */
int bow_prune_vocab_by_occur_count_n = 0;

/* The weight-setting and scoring method set on the command-line. */
bow_method *bow_argp_method = NULL;

/* The directory in which we'll store word-vector data. */
const char *bow_data_dirname = NULL;

/* If non-zero, print to stdout the contribution of each word to
   each class. */
int bow_print_word_scores = 0;

/* If non-zero, use equal prior probabilities on classes when setting
   weights, calculating infogain and scoring */
int bow_uniform_class_priors = 0;

/* If non-zero, use binary occurrence counts for words. */
int bow_binary_word_counts = 0;

/* Don't lex any files with names matching this. */
const char *bow_exclude_filename = NULL;

/* Pipe the files through this shell command before lexing. */
const char *bow_lex_pipe_command = NULL;

/* If non-zero, check for eencoding blocks before istext() says that
   the file is text. */
int bow_istext_avoid_uuencode = 0;

/* Value added to key to get the key of the opposite option.  For
   example "do not use stoplist" has key 's'; "use stoplist" has key
   's'+KEY_OPPOSITE. */
#define KEY_OPPOSITE 256

enum {
  APPEND_STOPLIST_FILE_KEY = 10000,
  PRINT_WORD_SCORES_KEY,
  UNIFORM_CLASS_PRIORS_KEY,
  NAIVEBAYES_SCORE_WITH_LOG_PROBS_KEY,
  BINARY_WORD_COUNTS_KEY,
  EXCLUDE_FILENAME_KEY,
  LEX_PIPE_COMMAND_KEY,
  ISTEXT_AVOID_UUENCODE_KEY,
  LEX_WHITE_KEY
};

static struct argp_option bow_options[] =
{
  {0, 0, 0, 0,
   "General options", 1},
  {"verbosity", 'v', "LEVEL", 0,
   "Set amount of info printed while running; "
   "(0=silent, 1=quiet, 2=show-progess,...5=max)"},
  {"no-backspaces", 'b', 0, 0,
   "Don't use backspace when verbosifying progress (good for use in emacs)"},
  {"data-dir", 'd', "DIR", 0,
   "Set the directory in which to read/write word-vector data "
   "(default=~/.<program_name>)."},

  {0, 0, 0, 0,
   "Lexing options", 2},
  {"skip-header", 'h', 0, 0,
   "Avoid lexing news/mail headers by scanning forward until two newlines."},
  {"no-stoplist", 's', 0, 0,
   "Do not toss lexed words that appear in the stoplist."},
  {"use-stoplist", 's'+KEY_OPPOSITE, 0, 0,
   "Toss lexed words that appear in the stoplist. "
   "(usually the default, depending on lexer)"},
  {"append-stoplist-file", APPEND_STOPLIST_FILE_KEY, "FILE", 0,
   "Add words in FILE to the stoplist."},
  {"no-stemming", 'S'+KEY_OPPOSITE, 0, 0,
   "Do not modify lexed words with a stemming function. "
   "(usually the default, depending on lexer)"},
  {"use-stemming", 'S', 0, 0,
   "Modify lexed words with the `Porter' stemming function."},
  {"gram-size", 'g', "N", 0,
   "Create tokens for all 1-grams,... N-grams."},
  {"exclude-filename", EXCLUDE_FILENAME_KEY, "FILENAME", 0,
   "When scanning directories for text files, skip files with name "
   "matching FILENAME."},
  {"istext-avoid-uuencode", ISTEXT_AVOID_UUENCODE_KEY, 0, 0,
   "Check for uuencoded blocks before saying that the file is text, "
   "and say no if there are many lines of the same length."},
  {"lex-pipe-command", LEX_PIPE_COMMAND_KEY, "SHELLCMD", 0,
   "Pipe files through this shell command before lexing them."},

  {0, 0, 0, 0,
   "Mutually exclusive choice of lexers", 3},
  {"skip-html", 'H', 0, 0,
   "Skip HTML tokens when lexing."},
  {"keep-html", 'H'+KEY_OPPOSITE, 0, OPTION_HIDDEN,
   "Treat HTML tokens the same as any other chars when lexing. (default)"},
  {"lex-white", LEX_WHITE_KEY, 0, 0,
   "Use a special lexer that delimits tokens by whitespace only, and "
   "does not change the contents of the token at all---no downcasing, "
   "no stemming, no stoplist, nothing.  Ideal for use with an externally-"
   "written lexer interfaced to rainbow with --lex-pipe-cmd."},
  {"lex-for-usenet", 'U', 0, 0,
   "Use a special lexer for UseNet articles, ignore some headers and "
   "uuencoded blocks."},

  {0, 0, 0, 0,
   "Feature-selection options", 4},
  {"prune-vocab-by-infogain", 'T', "N", 0,
   "Remove all but the top N words by selecting words with highest "
   "information gain."},
  {"prune-vocab-by-occur-count", 'O', "N", 0,
   "Remove words that occur less than N times."},

  {0, 0, 0, 0,
   "Weight-vector setting/scoring method options", 5},
  {"method", 'm', "METHOD", 0,
   "Set the weight-method; METHOD may be one of: "},
  {"print-word-scores", PRINT_WORD_SCORES_KEY, 0, 0,
   "During scoring, print the contribution of each word to each class."},
  {"uniform-class-priors", UNIFORM_CLASS_PRIORS_KEY, 0, 0,
   "When setting weights, calculating infogain and scoring, use equal prior "
   "probabilities on classes."},
  {"binary-word-counts", BINARY_WORD_COUNTS_KEY, 0, 0,
   "Instead of using integer occurrence counts of words to set weights, "
   "use binary absence/presence."},

  {0, 0}
};

static int
parse_bow_opt (int opt, char *arg, struct argp_state *state)
{
  switch (opt)
    {
    case 'v':
      bow_verbosity_level = atoi (optarg);
      break;
    case 'b':
      /* Don't print backspaces when verbosifying at level BOW_PROGRESS. */
      bow_verbosity_use_backspace = 0;
      break;
    case 'd':
      /* Set name of the directory in which we'll store word-vector data. */
      bow_data_dirname = arg;
      break;


      /* Lexing options. */

    case 'h':
      /* Avoid lexing news/mail headers by scanning fwd until two newlines */
      bow_default_lexer->document_start_pattern = "\n\n";
      break;
    case 's':
      /* Do not toss lexed words that appear in the stoplist */
      bow_default_lexer_simple->stoplist_func = NULL;
      break;
    case 's'+KEY_OPPOSITE:
      /* Toss lexed words that appear in the stoplist */
      bow_default_lexer_simple->stoplist_func = bow_stoplist_present;
      break;
    case 'S':
      /* Modify lexed words with the `Porter' stemming function */
      bow_default_lexer_simple->stem_func = bow_stem_porter;
      break;
    case 'S'+KEY_OPPOSITE:
      /* Do not modify lexed words with a stemming function. (default) */
      bow_default_lexer_simple->stem_func = NULL;
      break;
    case APPEND_STOPLIST_FILE_KEY:
      bow_stoplist_add_from_file (arg);
      break;
    case 'g':
      /* Create tokens for all 1-grams,... N-grams */
      {
	int n = atoi (arg);
	if (n == 0)
	  {
	    fprintf (stderr,
		     "--gram-size, -N:  gram size must be a positive int\n");
	    return ARGP_ERR_UNKNOWN;
	  }
	bow_default_lexer_gram->gram_size = n;
      }
    case 'H':
      /* Skip HTML tokens when lexing */
      bow_default_lexer_indirect->underlying_lexer = 
	(bow_lexer*) bow_default_lexer_html;
      break;
    case LEX_WHITE_KEY:
      /* Use the whitespace lexer */
      bow_default_lexer_indirect->underlying_lexer = 
	(bow_lexer*) bow_default_lexer_white;
      break;
    case 'H'+KEY_OPPOSITE:
      /* Treat HTML tokens the same as any other chars when lexing (default) */
      bow_default_lexer_indirect->underlying_lexer = 
	(bow_lexer*) bow_default_lexer_simple;
      break;
    case 'U':
      /* Use a special lexer for UseNet articles, ignore some headers and
	 uuencoded blocks. */
      bow_default_lexer_indirect->underlying_lexer = 
	(bow_lexer*) bow_default_lexer_email;
      break;
    case EXCLUDE_FILENAME_KEY:
      bow_exclude_filename = arg;
      break;
    case LEX_PIPE_COMMAND_KEY:
      bow_lex_pipe_command = arg;
      break;
    case ISTEXT_AVOID_UUENCODE_KEY:
      bow_istext_avoid_uuencode = 1;
      break;

      /* Feature selection options. */

    case 'T':
      /* Remove all but the top N words by selecting words with highest 
	 information gain */
      bow_prune_vocab_by_infogain_n = atoi (arg);
      break;
    case 'O':
      /* Remove words that occur less than N times */
      bow_prune_vocab_by_occur_count_n = atoi (arg);
      break;

    case 'm':
      bow_argp_method = bow_method_at_name (arg);
      break;
    case PRINT_WORD_SCORES_KEY:
      bow_print_word_scores = 1;
      break;
    case UNIFORM_CLASS_PRIORS_KEY:
      bow_uniform_class_priors = 1;
      break;
    case BINARY_WORD_COUNTS_KEY:
      /* Use binary absence/presence, instead of integer occurrence
         counts for words. */
      bow_binary_word_counts = 1;
      break;

    case ARGP_KEY_INIT:
      /* Things to do before any arguments are processed. */

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
 
      /* Build the default data directory name, in case it wasn't
	 specified on the command line. */
      assert (program_invocation_short_name);
      if (!bow_data_dirname)
	{
	  char *homedir = getenv ("HOME");
	  char *dirname = bow_malloc (strlen (homedir) 
				      + strlen ("/.")
				      + strlen (program_invocation_short_name)
				      + 1);
	  strcpy (dirname, homedir);
	  strcat (dirname, "/.");
	  strcat (dirname, program_invocation_short_name);
	  bow_data_dirname = dirname;
	}

    case ARGP_KEY_ARG:
      break;
    case ARGP_KEY_END:
      /* Create the data directory, if it doesn't exist already. */
      {
	struct stat st;
	int e;
	e = stat (bow_data_dirname, &st);
	if (e == 0)
	  {
	    /* Assume this means the file exists. */
	    if (!S_ISDIR (st.st_mode))
	      bow_error ("`%s' already exists, but is not a directory");
	  }
	else
	  {
	    if (mkdir (bow_data_dirname, 0777) == 0)
	      bow_verbosify (bow_quiet, "Created directory `%s'.\n", 
			     bow_data_dirname);
	    else if (errno != EEXIST)
	      bow_error ("Couldn't create default data directory `%s'",
			 bow_data_dirname);
	  }
      }

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static char *
_help_filter (int key, const char *text, void *input)
{
  char *ret;

  /* Add the names of the available methods to the help text. */
  if (key == 'm')
    {
      static const len = 1024;
      char methodnames[len];
      int i;
      bow_method *m;

      methodnames[0] = '\0';
      for (i = bow_methods->array->length-1; i >= 0; i--)
	{
	  m = bow_sarray_entry_at_index (bow_methods, i);
	  strcat (methodnames, m->name);
	  strcat (methodnames, ", ");
	}
      strcat (methodnames, "default=naivebayes.");
      assert (strlen (methodnames) < len);
      ret = malloc (strlen (text) + len);
      strcpy (ret, text);
      strcat (ret, methodnames);
      return ret;
    }
  return (char*)text;
}

/* This may be used with argp_parse to parse standard libbow startup
   options, possible chained onto the end of a user argp structure.  */
const struct argp bow_argp =
{
  bow_options,			/* data structure describing cmdline options */
  parse_bow_opt,		/* the function to handle the options */
  0,				/* non-option argument documention */
  0,				/* extra text printed before and after help */
  0,				/* argp children */
  _help_filter
};


#define MAX_NUM_CHILDREN 10
struct argp_child bow_argp_children[MAX_NUM_CHILDREN] =
{
  {
    &bow_argp,			/* the argp structure */
    0,				/* flags for child */
    "Libbow options",		/* optional header */
    999				/* group (general lib flags at end of help)*/
  },
  {0}
};

/* The number of children already initialized in the const assignment above. */
static int bow_argp_children_length = 1;

void
bow_argp_add_child (struct argp_child *child)
{
  assert (bow_argp_children_length-1 < MAX_NUM_CHILDREN);
  memcpy (bow_argp_children + bow_argp_children_length,
	  child,
	  sizeof (struct argp_child));
  bow_argp_children_length++;
#if 1
  memset (bow_argp_children + bow_argp_children_length,
	  0, sizeof (struct argp_child));
#endif
}

static void
_print_version (FILE *stream, struct argp_state *state)
{
  if (argp_program_version)
    /* If this is non-zero, then the program's probably defined it, so let
       that take precedence over the default.  */
    fprintf (stream, "%s\n", argp_program_version);

  /* And because libbow is a changing, integral part, put our
     information out too. */
  fprintf (stream, "libbow %d.%d\n",
	   BOW_MAJOR_VERSION, BOW_MINOR_VERSION);
}

void (*argp_program_version_hook) (FILE *stream, struct argp_state *state)
     = _print_version;
