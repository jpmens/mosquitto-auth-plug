/* cdb.c: cdb command line tool
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#define _GNU_SOURCE	/* #define this even on Windows */

#ifdef _WIN32		/* by the way, how about win64? */
# include <io.h>
# include <malloc.h>
/* This pragma suppresses snippy VC warnings for POSIX functions like read() */
# pragma warning(disable: 4996)
#else
# include <unistd.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "cdb.h"

#ifndef EPROTO
# define EPROTO EINVAL
#endif

#ifdef __GLIBC__
# define HAVE_PROGRAM_INVOCATION_SHORT_NAME
#endif

#ifdef HAVE_PROGRAM_INVOCATION_SHORT_NAME
# define progname program_invocation_short_name
#else
static char *progname;
#endif

#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0
#endif

#ifdef _WIN32
# define FBINMODE "b"
#else
# define FBINMODE
#endif

#define F_DUPMASK	0x000f
#define F_WARNDUP	0x0100
#define F_ERRDUP	0x0200
#define F_MAP		0x1000	/* map format (or else CDB native format) */

/* Silly defines just to suppress silly compiler warnings.
 * The thing is, trivial routines like strlen(), fgets() etc expects
 * char* argument, and GCC>=4 complains about using unsigned char* here.
 * Silly silly silly.
 */
#ifdef __GNUC__
static inline size_t ustrlen(const unsigned char *s) {
  return strlen((const char*)s);
}
static inline unsigned char *ufgets(unsigned char *s, int size, FILE *f) {
  return (unsigned char*)fgets((char*)s, size, f);
}
#else
# define ustrlen strlen
# define ufgets fgets
#endif

static unsigned char *buf;
static unsigned blen;

static void
#ifdef __GNUC__
__attribute__((noreturn,format(printf,2,3)))
#endif
error(int errnum, const char *fmt, ...)
{
  if (fmt) {
    va_list ap;
    fprintf(stderr, "%s: ", progname);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }
  if (errnum)
    fprintf(stderr, ": %s\n", strerror(errnum));
  else {
    if (fmt) putc('\n', stderr);
    fprintf(stderr, "%s: try `%s -h' for help\n", progname, progname);
  }
  fflush(stderr);
  exit(errnum ? 111 : 2);
}

static void allocbuf(unsigned len) {
  if (blen < len) {
    buf = (unsigned char*)(buf ? realloc(buf, len) : malloc(len));
    if (!buf)
      error(ENOMEM, "unable to allocate %u bytes", len);
    blen = len;
  }
}

static int qmode(char *dbname, const char *key, int num, int flags)
{
  struct cdb c;
  struct cdb_find cf;
  int r;
  int n, found;

  r = open(dbname, O_RDONLY);
  if (r < 0 || cdb_init(&c, r) != 0)
    error(errno, "unable to open database `%s'", dbname);

  r = cdb_findinit(&cf, &c, key, strlen(key));
  if (!r)
    return 100;
  else if (r < 0)
    error(errno, "%s", key);
  n = 0; found = 0;
  while((r = cdb_findnext(&cf)) > 0) {
    ++n;
    if (num && num != n) continue;
    ++found;
    allocbuf(cdb_datalen(&c));
    if (cdb_read(&c, buf, cdb_datalen(&c), cdb_datapos(&c)) != 0)
      error(errno, "unable to read value");
    fwrite(buf, 1, cdb_datalen(&c), stdout);
    if (flags & F_MAP) putchar('\n');
    if (num)
      break;
  }
  if (r < 0)
    error(0, "%s", key);
  return found ? 0 : 100;
}

static void
fget(FILE *f, unsigned char *b, unsigned len, unsigned *posp, unsigned limit)
{
  if (posp && limit - *posp < len)
    error(EPROTO, "invalid database format");
  if (fread(b, 1, len, f) != len) {
    if (ferror(f)) error(errno, "unable to read");
    fprintf(stderr, "%s: unable to read: short file\n", progname);
    exit(2);
  }
  if (posp) *posp += len;
}

static int
fcpy(FILE *fi, FILE *fo, unsigned len, unsigned *posp, unsigned limit)
{
  while(len > blen) {
    fget(fi, buf, blen, posp, limit);
    if (fo && fwrite(buf, 1, blen, fo) != blen) return -1;
    len -= blen;
  }
  if (len) {
    fget(fi, buf, len, posp, limit);
    if (fo && fwrite(buf, 1, len, fo) != len) return -1;
  }
  return 0;
}

