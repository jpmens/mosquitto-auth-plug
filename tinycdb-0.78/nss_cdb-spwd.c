/* nss_cdb-spwd.c: nss_cdb shadow passwd database routines.
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "nss_cdb.h"
#include <shadow.h>

nss_common(shadow, struct spwd, spent);
nss_getbyname(getspnam, struct spwd);

static int
nss_shadow_parse(struct spwd *result, char *buf, size_t bufl) {

  STRING_FIELD(buf, result->sp_namp);
  if (!result->sp_namp[0]) return -1;
  STRING_FIELD(buf, result->sp_pwdp);
  INT_FIELD_MAYBE_NULL(buf, result->sp_lstchg, (long), -1);
  INT_FIELD_MAYBE_NULL(buf, result->sp_min, (long), -1);
  INT_FIELD_MAYBE_NULL(buf, result->sp_max, (long), -1);
  INT_FIELD_MAYBE_NULL(buf, result->sp_warn, (long), -1);
  INT_FIELD_MAYBE_NULL(buf, result->sp_inact, (long), -1);
  INT_FIELD_MAYBE_NULL(buf, result->sp_expire, (long), -1);
  if (*buf) {
    result->sp_flag = strtoul(buf, &buf, 10);
    if (*buf) return -1;
  }
  else
    result->sp_flag = ~0ul;

  bufl = bufl;

  return 1;
}
