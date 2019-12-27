/* server.c - handles server mode for archer */

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

/* right now this file is a great mess. mostly it is code copied over and
   slightly modified from archer.c, and it is ALL spaghetti. once a better
   query engine has been created, this file will be greatly simplified */

#include <crypt.h>
#include <unistd.h>
#include <errno.h>		/* needed on DEC Alpha's */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
//The following file doesn't exist in Solaris
//#include <signum.h>
#include <bow/archer.h>
#include <bow/archer_query.h>
#include <bow/archer_query_execute.h>

/* Temporary constant.  Fix this soon! */
#define MAX_QUERY_WORDS 50

/* The file descriptor of the socket on which we can act as a query-server. */
int archer_sockfd;

/* File descriptor for the socket that handles prefork commands. */
int archer_prefork_sockfd;

/* various global variables we need from archer.c */
extern struct archer_arg_state_s archer_arg_state;
extern bow_wi2pv *archer_wi2pv;
extern bow_wi2pv *archer_li2pv;

/* The list of documents. */
extern bow_sarray *archer_docs;

extern bow_sarray *archer_labels;

extern const char* bow_annotation_filename;
bow_sarray *archer_annotations = NULL;

static int last_query_wi[MAX_QUERY_WORDS];
static int last_query_li[MAX_QUERY_WORDS];
static int last_query_wi_len;

/* Lexer for processing the query string */
bow_lexer _archer_query_lexer;
bow_lexer *archer_query_lexer;

/* Control which hits are shown.
   archer_first_hit == archer_last_hit == -1
   means show all hits. 
*/
int archer_first_hit = -1;
int archer_last_hit  = -1;

extern void
archer_archive(void);

extern int
archer_index_filename (const char *filename, void *unused);

static void
archer_query_server_command_loop(FILE *in, FILE *out, 
				 int (*command_processor)(char *command, FILE *out));
 
static int
archer_query_server_process_commands (char *command, FILE * out);

static void
archer_server_query (char *query, FILE * out);

static void
archer_server_dump (char *id, FILE * out);

static void
archer_server_hits (char *hits, FILE * out);

static void
archer_server_index (char *filename, FILE * out);

static void
archer_server_index_with_markup (char *command, FILE * out);

static bow_wa*
archer_server_query_hits_matching_sequence (const char *query_string,
				     const char *suffix_string);

bow_wa *
archer_query_hits_matching_wi (int wi, int fld, int *occurrence_count);

static void
archer_server_query_new (char* query_string, FILE* out);

static void
archer_server_dump_new (char *id, FILE* out);

static void
archer_server_fields(FILE *out);

static void
archer_server_docs(FILE *out);

static void
archer_server_rank(char *command, FILE *out);


