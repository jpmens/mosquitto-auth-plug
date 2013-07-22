/* cdb_seek.c: old interface for reading cdb file
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include <unistd.h>
#include "cdb_int.h"

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* read a chunk from file, ignoring interrupts (EINTR) */

int
cdb_bread(int fd, void *buf, int len)
{
  int l;
  while(len > 0) {
    do l = read(fd, buf, len);
    while(l < 0 && errno == EINTR);
    if (l <= 0) {
      if (!l)
        errno = EIO;
      return -1;
    }
    buf = (char*)buf + l;
    len -= l;
  }
  return 0;
}

/* find a given key in cdb file, seek a file pointer to it's value and
   place data length to *dlenp. */

int
cdb_seek(int fd, const void *key, unsigned klen, unsigned *dlenp)
{
  unsigned htstart;		/* hash table start position */
  unsigned htsize;		/* number of elements in a hash table */
  unsigned httodo;		/* hash table elements left to look */
  unsigned hti;			/* hash table index */
  unsigned pos;			/* position in a file */
  unsigned hval;			/* key's hash value */
  unsigned char rbuf[64];	/* read buffer */
  int needseek = 1;		/* if we should seek to a hash slot */

  hval = cdb_hash(key, klen);
  pos = (hval & 0xff) << 3; /* position in TOC */
  /* read the hash table parameters */
  if (lseek(fd, pos, SEEK_SET) < 0 || cdb_bread(fd, rbuf, 8) < 0)
    return -1;
  if ((htsize = cdb_unpack(rbuf + 4)) == 0)
    return 0;
  hti = (hval >> 8) % htsize;	/* start position in hash table */
  httodo = htsize;
  htstart = cdb_unpack(rbuf);

  for(;;) {
    if (needseek && lseek(fd, htstart + (hti << 3), SEEK_SET) < 0)
      return -1;
    if (cdb_bread(fd, rbuf, 8) < 0)
      return -1;
    if ((pos = cdb_unpack(rbuf + 4)) == 0) /* not found */
      return 0;

    if (cdb_unpack(rbuf) != hval) /* hash value not matched */
      needseek = 0;
    else { /* hash value matched */
      if (lseek(fd, pos, SEEK_SET) < 0 || cdb_bread(fd, rbuf, 8) < 0)
	return -1;
      if (cdb_unpack(rbuf) == klen) { /* key length matches */
	/* read the key from file and compare with wanted */
	unsigned l = klen, c;
	const char *k = (const char*)key;
	if (dlenp)
	  *dlenp = cdb_unpack(rbuf + 4); /* save value length */
	for(;;) {
	  if (!l) /* the whole key read and matches, return */
	    return 1;
	  c = l > sizeof(rbuf) ? sizeof(rbuf) : l;
	  if (cdb_bread(fd, rbuf, c) < 0)
	    return -1;
	  if (memcmp(rbuf, k, c) != 0) /* no, it differs, stop here */
	    break;
	  k += c; l -= c;
	}
      }
      needseek = 1; /* we're looked to other place, should seek back */
    }
    if (!--httodo)
      return 0;
    if (++hti == htsize) {
      hti = 0;
      needseek = 1;
    }
  }
}
