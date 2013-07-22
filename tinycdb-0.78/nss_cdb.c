/* nss_cdb.c: nss_cdb common routines.
 *
 * This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
 * Public domain.
 */

#include "nss_cdb.h"
#include "cdb_int.h"	/* for internal_function */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#if __GLIBC__ /* XXX this is in fact not a right condition */
/* XXX on glibc, this stuff works due to linker/libpthreads stubs/tricks.
 * On other libcs, it may require linking whole -lpthread, which is
 * not a good thing to do for nss module...
 */

#include <pthread.h>

#define lock_define(class,name) \
  class pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER;
#define lock_lock(name) pthread_mutex_lock(&(name))
#define lock_unlock(name) pthread_mutex_unlock(&(name))

#else /* !__GNU_LIBRARY__ */

# define lock_define_initialized(class,name)
# define lock_lock(name)
# define lock_unlock(name)

#endif /* __GNU_LIBRARY__ */

lock_define(static, lock)

/* General principle: we skip invalid/unparseable entries completely,
 * as if there was no such entry at all (returning NOTFOUND).
 * In case of data read error (e.g. invalid .cdb structure), we
 * return UNAVAIL.
 */

#define isopen(dbp) ((dbp)->lastpos)

static int
__nss_cdb_dosetent(struct nss_cdb *dbp) {
  int fd;

  fd = open(dbp->dbname, O_RDONLY);
  if (fd < 0)
    return 0;
  if (cdb_init(&dbp->cdb, fd) != 0) {
    close(fd);
    return 0;
  }
  close(fd);
  dbp->lastpos = 2048; /* cdb_seqinit() */
  return 1;
}

static void
__nss_cdb_doendent(struct nss_cdb *dbp) {
  cdb_free(&dbp->cdb);
  dbp->lastpos = 0;
  dbp->keepopen = 0;
}

enum nss_status internal_function
__nss_cdb_setent(struct nss_cdb *dbp, int stayopen) {
  enum nss_status r;
  lock_lock(lock);
  if (isopen(dbp) || __nss_cdb_dosetent(dbp))
    r = NSS_STATUS_SUCCESS, dbp->keepopen |= stayopen;
  else
    r = NSS_STATUS_UNAVAIL;
  lock_unlock(lock);
  return r;
}

enum nss_status internal_function
__nss_cdb_endent(struct nss_cdb *dbp) {
  lock_lock(lock);
  if (isopen(dbp))
    __nss_cdb_doendent(dbp);
  lock_unlock(lock);
  return NSS_STATUS_SUCCESS;
}

static enum nss_status
__nss_cdb_dobyname(struct nss_cdb *dbp, const char *key, unsigned len,
                   void *result, char *buf, size_t bufl, int *errnop) {
  int r;

  if ((r = cdb_find(&dbp->cdb, key, len)) < 0)
    return *errnop = errno, NSS_STATUS_UNAVAIL;
  len = cdb_datalen(&dbp->cdb);
  if (!r || len < 2)
    return *errnop = ENOENT, NSS_STATUS_NOTFOUND;
  if (len >= bufl)
    return *errnop = ERANGE, NSS_STATUS_TRYAGAIN;
  if (cdb_read(&dbp->cdb, buf, len, cdb_datapos(&dbp->cdb)) != 0)
    return *errnop = errno, NSS_STATUS_UNAVAIL;
  buf[len] = '\0';
  if ((r = dbp->parsefn(result, buf, bufl)) < 0)
    return *errnop = ENOENT, NSS_STATUS_NOTFOUND;
  if (!r)
    return *errnop = ERANGE, NSS_STATUS_TRYAGAIN;

  return NSS_STATUS_SUCCESS;
}