/* Set up to listen for queries on a socket */
static void
archer_query_socket_init (int *sockfd, const char *socket_name, int use_unix_socket)
{
  int one = 1;
  int ret;
  int servlen, type, bind_ret;
  struct sockaddr_un un_addr;
  struct sockaddr_in in_addr;
  struct sockaddr *sap;

  if(bow_annotation_filename != NULL)
    archer_annotations = annotation_sarray_read(bow_annotation_filename);

  type = use_unix_socket ? AF_UNIX : AF_INET;
  *sockfd = socket (type, SOCK_STREAM, 0);
  assert (*sockfd >= 0);
  if (type == AF_UNIX)
    {
      sap = (struct sockaddr *) &un_addr;
      bzero ((char *) sap, sizeof (un_addr));
      strcpy (un_addr.sun_path, socket_name);
      servlen = strlen (un_addr.sun_path) + sizeof (un_addr.sun_family) + 1;
    }
  else
    {
      sap = (struct sockaddr *) &in_addr;
      bzero ((char *) sap, sizeof (in_addr));
      in_addr.sin_port = htons (atoi (socket_name));
      in_addr.sin_addr.s_addr = htonl (INADDR_ANY);
      servlen = sizeof (in_addr);
    }
  sap->sa_family = type;

  /* Allow the port to be released immediately when we're done with it */
  ret = setsockopt (*sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
  assert(!ret);

  bind_ret = bind (*sockfd, sap, servlen);
  assert (bind_ret >= 0);
  if (use_unix_socket)
    bow_verbosify (bow_progress, "Listening on socket %s\n", socket_name);
  else
    bow_verbosify (bow_progress, "Listening on port %d\n", atoi (socket_name));
  listen (*sockfd, 5);

  signal(SIGPIPE, SIG_IGN);
}

static int
archer_remote_host_matches_spec(int s)
{
  extern struct in_addr archer_ip_spec;
  struct sockaddr saddr;
  struct in_addr *iap;
  int saddrlen;
  unsigned int ip_num;
  u_char *spec, *host;
  int i;

  saddrlen = sizeof(saddr);
  if (getpeername(s, &saddr, &saddrlen))
    bow_error("Can't call getpeername");

  /*
  sap = (struct sockaddr_in *)saddr.sa_data;
  iap = &sap->sin_addr;
  ip_num = ntohs(iap->s_addr);
  */

  /* This is very bad, but the right thing doesn't appear to work */
  ip_num = *(unsigned int *)(&saddr.sa_data[2]);
  iap = (struct in_addr *)&ip_num;

  /* Presume OK to accept all connections over Unix-domain socket */
  if (saddr.sa_family == AF_UNIX)
    return 1;

  /* Internet-domain socket */
  /* Let everyone in if archer_ip_spec == 255.255.255.255 */
  if (archer_ip_spec.s_addr == 0xffffffff)
    return 1;

  /* Restrictions apply:  
     Each byte must match archer_ip_spec--unless spec byte == -1 */
  spec = (u_char *)&archer_ip_spec.s_addr;
  host = (u_char *)&ip_num;
  for (i = 0; i < 4; ++i)
  {
    if (spec[i] != 0xff && spec[i] != host[i])
    {
      fprintf(stderr, "Host is %s, ", inet_ntoa(*iap));
      fprintf(stderr, "spec is %s: Refusing.\n", inet_ntoa(archer_ip_spec));
      return 0;
    }
  }
 
  return 1;
}


static int
archer_query_serve_one_admin_command(char *command, FILE *out)
{
  if (!strncasecmp(command, "help", 4))
  {
    fprintf(out, "<archer-result>\n"
	         "   <help>\n"
	         "      Administrative commands available:\n"
	         "         help                   this message\n"
	         "         index <file>           add <file> to the index\n"
	         "         nindex <file> <mfile>  add <file> to index\n"
	         "                                using marked up copy (<mfile>)\n"
	         "         quit                   close connection\n"
	         "    </help>\n"
	         "</archer-result>\n");
  }

  else if (!strncasecmp(command, "quit", 4))
  {
    return 1;
  }

  else if (!strncasecmp (command, "index ", 6))
    { 
      bow_verbosify (bow_progress, "Indexing %s...\n", &command[6]);
      archer_server_index (&command[6], out);
      bow_verbosify (bow_progress, "Done indexing %s.\n", &command[6]);
    }

  else if (!strncasecmp(command, "nindex ", 7))
  {
    archer_server_index_with_markup(&command[6], out);
  }
  
  else
    {
      fprintf (out,
	       "<archer-error>\n"
	       "        I don't understand `%s'\n"
	       "</archer-error>\n", command);
    }

  return 0;
}


static int
archer_query_password_ok(FILE *in, FILE *out)
{
  extern char archer_password[];
  char password_buf[100], *end;

  if (archer_password[0] == 0)            /* Not password protected */
    return 1;

  fprintf(out, 
	  "<?xml version='1.0' encoding='US-ASCII' ?>\n"
	  "<archer-password>\n"
	  "  Password required.  Please send.\n"
	  "</archer-password>\n"
	  ".\n");
  fflush(out);

  fgets(password_buf, 100, in);
  end = strpbrk(password_buf, "\n\r");
  if (end)
    *end = '\0';
  if (strcmp(crypt(password_buf, archer_password), archer_password) == 0)
    return 1;

  return 0;
}


static void
archer_query_serve_admin_commands(int sockfd)
{
  int newsockfd, clilen;
  struct sockaddr cli_addr;
  FILE *in, *out;
 
  newsockfd = accept (sockfd, &cli_addr, &clilen);
  if (newsockfd == -1)
    bow_error ("Not able to accept connections!\n");
  in = fdopen (newsockfd, "r");
  out = fdopen (newsockfd, "w");

  if (archer_query_password_ok(in, out))
    archer_query_server_command_loop(in, out, 
				     archer_query_serve_one_admin_command);

  fclose (in);
  fclose (out);
  close (newsockfd);
}


/*
  Get carriage-return terminated string, however long.
*/
static char *
archer_server_fgets(FILE *in)
{
  static char *buf = NULL;
  static int size = 0;
  int count = 0;
  char c;

  if (!buf)
    {
      size = BOW_MAX_WORD_LENGTH;
      buf = bow_malloc(size);
    }

  while (!feof(in))
    {
      c = fgetc(in);

      if (count + 2 > size)  /* Must hold carriage return and \0 */
	{
	  size *= 2;
	  buf = bow_realloc(buf, size);
	}

      buf[count++] = c;

      if (c == '\n')
	break;
    }

  buf[count] = 0;

  if (count == 0)
    return NULL;
  else
    return buf;
}


static void
archer_query_server_command_loop(FILE *in, FILE *out, int
				 (*command_processor)(char *command, FILE *out))
{
  char *end;
  char *query_buf;
  int quit = 0;

  while (!feof (in) && !quit)
  {
    fprintf(out, ".\n");
    fflush (out);
    query_buf = archer_server_fgets(in);
    if (!query_buf)
      break;
    end = strpbrk (query_buf, "\n\r");
    if (end)
      *end = '\0';
    quit = command_processor(query_buf, out);
    fflush (out);
  }

  fflush (out);
}


static void
archer_query_serve_regular_query(int sockfd)
{
  int newsockfd, clilen;
  struct sockaddr cli_addr;
  FILE *in, *out;
  pid_t pid;

  newsockfd = accept (sockfd, &cli_addr, &clilen);
  if (newsockfd == -1)
    bow_error ("Not able to accept connections!\n");

  if (archer_remote_host_matches_spec(newsockfd))
    bow_verbosify (bow_progress, "Accepted connection\n");
  else
  {
    close(newsockfd);
    bow_verbosify (bow_progress, "Connection refused\n");
    return;
  }

  assert (newsockfd >= 0);
  in = fdopen (newsockfd, "r");
  out = fdopen (newsockfd, "w");

  if (archer_arg_state.serve_with_forking)
    {
      if ((pid = fork ()) != 0)
	{
	  /* parent - return to server mode */
	  fclose (in);
	  fclose (out);
	  close (newsockfd);
	  return;
	}
      else
	{
	  /* child - reopen the PV file so we get our own lseek() position */
	  bow_wi2pv_reopen_pv (archer_wi2pv);
	}
    }

  bow_verbosify (bow_progress, "Ready for commands.\n");

  if (archer_query_password_ok(in, out))
  {
    fprintf (out,
	     "<?xml version='1.0' encoding='US-ASCII' ?>\n"
	     "<archer-greeting>\n"
	     "<hello>\n"
	     "        Hello, I am Archer. I speak XML. You can still talk to me\n"
	     "        if you're a human though. Try saying `help'.\n"
	     "</hello>\n"
	     "<version>\n"
	     "        <major>%d</major>\n"
	     "        <minor>%d</minor>\n"
	     "</version>\n</archer-greeting>\n",
	     ARCHER_MAJOR_VERSION, ARCHER_MINOR_VERSION);
    fflush (out);
    archer_query_server_command_loop(in, out, 
				     archer_query_server_process_commands);
  }

  fclose (in);
  fclose (out);
  close (newsockfd);
  bow_verbosify (bow_progress, "Closed connection.\n");

  /* Kill the child - don't want it hanging around, sucking up memory :)  */
  if (archer_arg_state.serve_with_forking)
    exit (0);
}


static void
archer_query_serve_one_query ()
{
  int clilen;
  struct sockaddr cli_addr;
  fd_set rfds;

  clilen = sizeof (cli_addr);

  /* Check the two master sockets for incoming connections */
  FD_ZERO (&rfds);
  FD_SET (archer_sockfd,         &rfds);
  FD_SET (archer_prefork_sockfd, &rfds);
  select((archer_sockfd > archer_prefork_sockfd ? archer_sockfd : archer_prefork_sockfd) + 1, 
	 &rfds, NULL, NULL, NULL);
  
  if (FD_ISSET(archer_prefork_sockfd, &rfds))
    archer_query_serve_admin_commands(archer_prefork_sockfd);
  else
    archer_query_serve_regular_query(archer_sockfd);
}


static int
archer_query_server_process_commands (char *command, FILE * out)
{
  int quit = 0;

  fprintf(out, "<?xml version='1.0' encoding='US-ASCII' ?>\n");

  if (!strcasecmp (command, "help"))
    {
      fprintf (out,
	       "<archer-result><help>\n"
	       "        Commands available to you:\n"
	       "        help                    prints this message\n"
	       "        quit                    closes the connection\n"
	       "        query <str>             search for <str>\n"
	       "        nquery <str>            search for <str> using the new\n"
	       "                                query processor (experimental)\n"
	       "        dump <id> [<filename>]  dumps out the file associated\n"
	       "                                with <id>, or <filename> instead if it is given,\n"
	       "                                XML-izing and marking all the matches.\n"
	       "        ndump <id>              for use with nquery\n"
	       "        hits <first> <last>     show only hits <first> through <last>\n"
	       "        hits all                show all hits\n"
	       "        docs                    list documents in index\n"
               "        rank <filename> <query> return the rank of <filename> in result of <query>\n"
	       "</help></archer-result>\n");
    }

  else if (!strcasecmp (command, "quit"))
    {
      fprintf (out, "<archer-result></archer-result>\n");
      quit = 1;
    }
  
  else if (!strncasecmp (command, "query ", 6))
    {
      fprintf (out, "<archer-result>\n");
      archer_server_query (&command[6], out);
      fprintf (out, "</archer-result>\n\n");
    }

  else if (!strncasecmp (command, "nquery ", 7))
    {
      fprintf (out, "<archer-result>\n");
      bow_verbosify(bow_progress, "Got query: '%s'\n", &command[6]);
      archer_server_query_new (&command[6], out); 
      fprintf (out, "</archer-result>\n\n");
    }

  else if (!strncasecmp (command, "dump ", 5))
    {
      fprintf (out, "<archer-result>\n");
      archer_server_dump (&command[5], out);
      fprintf (out, "</archer-result>\n");
    }

  else if (!strncasecmp (command, "ndump ", 6))
    {
      fprintf (out, "<archer-result>\n");
      archer_server_dump_new (&command[5], out);
      fprintf (out, "</archer-result>\n");
    }

  else if (!strncasecmp (command, "hits ", 5))
    {
      fprintf (out, "<archer-result>\n");
      archer_server_hits (&command[5], out);
      fprintf (out, "</archer-result>\n");
    }
  else if (!strcasecmp(command, "fields"))
  {
    fprintf(out, "<archer-result>\n");
    archer_server_fields(out);
    fprintf(out, "</archer-result>\n");
  }
  else if (!strcasecmp(command, "docs"))
    {
      fprintf(out, "<archer-result>\n");
      archer_server_docs(out);
      fprintf(out, "</archer-result>\n");
    }
  else if (!strncasecmp(command, "rank ", 5))
    {
      fprintf(out, "<archer-result>\n");
      archer_server_rank(&command[5], out);
      fprintf(out, "</archer-result>\n");
    }      

  else
    {
      fprintf (out,
	       "<archer-error>\n"
	       "        I don't understand `%s'-- try `help' for help.\n"
	       "</archer-error>\n", command);
    }

  fflush (out);
  return quit;
}

static void
xml_fputc(int c, FILE * out)
{
  if (!iscntrl (c) || isspace (c)) /* control characters break xml parsers, in general */
    switch (c)
      {
      case '<': fputs ("&lt;", out); break;
      case '>': fputs ("&gt;", out); break;
      case '&': fputs ("&amp;", out); break;
      case '\'': fputs ("&apos;", out); break;
      case '"': fputs ("&quot;", out); break;
      default: fputc (c, out);
    }
}

static void
xml_fputs (const char * s, FILE * out)
{
  int i;

  i = 0;
  while (s[i] != '\0')
    xml_fputc (s[i++], out);
}

static bow_wa *
archer_server_query_hits_matching_di_wi_li (int di, int wi, int li, int *occurrence_count)
{
  int count = 0;
  int this_di, this_pi, this_li[100], this_ln;
  int i;
  bow_wa *wa;

  if (wi >= archer_wi2pv->entry_count && archer_wi2pv->entry[wi].count <= 0)
    return NULL;
  wa = bow_wa_new (0);
  bow_pv_rewind (&(archer_wi2pv->entry[wi]), archer_wi2pv->fp);
  this_ln = 100;
  bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, wi, &this_di, this_li, &this_ln,
			      &this_pi);
  while (this_di != -1)
    {
      if (this_di == di) 
	{
	  if (li >= 0)
	    {
	      for (i = 0; i < this_ln; ++i) {
		if (this_li[i] == li)
		  {
		    bow_wa_add_to_end (wa, this_pi, 1);
		    break;
		  }
	      }
	    }
	  else
	    bow_wa_add_to_end (wa, this_pi, 1);
	  count++;
	}

      this_ln = 100;
      bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, wi, &this_di, this_li,
				  &this_ln, &this_pi);
    }
  *occurrence_count = count;
  return wa;
}

