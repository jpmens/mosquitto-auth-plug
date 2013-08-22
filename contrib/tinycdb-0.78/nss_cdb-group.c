/* nss_cdb-group.c: nss_cdb group database routines.
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "nss_cdb.h"
#include <grp.h>
#include <stdlib.h>
#include <string.h>

nss_common(group, struct group, grent);
nss_getbyname(getgrnam, struct group);
nss_getbyid(getgrgid, struct group, gid_t);

static char *getmember(char **bp) {
  char *b, *m;
  b = *bp;
  while(*b == ',') ++b;
  if (!*b) return NULL;
  m = b++;
  for(;;) {
    if (*b == ',') {
      *b++ = '\0';
      *bp = b;
      return m;
    }
    else if (*b == '\0') {
      *bp = b;
      return m;
    }
    else
      ++b;
  }
}

static int
nss_group_parse(struct group *result, char *buf, size_t bufl) {
  char *bufend;
  char **mem;
  int n;

  bufend = buf + strlen(buf) + 1;
  n = (unsigned)bufend % sizeof(char*);
  if (n)
    bufend += sizeof(char*) - n;
  result->gr_mem = mem = (char**)bufend;

  bufend = buf + bufl - sizeof(char*);

  STRING_FIELD(buf, result->gr_name);
  if (!result->gr_name[0]) return -1;
  STRING_FIELD(buf, result->gr_passwd);
  INT_FIELD(buf, result->gr_gid, (gid_t));

  for(;;) {
    if ((char*)mem > bufend)
      return 0;
    if (!(*mem++ = getmember(&buf)))
      break;
  }

  return 1;
}

#ifdef TEST
#include <stdio.h>

void printit(struct group *g) {
  char **mem;
  printf("name=%s pass=%s gid=%d mem:", g->gr_name, g->gr_passwd, g->gr_gid);
  mem = g->gr_mem;
  if (!mem)
    printf(" none");
  else
    while(*mem)
      printf(" %s", *mem++);
  putchar('\n');
}

int main(int argc, char **argv) {
  struct group gr, *g;
  char buf[36];
  int err, r;
  while(*++argv) {
    r = _nss_cdb_getgrgid_r(atoi(*argv), &gr, buf, sizeof(buf), &err);
    if (r == NSS_STATUS_SUCCESS)
      printit(&gr);
    else
      printf("cdb(%s): %d %s\n", *argv, r, strerror(err));
    g = getgrgid(atoi(*argv));
    if (g)
      printit(g);
    else
      printf("grgid(%s): %m\n", *argv);
  }
  return 0;
}

#endif
