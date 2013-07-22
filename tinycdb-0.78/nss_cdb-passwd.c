/* nss_cdb-passwd.c: nss_cdb passwd database routines.
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "nss_cdb.h"
#include <pwd.h>
#include <stdlib.h>

nss_common(passwd, struct passwd, pwent);
nss_getbyname(getpwnam, struct passwd);
nss_getbyid(getpwuid, struct passwd, uid_t);

static int
nss_passwd_parse(struct passwd *result, char *buf, size_t bufl) {

  STRING_FIELD(buf, result->pw_name);
  if (!result->pw_name[0]) return -1;
  STRING_FIELD(buf, result->pw_passwd);
  INT_FIELD(buf, result->pw_uid, (uid_t));
  INT_FIELD(buf, result->pw_gid, (gid_t));
  STRING_FIELD(buf, result->pw_gecos);
  STRING_FIELD(buf, result->pw_dir);
  result->pw_shell = buf;

  bufl = bufl;

  return 1;
}

#ifdef TEST
#include <stdio.h>

static void printit(const struct passwd *p) {
  printf("name=`%s' pass=`%s' uid=%d gid=%d gecos=`%s' dir=`%s' shell=`%s'\n",
	 p->pw_name, p->pw_passwd, p->pw_uid, p->pw_gid, p->pw_gecos, p->pw_dir, p->pw_shell);
}

int main(int argc, char **argv) {
  struct passwd pw, *p;
  char buf[1024];
  int err, r;
  while(*++argv) {
    r = _nss_cdb_getpwuid_r(atoi(*argv), &pw, buf, sizeof(buf), &err);
    if (r == NSS_STATUS_SUCCESS)
      printit(&pw);
    else
      printf("cdb(%s): %d %s\n", *argv, r, strerror(err));
    p = getpwuid(atoi(*argv));
    if (p) printit(p);
    else printf("pwuid(%s): %m\n", *argv);
  }
  return 0;
}
#endif
