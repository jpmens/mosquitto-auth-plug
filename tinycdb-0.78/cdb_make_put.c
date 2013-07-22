/* cdb_make_put.c: "advanced" cdb_make_put routine
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "cdb_int.h"

static void
fixup_rpos(struct cdb_make *cdbmp, unsigned rpos, unsigned rlen) {
  unsigned i;
  struct cdb_rl *rl;
  register struct cdb_rec *rp, *rs;
  for (i = 0; i < 256; ++i) {
    for (rl = cdbmp->cdb_rec[i]; rl; rl = rl->next)
      for (rs = rl->rec, rp = rs + rl->cnt; --rp >= rs;)
        if (rp->rpos <= rpos) goto nexthash;
        else rp->rpos -= rlen;
nexthash:;
  }
}

static int
remove_record(struct cdb_make *cdbmp, unsigned rpos, unsigned rlen) {
  unsigned pos, len;
  int r, fd;

  len = cdbmp->cdb_dpos - rpos - rlen;
  cdbmp->cdb_dpos -= rlen;
  if (!len)
    return 0;	/* it was the last record, nothing to do */
  pos = rpos;
  fd = cdbmp->cdb_fd;
  do {
    r = len > sizeof(cdbmp->cdb_buf) ? sizeof(cdbmp->cdb_buf) : len;
    if (lseek(fd, pos + rlen, SEEK_SET) < 0 ||
        (r = read(fd, cdbmp->cdb_buf, r)) <= 0)
      return -1;
    if (lseek(fd, pos, SEEK_SET) < 0 ||
        _cdb_make_fullwrite(fd, cdbmp->cdb_buf, r) < 0)
      return -1;
    pos += r;
    len -= r;
  } while(len);
  assert(cdbmp->cdb_dpos == pos);
  fixup_rpos(cdbmp, rpos, rlen);
  return 0;
}

static int
zerofill_record(struct cdb_make *cdbmp, unsigned rpos, unsigned rlen) {
  if (rpos + rlen == cdbmp->cdb_dpos) {
    cdbmp->cdb_dpos = rpos;
    return 0;
  }
  if (lseek(cdbmp->cdb_fd, rpos, SEEK_SET) < 0)
    return -1;
  memset(cdbmp->cdb_buf, 0, sizeof(cdbmp->cdb_buf));
  cdb_pack(rlen - 8, cdbmp->cdb_buf + 4);
  for(;;) {
    rpos = rlen > sizeof(cdbmp->cdb_buf) ? sizeof(cdbmp->cdb_buf) : rlen;
    if (_cdb_make_fullwrite(cdbmp->cdb_fd, cdbmp->cdb_buf, rpos) < 0)
      return -1;
    rlen -= rpos;
    if (!rlen) return 0;
    memset(cdbmp->cdb_buf + 4, 0, 4);
  }
}

/* return: 0 = not found, 1 = error, or record length */
static unsigned
match(struct cdb_make *cdbmp, unsigned pos, const char *key, unsigned klen)
{
  int len;
  unsigned rlen;
  if (lseek(cdbmp->cdb_fd, pos, SEEK_SET) < 0)
    return 1;
  if (read(cdbmp->cdb_fd, cdbmp->cdb_buf, 8) != 8)
    return 1;
  if (cdb_unpack(cdbmp->cdb_buf) != klen)
    return 0;

  /* record length; check its validity */
  rlen = cdb_unpack(cdbmp->cdb_buf + 4);
  if (rlen > cdbmp->cdb_dpos - pos - klen - 8)
    return errno = EPROTO, 1;	/* someone changed our file? */
  rlen += klen + 8;

  while(klen) {
    len = klen > sizeof(cdbmp->cdb_buf) ? sizeof(cdbmp->cdb_buf) : klen;
    len = read(cdbmp->cdb_fd, cdbmp->cdb_buf, len);
    if (len <= 0)
      return 1;
    if (memcmp(cdbmp->cdb_buf, key, len) != 0)
      return 0;
    key += len;
    klen -= len;
  }

  return rlen;
}

static int
findrec(struct cdb_make *cdbmp,
        const void *key, unsigned klen, unsigned hval,
        enum cdb_put_mode mode)
{
  struct cdb_rl *rl;
  struct cdb_rec *rp, *rs;
  unsigned r;
  int seeked = 0;
  int ret = 0;
  for(rl = cdbmp->cdb_rec[hval&255]; rl; rl = rl->next)
    for(rs = rl->rec, rp = rs + rl->cnt; --rp >= rs;) {
      if (rp->hval != hval)
	continue;
      /*XXX this explicit flush may be unnecessary having
       * smarter match() that looks into cdb_buf too, but
       * most of a time here spent in finding hash values
       * (above), not keys */
      if (!seeked && _cdb_make_flush(cdbmp) < 0)
        return -1;
      seeked = 1;
      r = match(cdbmp, rp->rpos, key, klen);
      if (!r)
	continue;
      if (r == 1)
	return -1;
      ret = 1;
      switch(mode) {
      case CDB_FIND_REMOVE:
        if (remove_record(cdbmp, rp->rpos, r) < 0)
          return -1;
	break;
      case CDB_FIND_FILL0:
	if (zerofill_record(cdbmp, rp->rpos, r) < 0)
          return -1;
	break;
      default: goto finish;
      }
      memmove(rp, rp + 1, (rs + rl->cnt - 1 - rp) * sizeof(*rp));
      --rl->cnt;
      --cdbmp->cdb_rcnt;
  }
finish:
  if (seeked && lseek(cdbmp->cdb_fd, cdbmp->cdb_dpos, SEEK_SET) < 0)
    return -1;
  return ret;
}

int
cdb_make_find(struct cdb_make *cdbmp,
              const void *key, unsigned klen,
              enum cdb_put_mode mode)
{
  return findrec(cdbmp, key, klen, cdb_hash(key, klen), mode);
}

int
cdb_make_exists(struct cdb_make *cdbmp,
                const void *key, unsigned klen)
{
  return cdb_make_find(cdbmp, key, klen, CDB_FIND);
}

int
cdb_make_put(struct cdb_make *cdbmp,
	     const void *key, unsigned klen,
	     const void *val, unsigned vlen,
	     enum cdb_put_mode mode)
{
  unsigned hval = cdb_hash(key, klen);
  int r;

  switch(mode) {
    case CDB_PUT_REPLACE:
    case CDB_PUT_INSERT:
    case CDB_PUT_WARN:
    case CDB_PUT_REPLACE0:
      r = findrec(cdbmp, key, klen, hval, mode);
      if (r < 0)
        return -1;
      if (r && mode == CDB_PUT_INSERT)
        return errno = EEXIST, 1;
      break;

    case CDB_PUT_ADD:
      r = 0;
      break;

    default:
      return errno = EINVAL, -1;
  }

  if (_cdb_make_add(cdbmp, hval, key, klen, val, vlen) < 0)
    return -1;

  return r;
}

