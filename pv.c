/* "Position vector", a (compressed) list of word positions in documents */

/* Copyright (C) 1998 Andrew McCallum

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

#include <bow/libbow.h>
#include <bow/archer.h>

#define PV_DEBUG 1

/* The first four bytes of a segment are an int that indicate how many
   bytes are allocated in this segment.  The last four bytes of a
   segment are an int that indicates the seek location of the next
   segment.  The read/write_segment_bytes_remaining does not include
   the size of the two int's. */

/* Always enough for one "document index"/"word index" pair:
   5 bytes == 6+4*7 == 34 bits for di, likewise for pi. */
#define bow_pv_max_sizeof_di_pi (2 * 5)
static int bow_pv_sizeof_first_segment = 2 * bow_pv_max_sizeof_di_pi;

/* Fill in PV with the correct initial values, and write the first
   segment header to disk.  What this function does must match what
   bow_pv_add_di_pi() does when it adds a new segment. */
void
bow_pv_init (bow_pv *pv, FILE *fp)
{
  pv->count = 0;
  /* Set the size of this first segment to its default */
  pv->write_segment_bytes = bow_pv_sizeof_first_segment;
  pv->write_segment_bytes_remaining = bow_pv_sizeof_first_segment;
  pv->read_segment_bytes_remaining = bow_pv_sizeof_first_segment;
  /* Seek to the end of the file, which is the position at which this
     PV will begin. */
  fseek (fp, 0, SEEK_END);
  /* Remember this value so that we can rewind the read position later. */
  pv->seek_start = ftell (fp);
  /* Write the "header", which is the number of bytes in this segment. */
  bow_fwrite_int (pv->write_segment_bytes_remaining, fp);
  /* Remember this position.  It is the seek position at which we will
     start writing and reading. */
  pv->write_seek_end = ftell (fp);
  pv->read_seek_end = pv->write_seek_end;
  /* Seek forward to the end of this segment and write a (temporary
     value for) the "tailer".  This will reserve all the intervening
     bytes for this segment, and future calls to seek to the end of
     the file (for other PV's) will go after the end of this
     segment. */
  fseek (fp, pv->write_segment_bytes_remaining, SEEK_CUR);
  bow_fwrite_int (-1, fp);
  /* Initialize our DI and PI context. */
  pv->read_last_di = -1;
  pv->read_last_pi = -1;
  pv->write_last_di = -1;
  pv->write_last_pi = -1;
}

/* Write to FP the unsigned integer I, marked with the special flag
   saying if it is a DI or a PI, (as indicated by IS_DI).  Assumes
   that FP is already seek'ed to the correct position, and there is
   enough space there in this segment to write the info.  Returns the
   number of bytes written. */
static inline int
old_bow_pv_write_unsigned_int (unsigned int i, int is_di, FILE *fp)
{
  bow_pe pe;
  int byte_count = 1;

  if (is_di)
    pe.bits.is_di = 1;
  else
    pe.bits.is_di = 0;
  while (i > 0x3f)		/* binary = 00111111 */
    {
      pe.bits.is_more = 1;
      pe.bits.index = i & 0x3f;	/* binary = 00111111 */
      fputc (pe.byte, fp);
      byte_count++;
      i = i >> 6;
    }
  pe.bits.is_more = 0;
  pe.bits.index = i;
  fputc (pe.byte, fp);
  return byte_count;
}

/* Write to FP the unsigned integer I, marked with the special flag
   saying if it is a DI or a PI, (as indicated by IS_DI).  Assumes
   that FP is already seek'ed to the correct position, and there is
   enough space there in this segment to write the info.  Returns the
   number of bytes written. */
