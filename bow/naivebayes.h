/* Copyright (C) 1997, 1998 Andrew McCallum

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

#ifndef __BOW_NAIVEBAYES_H
#define __BOW_NAIVEBAYES_H

void bow_naivebayes_set_weights (bow_barrel *barrel);

int bow_naivebayes_score (bow_barrel *barrel, bow_wv *query_wv, 
			  bow_score *bscores, int bscores_len,
			  int loo_class);
/* Print the top N words by odds ratio for each class. */
void bow_naivebayes_print_odds_ratio_for_all_classes (bow_barrel *barrel, 
						      int n);


/* The method and parameters of NaiveBayes weight settings. */
extern bow_method bow_method_naivebayes;
typedef struct _bow_naivebayes_params {
  bow_boolean uniform_priors;
  bow_boolean normalize_scores;
} bow_params_naivebayes;

/* Print the top N words by odds ratio for each class. */
void bow_naivebayes_print_odds_ratio_for_all_classes (bow_barrel *barrel, 
						      int n);

void bow_naivebayes_print_odds_ratio_for_class (bow_barrel *barrel,
						const char *classname);

void bow_naivebayes_print_word_probabilities_for_class (bow_barrel *barrel,
							const char *classname);


/* Get the total number of terms in each class; store this in
   CDOC->WORD_COUNT. */
void bow_naivebayes_set_cdoc_word_count_from_wi2dvf_weights
(bow_barrel *barrel);

#endif /* __BOW_NAIVEBAYES_H */
