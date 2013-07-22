/* cdb_make_add.c: basic cdb_make_add routine
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include <stdlib.h> /* for malloc */
#include "cdb_int.h"

int internal_function
_cdb_make_add(struct cdb_make *cdbmp, unsigned hval,
              const void *key, unsigned klen,
              const void *val, unsigned vlen)
{
  unsigned char rlen[8];
  struct cdb_rl *rl;
  unsigned i;
  if (klen > 0xffffffff - (cdbmp->cdb_dpos + 8) ||
      vlen > 0xffffffff - (cdbmp->cdb_dpos + klen + 8))
    return errno = ENOMEM, -1;
  i = hval & 255;
  rl = cdbmp->cdb_rec[i];
  if (!rl || rl->cnt >= sizeof(rl->rec)/sizeof(rl->rec[0])) {
    rl = (struct cdb_rl*)malloc(sizeof(struct cdb_rl));
    if (!rl)
      return errno = ENOMEM, -1;
    rl->cnt = 0;
    rl->next = cdbmp->cdb_rec[i];
    cdbmp->cdb_rec[i] = rl;
  }
  i = rl->cnt++;
  rl->rec[i].hval = hval;
  rl->rec[i].rpos = cdbmp->cdb_dpos;
  ++cdbmp->cdb_rcnt;
  cdb_pack(klen, rlen);
  cdb_pack(vlen, rlen + 4);
  if (_cdb_make_write(cdbmp, rlen, 8) < 0 ||
      _cdb_make_write(cdbmp, key, klen) < 0 ||
      _cdb_make_write(cdbmp, val, vlen) < 0)
    return -1;
  return 0;
}

int
cdb_make_add(struct cdb_make *cdbmp,
             const void *key, unsigned klen,
             const void *val, unsigned vlen) {
  return _cdb_make_add(cdbmp, cdb_hash(key, klen), key, klen, val, vlen);
}
