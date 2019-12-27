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

#ifndef __BOW_EM_H
#define __BOW_EM_H

/* The method and parameters of EM weight settings. */
extern bow_method bow_method_em;

void
bow_em_compare_to_nb (bow_barrel *doc_barrel);

void
bow_em_print_log_odds_ratio (bow_barrel *barrel, int num_to_print);

#endif /* __BOW_EM_H */