static int
dmode(char *dbname, char mode, int flags)
{
  unsigned eod, klen, vlen;
  unsigned pos = 0;
  FILE *f;
  if (strcmp(dbname, "-") == 0)
    f = stdin;
  else if ((f = fopen(dbname, "r" FBINMODE)) == NULL)
    error(errno, "open %s", dbname);
  allocbuf(2048);
  fget(f, buf, 2048, &pos, 2048);
  eod = cdb_unpack(buf);
  while(pos < eod) {
    fget(f, buf, 8, &pos, eod);
    klen = cdb_unpack(buf);
    vlen = cdb_unpack(buf + 4);
    if (!(flags & F_MAP))
      if (printf(mode == 'd' ? "+%u,%u:" : "+%u:", klen, vlen) < 0) return -1;
    if (fcpy(f, stdout, klen, &pos, eod) != 0) return -1;
    if (mode == 'd')
      if (fputs(flags & F_MAP ? " " : "->", stdout) < 0)
        return -1;
    if (fcpy(f, mode == 'd' ? stdout : NULL, vlen, &pos, eod) != 0)
      return -1;
    if (putc('\n', stdout) < 0)
      return -1;
  }
  if (pos != eod)
    error(EPROTO, "invalid cdb file format");
  if (!(flags & F_MAP))
    if (putc('\n', stdout) < 0)
      return -1;
  return 0;
}

static int smode(char *dbname) {
  FILE *f;
  unsigned pos, eod;
  unsigned cnt = 0;
  unsigned kmin = 0, kmax = 0, ktot = 0;
  unsigned vmin = 0, vmax = 0, vtot = 0;
  unsigned hmin = 0, hmax = 0, htot = 0, hcnt = 0;
#define NDIST 11
  unsigned dist[NDIST];
  unsigned char toc[2048];
  unsigned k;

  if (strcmp(dbname, "-") == 0)
    f = stdin;
  else if ((f = fopen(dbname, "r" FBINMODE)) == NULL)
    error(errno, "open %s", dbname);

  pos = 0;
  fget(f, toc, 2048, &pos, 2048);

  allocbuf(2048);

  eod = cdb_unpack(toc);
  while(pos < eod) {
    unsigned klen, vlen;
    fget(f, buf, 8, &pos, eod);
    klen = cdb_unpack(buf);
    vlen = cdb_unpack(buf + 4);
    fcpy(f, NULL, klen, &pos, eod);
    fcpy(f, NULL, vlen, &pos, eod);
    ++cnt;
    ktot += klen;
    if (!kmin || kmin > klen) kmin = klen;
    if (kmax < klen) kmax = klen;
    vtot += vlen;
    if (!vmin || vmin > vlen) vmin = vlen;
    if (vmax < vlen) vmax = vlen;
    vlen += klen;
  }
  if (pos != eod) error(EPROTO, "invalid cdb file format");

  for (k = 0; k < NDIST; ++k)
    dist[k] = 0;
  for (k = 0; k < 256; ++k) {
    unsigned i = cdb_unpack(toc + (k << 3));
    unsigned hlen = cdb_unpack(toc + (k << 3) + 4);
    if (i != pos) error(EPROTO, "invalid cdb hash table");
    if (!hlen) continue;
    for (i = 0; i < hlen; ++i) {
      unsigned h;
      fget(f, buf, 8, &pos, 0xffffffff);
      if (!cdb_unpack(buf + 4)) continue;
      h = (cdb_unpack(buf) >> 8) % hlen;
      if (h == i) h = 0;
      else {
        if (h < i) h = i - h;
        else h = hlen - h + i;
        if (h >= NDIST) h = NDIST - 1;
      }
      ++dist[h];
    }
    if (!hmin || hmin > hlen) hmin = hlen;
    if (hmax < hlen) hmax = hlen;
    htot += hlen;
    ++hcnt;
  }
  printf("number of records: %u\n", cnt);
  printf("key min/avg/max length: %u/%u/%u\n",
         kmin, cnt ? (ktot + cnt / 2) / cnt : 0, kmax);
  printf("val min/avg/max length: %u/%u/%u\n",
         vmin, cnt ? (vtot + cnt / 2) / cnt : 0, vmax);
  printf("hash tables/entries/collisions: %u/%u/%u\n",
         hcnt, htot, cnt - dist[0]);
  printf("hash table min/avg/max length: %u/%u/%u\n",
         hmin, hcnt ? (htot + hcnt / 2) / hcnt : 0, hmax);
  printf("hash table distances:\n");
  for(k = 0; k < NDIST; ++k)
    printf(" %c%u: %6u %2u%%\n",
           k == NDIST - 1 ? '>' : 'd', k == NDIST - 1 ? k - 1 : k,
           dist[k], cnt ? dist[k] * 100 / cnt : 0);
  return 0;
}

