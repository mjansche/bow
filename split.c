/* Splitting the documents into training and test sets. */

/* Copyright (C) 1997, 1998 Andrew McCallum

   Written by:  Sean Slattery <jslttery@cs.cmu.edu>

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

#include <argp.h>
#include <bow/libbow.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* Different ways of specifying how to do the split */
typedef enum {
  bow_files_source_file = 10,     /* get docs for a type from a file */
  bow_files_source_fraction,       /* do a random fraction split of docs */
  bow_files_source_number,        /* do a random number split of docs */
  bow_files_source_remaining,     /* use remaining docs for a file type */
  bow_files_source_num_per_class, /* pick a random number from each class */
  bow_files_source_fancy_counts   /* pick the random number specified for each class */
} bow_files_source_type;

/* A structure for maintining information about fancy counts */
typedef struct _bow_split_fancy_count {
  char *class_name;
  int num_docs;
} bow_split_fancy_count;


/* How documents of each type are being selected */
static bow_files_source_type bow_test_files_source = bow_files_source_number;
static bow_files_source_type bow_train_files_source = bow_files_source_remaining;
static bow_files_source_type bow_unlabeled_files_source = bow_files_source_number;
static bow_files_source_type bow_ignore_files_source = bow_files_source_number;

/* The fraction used for selecting each type */
static float bow_test_fraction;
static float bow_train_fraction;
static float bow_unlabeled_fraction;
static float bow_ignore_fraction;

/* The number for selecting each type for both num_per_class and number */
static int bow_test_number = 0;
static int bow_train_number;
static int bow_unlabeled_number = 0;
static int bow_ignore_number = 0;

/* The filename containing lists of documents for each type */
static char *bow_test_filename;
static char *bow_train_filename;
static char *bow_unlabeled_filename;
static char *bow_ignore_filename;

/* The numbers to select from each class for fancy counts for each type*/
static bow_split_fancy_count *bow_test_fancy_counts;
static bow_split_fancy_count *bow_train_fancy_counts;
static bow_split_fancy_count *bow_unlabeled_fancy_counts;
static bow_split_fancy_count *bow_ignore_fancy_counts;

/* When using files to set the test/train split, compare filenames by
   using this many directory components as basename only, not their
   complete filenames. */
int bow_test_set_files_use_basename = 0;


enum {
  TEST_SOURCE = 5000,
  TRAIN_SOURCE,
  UNLABELED_SOURCE,
  IGNORE_SOURCE,
  SET_TEST_FILES_KEY,
  SET_TEST_FILES_USE_BASENAME_KEY
};

static struct argp_option bow_split_options[] =
{
  {0, 0, 0, 0,
   "Splitting options:", 10},
  {"test-set", TEST_SOURCE, "SOURCE", 0,
   "How to select the testing documents.  A number between 0 and 1 inclusive "
   "with a decimal point indicates a random fraction of all documents.  A number "
   "with no decimal point indicates the number of documents to select randomly.  "
   "`remaining' indicates to take all untagged documents.  Anything else is "
   "interpreted as a filename listing documents to select.  Default is `0.3'."},
  {"train-set", TRAIN_SOURCE, "SOURCE", 0,
   "How to select the training documents.  Same format as --test-set.  Default is "
   "`remaining'."},
  {"unlabeled-set", UNLABELED_SOURCE, "SOURCE", 0,
   "How to select the unlabeled documents.  Same format as --test-set.  Default is "
   "`0'."},
  {"ignore-set", IGNORE_SOURCE, "SOURCE", 0,
   "How to select the ignored documents.  Same format as --test-set.  Default is "
   "`0'."},
  {"test-percentage", 'p', "P", OPTION_HIDDEN,
   "Use P percent of the indexed documents as test data.  Default is 30."},
  {"set-test-files", SET_TEST_FILES_KEY, "FILENAME", OPTION_HIDDEN,
   "Instead of splitting the data among test/train randomly (using the "
   "-p option), use the indexed files named in the contents of FILENAME "
   "for testing, and all the others in the model for training.  FILENAME "
   "should contain a list of file paths (with path identical to the path "
   "used in indexing), each path separated by a newline."},
  {"testing-files", SET_TEST_FILES_KEY, "FILENAME", 
   OPTION_ALIAS | OPTION_HIDDEN},
  {"set-files-use-basename", SET_TEST_FILES_USE_BASENAME_KEY, "N",
   OPTION_ARG_OPTIONAL,
   "When using files to specify doc types, compare only the last N "
   "components the doc's pathname.  That is use the filename and "
   "the last N-1 directory names.  If N is not specified, it default to 1."},
  {"testing-files-use-basename", SET_TEST_FILES_USE_BASENAME_KEY, "N",
   OPTION_ALIAS | OPTION_HIDDEN | OPTION_ARG_OPTIONAL},
  {0,0}
};

