/* Copyright (C) 1997 Andrew McCallum

   Written by:  Andrew Kachites McCallum <mccallum@cs.cmu.edu>

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

#ifndef __BOW_TFIDF_H
#define __BOW_TFIDF_H

/* The method and parameters of TFIDF-like weight settings. */
extern bow_method bow_method_tfidf_words;
extern bow_method bow_method_tfidf_log_words;
extern bow_method bow_method_tfidf_log_occur;
typedef struct _bow_tfidf_params {
  enum { bow_tfidf_words, bow_tfidf_occurrences } df_counts;
  enum { bow_tfidf_log, bow_tfidf_sqrt, bow_tfidf_straight } df_transform;
} bow_params_tfidf;

#endif /* __BOW_TFIDF_H */