static inline int
bow_pv_write_unsigned_int (unsigned int i, int is_di, FILE *fp)
{
  bow_pe pe;
  int byte_count = 1;		/* Count already the last byte */

  /* assert (i < (1 < 6+7+7+7+1)); */
  if (is_di)
    pe.bits.is_di = 1;
  else
    pe.bits.is_di = 0;
  if (i > 0x3f)			/* binary = 00111111 */
    {
      pe.bits.is_more = 1;
      pe.bits.index = i & 0x3f;	/* binary = 00111111 */
      fputc (pe.byte, fp);	/* Write the first byte */
      byte_count++;
      i = i >> 6;
      while (i > 0x7f)		/* binary = 01111111 */
	{
	  pe.bits_more.is_more = 1;
	  pe.bits_more.index = i & 0x7f;
	  fputc (pe.byte, fp);
	  byte_count++;
	  i = i >> 7;
	}
	pe.bits_more.is_more = 0;
	pe.bits_more.index = i;
	fputc (pe.byte, fp);
    }
  else
    {
      pe.bits.is_more = 0;
      pe.bits.index = i;
      /* Write the first byte and only */
      fputc (pe.byte, fp);
    }
  return byte_count;
}

/* Read an unsigned integer into I, and indicate whether it is a
   "document index" or a "position index" by the value of IS_DI.
   Assumes that FP is already seek'ed to the correct position. Returns
   the number of bytes read. */
static inline int
old_bow_pv_read_unsigned_int (unsigned int *i, int *is_di, FILE *fp)
{
  bow_pe pe;
  int index;
  int shift = 0;
  int byte_count = 1;

  pe.byte = fgetc (fp);
  if (pe.bits.is_di)
    *is_di = 1;
  else
    *is_di = 0;
  index = pe.bits.index;
  while (pe.bits.is_more)
    {
      shift += 6;
      pe.byte = fgetc (fp);
      byte_count++;
      index |= pe.bits.index << shift;
    }
  *i = index;
  return byte_count;
}

/* Read an unsigned integer into I, and indicate whether it is a
   "document index" or a "position index" by the value of IS_DI.
   Assumes that FP is already seek'ed to the correct position. Returns
   the number of bytes read. */
static inline int
bow_pv_read_unsigned_int (unsigned int *i, int *is_di, FILE *fp)
{
  bow_pe pe;
  int index;
  int shift = 6;
  int byte_count = 1;

  pe.byte = fgetc (fp);
  if (pe.bits.is_di)
    *is_di = 1;
  else
    *is_di = 0;
  index = pe.bits.index;
  while (pe.bits.is_more)
    /* The above test relies on pe.bits.is_more == pe.bits_more.is_more */
    {
      pe.byte = fgetc (fp);
      byte_count++;
      index |= pe.bits_more.index << shift;
      shift += 7;
    }
  *i = index;
  return byte_count;
}

#define PV_WRITE_SIZE_INT(N)			\
(((N) < (1 << (6)))				\
 ? 1						\
 : (((N) < (1 << (6+7)))			\
    ? 2						\
    : (((N) < (1 << (6+7+7)))      		\
       ? 3					\
       : (((N) < (1 << (6+7+7+7)))		\
	  ? 4					\
	  : 5))))

static inline int
bow_pv_write_size_di_pi (bow_pv *pv, int di, int pi)
{
  int size = 0;
  if (pv->write_last_di != di)
    size += PV_WRITE_SIZE_INT (di - pv->write_last_di);
  size += PV_WRITE_SIZE_INT (pi - pv->write_last_pi);
  return size;
}

static inline int
bow_pv_write_size_di_li_pi (bow_pv *pv, int di, int li[], int ln, int pi)
{
  int i;
  int size = 0;

  assert(ln <= BOW_MAX_WORD_LABELS);

  if ((pv->write_last_di != di) || (ln != 0)) 
    size += PV_WRITE_SIZE_INT (di - pv->write_last_di);
  for (i = 0; i < ln; i++) size += PV_WRITE_SIZE_INT (li[i]);
  if(pv->write_last_di == di)
    size += PV_WRITE_SIZE_INT (pi - pv->write_last_pi);
  else size += PV_WRITE_SIZE_INT(pi + 1);

  return size;
}