static error_t
bow_split_parse_opt (int key, char *arg, struct argp_state *state)
{
  bow_files_source_type *files_source;
  float *fraction;
  int *number;
  char **filename;
  bow_split_fancy_count **fancy_counts;
  int length;

  switch (key)
    {
    case TEST_SOURCE:
      files_source = &bow_test_files_source;
      fraction = &bow_test_fraction;
      number = &bow_test_number;
      filename = &bow_test_filename;
      fancy_counts = &bow_test_fancy_counts;
      break;
    case TRAIN_SOURCE:
      files_source = &bow_train_files_source;
      fraction = &bow_train_fraction;
      number = &bow_train_number;
      filename = &bow_train_filename;
      fancy_counts = &bow_train_fancy_counts;
      break;
    case UNLABELED_SOURCE:
      files_source = &bow_unlabeled_files_source;
      fraction = &bow_unlabeled_fraction;
      number = &bow_unlabeled_number;
      filename = &bow_unlabeled_filename;
      fancy_counts = &bow_unlabeled_fancy_counts;
      break;
    case IGNORE_SOURCE:
      files_source = &bow_ignore_files_source;
      fraction = &bow_ignore_fraction;
      number = &bow_ignore_number;
      filename = &bow_ignore_filename;
      fancy_counts = &bow_ignore_fancy_counts;
      break;
    case 'p':
      bow_test_files_source = bow_files_source_fraction;
      bow_test_fraction = atof (arg) / 100.0;
      return 0;
      break;
    case SET_TEST_FILES_KEY:
      bow_test_files_source = bow_files_source_file;
      bow_test_filename = arg;
      return 0;
      break;
    case SET_TEST_FILES_USE_BASENAME_KEY:
      if (arg)
	bow_test_set_files_use_basename = atoi (arg);
      else
	bow_test_set_files_use_basename = 1;
      return 0;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  
  assert (key == TEST_SOURCE || key == TRAIN_SOURCE ||
	  key == UNLABELED_SOURCE || key == IGNORE_SOURCE);

  length = strlen(arg);

  /* Now parse the split option */
  if (!strcmp(arg, "remaining"))
    *files_source = bow_files_source_remaining;
  else if (length == strspn(arg, "0123456789"))
    {
      *files_source = bow_files_source_number;
      *number = atoi(arg);
    }
  else if (length == strspn(arg, ".0123456789") &&
	   (strchr(arg, '.') == strrchr(arg, '.')))
    {
      *files_source = bow_files_source_fraction;
      *fraction = atof(arg);
      assert (*fraction >= 0 && *fraction <= 1);
    }
  else if (length > 2 && 
	   index(arg, 'p') == arg + length - 2 &&
	   index(arg, 'c') == arg + length - 1 &&
	   strspn(arg, "0123456789pc"))
    {
      *files_source = bow_files_source_num_per_class;
      *number = atoi(arg);
    }
  else if (length > 2 &&
	   arg[0] == '[' && arg[length-1] == ']')
    {
      char *charp;
      int num_entries;
      int x;

      *files_source = bow_files_source_fancy_counts;
      
      /* see how many classes we'll need to malloc for */
      for (charp=arg, num_entries = 0; *charp != '\0'; charp++)
	{
	  if (*charp == '=') 
	    num_entries++;
	}

      /* malloc and initialize the space to store the counts */
      assert (num_entries > 0);
      *fancy_counts = malloc (sizeof (bow_split_fancy_count) * (num_entries + 1));
      (*fancy_counts)[num_entries].class_name = NULL;

      /* extract the class num arg pairs */
      strtok(arg, "[");
      for (charp = strtok(NULL, "="), x=0; x < num_entries;
	   x++, charp = strtok(NULL, "=")) 
	{
	  (*fancy_counts)[x].class_name = charp;
	  (*fancy_counts)[x].num_docs = atoi(strtok(NULL, ","));
	}
      assert(NULL == strtok(NULL, "="));
    }
  else
    {
      *files_source = bow_files_source_file;
      *filename = arg;
    }
  
  return 0;
}

static const struct argp bow_split_argp =
{
  bow_split_options,
  bow_split_parse_opt
};

static struct argp_child bow_split_argp_child =
{
  &bow_split_argp,	/* This child's argp structure */
  0,			/* flags for child */
  0,			/* optional header in help message */
  0			/* arbitrary group number for ordering */
};



/* Mark all cdoc's in BARREL to be of type BOW_DOC_UNTAGGED. */
void
bow_set_all_files_untagged (bow_barrel *barrel)
{
  int i;
  bow_cdoc *doc;

  for (i = 0; i < barrel->cdocs->length ; i++)
    {
      doc = bow_array_entry_at_index (barrel->cdocs, i);
      doc->type = bow_doc_untagged;
    }
}

/* Randomly select some untagged documents and label them with tag
   indicated by TYPE.  The number of documents from each class are
   determined by the array NUM_PER_CLASS. */
void
bow_set_file_types_randomly_by_count_per_class (bow_barrel *barrel, 
						int *num_per_class, int tag)
{
  int ci, di;
  bow_cdoc *cdoc = NULL;
  /* All the below include only the test/model docs, not the ignore docs.*/
  int num_classes = bow_barrel_num_classes (barrel);
  int *local_num_per_class;
  int total_num_to_tag;
  int num_tagged;
  int num_loops;

  /* Seed the random number generator if it hasn't been already */
  bow_random_set_seed ();

  /* Create a local array of the number of taggings to perform in each class,
     which we will change by decrimenting it as we tag. */
  local_num_per_class = alloca (num_classes * sizeof (int));
  total_num_to_tag = 0;
  for (ci = 0; ci < num_classes; ci++)
    {
      local_num_per_class[ci] = num_per_class[ci];
      total_num_to_tag += num_per_class[ci];
    }

#if 0
  /* Print the number of documents in each class. */
  fprintf (stderr, "Number of docs per class: ");
  for (i = 0; i < max_ci; i++)
    fprintf (stderr, "%d:%d ",
	     num_docs_per_class[i],
	     num_test_docs_allowed_per_class[i]);
  fprintf (stderr, "\n");
#endif

  /* Now loop until we have created a test set of size num_test */
  for (num_tagged = 0, num_loops = 0; num_tagged < total_num_to_tag; 
       num_loops++)
    {
      di = random() % barrel->cdocs->length;

      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      assert (cdoc);
      if (cdoc->type == bow_doc_untagged
	  && local_num_per_class[cdoc->class] > 0)
	{
	  cdoc->type = tag;
	  num_tagged++;
	  local_num_per_class[cdoc->class]--;
	  assert (local_num_per_class[cdoc->class] >= 0);
	}
      if (num_loops > barrel->cdocs->length * 1000)
	bow_error ("Random number generator could not find enough "
		   "model document indices with balanced classes");
    }
}


/* Randomly select NUM untagged documents and label them with tag
   indicated by TAG.  The number of documents from each class are
   determined by attempting to match the proportion of classes among
   the non-ignore documents. */
void
bow_set_file_types_randomly_by_count (bow_barrel *barrel, int num, int tag)
{
  int ci, di;
  bow_cdoc *cdoc;
  int num_classes = bow_barrel_num_classes (barrel);
  int *num_per_class = alloca (num_classes * sizeof (int));
  int *num_docs_per_class = alloca (num_classes * sizeof (int));
  int total_num_docs = 0;
  int total;

  /* Find out the number of documents in each class. */
  for (ci = 0; ci < num_classes; ci++)
    num_docs_per_class[ci] = 0;
  for (di = 0; di < barrel->cdocs->length ; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      if (cdoc->type == bow_doc_ignore)
	continue;
      assert (cdoc->class < num_classes);
      num_docs_per_class[cdoc->class]++;
      total_num_docs++;
    }

  /* Initialize the array NUM_PER_CLASS, indicating how many documents 
     per class should be tagged. */
  total = 0;
  for (ci = 0; ci < num_classes; ci++)
    {
      num_per_class[ci] = ((float) num / (float) total_num_docs) * 
	(float) num_docs_per_class[ci];
      total += num_per_class[ci];
    }
  /* Add more to take care of round-off error. */
  for (ci = 0; total < num; ci = (ci+1) % barrel->cdocs->length)
    {
      num_per_class[ci]++;
      total++;
    }

  /* Do it. */
  bow_set_file_types_randomly_by_count_per_class (barrel, num_per_class, tag);
}

/* Randomly select a FRACTION of the untagged documents and label them
   with tag indicated by TYPE.  The number of documents from each
   class are determined by attempting to match the proportion of
   classes among the untagged documents. */
void
bow_set_file_types_randomly_by_fraction (bow_barrel *barrel, 
					 double fraction, int type)
{
  bow_set_file_types_randomly_by_count (barrel,
					fraction * barrel->cdocs->length,
					type);
}



/* Setting file tags with lists of filenames. */

/* If opts.c:bow_test_set_files_use_basename is non-zero, ignore the
   directory names in the filenames read from TEST_FILES_FILENAMES in
   bow_test_set_file(). */
static inline const char *
bow_basename (const char *str, int num_components)
{
  int i;

  if (num_components == 0)
    return str;

  i = strlen (str) - 1;
  assert (str[i] != '/');
  while (i > 0)
    {
      if (str[i] == '/')
	{
	  num_components--;
	  if (num_components == 0)
	    break;
	}
      i--;
    }
  if (str[i] == '/')
    i++;
  return &(str[i]);
}

/* Set all the cdoc's named in TEST_FILES_FILENAME to type indicated
   by TYPE.  Raises error if any of the files already have a
   non-"untagged" type.  BARREL should be a doc barrel, not a class
   barrel. */
void
bow_set_files_to_type (bow_barrel *barrel, 
		       const char *test_files_filename,
		       int type)
{
  bow_int4str *map;
  bow_cdoc *cdoc;
  int di;
  int test_files_count = 0;
  int model_files_count = 0;
  const char *filename;

  map = bow_int4str_new_from_string_file (test_files_filename);
  if (bow_test_set_files_use_basename)
    {
      /* Convert the filename strings in map to only the basenames of
	 the files. */
      int si, index;
      bow_int4str *map2 = bow_int4str_new (0);
      for (si = 0; si < map->str_array_length; si++)
	{
	  index =
	    bow_str2int_no_add
	    (map2, bow_basename(bow_int2str (map, si), 
				bow_test_set_files_use_basename));
	  if (index != -1)
	    bow_verbosify (bow_quiet, "WARNING: Repeated file basename `%s'\n", 
			   bow_int2str (map, si));
	  bow_str2int (map2, bow_basename (bow_int2str (map, si),
					   bow_test_set_files_use_basename));
	}
      bow_int4str_free (map);
      map = map2;
    }  
  
  for (di = 0; di < barrel->cdocs->length; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      assert (cdoc->type == bow_doc_untagged);
      if (bow_test_set_files_use_basename)
	filename = bow_basename (cdoc->filename, 
				 bow_test_set_files_use_basename);
      else
	filename = cdoc->filename;
      if (bow_str2int_no_add (map, filename) != -1)
	{
	  /* This filename is in the map; tag this cdoc. */
	  cdoc->type = type;
	}
    }

  bow_verbosify (bow_progress, 
		 "Using %s, placed %d documents in the %s set, "
		 "%d in model set.\n", 
		 test_files_filename, test_files_count, model_files_count,
		 (type == bow_doc_test
		  ? "test"
		  : (type == bow_doc_train
		     ? "train"
		     : "(unknown)")));
  bow_int4str_free (map);
  return;
}

/* Postprocess the tags on documents by setting untagged documents to
   train or test, depending on context. */
void
bow_set_file_types_of_remaining (bow_barrel *barrel, int type)
{
  int di;
  bow_cdoc *cdoc;

  for (di = 0; di < barrel->cdocs->length; di++)
    {
      cdoc = bow_array_entry_at_index (barrel->cdocs, di);
      if (cdoc->type == bow_doc_untagged)
	  cdoc->type = type;
    }
}

/* Use the command line arguments to create the appropriate train/test split */
void
set_doc_types (bow_barrel *barrel)
{      
  int num_docs;
  int ti;
  int num_types;
  int num_remaining = 0;
  /* note it is important that ignore comes first, so we can 
     ignore them when doing even prior random splits later */
  struct {
    bow_files_source_type source;
    float fraction;
    int number;
    char *filename;
    bow_split_fancy_count *fancy_counts;
    bow_files_source_type doc_type;
  } types[] = {{bow_ignore_files_source, 
		bow_ignore_fraction, 
		bow_ignore_number, 
		bow_ignore_filename, 
		bow_ignore_fancy_counts,
		bow_doc_ignore},
	       {bow_test_files_source, 
		bow_test_fraction, 
		bow_test_number, 
		bow_test_filename, 
		bow_test_fancy_counts,
		bow_doc_test},
	       {bow_train_files_source, 
		bow_train_fraction, 
		bow_train_number, 
		bow_train_filename, 
		bow_train_fancy_counts,
		bow_doc_train},
	       {bow_unlabeled_files_source, 
		bow_unlabeled_fraction, 
		bow_unlabeled_number, 
		bow_unlabeled_filename, 
		bow_unlabeled_fancy_counts,
		bow_doc_unlabeled},
	       {0,0,0,0,0,0}};
  
  /* First set all files to be untagged. */
  bow_set_all_files_untagged (barrel);

  /* count the number of document types */
  for (num_types = 0; types[num_types].source; num_types++);

  /* Set document types based on input files first */
  for (ti = 0; ti < num_types; ti++)
    {
      if (types[ti].source == bow_files_source_file)
	bow_set_files_to_type (barrel,
			       types[ti].filename,
			       types[ti].doc_type);
    }

  /* Do any random splits to set document types */
  for (ti = 0; ti < num_types; ti++)
    {
      if (types[ti].source == bow_files_source_fraction)
	{
	  num_docs = (barrel->cdocs->length * types[ti].fraction);      
	  bow_set_file_types_randomly_by_count (barrel, 
						num_docs,
						types[ti].doc_type);
	}
      else if (types[ti].source == bow_files_source_number)
	bow_set_file_types_randomly_by_count (barrel, 
					      types[ti].number,
					      types[ti].doc_type);
      else if (types[ti].source == bow_files_source_num_per_class)
	{
	  int ci;
	  int *class_nums;

	  class_nums = bow_malloc (sizeof (int) * bow_barrel_num_classes(barrel));
	  for (ci = 0; ci < bow_barrel_num_classes(barrel); ci ++)
	    class_nums[ci] = types[ti].number;
	  
	  bow_set_file_types_randomly_by_count_per_class (barrel, 
							  class_nums, 
							  types[ti].doc_type);
	  bow_free(class_nums);
	}
      else if (types[ti].source == bow_files_source_fancy_counts)
	{
	  int *counts = bow_malloc (sizeof (int) * bow_barrel_num_classes(barrel));
	  int ci;
	  bow_split_fancy_count *class_count;

	  for (ci = 0; ci < bow_barrel_num_classes(barrel); ci++)
	    counts[ci] = -1;

	  for (class_count = types[ti].fancy_counts; 
	       class_count->class_name; 
	       class_count++)
	    {
	      int class = -1;

	      for (ci = 0; ci < bow_barrel_num_classes(barrel); ci++)
		if (!strcmp(class_count->class_name, 
			    bow_barrel_classname_at_index (barrel, ci)))
		  {
		    class = ci;
		    break;
		  }
	      
	      if (class == -1)
		bow_error ("Unknown class %s.\n", class_count->class_name);
	      
	      counts[class] = class_count->num_docs;
	    }

	  for (ci = 0; ci < bow_barrel_num_classes(barrel); ci++)
	    if (counts[ci] == -1)
	      bow_error("Under-specified class counts");
	  
	  bow_set_file_types_randomly_by_count_per_class (barrel, 
							  counts, 
							  types[ti].doc_type);
	  bow_free(counts);
	}
    }

  /* Set remaining untagged docs if appropriate */
  for (ti = 0; ti < num_types; ti++)
    {
      if (types[ti].source == bow_files_source_remaining)
	{
	  assert (num_remaining == 0);
	  num_remaining = 1;
	  bow_set_file_types_of_remaining (barrel, types[ti].doc_type);
	}
    }
}

void _register_split_args () __attribute__ ((constructor));
void _register_split_args ()
{
  static int done = 0;
  if (done) 
    return;
  bow_argp_add_child (&bow_split_argp_child);
  done = 1;
}


