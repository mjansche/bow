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

#ifndef __HDB_H
#define __HDB_H

#define HDB_MAJOR_VERSION 1
#define HDB_MINOR_VERSION 0

/* Prepares for database access by opening Berkeley DB index file and
 * opening barrel file.  If NO_CREATE is *not* set, database files will
 * be created if they do not already exist.
 * Returns 1 upon success, 0 upon failure. */
int hdb_open (char *dir, int no_create);

/* Closes an HDB database; makes sure that all files have been sync'ed.
 * Returns 1 upon success, 0 upon failure */
int hdb_close ();

/* Adds an entry to the database or modifies an existing entry.  Note that
 * if an entry is modified, some of the space used to store the old entry
 * will remain used throughout the life of the database.
 * Returns 1 upon success, 0 upon failure. */
int hdb_put (char *key, char *data);

/* Reads an entry from the database.  Returns a copy (using strcpy) of the
 * value cooresponding to the given key.  Returns NULL upon failure */
char *hdb_get (char *key);

/* Upon the first call to HDB_EACH, the first KEY/DATA pair in the database
 * is put in KEY/DATA.  Upon subsequent calls, other KEY/DATA pairs are
 * returned until all pairs have been returned exactly once.
 * Returns 1 upon success, 0 upon failure or the end of the sequence.
 * If RESET is set to a non-zero value, the function acts as though it were
 * being called for the first time.
 * NOTE: Memory for KEY and DATA is allocated by function. */
int hdb_each (char **key, char **data, int reset);

#endif /* __HDB_H */