static inline int
bow_pv_read_size_di_pi (bow_pv *pv, int di, int pi)
{
  int size = 0;
  if (pv->read_last_di != di)
    size += PV_WRITE_SIZE_INT (di - pv->read_last_di);
  size += PV_WRITE_SIZE_INT (pi - pv->read_last_pi);
  return size;
}

/* Write "document index" DI and "position index" PI to FP.  Assumes
   that FP is already seek'ed to the correct position, and there is
   space there in this segment to write the info.  Returns the number
   of bytes written. */
static inline int
bow_pv_write_next_di_pi (bow_pv *pv, int di, int pi, FILE *fp)
{
  int bytes_written = 0;
  assert (di >= pv->write_last_di);
  if (di != pv->write_last_di)
    {
      bytes_written += 
	bow_pv_write_unsigned_int (di - pv->write_last_di, 1, fp);
      pv->write_last_di = di;
      pv->write_last_pi = -1;
    }
  bytes_written +=
    bow_pv_write_unsigned_int (pi - pv->write_last_pi, 0, fp);
  pv->write_last_pi = pi;
  return bytes_written;
}

/* Write "document index" DI, "list indices" LI (LN of them) and
   "position index" PI to FP.  Assumes that FP is already seek'ed to
   the correct position, and there is space there in this segment to
   write the info.  Returns the number of bytes written. */
static inline int
bow_pv_write_next_di_li_pi (bow_pv *pv, int di, int li[], int ln, int pi,
			    FILE *fp)
{
  int i, bytes_written = 0;
  assert (di >= pv->write_last_di);
  assert(ln <= BOW_MAX_WORD_LABELS);

  if ((di != pv->write_last_di) || (ln > 0))
    {
      bytes_written += 
	bow_pv_write_unsigned_int (di - pv->write_last_di, 1, fp);
      if(di != pv->write_last_di) pv->write_last_pi = -1;
      pv->write_last_di = di;
    }

  for(i = 0; i < ln; i++) 
    bytes_written += bow_pv_write_unsigned_int(li[i], 1, fp);

  bytes_written += bow_pv_write_unsigned_int (pi - pv->write_last_pi, 0, fp);
  pv->write_last_pi = pi;
  return bytes_written;
}

/* Read "document index" DI and "position index" PI from FP.  Assumes
   that FP is already seek'ed to the correct position.  Returns the
   number of bytes read. */
static inline int
bow_pv_read_next_di_pi (bow_pv *pv, int *di, int *pi, FILE *fp)
{
  int incr;
  int bytes_read = 0;
  int is_di;

  bytes_read += bow_pv_read_unsigned_int (&incr, &is_di, fp);
  if (is_di)
    {
      pv->read_last_di += incr;
      pv->read_last_pi = -1;
      bytes_read += bow_pv_read_unsigned_int (&incr, &is_di, fp);
      assert (!is_di);
    }
  pv->read_last_pi += incr;
  *di = pv->read_last_di;
  *pi = pv->read_last_pi;
  return bytes_read;
}

/* Read "document index" DI, "list indices" LI (LN of them) and
   "position index" PI from FP.  The size of LI is read from LN which
   is then set to the actual number of "list indices" returned (thus
   if LN is too small you may not get all of them). Assumes that FP is
   already seek'ed to the correct position.  Returns the number of
   bytes read. */
