#ifndef ARCHER_QUERY_H_
#define ARCHER_QUERY_H_

struct archer_query_term_s;

typedef struct archer_query_string_list_s
  {
    char *string;
    struct archer_query_string_list_s *next;
  }
archer_query_string_list;

typedef struct archer_query_pterm_s
  {
    enum
      {
	ARCHER_QUERY_PTERM_BEFORE,
	ARCHER_QUERY_PTERM_AFTER,
	ARCHER_QUERY_PTERM_WITHIN
      }
    position;

    int proximity;
    struct archer_query_term_s *term;
  }
archer_query_pterm;

typedef struct archer_query_term_s
  {
    char *word;
    float weight;
    int df;
    float idf;
    archer_query_string_list *labels;
    char *range_start;
    char *range_end;
    struct archer_query_term_s *next;
    struct archer_query_term_s *head;
    struct archer_query_pterm_s *proximity;
  }
archer_query_term;

typedef struct
  {
    archer_query_term *exclusion;
    archer_query_term *inclusion;
    archer_query_term *ranking;
  }
archer_query_info;

/* utility functions provided by archer_query.c and mostly used by archer_query.y */
archer_query_string_list *archer_query_string_list_new (char *string);
archer_query_string_list *archer_query_string_list_prepend (char *string, archer_query_string_list * list);
archer_query_term *archer_query_term_new ();
int archer_query_term_length(archer_query_term *term);
archer_query_pterm *archer_query_pterm_new ();
archer_query_info *archer_query_info_new ();

/* interfaces to the lexer and parser */
int archer_query_parse ();	/* in archer_query_y.c */
int archer_query_lex ();	/* in archer_query_lex.c */
void archer_query_setup (const char *string);	/* in archer_query_lex.c --- sets the string to be scanned */
void archer_query_error (char *s);	/* in archer_query.c */

#include "archer_query_execute.h"

/* global variables (in archer_query.c) that hold the result of the scan */
extern archer_query_info *archer_query_tree;
extern char *archer_query_errorstr;

#endif
