#include "libbow.h"
#include <stdio.h>
#include <stdarg.h>

/* Examined by bow_verbosify() to determine whether to print the message.
   Default is bow_progress. */
int bow_verbosity_level = bow_progress;

/* If this is 0, and the message passed to bow_verbosify() contains
   backspaces, then the message will not be printed.  It is useful to
   turn this off when debugging inside an emacs window.  The default
   value is on. */
int bow_verbosity_use_backspace = 1;

/* Print the printf-style FORMAT string and ARGS on STDERR, only if
   the BOW_VERBOSITY_LEVEL is equal or greater than the argument 
   VERBOSITY_LEVEL. */
int
bow_verbosify (int verbosity_level, const char *format, ...)
{
  int ret;
  va_list ap;

  if ((bow_verbosity_level < verbosity_level)
      || (!bow_verbosity_use_backspace 
	  && strchr (format, '\b')))
    return 0;

  va_start (ap, format);
  ret = vfprintf (stderr, format, ap);
  va_end (ap);
  fflush (stderr);

  return ret;
}

volatile void
_bow_error (const char *format, ...)
{
  va_list ap;

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fputc ('\n', stderr);

#if __linux__
  abort ();
#else
  exit (-1);
#endif
}

