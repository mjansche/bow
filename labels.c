/* labels (aka fields) in archer */

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
#include <bow/archer.h>

static char *last_label = NULL;
static char *labels[BOW_MAX_WORD_LABELS];
static int label_count = 0, label_iter;

const char *bow_last_label(void)
{
  return last_label;
}


void bow_push_label(const char *label)
{
  if (label_count == BOW_MAX_WORD_LABELS)
    return;

  assert(labels[label_count] = strdup(label));
  if (last_label)
    free(last_label);
  last_label = strdup(labels[label_count++]);
}

char *bow_pop_label(char buf[], int bufsz)
{
  if (!label_count)
    return NULL;

  strncpy(buf, labels[--label_count], bufsz-1);
  buf[bufsz-1] = 0;

  free(labels[label_count]);

  if (last_label)
    free(last_label);
  last_label= strdup(buf);

  return buf;
}

char *bow_first_label(char buf[], int bufsz)
{
  if (!label_count)
    return 0;

  strncpy(buf, labels[label_iter=0], bufsz-1);
  buf[bufsz-1] = 0;

  return buf;
}

char *bow_next_label(char buf[], int bufsz)
{
  if (++label_iter >= label_count)
    return 0;

  strncpy(buf, labels[label_iter], bufsz-1);
  buf[bufsz-1] = 0;

  return buf;
}

void bow_reset_labels(void)
{
  while (label_count)
    free(labels[--label_count]);
}
