/* Managing all the word-vector weighting/scoring methods
   Copyright (C) 1997 Andrew McCallum

   Written by:  Andrew Kachites McCallum <mccallum@jprc.com>

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

#include <bow/libbow.h>

bow_sarray *bow_methods = NULL;

/* Associate method M with the string NAME, so the method structure can
   be retrieved later with BOW_METHOD_AT_NAME(). */
int
bow_method_register_with_name (bow_method *m, const char *name)
{
  if (!bow_methods)
    {
      bow_methods = bow_sarray_new (0, sizeof (bow_method), 0);
    }
  return bow_sarray_add_entry_with_keystr (bow_methods, m, name);
}

/* Return a pointer to a method structure that was previous registered 
   with string NAME using BOW_METHOD_REGISTER_WITH_NAME(). */
bow_method *
bow_method_at_name (const char *name)
{
  return bow_sarray_entry_at_keystr (bow_methods, name);
}

bow_method *
bow_method_at_index (int id)
{
  return bow_sarray_entry_at_index (bow_methods, id);
}