static void
archer_server_dump(char *id, FILE * out)
{
  int di, i, current_pi;
  bow_wa* query_pi[MAX_QUERY_WORDS];
  int query_pi_len[MAX_QUERY_WORDS];
  char word[BOW_MAX_WORD_LENGTH];
  char filename[255];
  archer_doc *doc;
  FILE* file;
  long start, end, cur;
  int (*get_word_extended)(char buf[], int bufsz, long* start, long* end)
    = NULL;
  void (*set_fp)(FILE *fp, const char * name) = NULL;

  i = sscanf(id, "%d %s", &di, filename);
  if(i < 2) filename[0] = '\0';

  doc = bow_sarray_entry_at_index (archer_docs, di);
  cur = 0;

  if (!doc)
  {
    fprintf(out, "<archer-error>No such document id (%d)</archer-error>\n",
	    di);
    fflush(out);
    return;
  }

  for (i = 0; i < last_query_wi_len; i++)
    query_pi[i] =
      archer_server_query_hits_matching_di_wi_li (di, last_query_wi[i],
						  last_query_li[i],
						  &query_pi_len[i]);

  fprintf (out, "<document>");

  file = fopen ((filename[0] == '\0' ? bow_sarray_keystr_at_index (archer_docs, di) : filename), "r");
  if (file == NULL)
    {
      perror (bow_sarray_keystr_at_index(archer_docs, di));
      fprintf (out, "</document>\n");
      return;
    }
  
  switch (bow_flex_option)
    {
    case USE_MAIL_FLEXER :
      set_fp = flex_mail_open;
      get_word_extended = flex_mail_get_word_extended;
      break;
    case USE_TAGGED_FLEXER :
      set_fp = tagged_lex_open_dont_parse_tags;
      get_word_extended = tagged_lex_get_word_extended;
      break;
    default :
      bow_error("Unrecognized bow_flex_option=%d\n", bow_flex_option);
    }
  
  set_fp(file, bow_sarray_keystr_at_index (archer_docs, di));

  current_pi = 0;
  while (get_word_extended(word, BOW_MAX_WORD_LENGTH, &start, &end))
    {
      int this_wi, match;
      long save;

      this_wi = bow_word2int_no_add(word);
      match = 0;

      /*      printf("%d %s\n", current_pi, word); */

      for (i = 0; (i < last_query_wi_len) && !match; i++)
	if (last_query_wi[i] == this_wi)
	  {
	    int j;

	    for (j = 0; (j < query_pi_len[i]) && !match; j++) {
	      if (query_pi[i]->entry[j].wi == current_pi) 
		match = 1;
	    }
	  }

      save = ftell(file);
      fseek(file, cur, SEEK_SET);
      for (; cur < start; cur++) xml_fputc(fgetc(file), out);
      if(match) fprintf(out, "<match>");
      for (; cur <= end; cur++) xml_fputc(fgetc(file), out);
      if(match) fprintf(out, "</match>");

      fseek(file, save, SEEK_SET);
      current_pi++;
    }

  fseek(file, cur, SEEK_SET);
  {
    int c;

    while((c = fgetc(file)) != EOF) xml_fputc(c, out);
  }

  fclose (file);
  fprintf (out, "</document>\n");
  fflush (out);
}

