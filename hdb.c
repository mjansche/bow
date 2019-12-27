/* hdb.c - Simple File-system like database library

   Written by:  Jason Rennie <jrennie@jprc.com>, August, 1998

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

/**************************************************************
 * NOTE: This package assumes that longs are at least 32-bits *
 **************************************************************/

/*********************************************************************
 * NOTE: This package assumes that a database is read to and written *
 * from machines with the same basic architechture (same endian-ness *
 * and same size unsigned longs)                                     *
 *********************************************************************/

/* Maximum datbase size is 2^32 - 1 */


/* Need to write file position values as strings, not integers */

/***********************
 * Globals/Environment *
 ***********************/

#include <bow/hdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <db.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

/* Names of files for hash table index, text file storage */
char hdb_index[] = "index";
char hdb_barrel[] = "barrel";

/* Directory where opened database is stored.  NULL if no
 * database is opened */
char *hdb_open_db = NULL;

/* Pointer to Berkeley DB Hash object */
DB *HDB_HASH = NULL;

/* Pointer to barrel file */
int HDB_BARREL = 0;

/* Converts integer to string */
char *itoa (int num);

/***************
 * Subroutines *
 ***************/

/* Prepares for database access by opening Berkeley DB index file and
 * opening barrel file.  If NO_CREATE is *not* set, database files will
 * be created if they do not already exist.
 * Returns 1 upon success, 0 upon failure. */
int hdb_open (char * dir, int no_create)
{
  /* fully qualified file names */
  char *fq_index = NULL;
  char *fq_barrel = NULL;
  struct stat stat_index;
  struct stat stat_barrel;
  struct stat stat_dir;

  fq_index = malloc (strlen (dir) + strlen (hdb_index) + 2);
  fq_barrel = malloc (strlen (dir) + strlen (hdb_barrel) + 2);

  sprintf (fq_index, "%s/%s", dir, hdb_index);
  sprintf (fq_barrel, "%s/%s", dir, hdb_barrel);

  if (hdb_open_db != NULL)
    {
      fprintf (stderr, "hdb_open: %s has already been opened.  This system"
	       " does\n  not support multiple databases being opened at"
	       " once.\n", hdb_open_db);
      return 0;
    }

  /* Check for existence of index file */
  if (stat (fq_index, &stat_index))
    {
      if (no_create)
	{
	  fprintf (stderr, "hdb_open: Not able to open %s: %s\n", fq_index,
		   strerror (errno));
	  return 0;
	}
      /* Check for existence of database directory, create if necessary */
      if (stat (dir, &stat_dir))
	{
	  if (mkdir (dir, 0775))
	    {
	      fprintf (stderr, "hdb_open: Not able to make directory %s: %s\n",
		       dir, strerror (errno));
	      return 0;
	    }
	}
      if (! stat (fq_index, &stat_barrel))
	{
	  fprintf (stderr, "hdb_open: Stale %s file exists.  Please remove\n",
		   hdb_barrel);
	  return 0;
	}
      /* Create a new index using Berkeley DB calls */
      if (! (HDB_HASH = dbopen (fq_index, O_RDWR|O_CREAT, 0644, DB_HASH, NULL)))
	{
	    fprintf (stderr, "hdb_open: Not able to create %s as DB file: %s\n", fq_index, strerror (errno));
	    return 0;
	}
      if (! (HDB_BARREL = open (fq_barrel, O_RDWR|O_CREAT, 0644)))
	{
	  fprintf (stderr, "hdb_open: Not able to create %s: %s\n", fq_barrel,
		   strerror (errno));
	  HDB_HASH->close (HDB_HASH);
	  return 0;
	}
      hdb_open_db = strdup (dir);
      return 1;
    }
  
  /* Index exists.  Open it. */
  if (! (HDB_HASH = dbopen (fq_index, O_RDWR, 0644, DB_HASH, NULL)))
    {
      fprintf (stderr, "hdb_open: Not able to open %s as DB file: %s\n",
	       fq_index, strerror (errno));
      return 0;
    }
  if (! (HDB_BARREL = open (fq_barrel, O_RDWR, 0644)))
    {
      fprintf (stderr, "hdb_open: Not able to open %s for reading/writing: %s\n", fq_barrel, strerror (errno));
      HDB_HASH->close (HDB_HASH);
      return 0;
    }
  
  hdb_open_db = strdup (dir);
  return 1;
}


/* Closes an HDB database; makes sure that all files have been sync'ed.
 * Returns 1 upon success, 0 upon failure */
int hdb_close ()
{
  if (! hdb_open_db)
    {
      fprintf (stderr, "hdb_close: No database open\n");
      return 0;
    }

  close (HDB_BARREL);
  HDB_HASH->close (HDB_HASH);

  free (hdb_open_db);

  /* This tells us that the database is no longer open */
  hdb_open_db = NULL;

  return 1;
}