static inline int
bow_pv_read_next_di_li_pi (bow_pv *pv, int *di, int li[], int *ln, int *pi,
			   FILE *fp)
{
  int incr;
  int bytes_read = 0;
  int is_di;
  int li_count = 0;
  
  bytes_read += bow_pv_read_unsigned_int (&incr, &is_di, fp);
  if (is_di)
    {
      pv->read_last_di += incr;
      if(incr != 0) pv->read_last_pi = -1;
      bytes_read += bow_pv_read_unsigned_int (&incr, &is_di, fp);
      while (is_di) {
	if(li_count < *ln) li[li_count++] = incr;
	bytes_read += bow_pv_read_unsigned_int (&incr, &is_di, fp);
      }
    }
  pv->read_last_pi += incr;
  *di = pv->read_last_di;
  *pi = pv->read_last_pi;
  *ln = li_count;

  return bytes_read;
}

/* Add "document index" DI and "position index" PI to PV by writing
   the correct information to FP.  Does not assume that FP is already
   seek'ed to the correct position.  Will add a new PV segment on disk
   if necessary.  Assumes that both DI and PI are greater than or
   equal to the last DI and PI written, respectively. */
void
bow_pv_add_di_pi (bow_pv *pv, int di, int pi, FILE *fp)
{
  int byte_count;

  /* Make sure there is definitely enough room in this segment to
     write another DI and PI.  If not, start a new segment, write the
     "tailer" to point to its seek position, write the "header" of the
     new segment, and leave seek FP to a position in the new segment
     ready to write the DI and PI.  */
  if (pv->write_segment_bytes_remaining < bow_pv_max_sizeof_di_pi)
    {
      int seek_tailer;
      int seek_new_segment;
      /* Calculate the seek position at which we will write the
	 "tailer" of the current segment.  The "tailer" will contain
	 the seek position of the new segment. */
      seek_tailer = pv->write_seek_end + pv->write_segment_bytes_remaining;
      /* Go to the very end of the file, and remember this seek position.
	 This is the beginning of the new segment. */
      fseek (fp, 0, SEEK_END);
      seek_new_segment = ftell (fp);
      /* Calculate the size of the new segment. */
      pv->write_segment_bytes *= 2;
      pv->write_segment_bytes_remaining = pv->write_segment_bytes;
      /* Write the "header" of the new segment.  The "header" contains the
	 number of bytes in the new segment. */
      bow_fwrite_int (pv->write_segment_bytes_remaining, fp);
      /* Remember this seek position after the "header" of the new
         segment.  It is here that we will start writing new DI's and
         PI's. */
      pv->write_seek_end = ftell (fp);
      /* Seek forward to the end of this segment and write a (temporary
	 value for) the "tailer".  This will reserve all the intervening
	 bytes for this segment, and future calls to seek to the end of
	 the file (for other PV's) will go after the end of this
	 segment. */
      fseek (fp, pv->write_segment_bytes_remaining, SEEK_CUR);
      bow_fwrite_int (-1, fp);
      /* Go back to the "tailer" of the last segment, and write the
         position of the new segment there. */
      fseek (fp, seek_tailer, SEEK_SET);
#if PV_DEBUG
      /* Check to make sure it is the originally-written temporary value */
      bow_fread_int (&byte_count, fp);
      assert (byte_count == -1);
      fseek (fp, seek_tailer, SEEK_SET);
#endif
      bow_fwrite_int (seek_new_segment, fp);
    }
  
  /* Seek to the correct position, write the DI and PI, and decrement
     our count of the number of bytes remaining in this segment, and
     update the seek position for writing the next DI and PI. */
  fseek (fp, pv->write_seek_end, SEEK_SET);
  byte_count = bow_pv_write_next_di_pi (pv, di, pi, fp);
  pv->write_segment_bytes_remaining -= byte_count;
  pv->write_seek_end += byte_count;
  /* Increment the PV's count of the total number of word occurrences */
  pv->count++;
  assert (pv->write_segment_bytes_remaining >= 0);
}

/* Add "document index" DI, "label indices" LI (LN of them) and
   "position index" PI to PV by writing the correct information to FP.
   Does not assume that FP is already seek'ed to the correct position.
   Will add a new PV segment on disk if necessary.  Assumes that both
   DI and PI are greater than or equal to the last DI and PI written,
   respectively. */
