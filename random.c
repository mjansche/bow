/* random.c - pseudo-random number generators for libbow
   Copyright (C) 1998 Andrew McCallum

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
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* non-zero if the random number generator is already seeded */
static int already_seeded = 0;


/* This function seeds the random number generator if needed.  Call
   before and random number generator usage, instead of srand */
void
bow_random_set_seed ()
{
  if (!already_seeded)
    {
      if (bow_split_seed == -1)
	{
	  struct timeval tv;
	  struct timezone tz;
	  long seed;
	  
	  gettimeofday (&tv, &tz);
	  seed = tv.tv_usec;
	  srand(seed);
	}
      else
	srand(bow_split_seed);

      already_seeded = 1;
    }
  return;
}


/* Return an double between low and high, inclusive */
double
bow_random_double (double low, double high)
{
  long r;
  double rd;

  assert (high - low > 0);
  r = random();
  rd = ((double)r) / ((double)RAND_MAX);
  rd *= (high - low);
  rd += low;
  return rd;
}

/* Return an double between 0 and 1, exclusive */
double
bow_random_01 ()
{
  double r;
  while ((r = bow_random_double (0.0, 1.0)) <= 0.0 || r >= 1.0)
    ;
  return r;
}