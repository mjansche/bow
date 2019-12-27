%{
#include <stdlib.h>
#include <string.h>
#include <bow/archer_query.h>
#define YYERROR_VERBOSE

#define yyerror archer_query_error
#define yylex archer_query_lex

%}

%union {
  char* string;
  archer_query_string_list* slist;
  archer_query_term* query_term;
  archer_query_info* query_info;
}

%token <string> WORD NUMBER WEIGHT
%token ':' '[' ']' '+' '-' '"' ',' '<' '>' '(' ')'
%left '~'

%type <string> word
%type <slist> words field
%type <query_term> term wterm element range super_term
%type <query_info> tagged_term input

%%
input:        tagged_term         { archer_query_tree = $$ = $1; }
              | input tagged_term { /* we don't allow empty queries */
                                    if ($2->ranking)
				      {
					$2->ranking->next = $1->ranking;
					$1->ranking = $2->ranking;
				      }

				    if ($2->exclusion)
				      {
					$2->exclusion->next = $1->exclusion;
					$1->exclusion = $2->exclusion;
				      }
				    if ($2->inclusion)
				      {
					$2->inclusion->next = $1->inclusion;
					$1->inclusion = $2->inclusion;
				      }
				    $$ = $1;
				    free($2);
                                  }
;

tagged_term:  super_term       {
                                 $$ = archer_query_info_new ();
				 $$->ranking = $1;
                               }
              | '+' super_term {
                                 $$ = archer_query_info_new ();
				 $$->inclusion = $2;
                               }
              | '-' super_term {
                                 $$ = archer_query_info_new ();
				 $$->exclusion = $2;
                               }
;

super_term:   wterm
              | super_term '~' NUMBER wterm {
					     $4->proximity = archer_query_pterm_new ();
					     $4->proximity->position = ARCHER_QUERY_PTERM_WITHIN;
					     $4->proximity->proximity = atoi ($3);
					     $4->proximity->term = $1;
                                             $$ = $4;
                                           }
              | super_term '~' wterm        {
					     $3->proximity = archer_query_pterm_new ();
					     $3->proximity->position = ARCHER_QUERY_PTERM_WITHIN;
					     $3->proximity->proximity = 10;
					     $3->proximity->term = $1;
                                             $$ = $3;
                                           }
              | super_term '<' NUMBER wterm {
					     $4->proximity = archer_query_pterm_new ();
					     $4->proximity->position = ARCHER_QUERY_PTERM_AFTER;
					     $4->proximity->proximity = atoi ($3);
					     $4->proximity->term = $1;
                                             $$ = $4;
                                           }
              | super_term '<' wterm        {
					     $3->proximity = archer_query_pterm_new ();
					     $3->proximity->position = ARCHER_QUERY_PTERM_AFTER;
					     $3->proximity->proximity = 10;
					     $3->proximity->term = $1;
                                             $$ = $3;
                                           } 
/* This rule is unnecessary
              | super_term '>' NUMBER wterm {
					     $4->proximity = archer_query_pterm_new ();
					     $4->proximity->position = ARCHER_QUERY_PTERM_BEFORE;
					     $4->proximity->proximity = atoi ($3);
					     $4->proximity->term = $1;
                                             $$ = $4;
                                           }
;
*/


wterm:        term
              | '(' WEIGHT ')' term  { 
                                       $$ = $4;
				       $$->weight = atof($2);
                                     }
              | '(' NUMBER ')' term  { 
                                       $$ = $4;
				       $$->weight = atof($2);
                                     }
;

term:         element
              | field ':'            {
                                       $$ = archer_query_term_new ();
				       $$->labels = $1;
                                     }
              | field ':' element    {
                                       $$ = $3;
				       $$->labels = $1;
                                     }
              | field ':' range      {
                                       $$ = $3;
				       $$->labels = $1;
                                     }
;

field:        WORD             { $$ = archer_query_string_list_new ($1); }
              | field ',' WORD { $$ = archer_query_string_list_prepend ($3, $1); }
;

element:      word                        {
		                             $$ = archer_query_term_new ();
		                             $$->word = $1;
                                          }
              | '"' words '"'             {
					     archer_query_string_list* list;
					     archer_query_term* term;
					     
					     $$ = term = archer_query_term_new ();

				             list = $2;
					     term->word = list->string;
					     list = list->next;
					     
                 		             while (list != NULL)
					       {
						 archer_query_pterm* pterm;

						 pterm = archer_query_pterm_new ();
						 pterm->position = ARCHER_QUERY_PTERM_AFTER;
						 pterm->proximity = 1;
						 
						 term->proximity = pterm;
						 term = archer_query_term_new ();
						 term->word = list->string;
						 pterm->term = term;
						 
						 list = list->next;
					       }
	                                  }
;
range:        '[' word '-' word ']'      {
		                            $$ = archer_query_term_new ();
					    $$->range_start = $2;
					    $$->range_end = $4;
	                                  }

word:           WORD { $$ = $1; }
              | NUMBER { $$ = $1; }
;


words:        word         { $$ = archer_query_string_list_new ($1); }
              | words word { $$ = archer_query_string_list_prepend ($2, $1); }
;
%%

/*
bob jones
bob +jones
-bob +jones
"bob jones" === bob >1 jones
name:bob
name:
name:bob ~10 name:jones
name:bob ~10 phone_number:
name:"bob jones" === name:bob >1 name:jones
year[1990-1996]:
a ~1 b ~1 c
a ~1 +b ERROR
a ~1 -b ERROR
-a ~1 b ERROR
+a ~1 b === +a ~1 +b

+a -b (c & (d | e) & !f) g

*/