static void
archer_server_hits (char *hits, FILE * out)
{
  int i;
  int first_hit, last_hit;

  if (!strncmp(hits, "all", 3)) 
    {
      fprintf (out, "Will return all hits.\n");
      archer_first_hit = -1;
      archer_last_hit = -1;
      return;
    }
  i = sscanf (hits, "%d %d", &first_hit, &last_hit);
  if (i < 2 || last_hit < first_hit || first_hit < 0 || last_hit < 0) 
    {
      fprintf (out, "Invalid hits range: %s", hits);
      return;
    }
  fprintf (out, "Will return hits %d to %d\n", first_hit, last_hit);
  archer_first_hit = first_hit;
  archer_last_hit = last_hit;
}

static void
archer_server_index (char *filename, FILE * out)
{
  archer_index_filename (filename, NULL);
  archer_archive();
}

static void
archer_server_index_with_markup (char *command, FILE * out)
{
  extern char *archer_extraction_filename;
  char *cp, *filename, *filenameend = NULL, buf[strlen(command)];

  strcpy(buf, command);
  cp = buf;
  while (*cp && isspace(*cp))
    ++cp;

  if (!*cp)
    goto BAD_SYNTAX;

  filename = cp;
  while (*cp && !isspace(*cp))
    ++cp;

  if (!*cp)
    goto BAD_SYNTAX;

  filenameend = cp++;
  *filenameend = 0;

  while (*cp && isspace(*cp))
    ++cp;

  if (!*cp)
    goto BAD_SYNTAX;

  archer_extraction_filename = cp;

  while (*cp && !isspace(*cp))
    ++cp;

  if (*cp)
    *cp = 0;

  bow_verbosify (bow_progress, "Indexing %s (markup=%s)...\n", 
		 filename, archer_extraction_filename);
  archer_index_filename (filename, NULL);
  archer_archive();
  if(bow_annotation_filename != NULL)
    archer_annotations = 
      annotation_sarray_reread(archer_annotations, bow_annotation_filename);
  bow_verbosify (bow_progress, "Done indexing %s.\n", filename);
  archer_extraction_filename = NULL;

  return;

 BAD_SYNTAX:
  
  if (filenameend)
    *filenameend = ' ';

  bow_error("Can't understand line: nindex %s", command);
  archer_extraction_filename = NULL;
}

void
archer_query_serve ()
{
  const char *cp;
  char prefork_port_num[100];

  /* XXX: probably not the right way to do things */
  archer_query_lexer = &_archer_query_lexer;

  memcpy (archer_query_lexer, bow_simple_lexer,
	  sizeof(typeof(*bow_simple_lexer)));

  for (cp = archer_arg_state.server_port_num; *cp; ++cp)
    if (!isdigit(*cp))
      break;

  if (*cp)
    sprintf(prefork_port_num, "%s0", archer_arg_state.server_port_num);
  else
    snprintf(prefork_port_num, 9, "%d", atoi (archer_arg_state.server_port_num) + 1);

  archer_query_socket_init (&archer_sockfd, archer_arg_state.server_port_num, *cp); 
  archer_query_socket_init (&archer_prefork_sockfd, prefork_port_num, *cp);

  for (;;)
    archer_query_serve_one_query ();
}

/* A temporary hack.  Also, does not work for queries containing
   repeated words */