void
bow_pv_add_di_li_pi (bow_pv *pv, int di, int li[], int ln, int pi,
		     FILE *fp)
{
  int byte_count;
  int size = bow_pv_write_size_di_li_pi(pv, di, li, ln, pi);

  assert(ln <= BOW_MAX_WORD_LABELS);
  assert(di >= pv->write_last_di);
  assert(pi < pv->write_last_pi ? di != pv->write_last_di : 1 );
  /* Make sure there is definitely enough room in this segment to
     write this DI, PI and LIs.  If not, write an end-of-segment
     marker (a zero), start a new segment, write the "tailer" to point
     to its seek position, write the "header" of the new segment, and
     leave seek FP to a position in the new segment ready to write the
     DI, PI and LIs. "Enough room" is the size of the current record
     plus one byte for the end-of-segment marker */
  if (pv->write_segment_bytes_remaining < size + 1)
    {
      int seek_tailer;
      int seek_new_segment;

      assert(pv->write_segment_bytes_remaining > 0);
      /* first write an end-of-segment marker */
      fseek (fp, pv->write_seek_end, SEEK_SET);
      fputc (0, fp);
      /* Calculate the seek position at which we will write the
	 "tailer" of the current segment.  The "tailer" will contain
	 the seek position of the new segment. */
      seek_tailer = pv->write_seek_end + pv->write_segment_bytes_remaining;
      /* Go to the very end of the file, and remember this seek position.
	 This is the beginning of the new segment. */
      fseek (fp, 0, SEEK_END);
      seek_new_segment = ftell (fp);
      /* Calculate the size of the new segment. */
      do {
	pv->write_segment_bytes *= 2;
      }
      while (pv->write_segment_bytes < size);

      pv->write_segment_bytes_remaining = pv->write_segment_bytes;
      /* Write the "header" of the new segment.  The "header" contains the
	 number of bytes in the new segment. */
      bow_fwrite_int (pv->write_segment_bytes_remaining, fp);
      /* Remember this seek position after the "header" of the new
         segment.  It is here that we will start writing new DI's and
         PI's. */
      pv->write_seek_end = ftell (fp);
      /* Seek forward to the end of this segment and write a (temporary
	 value for) the "tailer".  This will reserve all the intervening
	 bytes for this segment, and future calls to seek to the end of
	 the file (for other PV's) will go after the end of this
	 segment. */
      fseek (fp, pv->write_segment_bytes_remaining, SEEK_CUR);
      bow_fwrite_int (-1, fp);
      /* Go back to the "tailer" of the last segment, and write the
         position of the new segment there. */
      fseek (fp, seek_tailer, SEEK_SET);
#if PV_DEBUG
      /* Check to make sure it is the originally-written temporary value */
      bow_fread_int (&byte_count, fp);
      assert (byte_count == -1);
      fseek (fp, seek_tailer, SEEK_SET);
#endif
      bow_fwrite_int (seek_new_segment, fp);
    }

  /* Seek to the correct position, write the DI and PI, and decrement
     our count of the number of bytes remaining in this segment, and
     update the seek position for writing the next DI and PI. */
  fseek (fp, pv->write_seek_end, SEEK_SET);
  byte_count = bow_pv_write_next_di_li_pi (pv, di, li, ln, pi, fp);
  pv->write_segment_bytes_remaining -= byte_count;
  pv->write_seek_end += byte_count;
  /* Increment the PV's count of the total number of word occurrences */
  pv->count++;
  assert (pv->write_segment_bytes_remaining >= 0);
}

/* Read the next "document index" DI and "position index" PI.  Does
   not assume that FP is already seek'ed to the correct position.
   Will jump to a new PV segment on disk if necessary. */
