/* annotations support for archer */

/* This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation, version 2.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA */

#include <stdio.h>
#include "bow/archer.h"

int annotation_read(void *p, FILE *fp)
{
  int ret, i;
  annotation *a = (annotation *)p;

  ret = bow_fread_int(&a->count, fp);

  a->size = a->count;
  a->feats = (char **)malloc(a->size * sizeof(char *));
  a->vals = (char **)malloc(a->size * sizeof(char *));

  for (i = 0; i < a->count; ++i)
  {
    ret += bow_fread_string(&a->feats[i], fp);
    ret += bow_fread_string(&a->vals[i], fp);
  }

  return ret;
}

int annotation_write(void *p, FILE *fp)
{
  int ret, i;
  annotation *a = (annotation *)p;

  ret = bow_fwrite_int(a->count, fp);
  for (i = 0; i < a->count; ++i)
  {
    ret += bow_fwrite_string(a->feats[i], fp);
    ret += bow_fwrite_string(a->vals[i], fp);
  }

  return ret;
}

void annotation_free(annotation *a)
{
  int i;

  if (!a->size)
    return;

  for (i = 0; i < a->count; ++i)
  {
    free(a->feats[i]);
    free(a->vals[i]);
  }

  free(a->feats);
  free(a->vals);
}


bow_sarray *annotation_sarray_new(void)
{
  return bow_sarray_new(bow_sarray_default_capacity, sizeof(annotation), annotation_free);
}

annotation *annotation_new(void)
{
  annotation *a = (annotation *)malloc(sizeof(annotation));

  a->count = 0;
  a->size = 2;
  a->feats = (char **)malloc(2 * sizeof(char *));
  memset(a->feats, 0, 2 * sizeof(char *));
  a->vals = (char **)malloc(2 * sizeof(char *));
  memset(a->vals, 0, 2 * sizeof(char *));

  return a;
}

void annotation_add_fval(annotation *a, char *feat, char *val)
{
  while (a->size <= a->count)
  {
    a->size *= 2;
    a->feats = (char **)realloc(a->feats, a->size * sizeof(char *));
    a->vals = (char **)realloc(a->vals, a->size * sizeof(char *));

    if (a->count < a->size)
    {
      memset(&a->feats[a->count], 0, sizeof(char *) * (a->size-a->count));
      memset(&a->vals[a->count], 0, sizeof(char *) * (a->size-a->count));
    }
  }

  a->feats[a->count] = strdup(feat);
  a->vals[a->count] = strdup(val);

  ++a->count;
}

void annotation_sarray_write(bow_sarray *sa, char *fname)
{
  FILE *fp = fopen(fname, "w");

  if (!fp)
  {
    fprintf(stderr, "Couldn't open %s for writing in annotation_sarray_write\n",fname);
    exit(1);
  }

  bow_sarray_write(sa, annotation_write, fp);

  fclose(fp);
}

bow_sarray *annotation_sarray_read(const char *fname)
{
  bow_sarray *sa;

  FILE *fp = fopen(fname, "r");

  if (!fp)
  {
    fprintf(stderr, "Couldn't open %s for reading in annotation_sarray_read\n",fname);
    exit(1);
  }

  sa = bow_sarray_new_from_data_fp(annotation_read, annotation_free, fp);

  fclose(fp);

  return sa;
}

bow_sarray *annotation_sarray_reread(bow_sarray *sa, const char *fname)
{
  FILE *fp = fopen(fname, "r");

  if (!fp)
  {
    fprintf(stderr, "Couldn't open %s for reading in annotation_sarray_read\n",fname);
    exit(1);
  }

  if (sa)
    bow_sarray_free(sa);
  sa = bow_sarray_new_from_data_fp(annotation_read, annotation_free, fp);

  fclose(fp);

  return sa;
}

int annotation_count(annotation *a)
{
  return a->count;
}

char *annotation_feat(annotation *a, int index)
{
  if (index < 0 || index >= a->count)
    return NULL;
  return a->feats[index];
}

char *annotation_val(annotation *a, int index)
{
  if (index < 0 || index >= a->count)
    return NULL;
  return a->vals[index];
}

annotation *annotation_sarray_entry_at_keystr(bow_sarray *sa, const char *keystr)
{
  return (annotation *)bow_sarray_entry_at_keystr(sa, keystr);
}