/* Adds an entry to the database or modifies an existing entry.  Note that
 * if an entry is modified, some of the space used to store the old entry
 * will remain used throughout the life of the database.
 * Returns 1 upon success, 0 upon failure. */
int hdb_put (char *key, char *data)
{
  unsigned long end, len;
  DBT dbt_key, dbt_data;
  char *str;

  dbt_key.data = key;
  dbt_key.size = strlen (key);
  
  if (! hdb_open_db)
    {
      fprintf (stderr, "hdb_put: No database open\n");
      return 0;
    }
  
  /* Go to end of barrel file */
  end = lseek (HDB_BARREL, 0, SEEK_END);
  if (end == -1)
    {
      fprintf (stderr, "hdb_put: Seek failed on barrel file: %s\n",
	       strerror (errno));
      return 0;
    }
  
  /* Store end of file location in DB as string */
  str = itoa (end);
  dbt_data.data = str;
  dbt_data.size = strlen (str);
  
  len = strlen (data);
  /* Check for overflow */
  if (end + 4 + len > 4294967295UL || end + 4 + len <= end)
    {
      fprintf (stderr, "hdb_put: Not able to add item.  Max size exceeded.\n"
	       "  Max size is 2^32 - 1 bytes\n");
      return 0;
    }

  if (HDB_HASH->put (HDB_HASH, &dbt_key, &dbt_data, 0))
    {
      fprintf (stderr, "Not able to add item (key: %s): %s\n", key,
	       strerror (errno));
      return 0;
    }

  write (HDB_BARREL, &len, 4);
  write (HDB_BARREL, data, strlen (data));
  
  return 1;
}
    

/* Reads an entry from the database.  Returns a copy (using strcpy) of the
 * value cooresponding to the given key.  Returns NULL upon failure */
char *hdb_get (char *key)
{
  DBT dbt_key, dbt_data;
  unsigned long pos, len;
  char *data;
  int ret;
  char *str;
 
  dbt_key.data = key;
  dbt_key.size = strlen (key);
  
   if (! hdb_open_db)
    {
      fprintf (stderr, "hdb_get: No database open\n");
      return NULL;
    }
  
   if (HDB_HASH->get (HDB_HASH, &dbt_key, &dbt_data, 0))
     {
       fprintf (stderr, "hdb_get: Not able to get data for %s: %s\n", key,
		strerror (errno));
       return NULL;
     }

   str = (char *) malloc (dbt_data.size+1);
   memcpy (str, dbt_data.data, dbt_data.size);
   str[dbt_data.size] = '\0';

   pos = atoi (str);
   free (str);

   if (-1 == lseek (HDB_BARREL, pos, SEEK_SET))
     {
       fprintf (stderr, "hdb_get: Failed seek: %s\n", strerror (errno));
       return NULL;
     }
   ret = read (HDB_BARREL, &len, sizeof (unsigned long));
   if ((sizeof (unsigned long) != ret))
     {
       fprintf (stderr, "hdb_get: Failed read (length): %s\n", strerror (errno));
       return NULL;
     }
   data = (char *) malloc (len+1);
   if (len != read (HDB_BARREL, data, len))
     {
       fprintf (stderr, "hdb_get: Failed read (data): %s\n", strerror (errno));
       return NULL;
     }
   data[len] = '\0';

   return data;
}


/* Upon the first call to HDB_EACH, the first KEY/DATA pair in the database
 * is put in KEY/DATA.  Upon subsequent calls, other KEY/DATA pairs are
 * returned until all pairs have been returned exactly once.
 * Returns 1 upon success, 0 upon failure or the end of the sequence.
 * If RESET is set to a non-zero value, the function acts as though it were
 * being called for the first time.
 * NOTE: Memory for KEY and DATA is allocated by function. */
int hdb_each (char **key, char **data, int reset)
{
  DBT dbt_key, dbt_data;
  int rtn;

  if (reset)
    rtn = HDB_HASH->seq (HDB_HASH, &dbt_key, &dbt_data, R_FIRST);
  else
    rtn = HDB_HASH->seq (HDB_HASH, &dbt_key, &dbt_data, R_NEXT);

  if (rtn == -1)
    {
      fprintf (stderr, "hdb_each: Unable to perform seq: %s\n",
	       strerror (errno));
      return 0;
    }
  else if (rtn)
    return 0;

  /* C-ify the string we are returning as KEY */
  *key = (char *) malloc (dbt_key.size + 1);
  memcpy ((*key), dbt_key.data, dbt_key.size);
  (*key)[dbt_key.size] = '\0';

  *data = hdb_get (*key);

  return 1;
}

/* Converts integer to string */
char *itoa (int num)
{
  /* Number of bits is upper bound for number of digits in decimal */
  char *str = (char *) malloc (sizeof(int)*8+1);

  sprintf (str, "%d", num);

  return str;
}