void
bow_pv_next_di_pi (bow_pv *pv, int *di, int *pi, FILE *fp)
{
  int byte_count;

  /* If we are about to read from the same location as we would write,
     then we are at the end of the PV.  Return special DI and PI
     values indicate that we are at the end. */
  if (pv->read_seek_end == pv->write_seek_end)
    {
    return_end_of_pv:
      *di = *pi = -1;
      return;
    }

  /* If the special flag was set by bow_pv_unnext(), then return the
     same values returned last time without reading the next entry,
     and unset the flag. */
  if (pv->read_seek_end < 0)
    {
      *di = pv->read_last_di;
      *pi = pv->read_last_pi;
      pv->read_seek_end = -pv->read_seek_end;
      assert (pv->read_seek_end > 0);
      return;
    }

  /* Make sure that there was definitely enough room in this segment
     to write another DI and PI.  If not, then it was written in the
     next segment, so go there and get set up for reading from it. */
  if (pv->read_segment_bytes_remaining < bow_pv_max_sizeof_di_pi)
    {
      int seek_new_segment;
      /* Go to the "tailer" of this segment, and read the seek
         position of the next segment. */
      fseek (fp, pv->read_seek_end + pv->read_segment_bytes_remaining, 
	     SEEK_SET);
      bow_fread_int (&seek_new_segment, fp);
      fseek (fp, seek_new_segment, SEEK_SET);
      /* Read the number of bytes in this segment, and remember it. */
      bow_fread_int (&(pv->read_segment_bytes_remaining), fp);
      /* Remember the new position from which to read the next DI and PI */
      pv->read_seek_end = ftell (fp);
      /* If this segment has not yet been written to, we are at end of PV */
      if (pv->read_seek_end == pv->write_seek_end)
	goto return_end_of_pv;
    }

  /* Seek to the correct position, read the DI and PI, decrement our
     count of the number of bytes remaining in this segment, and
     update the seek position for reading the next DI and PI. */
  fseek (fp, pv->read_seek_end, SEEK_SET);
  byte_count =
    bow_pv_read_next_di_pi (pv, di, pi, fp);
  pv->read_segment_bytes_remaining -= byte_count;
  pv->read_seek_end += byte_count;
  assert (pv->read_segment_bytes_remaining >= 0);
}

/* Read the next "document index" DI, "list indices" LI (LN of them)
   and "position index" PI.  Does not assume that FP is already
   seek'ed to the correct position.  Will jump to a new PV segment on
   disk if necessary. LN should contain the size of LI and will be set
   to the number of labels put in to the array (thus if LN is too
   small you may not get all of them).*/
void
bow_pv_next_di_li_pi (bow_pv *pv, int *di, int li[], int* ln, int *pi,
		      FILE *fp)
{
  int byte_count;

  assert(*ln <= BOW_MAX_WORD_LABELS);
  /* If we are about to read from the same location as we would write,
     then we are at the end of the PV.  Return special DI and PI
     values indicate that we are at the end. */
  if (pv->read_seek_end == pv->write_seek_end)
    {
    return_end_of_pv:
      *di = *pi = -1;
      return;
    }

  /* If the special flag was set by bow_pv_unnext(), then return the
     same values returned last time without reading the next entry,
     and unset the flag.  We ignore the labels (no good way to keep
     track of them in this case).*/
  if (pv->read_seek_end < 0)
    {
      *di = pv->read_last_di;
      *pi = pv->read_last_pi;
      pv->read_seek_end = -pv->read_seek_end;
      assert (pv->read_seek_end > 0);
      return;
    }

  /* Make sure that there was definitely enough room in this segment
     to write another DI and PI.  If not, then a zero was written and
     the record was written in the next segment, so go there and get
     set up for reading from it. */
  fseek (fp, pv->read_seek_end, SEEK_SET);
  if (fgetc (fp) == 0)
    {
      int seek_new_segment;
      /* Go to the "tailer" of this segment, and read the seek
         position of the next segment. */
      fseek (fp, pv->read_seek_end + pv->read_segment_bytes_remaining, 
	     SEEK_SET);
      bow_fread_int (&seek_new_segment, fp);
      fseek (fp, seek_new_segment, SEEK_SET);
      /* Read the number of bytes in this segment, and remember it. */
      bow_fread_int (&(pv->read_segment_bytes_remaining), fp);
      /* Remember the new position from which to read the next DI and PI */
      pv->read_seek_end = ftell (fp);
      /* If this segment has not yet been written to, we are at end of PV */
      if (pv->read_seek_end == pv->write_seek_end)
	goto return_end_of_pv;
    }

  /* Seek to the correct position, read the DI and PI, decrement our
     count of the number of bytes remaining in this segment, and
     update the seek position for reading the next DI and PI. */
  fseek (fp, pv->read_seek_end, SEEK_SET);
  byte_count = bow_pv_read_next_di_li_pi (pv, di, li, ln, pi, fp);
  pv->read_segment_bytes_remaining -= byte_count;
  pv->read_seek_end += byte_count;
  assert (pv->read_segment_bytes_remaining >= 0);
}