static void
archer_server_query (char *query, FILE * out)
{
  int i;
  int first_hit, last_hit;
#define NUM_FLAGS 3
  enum
    {
      pos = 0,
      reg,
      neg,
      num_flags
    };
  struct _word_hit
    {
      const char *term;
      bow_wa *wa;
      int flag;
    }
  word_hits[num_flags][MAX_QUERY_WORDS];
  int word_hits_count[num_flags];
  int current_wai[num_flags][MAX_QUERY_WORDS];
  struct _doc_hit
    {
      int di;
      float score;
      const char **terms;
      int terms_count;
    }
   *doc_hits;
  int doc_hits_count;
  int doc_hits_size;
  bow_wa *term_wa;
  int current_di, h, f, min_di;
  int something_was_greater_than_max;
  char *query_copy, *query_remaining, *end;
  char query_string[BOW_MAX_WORD_LENGTH];
  char suffix_string[BOW_MAX_WORD_LENGTH];
  int found_flag, flag, length;

  /* For sorting the combined list of document hits */
  int compare_doc_hits (struct _doc_hit *hit1, struct _doc_hit *hit2)
  {
    if (hit1->score < hit2->score)
      return 1;
    else if (hit1->score == hit2->score)
      return 0;
    else
      return -1;
  }

  last_query_wi_len = 0;

  /* Initialize the list of target documents associated with each term */
  for (i = 0; i < num_flags; i++)
    word_hits_count[i] = 0;

  /* Initialize the combined list of target documents */
  doc_hits_size = 1000;
  doc_hits_count = 0;
  doc_hits = bow_malloc (doc_hits_size * sizeof (struct _doc_hit));

  /* Process each term in the query.  Quoted sections count as one
     term here. */
  query_remaining = query_copy = strdup (query);
  assert (query_copy);
  /* Chop any trailing newline or carriage return. */
  end = strpbrk (query_remaining, "\n\r");
  if (end)
    *end = '\0';
  while (*query_remaining)
    {
      /* Find the beginning of the next query term, and record +/- flags */
      while (*query_remaining
	     && (!isalnum (*query_remaining)
		 && *query_remaining != ':'
		 && *query_remaining != '+'
		 && *query_remaining != '-'
		 && *query_remaining != '"'))
	query_remaining++;
      flag = reg;
      found_flag = 0;
      if (*query_remaining == '\0')
	{
	  break;
	}
      if (*query_remaining == '+')
	{
	  query_remaining++;
	  flag = pos;
	}
      else if (*query_remaining == '-')
	{
	  query_remaining++;
	  flag = neg;
	}

      /* See if there is a field-restricting tag here, and if so, deal
         with it */
      if ((end = strpbrk (query_remaining, ": \"\t"))
	  && *end == ':')
	{
	  /* The above condition ensures that a ':' appears before any
	     term-delimiters */
	  /* Find the end of the field-restricting suffix */
	  length = end - query_remaining;
	  assert (length < BOW_MAX_WORD_LENGTH);
	  /* Remember the suffix, and move ahead the QUERY_REMAINING */
	  memcpy (suffix_string, query_remaining, length);
	  suffix_string[length] = '\0';
	  query_remaining = end + 1;
	}
      else
	suffix_string[0] = '\0';

      /* Find the end of the next query term. */
      if (*query_remaining == '"')
	{
	  query_remaining++;
	  end = strchr (query_remaining, '"');
	}
      else
	{
	  end = strchr (query_remaining, ' ');
	}
      if (end == NULL)
	end = strchr (query_remaining, '\0');

      /* Put the next query term into QUERY_STRING and increment
         QUERY_REMAINING */
      length = end - query_remaining;
      length = MIN (length, BOW_MAX_WORD_LENGTH - 1);
      memcpy (query_string, query_remaining, length);
      query_string[length] = '\0';
      if (*end == '"')
	query_remaining = end + 1;
      else
	query_remaining = end;
      if (length == 0)
	continue;
      /* printf ("%d %s\n", flag, query_string); */

      /* Get the list of documents matching the term */
      term_wa = archer_server_query_hits_matching_sequence (query_string,
						     suffix_string);
      if (!term_wa)
	{
	  if (flag == pos)
	    /* A required term didn't appear anywhere.  Print nothing */
	    goto hit_combination_done;
	  else
	    continue;
	}

      word_hits[flag][word_hits_count[flag]].term = strdup (query_string);
      word_hits[flag][word_hits_count[flag]].wa = term_wa;
      word_hits[flag][word_hits_count[flag]].flag = flag;
      word_hits_count[flag]++;
      assert (word_hits_count[flag] < MAX_QUERY_WORDS);
      bow_verbosify (bow_progress, "%8d %s\n", term_wa->length, query_string);
    }

  /* Bring together the WORD_HITS[*], following the correct +/-
     semantics */
  current_di = 0;
  for (f = 0; f < num_flags; f++)
    for (h = 0; h < word_hits_count[f]; h++)
      current_wai[f][h] = 0;

next_current_di:
  if (word_hits_count[pos] == 0)
    {
      /* Find a document in which a regular term appears, and align the
         CURRENT_WAI[REG][*] to point to the document if exists in that list */
      min_di = INT_MAX;
      for (h = 0; h < word_hits_count[reg]; h++)
	{
	  if (current_wai[reg][h] != -1
	      && (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		  < current_di))
	    {
	      if (current_wai[reg][h] < word_hits[reg][h].wa->length - 1)
		current_wai[reg][h]++;
	      else
		current_wai[reg][h] = -1;
	    }
	  assert (current_wai[reg][h] == -1
		  || (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		      >= current_di));
	  if (current_wai[reg][h] != -1
	    && word_hits[reg][h].wa->entry[current_wai[reg][h]].wi < min_di)
	    min_di = word_hits[reg][h].wa->entry[current_wai[reg][h]].wi;
	}
      if (min_di == INT_MAX)
	goto hit_combination_done;

      current_di = min_di;
    }
  else
    {
      /* Find a document index in which all the +terms appear */
      /* Loop until current_wai[pos][*] all point to the same document index */
      do
	{
	  something_was_greater_than_max = 0;
	  for (h = 0; h < word_hits_count[pos]; h++)
	    {
	      while (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi
		     < current_di)
		{
		  if (current_wai[pos][h] < word_hits[pos][h].wa->length - 1)
		    current_wai[pos][h]++;
		  else
		    /* We are at the end of a + list, and thus are done. */
		    goto hit_combination_done;
		}
	      if (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi
		  > current_di)
		{
		  current_di =
		    word_hits[pos][h].wa->entry[current_wai[pos][h]].wi;
		  something_was_greater_than_max = 1;
		}
	    }
	}
      while (something_was_greater_than_max);
      /* At this point all the CURRENT_WAI[pos][*] should be pointing to the
         same document.  Verify this. */
      for (h = 1; h < word_hits_count[pos]; h++)
	assert (word_hits[pos][h].wa->entry[current_wai[pos][h]].wi
		== word_hits[pos][0].wa->entry[current_wai[pos][0]].wi);
    }

  /* Make sure the CURRENT_DI doesn't appear in any of the -term lists. */
  for (h = 0; h < word_hits_count[neg]; h++)
    {
      /* Loop until we might have found the CURRENT_DI in this neg list */
      while (current_wai[neg][h] != -1
	     && (word_hits[neg][h].wa->entry[current_wai[neg][h]].wi
		 < current_di))
	{
	  if (current_wai[neg][h] < word_hits[neg][h].wa->length - 1)
	    current_wai[neg][h]++;
	  else
	    current_wai[neg][h] = -1;
	}
      if (word_hits[neg][h].wa->entry[current_wai[neg][h]].wi == current_di)
	{
	  current_di++;
	  goto next_current_di;
	}
    }

  /* Add this CURRENT_DI to the combinted list of hits in DOC_HITS */
  assert (current_di < archer_docs->array->length);
  doc_hits[doc_hits_count].di = current_di;
  doc_hits[doc_hits_count].score = 0;
  for (h = 0; h < word_hits_count[pos]; h++)
    doc_hits[doc_hits_count].score +=
      word_hits[pos][h].wa->entry[current_wai[pos][h]].weight;
  doc_hits[doc_hits_count].terms_count = 0;
  doc_hits[doc_hits_count].terms = bow_malloc (MAX_QUERY_WORDS * sizeof (char *));

  /* Add score value from the regular terms, if CURRENT_DI appears there */
  for (h = 0; h < word_hits_count[reg]; h++)
    {
      if (word_hits_count[pos] != 0)
	{
	  while (current_wai[reg][h] != -1
		 && (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
		     < current_di))
	    {
	      if (current_wai[reg][h] < word_hits[reg][h].wa->length - 1)
		current_wai[reg][h]++;
	      else
		current_wai[reg][h] = -1;
	    }
	}
      if (word_hits[reg][h].wa->entry[current_wai[reg][h]].wi
	  == current_di)
	{
	  doc_hits[doc_hits_count].score +=
	    word_hits[reg][h].wa->entry[current_wai[reg][h]].weight;
	  doc_hits[doc_hits_count].
	    terms[doc_hits[doc_hits_count].terms_count]
	    = word_hits[reg][h].term;
	  doc_hits[doc_hits_count].terms_count++;
	}
    }

  doc_hits_count++;
  if (doc_hits_count >= doc_hits_size)
    {
      doc_hits_size *= 2;
      doc_hits = bow_realloc (doc_hits, (doc_hits_size
					 * sizeof (struct _doc_hit)));
    }

  current_di++;
  goto next_current_di;

hit_combination_done:

  /* Sort the DOC_HITS list */
  qsort (doc_hits, doc_hits_count, sizeof (struct _doc_hit),
	   (int (*)(const void *, const void *)) compare_doc_hits);

  fprintf (out, "<hitlist>\n<count>%d</count>\n", doc_hits_count);

  first_hit = archer_first_hit != -1 ? archer_first_hit : 0;
  last_hit = doc_hits_count - 1;
  if (archer_last_hit != -1 && archer_last_hit < last_hit)
    last_hit = archer_last_hit;

  for (i = first_hit; i <= last_hit; i++)
    {
      int t;
      fprintf (out, 
	       "<hit>\n"
	       "        <id>%d</id>\n"
	       "        <name>%s</name>\n"
	       "        <score>%f</score>\n",
	       doc_hits[i].di, 
	       bow_sarray_keystr_at_index (archer_docs, doc_hits[i].di), 
	       doc_hits[i].score);
      for (t = 0; t < doc_hits[i].terms_count; t++) 
	{
	  fprintf (out,
		   "        <term>%s</term>\n",
		   doc_hits[i].terms[t]);
	}
      if (archer_annotations != NULL) {
	int j;
	annotation* a = bow_sarray_entry_at_keystr (archer_annotations,
						    bow_sarray_keystr_at_index (archer_docs, doc_hits[i].di));
	if(a != NULL) { 
	  for (j = 0; j < a->count; j++) {
	    fprintf (out, 
		     "        <annotation>\n"
		     "            <name>");
	    xml_fputs (a->feats[j], out);
	    fprintf (out,
		     "</name>\n"
		     "            <value>");
	    xml_fputs (a->vals[j], out);
	    fprintf (out,
		     "</value>\n"
		     "        </annotation>\n");
	  }
	}
      }
      fprintf (out, "</hit>\n");
    }
  fprintf (out, "</hitlist>\n");

  /* Free all the junk we malloc'ed */
  for (f = 0; f < num_flags; f++)
    for (h = 0; h < word_hits_count[f]; h++)
      bow_free ((char *) word_hits[f][h].term);
  for (h = 0; h < doc_hits_count; h++)
    bow_free (doc_hits[h].terms);
  bow_free (doc_hits);
  bow_free (query_copy);
}

