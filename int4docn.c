/* A convient interface to int4str.c, specifically for document names. */

#include "libbow.h"

static bow_int4str *doc_map = NULL;

const char *
bow_int2docname (int index)
{
  if (!doc_map)
    bow_error ("No documents yet added to the int-docname mapping.\n");
  return bow_int2str (doc_map, index);
}

int
bow_docname2int (const char *docname)
{
  if (!doc_map)
    doc_map = bow_int4str_new (0);
  return bow_str2int (doc_map, docname);
}

int
bow_num_docnames ()
{
  return doc_map->str_array_length;
}

void
bow_docnames_write (FILE *fp)
{
  bow_int4str_write (doc_map, fp);
}

void
bow_docnames_read_from_fp (FILE *fp)
{
  if (doc_map)
    bow_int4str_free (doc_map);
  doc_map = bow_int4str_new_from_fp (fp);
}
