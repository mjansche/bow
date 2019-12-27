/* primelist.c - a list of prime numbers close to a power of 2.
   Source: http://www.utm.edu/research/primes/lists/2small/0bit.html

   Copyright (C) 2000 Andrew McCallum

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

#include <limits.h>
#include <assert.h>

static unsigned
bow_primelist[] = {
  (2 << 8) - 5,
  (2 << 9) - 3,
  (2 << 10) - 3,
  (2 << 11) - 9,
  (2 << 12) - 3,
  (2 << 13) - 1,
  (2 << 14) - 3,
  (2 << 15) - 19,
  (2 << 16) - 15,
  (2 << 17) - 1,
  (2 << 18) - 5,
  (2 << 19) - 1,
  (2 << 20) - 3,
  (2 << 21) - 9,
  (2 << 22) - 3,
  (2 << 23) - 15,
  (2 << 24) - 3,
  (2 << 25) - 39,
  (2 << 26) - 5,
  (2 << 27) - 39,
  (2 << 28) - 57,
  (2 << 29) - 3,
  (2U << 30) - 35,
  (2U << 31) - 1,
  UINT_MAX
};

/* Return a prime number that is "near" two times N */
int
_bow_primedouble (unsigned n)
{
  int i;
  for (i = 0; bow_primelist[i] < 2*n - n/2; i++)
    ;
  assert (bow_primelist[i] != UINT_MAX);
  return bow_primelist[i];
}
