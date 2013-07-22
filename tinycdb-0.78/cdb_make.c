/* cdb_make.c: basic cdb creation routines
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "cdb_int.h"

void
cdb_pack(unsigned num, unsigned char buf[4])
{
  buf[0] = num & 255; num >>= 8;
  buf[1] = num & 255; num >>= 8;
  buf[2] = num & 255;
  buf[3] = num >> 8;
}

int
cdb_make_start(struct cdb_make *cdbmp, int fd)
{
  memset(cdbmp, 0, sizeof(*cdbmp));
  cdbmp->cdb_fd = fd;
  cdbmp->cdb_dpos = 2048;
  cdbmp->cdb_bpos = cdbmp->cdb_buf + 2048;
  return 0;
}

int internal_function
_cdb_make_fullwrite(int fd, const unsigned char *buf, unsigned len)
{
  while(len) {
    int l = write(fd, buf, len);
    if (l > 0) {
      len -= l;
      buf += l;
    }
    else if (l < 0 && errno != EINTR)
      return -1;
  }
  return 0;
}

int internal_function
_cdb_make_flush(struct cdb_make *cdbmp) {
  unsigned len = cdbmp->cdb_bpos - cdbmp->cdb_buf;
  if (len) {
    if (_cdb_make_fullwrite(cdbmp->cdb_fd, cdbmp->cdb_buf, len) < 0)
      return -1;
    cdbmp->cdb_bpos = cdbmp->cdb_buf;
  }
  return 0;
}

int internal_function
_cdb_make_write(struct cdb_make *cdbmp, const unsigned char *ptr, unsigned len)
{
  unsigned l = sizeof(cdbmp->cdb_buf) - (cdbmp->cdb_bpos - cdbmp->cdb_buf);
  cdbmp->cdb_dpos += len;
  if (len > l) {
    memcpy(cdbmp->cdb_bpos, ptr, l);
    cdbmp->cdb_bpos += l;
    if (_cdb_make_flush(cdbmp) < 0)
      return -1;
    ptr += l; len -= l;
    l = len / sizeof(cdbmp->cdb_buf);
    if (l) {
      l *= sizeof(cdbmp->cdb_buf);
      if (_cdb_make_fullwrite(cdbmp->cdb_fd, ptr, l) < 0)
        return -1;
      ptr += l; len -= l;
    }
  }
  if (len) {
    memcpy(cdbmp->cdb_bpos, ptr, len);
    cdbmp->cdb_bpos += len;
  }
  return 0;
}

static int
cdb_make_finish_internal(struct cdb_make *cdbmp)
{
  unsigned hcnt[256];		/* hash table counts */
  unsigned hpos[256];		/* hash table positions */
  struct cdb_rec *htab;
  unsigned char *p;
  struct cdb_rl *rl;
  unsigned hsize;
  unsigned t, i;

  if (((0xffffffff - cdbmp->cdb_dpos) >> 3) < cdbmp->cdb_rcnt)
    return errno = ENOMEM, -1;

  /* count htab sizes and reorder reclists */
  hsize = 0;
  for (t = 0; t < 256; ++t) {
    struct cdb_rl *rlt = NULL;
    i = 0;
    rl = cdbmp->cdb_rec[t];
    while(rl) {
      struct cdb_rl *rln = rl->next;
      rl->next = rlt;
      rlt = rl;
      i += rl->cnt;
      rl = rln;
    }
    cdbmp->cdb_rec[t] = rlt;
    if (hsize < (hcnt[t] = i << 1))
      hsize = hcnt[t];
  }

  /* allocate memory to hold max htable */
  htab = (struct cdb_rec*)malloc((hsize + 2) * sizeof(struct cdb_rec));
  if (!htab)
    return errno = ENOENT, -1;
  p = (unsigned char *)htab;
  htab += 2;

  /* build hash tables */
  for (t = 0; t < 256; ++t) {
    unsigned len, hi;
    hpos[t] = cdbmp->cdb_dpos;
    if ((len = hcnt[t]) == 0)
      continue;
    for (i = 0; i < len; ++i)
      htab[i].hval = htab[i].rpos = 0;
    for (rl = cdbmp->cdb_rec[t]; rl; rl = rl->next)
      for (i = 0; i < rl->cnt; ++i) {
       hi = (rl->rec[i].hval >> 8) % len;
        while(htab[hi].rpos)
          if (++hi == len)
            hi = 0;
        htab[hi] = rl->rec[i];
      }
    for (i = 0; i < len; ++i) {
      cdb_pack(htab[i].hval, p + (i << 3));
      cdb_pack(htab[i].rpos, p + (i << 3) + 4);
    }
    if (_cdb_make_write(cdbmp, p, len << 3) < 0) {
      free(p);
      return -1;
    }
  }
  free(p);
  if (_cdb_make_flush(cdbmp) < 0)
    return -1;
  p = cdbmp->cdb_buf;
  for (t = 0; t < 256; ++t) {
    cdb_pack(hpos[t], p + (t << 3));
    cdb_pack(hcnt[t], p + (t << 3) + 4);
  }
  if (lseek(cdbmp->cdb_fd, 0, 0) != 0 ||
      _cdb_make_fullwrite(cdbmp->cdb_fd, p, 2048) != 0)
    return -1;

  return 0;
}

static void
cdb_make_free(struct cdb_make *cdbmp)
{
  unsigned t;
  for(t = 0; t < 256; ++t) {
    struct cdb_rl *rl = cdbmp->cdb_rec[t];
    while(rl) {
      struct cdb_rl *tm = rl;
      rl = rl->next;
      free(tm);
    }
  }
}

int
cdb_make_finish(struct cdb_make *cdbmp)
{
  int r = cdb_make_finish_internal(cdbmp);
  cdb_make_free(cdbmp);
  return r;
}