static bow_wa*
archer_server_query_hits_matching_sequence (const char *query_string,
				     const char *suffix_string)
{
  int query[MAX_QUERY_WORDS];		/* WI's in the query */
  int di[MAX_QUERY_WORDS];
  int pi[MAX_QUERY_WORDS];
  int li = -1;
  int query_len;
  int max_di, max_pi;
  int wi, i;
  bow_lex *lex;
  char word[BOW_MAX_WORD_LENGTH];
  int sequence_occurrence_count = 0;
  int something_was_greater_than_max;
  bow_wa *wa;
  float scaler;
  archer_doc *doc;
  archer_label *label;

  if (bow_flex_option) { /* (bow_flex_option != 0) == we should use labels ? */ 
    if (suffix_string[0])
      {
	label = bow_sarray_entry_at_keystr(archer_labels, suffix_string);
	
	/* No words occur in label */
	if (!label)
	  return NULL;
	
	li = label->li;
      }
  }

  /* Parse the query */
  /* I've changed bow_default_lexer to archer_query_lexer (==bow_simple_lexer)
     everywhere in this function because it seems to make things work. 
     Have no idea if this is the right way to do it, though. -JCR */
  lex = archer_query_lexer->open_str (archer_query_lexer, (char*)query_string);
  if (lex == NULL)
    return NULL;
  query_len = 0;
  while (archer_query_lexer->get_word (archer_query_lexer, lex,
				      word, BOW_MAX_WORD_LENGTH))
    {
      
      if (!bow_flex_option) { /* (bow_flex_option == 0) == we should use 'xxx' ? */ 
	/* Add the field-restricting suffix string, e.g. "xxxtitle" */
	if (suffix_string[0])
	  {
	    strcat (word, "xxx");
	    strcat (word, suffix_string);
	    assert (strlen (word) < BOW_MAX_WORD_LENGTH);
	  }
      }
      wi = bow_word2int_no_add (word);
      if (wi >= 0)
	{
	  di[query_len] = pi[query_len] = -300;
	  query[query_len++] = wi;
	}
      else if ((bow_lexer_stoplist_func
		&& !(*bow_lexer_stoplist_func) (word))
	       || (!bow_lexer_stoplist_func
		   && strlen (word) < 2))
	{
	  /* If a query term wasn't present, and its not because it
	     was in the stoplist or the word is a single char, then
	     return no hits. */
	  query_len = 0;
	  break;
	}
      /* If we have no more room for more query terms, just don't use
         the rest of them. */
      if (query_len >= MAX_QUERY_WORDS)
	break;
    }
  archer_query_lexer->close (archer_query_lexer, lex);
  if (query_len == 0)
    return NULL;

  if (query_len == 1)
    {
      last_query_wi[last_query_wi_len] = query[0];
      last_query_li[last_query_wi_len++] = li;
      wa = archer_query_hits_matching_wi (query[0], li,
					  &sequence_occurrence_count);
      goto search_done;
    }

  /* Initialize the array of document scores */
  wa = bow_wa_new (0);

  /* Search for documents containing the query words in the same order
     as the query. */
  bow_wi2pv_rewind (archer_wi2pv);
  max_di = max_pi = -200;
  /* Loop while we look for matches.  We'll break out of this loop when
     all query words are at the end of their PV's. */
  for (;;)
    {
      /* Keep reading DI and PI from one or more of the query-word PVs
	 until none of the DIs or PIs is greater than the MAX_DI or
	 MAX_PI.  At this point the DIs and PI's should all be equal,
	 indicating a match.  Break out of this loop if all PVs are
	 at the end, (at which piont they return -1 for both DI and
	 PI). */
      do
	{
	  int lia[100];
	  int ln;

	  something_was_greater_than_max = 0;
	  for (i = 0; i < query_len; i++)
	    {
	      while (di[i] != -1
		  && (di[i] < max_di
		      || (di[i] <= max_di && pi[i] < max_pi)))
		{
		  ln = 100;
		  bow_wi2pv_wi_next_di_li_pi (archer_wi2pv, query[i],
					   &(di[i]), lia, &ln, &(pi[i]));

		  /* If any of the query words is at the end of their
		     PV, then we're not going to find any more
		     matches, and we're done setting the scores.  Go
		     print the matching documents. */
		  if (di[i] == -1)
		    goto search_done;

		  /* Make it so that all PI's will be equal if the words
		     are in order. */
		  pi[i] -= i;
		  bow_verbosify (bow_verbose, "%20s %10d %10d %10d %10d\n", 
				 bow_int2word (query[i]), 
				 di[i], pi[i], max_di, max_pi);
		}
	      if (di[i] > max_di) 
		{
		  max_di = di[i];
		  max_pi = pi[i];
		  something_was_greater_than_max = 1;
		}
	      else if (pi[i] > max_pi && di[i] == max_di) 
		{
		  max_pi = pi[i];
		  something_was_greater_than_max = 1;
		}
	      else if ((di[i] == max_di) && (pi[i] == max_pi))
		{
		  int j;

		  for (j = 0; j < ln; ++j) if (lia[j] == li) break;
		  if((ln != 0) && (j == ln)) something_was_greater_than_max = 1;
		}
	    }
	}
      while (something_was_greater_than_max);
      bow_verbosify (bow_verbose, 
		     "something_was_greater_than_max di=%d\n", di[0]);
      for (i = 1; i < query_len; i++)
	assert (di[i] == di[0] && pi[i] == pi[0]);
      
      /* Make sure this DI'th document hasn't been deleted.  If it
         hasn't then add this DI to the WA---the list of hits */
      doc = bow_sarray_entry_at_index (archer_docs, di[0]);
      if (doc->word_count > 0)
	{
	  bow_wa_add_to_end (wa, di[0], 1);
	  sequence_occurrence_count++;
	}

      /* Set up so that next time through we'll read the next words
         from each PV. */
      for (i = 0; i < query_len; i++)
	{
	  if (di[i] != -1)
	    di[i] = -300;
	  if (pi[i] != -1)
	    pi[i] = -300;
	}
    }
 search_done:

  /* Scale the scores by the log of the occurrence count of this sequence,
     and take the log of the count (shifted) to encourage documents that
     have all query term to be ranked above documents that have many 
     repetitions of a few terms. */
  scaler = 1.0 / log (5 + sequence_occurrence_count);
  for (i = 0; i < wa->length; i++)
    wa->entry[i].weight = scaler * log (5 + wa->entry[i].weight);

  if (wa->length == 0)
    {
      bow_wa_free (wa);
      return NULL;
    }
  return wa;
}


