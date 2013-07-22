/* cdb_hash.c: cdb hashing routine
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "cdb.h"

unsigned
cdb_hash(const void *buf, unsigned len)
{
  register const unsigned char *p = (const unsigned char *)buf;
  register const unsigned char *end = p + len;
  register unsigned hash = 5381;	/* start value */
  while (p < end)
    hash = (hash + (hash << 5)) ^ *p++;
  return hash;
}
