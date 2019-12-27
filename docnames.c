/* Managing lists of document names. */

#include "libbow.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>

int bow_map_verbosity_level = bow_chatty;

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
			    const char *exclude_patterns)
{
  DIR *dir;
  struct dirent *dirent_p;
  struct stat st;
  char initial_cwd[PATH_MAX];
  char cwd[PATH_MAX];
  int cwdlen;
  int num_files = 0;

  if (!(dir = opendir (dirname)))
    bow_error ("Couldn't open directory `%s'", dirname);

  getcwd (initial_cwd, PATH_MAX);
  chdir (dirname);
  getcwd (cwd, PATH_MAX);
  strcat (cwd, "/");
  cwdlen = strlen (cwd);

  if (bow_verbosity_use_backspace 
      && bow_verbosity_level >= bow_map_verbosity_level)
    bow_verbosify (bow_progress, "%s:       ", dirname);
  while ((dirent_p = readdir (dir)))
    {
      stat (dirent_p->d_name, &st);
      if (S_ISDIR (st.st_mode)
	  && strcmp (dirent_p->d_name, ".")
	  && strcmp (dirent_p->d_name, ".."))
	{
	  /* This directory entry is a subdirectory.  Recursively 
	     descend into it and append its files also. */
	  char subdirname[strlen(dirname) + 
			 strlen(dirent_p->d_name) + 2];
	  strcpy (subdirname, dirname);
	  strcat (subdirname, "/");
	  strcat (subdirname, dirent_p->d_name);
	  num_files += 
	    bow_map_filenames_from_dir (callback, context,
					subdirname, exclude_patterns);
	}
      else if (S_ISREG (st.st_mode))
	{
	  /* It's a regular file; add it to the list. */
	  int filename_len = cwdlen + strlen (dirent_p->d_name) + 1;
	  char filename[filename_len];

	  strcpy (filename, cwd);
	  strcat (filename, dirent_p->d_name);
#if TESTING_TEXT_IN_DOC_LIST_APPEND
	  FILE *fp;

	  /* xxx Is this worth it at this point?  It means we're probably
	     opening and closing each file twice: once for text testing,
	     and again to read the actual words.  The advantage of doing
	     it here is that we won't overestimate the number of documents
	     that we will actually parse, so we can nicely create arrays
	     of just the right size. */
	  if (!(fp = fopen (filename, "r")))
	    {
	      bow_verbosify (bow_verbose,
			     "%s: Couldn't open file %s to test for text\n",
			     __PRETTY_FUNCTION__, filename);
	      continue;
	    }
	  if (bow_fp_is_text (fp))
#endif /* TESTING_TEXT_IN_DOC_LIST_APPEND */
	    {
	      /* Here is where we actually call the map-function with
		 the filename. */
	      (*callback) (filename, context);
	      num_files++;
	      if (!bow_verbosify (bow_screaming, "%6d Adding %s\n",
				  num_files, filename))
		if (bow_verbosity_level >= bow_map_verbosity_level)
		  bow_verbosify (bow_progress, "\b\b\b\b\b\b%6d", num_files);
	    }
#if TESTING_TEXT_IN_DOC_LIST_APPEND
	  fclose (fp);
#endif /* TESTING_TEXT_IN_DOC_LIST_APPEND */
	}
    }
  closedir (dir);
  chdir (initial_cwd);

  if (bow_verbosity_use_backspace
      && bow_verbosity_level >= bow_map_verbosity_level)
    bow_verbosify (bow_progress, "\n");
  
  return num_files;
}

/* Create a linked list of filenames, and append the document list
   pointed to by DL to it; return the new concatenated lists in *DL.
   The function returns the total number of filenames.  When creating
   the list, look for files (and recursively descend directories) in
   the directory DIRNAME, but don't include those matching
   EXCLUDE_PATTERNS. */
int
bow_doc_list_append (bow_doc_list **dl,
		     const char *dirname,
		     const char *exclude_patterns)
{
  bow_doc_list *dl_next = NULL;
  int append_filename (const char *filename, void *context)
    {
      dl_next = *dl;
      *dl = bow_malloc (sizeof (bow_doc_list) + strlen (filename) + 1);
      (*dl)->next = dl_next;
      strcpy ((*dl)->filename, filename);
      return 0;
    }

  return bow_map_filenames_from_dir (append_filename, NULL,
				     dirname, exclude_patterns);
}

/* Return the number of entries in the "docname list" DL. */
int
bow_doc_list_length (bow_doc_list *dl)
{
  int c = 0;
  for ( ; dl; dl = dl->next)
    c++;
  return c;
}

void
bow_doc_list_fprintf (FILE *fp, bow_doc_list *dl)
{
  while (dl)
    {
      fprintf (fp, "%s\n", dl->filename);
      dl = dl->next;
    }
}

void
bow_doc_list_free (bow_doc_list *dl)
{
  bow_doc_list *next_dl;

  while (dl)
    {
      next_dl = dl->next;
      bow_free (dl);
      dl = next_dl;
    }
}