static void
archer_server_print_hit(archer_query_result *aqrp, FILE *out)
{
  const char *str;
  int j;
  annotation *a;

  fprintf(out,
	  "<hit>\n"
	  "        <id>%d</id>\n"
	  "        <name>%s</name>\n"
	  "        <score>%f</score>\n",
	  aqrp->di,
	  bow_sarray_keystr_at_index(archer_docs, aqrp->di),
	  aqrp->score);

  for (j = 0; j < aqrp->wo->length; ++j)
  {
    archer_query_word_occurence *aqwop =(archer_query_word_occurence *)
      bow_array_entry_at_index(aqrp->wo, j);
    fprintf (out, "        <term>%s</term>\n",bow_int2word(aqwop->wi));
  }

  if (archer_annotations == NULL)
    goto DONE_PRINTING_HIT;

  str = bow_sarray_keystr_at_index (archer_docs, aqrp->di);
  a = bow_sarray_entry_at_keystr (archer_annotations, str);

  if (a == NULL)
    goto DONE_PRINTING_HIT;
				
  for (j = 0; j < a->count; j++) 
  {
    fprintf (out, "        <annotation>\n            <name>");
    xml_fputs (a->feats[j], out);
    fprintf (out,"</name>\n            <value>");
    xml_fputs (a->vals[j], out);
    fprintf (out,"</value>\n        </annotation>\n");
  }

 DONE_PRINTING_HIT:
  fprintf (out, "</hit>\n");
}


/*
  Compare two query result scores (for use with qsort).
*/
static int
compare_qr (const void *p1, const void *p2)
{
  const archer_query_result *hit1 = *(archer_query_result **)p1,
                            *hit2 = *(archer_query_result **)p2;
  if (hit1->score < hit2->score)
    return 1;
  else if (hit1->score == hit2->score)
    return 0;
  else
    return -1;
}


static void
archer_server_print_hitlist(bow_array *results, FILE *out)
{
  int i, first_hit, last_hit;
  archer_query_result *res[results->length];

  for (i = 0; i < results->length; ++i)
    res[i] = (archer_query_result *)bow_array_entry_at_index(results, i);

  qsort(res, results->length, sizeof(archer_query_result *), compare_qr);

  first_hit = archer_first_hit != -1 ? archer_first_hit : 0;
  last_hit = results->length - 1;
  if (archer_last_hit != -1 && archer_last_hit < last_hit)
    last_hit = archer_last_hit;
  
  for (i = first_hit; i <= last_hit; i++)
    archer_server_print_hit(res[i], out);
}


static void
archer_server_query_new (char* query_string, FILE* out)
{
  bow_index index;
  bow_array *results;

  archer_query_setup(query_string);

  if (archer_query_parse ())
  { 
    fprintf (out,
	     "<archer-error>\n"
	     "        Error processing the query: %s\n"
	     "</archer-error>\n", archer_query_errorstr);
    return;
  }

  index.wi2pv = archer_wi2pv;
  index.li2pv = archer_li2pv;
  results = archer_query_execute (&index, archer_query_tree);

  fprintf(out, "<hitlist>\n<count>%d</count>\n", results->length);

  if (results->length)
    archer_server_print_hitlist(results, out);

  bow_array_free(results);

  fprintf(out, "</hitlist>\n");
}