static void badinput(const char *fn) {
  fprintf(stderr, "%s: %s: bad format\n", progname, fn);
  exit(2);
}

static int getnum(FILE *f, unsigned *np, const char *fn) {
  unsigned n;
  int c = getc(f);
  if (c < '0' || c > '9') badinput(fn);
  n = c - '0';
  while((c = getc(f)) >= '0' && c <= '9') {
    c -= '0';
    if (0xffffffff / 10 - c < n) badinput(fn);
    n = n * 10 + c;
  }
  *np = n;
  return c;
}

static void
addrec(struct cdb_make *cdbmp,
       const unsigned char *key, unsigned klen,
       const unsigned char *val, unsigned vlen,
       int flags)
{
  int r = cdb_make_put(cdbmp, key, klen, val, vlen, flags & F_DUPMASK);
  if (r < 0)
    error(errno, "cdb_make_put");
  else if (r && (flags & F_WARNDUP)) {
    fprintf(stderr, "%s: key `", progname);
    fwrite(key, 1, klen, stderr);
    fputs("' duplicated\n", stderr);
    if (flags & F_ERRDUP)
      exit(1);
  }
}

static void
dofile_cdb(struct cdb_make *cdbmp, FILE *f, const char *fn, int flags)
{
  unsigned klen, vlen;
  int c;
  while((c = getc(f)) == '+') {
    if ((c = getnum(f, &klen, fn)) != ',' ||
        (c = getnum(f, &vlen, fn)) != ':' ||
        0xffffffff - klen < vlen)
      badinput(fn);
    allocbuf(klen + vlen);
    fget(f, buf, klen, NULL, 0);
    if (getc(f) != '-' || getc(f) != '>') badinput(fn);
    fget(f, buf + klen, vlen, NULL, 0);
    if (getc(f) != '\n') badinput(fn);
    addrec(cdbmp, buf, klen, buf + klen, vlen, flags);
  }
  if (c != '\n') badinput(fn);
}

static void
dofile_ln(struct cdb_make *cdbmp, FILE *f, int flags)
{
  unsigned char *k, *v;
  while(ufgets(buf, blen, f) != NULL) {
    unsigned l = 0;
    for (;;) {
      l += ustrlen(buf + l);
      v = buf + l;
      if (v > buf && v[-1] == '\n') {
        v[-1] = '\0';
        break;
      }
      if (l < blen)
        allocbuf(l + 512);
      if (!ufgets(buf + l, blen - l, f))
        break;
    }
    k = buf;
    while(*k == ' ' || *k == '\t') ++k;
    if (!*k || *k == '#')
      continue;
    v = k;
    while(*v && *v != ' ' && *v != '\t') ++v;
    if (*v) *v++ = '\0';
    while(*v == ' ' || *v == '\t') ++v;
    addrec(cdbmp, k, ustrlen(k), v, ustrlen(v), flags);
  }
}

static void
dofile(struct cdb_make *cdbmp, FILE *f, const char *fn, int flags)
{
  if (flags & F_MAP)
    dofile_ln(cdbmp, f, flags);
  else
    dofile_cdb(cdbmp, f, fn, flags);
  if (ferror(f))
    error(errno, "read error");
}

static int
cmode(char *dbname, char *tmpname, int argc, char **argv, int flags, int perms)
{
  struct cdb_make cdb;
  int fd;
  if (!tmpname) {
    tmpname = (char*)malloc(strlen(dbname) + 5);
    if (!tmpname)
      error(ENOMEM, "unable to allocate memory");
    /* OpenBSD compiler complains about strcat() and strcpy() usage,
     * and suggests to replace them with (non-standard) strlcat() and
     * strlcpy().  This is silly, since it's obvious that usage of
     * original str*() routines here is correct.
     * This is compiler/environment bug, not tinycdb bug, so please
     * fix it in proper place, and don't send patches to me.  Thank you.
     */
    strcat(strcpy(tmpname, dbname), ".tmp");
  }
  else if (strcmp(tmpname, "-") == 0 || strcmp(tmpname, dbname) == 0)
    tmpname = dbname;
  if (perms >= 0)
    umask(0);
  unlink(tmpname);
  fd = open(tmpname, O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,
            perms >= 0 ? perms : 0666);
  if (fd < 0)
    error(errno, "unable to create %s", tmpname);
  cdb_make_start(&cdb, fd);
  allocbuf(4096);
  if (argc) {
    int i;
    for (i = 0; i < argc; ++i) {
      if (strcmp(argv[i], "-") == 0)
        dofile(&cdb, stdin, "(stdin)", flags);
      else {
        FILE *f = fopen(argv[i], "r");
        if (!f)
          error(errno, "%s", argv[i]);
        dofile(&cdb, f, argv[i], flags);
        fclose(f);
      }
    }
  }
  else
    dofile(&cdb, stdin, "(stdin)", flags);
  if (cdb_make_finish(&cdb) != 0)
    error(errno, "cdb_make_finish");
  close(fd);
  if (tmpname != dbname)
    if (rename(tmpname, dbname) != 0)
      error(errno, "rename %s->%s", tmpname, dbname);
  return 0;
}