/* Undo the effect of the last call to bow_pv_next_di_li_pi().  That
   is, make the next call to bow_pv_next_di_li_pi() return the same DI
   and PI as the last call did.  This function may not be called
   multiple times in a row without calling bow_pv_next_di_li_pi() in
   between. Furthermore note that the subsequent call to
   bow_pv_next_di_li_pi() will not have labels set correctly. */
void
bow_pv_unnext (bow_pv *pv)
{
  /* Make sure that this function wasn't call two times in a row. */
  assert (pv->read_seek_end > 0);
  pv->read_seek_end = -pv->read_seek_end;
}

/* Rewind the read position to the beginning of the PV */
void
bow_pv_rewind (bow_pv *pv, FILE *fp)
{
  /* If PV is already rewound, just return immediately */
  if (pv->read_last_di == -1 && pv->read_last_pi == -1)
    return;
  fseek (fp, pv->seek_start, SEEK_SET);
  bow_fread_int (&(pv->read_segment_bytes_remaining), fp);
  pv->read_seek_end = ftell (fp);
  pv->read_last_di = -1;
  pv->read_last_pi = -1;
}

/* Write the in-memory portion of PV to FP */
void
bow_pv_write (bow_pv *pv, FILE *fp)
{
  bow_fwrite_int (pv->count, fp);
  bow_fwrite_int (pv->seek_start, fp);
  bow_fwrite_int (pv->read_seek_end, fp);
  bow_fwrite_int (pv->read_last_di, fp);
  bow_fwrite_int (pv->read_last_pi, fp);
  bow_fwrite_int (pv->read_segment_bytes_remaining, fp);
  bow_fwrite_int (pv->write_seek_end, fp);
  bow_fwrite_int (pv->write_last_di, fp);
  bow_fwrite_int (pv->write_last_pi, fp);
  bow_fwrite_int (pv->write_segment_bytes, fp);
  bow_fwrite_int (pv->write_segment_bytes_remaining, fp);
}

/* Read the in-memory portion of PV from FP */
void
bow_pv_read (bow_pv *pv, FILE *fp)
{
  bow_fread_int (&pv->count, fp);
  bow_fread_int (&pv->seek_start, fp);
  bow_fread_int (&pv->read_seek_end, fp);
  bow_fread_int (&pv->read_last_di, fp);
  bow_fread_int (&pv->read_last_pi, fp);
  bow_fread_int (&pv->read_segment_bytes_remaining, fp);
  bow_fread_int (&pv->write_seek_end, fp);
  bow_fread_int (&pv->write_last_di, fp);
  bow_fread_int (&pv->write_last_pi, fp);
  bow_fread_int (&pv->write_segment_bytes, fp);
  bow_fread_int (&pv->write_segment_bytes_remaining, fp);
}