enum nss_status internal_function
__nss_cdb_byname(struct nss_cdb *dbp, const char *name,
                 void *result, char *buf, size_t bufl, int *errnop) {
  enum nss_status r;
  if (*name == ':')
    return *errnop = ENOENT, NSS_STATUS_NOTFOUND;
  lock_lock(lock);
  if (!isopen(dbp) && !__nss_cdb_dosetent(dbp))
    *errnop = errno, r = NSS_STATUS_UNAVAIL;
  else {
    r = __nss_cdb_dobyname(dbp, name, strlen(name), result, buf, bufl, errnop);
    if (!dbp->keepopen)
      __nss_cdb_doendent(dbp);
  }
  lock_unlock(lock);
  return r;
}

static enum nss_status
__nss_cdb_dobyid(struct nss_cdb *dbp, unsigned long id,
                 void *result, char *buf, size_t bufl, int *errnop) {
  int r;
  unsigned len;
  const char *data;

  if ((r = cdb_find(&dbp->cdb, buf, sprintf(buf, ":%lu", id))) < 0)
    return *errnop = errno, NSS_STATUS_UNAVAIL;
  len = cdb_datalen(&dbp->cdb);
  if (!r || len < 2)
    return *errnop = ENOENT, NSS_STATUS_NOTFOUND;
  if (!(data = (const char*)cdb_get(&dbp->cdb, len, cdb_datapos(&dbp->cdb))))
    return *errnop = errno, NSS_STATUS_UNAVAIL;

  return __nss_cdb_dobyname(dbp, data, len, result, buf, bufl, errnop);
}

enum nss_status internal_function
__nss_cdb_byid(struct nss_cdb *dbp, unsigned long id,
               void *result, char *buf, size_t bufl, int *errnop) {
  enum nss_status r;
  if (bufl < 30)
    return *errnop = ERANGE, NSS_STATUS_TRYAGAIN;
  lock_lock(lock);
  if (!isopen(dbp) && !__nss_cdb_dosetent(dbp))
    *errnop = errno, r = NSS_STATUS_UNAVAIL;
  else {
    r = __nss_cdb_dobyid(dbp, id, result, buf, bufl, errnop);
    if (!dbp->keepopen)
      __nss_cdb_doendent(dbp);
  }
  lock_unlock(lock);
  return r;
}

static enum nss_status
__nss_cdb_dogetent(struct nss_cdb *dbp,
                   void *result, char *buf, size_t bufl, int *errnop) {
  int r;
  unsigned lastpos;

  if (!isopen(dbp) && !__nss_cdb_dosetent(dbp))
    return *errnop = errno, NSS_STATUS_UNAVAIL;

  while((lastpos = dbp->lastpos, r = cdb_seqnext(&dbp->lastpos, &dbp->cdb)) > 0)
  {
    if (cdb_keylen(&dbp->cdb) < 2) continue;
    if (((const char *)cdb_getkey(&dbp->cdb))[0] == ':') /* can't fail */
      continue;
    if (cdb_datalen(&dbp->cdb) >= bufl)
      return dbp->lastpos = lastpos, *errnop = ERANGE, NSS_STATUS_TRYAGAIN;
    cdb_readdata(&dbp->cdb, buf);
    buf[cdb_datalen(&dbp->cdb)] = '\0';
    if ((r = dbp->parsefn(result, buf, bufl)) == 0)
      return dbp->lastpos = lastpos, *errnop = ERANGE, NSS_STATUS_TRYAGAIN;
    if (r > 0)
      return NSS_STATUS_SUCCESS;
  }
  if (r < 0)
    return *errnop = errno, NSS_STATUS_UNAVAIL;
  else
    return *errnop = ENOENT, NSS_STATUS_NOTFOUND;
}

enum nss_status internal_function
__nss_cdb_getent(struct nss_cdb *dbp,
                 void *result, char *buf, size_t bufl, int *errnop) {
  enum nss_status r;
  if (bufl < 30)
    return *errnop = ERANGE, NSS_STATUS_TRYAGAIN;
  lock_lock(lock);
  dbp->keepopen |= 1;
  r = __nss_cdb_dogetent(dbp, result, buf, bufl, errnop);
  lock_unlock(lock);
  return r;
}