int main(int argc, char **argv)
{
  int c;
  char mode = 0;
  char *tmpname = NULL;
  int flags = 0;
  int num = 0;
  int r;
  int perms = -1;
  extern char *optarg;
  extern int optind;

#ifdef HAVE_PROGRAM_INVOCATION_SHORT_NAME
  argv[0] = progname;
#else
  if (argv[0] && (progname = strrchr(argv[0], '/')) != NULL)
    argv[0] = ++progname;
  else
    progname = argv[0];
#endif

  if (argc <= 1)
    error(0, "no arguments given");

  while((c = getopt(argc, argv, "qdlcsht:n:mwruep:0")) != EOF)
    switch(c) {
    case 'q': case 'd':  case 'l': case 'c': case 's':
      if (mode && mode != c)
        error(0, "different modes of operation requested");
      mode = c;
      break;
    case 't': tmpname = optarg; break;
    case 'w': flags |= F_WARNDUP; break;
    case 'e': flags |= F_WARNDUP | F_ERRDUP; break;
    case 'r': flags = (flags & ~F_DUPMASK) | CDB_PUT_REPLACE; break;
    case 'u': flags = (flags & ~F_DUPMASK) | CDB_PUT_INSERT; break;
    case '0': flags = (flags & ~F_DUPMASK) | CDB_PUT_REPLACE0; break;
    case 'm': flags |= F_MAP; break;
    case 'p': {
      char *ep = NULL;
      perms = strtol(optarg, &ep, 0);
      if (perms < 0 || perms > 0777 || (ep && *ep))
        error(0, "invalid permissions `%s'", optarg);
      break;
    }
    case 'n': {
      char *ep = NULL;
      if ((num = strtol(optarg, &ep, 0)) <= 0 || (ep && *ep))
        error(0, "invalid record number `%s'", optarg);
      break;
    }
    case 'h':
#define strify(x) _strify(x)
#define _strify(x) #x
      printf("\
%s: Constant DataBase (CDB) tool version " strify(TINYCDB_VERSION)
". Usage is:\n\
 query:  %s -q [-m] [-n recno|-a] cdbfile key\n\
 dump:   %s -d [-m] [cdbfile|-]\n\
 list:   %s -l [-m] [cdbfile|-]\n\
 create: %s -c [-m] [-wrue0] [-t tempfile|-] [-p perms] cdbfile [infile...]\n\
 stats:  %s -s [cdbfile|-]\n\
 help:   %s -h\n\
", progname, progname, progname, progname, progname, progname, progname);
      return 0;

    default:
      error(0, NULL);
    }

  argv += optind;
  argc -= optind;
  switch(mode) {
    case 'q':
      if (argc < 2) error(0, "no database or key to query specified");
      if (argc > 2) error(0, "extra arguments in command line");
      r = qmode(argv[0], argv[1], num, flags);
      break;
    case 'c':
      if (!argc) error(0, "no database name specified");
      if ((flags & F_WARNDUP) && !(flags & F_DUPMASK))
        flags |= CDB_PUT_WARN;
      r = cmode(argv[0], tmpname, argc - 1, argv + 1, flags, perms);
      break;
    case 'd':
    case 'l':
      if (argc > 1) error(0, "extra arguments for dump/list");
      r = dmode(argc ? argv[0] : "-", mode, flags);
      break;
    case 's':
      if (argc > 1) error(0, "extra argument(s) for stats");
      r = smode(argc ? argv[0] : "-");
      break;
    default:
      error(0, "no -q, -c, -d, -l or -s option specified");
  }
  if (r < 0 || fflush(stdout) < 0)
    error(errno, "unable to write: %d", c);
  return r;
}

