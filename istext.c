/* istext.c - test if a file contains text or not. */

#include "libbow.h"
#include <ctype.h>		/* for isprint(), etc. */

/* The percentage of characters that must be text-like in order for
   us to say this is a text file. */
#define TEXT_PRINTABLE_PERCENT 95

/* Examine the first NUM_TEST_CHARS characters of `fp', and return a 
   non-zero value iff TEXT_PRINTABLE_PERCENT of them are printable. */
int
bow_fp_is_text (FILE *fp)
{
  static const int NUM_TEST_CHARS = 2048;
  char buf[NUM_TEST_CHARS];
  int num_read;
  int num_printable = 0;
  int fpos;
  int i;

  fpos = ftell (fp);
  num_read = fread (buf, sizeof (char), NUM_TEST_CHARS, fp);
  fseek (fp, fpos, SEEK_SET);

  for (i = 0; i < num_read; i++)
    if (isprint (buf[i]) || isspace (buf[i]))
      num_printable++;

  if (num_read > 0 
      && (((100 * num_printable) / num_read) > TEXT_PRINTABLE_PERCENT))
    return 1;
  else
    return 0;
}