static int *
archer_server_matching_pis(int di, int *count)
{
  int i, j, space, *matches = NULL;
  bow_index index;
  bow_array *ret;
  archer_query_result *aqrp;
  int int_cmp (const void *p1, const void *p2)
  {
    const int i1 = *((int *)p1), 
              i2 = *((int *)p2);

    if (i1 < i2)
      return -1;
    else if (i1 > i2)
      return +1;
    else
      return 0;
  }

  index.wi2pv = archer_wi2pv;
  index.li2pv = archer_li2pv;
  ret = archer_query_repeat_for_document(&index, di); 
  if (!ret)
    return NULL;
  if (ret->length < 1)
  {
    bow_array_free(ret);
    return NULL;
  }

  aqrp = (archer_query_result *)bow_array_entry_at_index(ret, 0);
  matches = calloc(space = 1024, sizeof(int));
  *count = 0;
  for (i = 0; i < aqrp->wo->length; ++i)
  {
    archer_query_word_occurence *aqwop =
      (archer_query_word_occurence *)bow_array_entry_at_index(aqrp->wo, i);
    for (j = 0; j < aqwop->pi->length; ++j)
    {
      int pi = *((int *)bow_array_entry_at_index(aqwop->pi, j));
      if (*count >= space)
      {
	space *= 2;
	matches = realloc(matches, space * sizeof(int));
      }
      matches[(*count)++] = pi;
    }
  }

  qsort(matches, *count, sizeof(int), int_cmp);

  bow_array_free(ret);

  return matches;
}

typedef int (*getword_func)(char buf[], int bufsz, long* start, long* end);

static void
archer_server_do_dump(FILE *out, FILE *file, getword_func getw, 
		      int *matching_pis, int match_count)
{
  char word[BOW_MAX_WORD_LENGTH];
  int current_pi, matchi, match, c;
  long start, end, save, cur;

  current_pi = 0;
  matchi = 0;
  cur = 0;
  while (getw(word, BOW_MAX_WORD_LENGTH, &start, &end))
  {
    if (matchi < match_count && current_pi == matching_pis[matchi])
    {
      match = 1;
      ++matchi;
    }
    else
      match = 0;

    save = ftell(file);
    fseek(file, cur, SEEK_SET);
    for (; cur < start; cur++) xml_fputc(fgetc(file), out);
    if(match) fprintf(out, "<match>");
    for (; cur <= end; cur++) xml_fputc(fgetc(file), out);
    if(match) fprintf(out, "</match>");

    fseek(file, save, SEEK_SET);
    current_pi++;
  }

  fseek(file, cur, SEEK_SET);
  while((c = fgetc(file)) != EOF) xml_fputc(c, out);
}


static int
archer_server_dump_preamble(char *id, FILE **file, getword_func *getw)
{
  char filename[255] = {0};
  int di;
  void (*set_fp)(FILE *fp, const char * name) = NULL;

  sscanf(id, "%d %s", &di, filename);
  *file = fopen ((filename[0] == '\0' ? bow_sarray_keystr_at_index (archer_docs, di) : filename), "r");
  if (*file == NULL)
      return -1;
  
  switch (bow_flex_option)
    {
    case USE_MAIL_FLEXER :
      set_fp = flex_mail_open;
      *getw = flex_mail_get_word_extended;
      break;
    case USE_TAGGED_FLEXER :
      set_fp = tagged_lex_open_dont_parse_tags;
      *getw = tagged_lex_get_word_extended;
      break;
    default :
      bow_error("Unrecognized bow_flex_option=%d\n", bow_flex_option);
    }
  
  set_fp(*file, bow_sarray_keystr_at_index (archer_docs, di));

  return di;
}

  
static void
archer_server_dump_new (char *id, FILE* out)
{
  int match_count, *match, di;
  FILE *file;
  getword_func get_word_extended = NULL;

  di = archer_server_dump_preamble(id, &file, &get_word_extended);
  if (di == -1)
    return;
  match = archer_server_matching_pis(di, &match_count);
  fprintf (out, "<document>");

  archer_server_do_dump(out, file, get_word_extended, match, (match ? match_count : 0));

  fclose (file);
  fprintf (out, "</document>\n");
  fflush (out);

  free(match);
}


static void
archer_server_fields(FILE *out)
{
  int len = archer_labels->array->length;
  int i;

  fprintf(out, "<fieldlist>\n");
  for (i = 0; i < len; ++i)
    fprintf(out, "   <field>%s</field>\n", 
	    bow_sarray_keystr_at_index(archer_labels, i));
  fprintf(out, "</fieldlist>\n");
  fflush(out);
}


/*
  Display the list of files indexed in XML format.
*/
static void
archer_server_docs(FILE *out)
{
  static char *fstr =
    "  <document>\n    <id>%d</id>\n    <name>%s</name>\n  </document>\n";
  int len = archer_docs->array->length;
  int i;

  fprintf(out, "<doclist>\n");
  for (i = 0; i < len; ++i)
    fprintf(out, fstr, i, bow_sarray_keystr_at_index(archer_docs, i));
  fprintf(out, "</doclist>\n");
  fflush(out);
}


/*
  Print the rank of named file in hit list of provided query.
*/
static void
archer_server_rank(char *command, FILE *out)
{
  char fname[BOW_MAX_WORD_LENGTH];
  char *cp;
  bow_index index;
  bow_array *results;

  while (*command && isspace(*command))
    ++command;

  if (!*command)
    goto RANK_ERROR;

  if (!sscanf(command, "%s", fname))
    goto RANK_ERROR;

  cp = command + strlen(fname);
  while (*cp && isspace(*cp))
    ++cp;

  if (!*cp)
    goto RANK_ERROR;

  archer_query_setup(cp);

  if (archer_query_parse())
    {
      fprintf (out,
	       "<archer-error>\n"
	       "        Error processing the query: %s\n"
	       "</archer-error>\n", archer_query_errorstr);
      return;
    }

  index.wi2pv = archer_wi2pv;
  index.li2pv = archer_li2pv;
  results = archer_query_execute (&index, archer_query_tree);

  fprintf(out, "<rank-result>\n  <count>%d</count>\n", results->length);

  if (results->length)
    {
      int i;
      archer_query_result *res[results->length];

      for (i = 0; i < results->length; ++i)
	res[i] = (archer_query_result *)bow_array_entry_at_index(results, i);

      qsort(res, results->length, sizeof(archer_query_result *), compare_qr);

      for (i = 0; i < results->length; ++i)
	if (strcmp(fname, bow_sarray_keystr_at_index(archer_docs, res[i]->di))
	    == 0)
	  {
	    fprintf(out, "  <rank>%d</rank>\n", i);
	    break;
	  }

      if (i == results->length)
	fprintf(out, 
		"  <rank-not-found>Document not found in hitlist<rank-not-found>\n");
    }

  bow_array_free(results);
  fprintf(out, "</rank-result>\n");
  return;

 RANK_ERROR:

  fprintf(out, "<rank-error>Couldn't understand command<rank-error>\n");
}
