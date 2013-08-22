#! /usr/bin/make -rf
# Makefile: make file for tinycdb package
#
# This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
# Public domain.

VERSION = 0.78

prefix=/usr/local
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
libdir=$(exec_prefix)/lib
syslibdir=$(libdir)
sysconfdir=/etc
includedir=$(prefix)/include
mandir=$(prefix)/man
NSSCDB_DIR = $(sysconfdir)
DESTDIR=

CC = cc -fPIC
CFLAGS = -O
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)
LDFLAGS =

AR = ar
ARFLAGS = rv
RANLIB = ranlib

NSS_CDB = libnss_cdb.so.2
LIBBASE = libcdb
LIB = $(LIBBASE).a
PICLIB = $(LIBBASE)_pic.a
SHAREDLIB = $(LIBBASE).so.1
SOLIB = $(LIBBASE).so
CDB_USELIB = $(LIB)
NSS_USELIB = $(PICLIB)
LIBMAP = $(LIBBASE).map
INSTALLPROG = cdb

# The following assumes GNU CC/LD -
# used for building shared libraries only
CFLAGS_PIC = -fPIC
LDFLAGS_SHARED = -shared
LDFLAGS_SONAME = -Wl,--soname=
LDFLAGS_VSCRIPT = -Wl,--version-script=

CP = cp

LIB_SRCS = cdb_init.c cdb_find.c cdb_findnext.c cdb_seq.c cdb_seek.c \
 cdb_unpack.c \
 cdb_make_add.c cdb_make_put.c cdb_make.c cdb_hash.c
NSS_SRCS = nss_cdb.c nss_cdb-passwd.c nss_cdb-group.c nss_cdb-spwd.c
NSSMAP = nss_cdb.map

DISTFILES = Makefile cdb.h cdb_int.h $(LIB_SRCS) cdb.c \
 $(NSS_SRCS) nss_cdb.h nss_cdb-Makefile \
 cdb.3 cdb.1 cdb.5 \
 tinycdb.spec tests.sh tests.ok \
 $(LIBMAP) $(NSSMAP) \
 ChangeLog NEWS
DEBIANFILES = debian/control debian/rules debian/copyright debian/changelog

all: static
static: staticlib cdb
staticlib: $(LIB)
nss: $(NSS_CDB)
piclib: $(PICLIB)
sharedlib: $(SHAREDLIB)
shared: sharedlib cdb-shared

LIB_OBJS = $(LIB_SRCS:.c=.o)
LIB_OBJS_PIC = $(LIB_SRCS:.c=.lo)
NSS_OBJS = $(NSS_SRCS:.c=.lo)

$(LIB): $(LIB_OBJS)
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)
	-$(RANLIB) $@

$(PICLIB): $(LIB_OBJS_PIC)
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS_PIC)
	-$(RANLIB) $@

$(SHAREDLIB): $(LIB_OBJS_PIC) $(LIBMAP)
	-rm -f $(SOLIB)
	ln -s $@ $(SOLIB)
	$(LD) $(LDFLAGS) $(LDFLAGS_SHARED) -o $@ \
	 $(LDFLAGS_SONAME)$(SHAREDLIB) $(LDFLAGS_VSCRIPT)$(LIBMAP) \
	 $(LIB_OBJS_PIC)

cdb: cdb.o $(CDB_USELIB)
	$(LD) $(LDFLAGS) -o $@ cdb.o $(CDB_USELIB)
cdb-shared: cdb.o $(SHAREDLIB)
	$(LD) $(LDFLAGS) -o $@ cdb.o $(SHAREDLIB)

$(NSS_CDB): $(NSS_OBJS) $(NSS_USELIB) $(NSSMAP)
	$(LD) $(LDFLAGS) $(LDFLAGS_SHARED) -o $@ \
	 $(LDFLAGS_SONAME)$@ $(LDFLAGS_VSCRIPT)$(NSSMAP) \
	 $(NSS_OBJS) $(NSS_USELIB)

.SUFFIXES:
.SUFFIXES: .c .o .lo

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
.c.lo:
	$(CC) $(CFLAGS) $(CDEFS) $(CFLAGS_PIC) -c -o $@ -DNSSCDB_DIR=\"$(NSSCDB_DIR)\" $<

cdb.o: cdb.h
$(LIB_OBJS) $(LIB_OBJS_PIC): cdb_int.h cdb.h
$(NSS_OBJS): nss_cdb.h cdb.h

clean:
	-rm -f *.o *.lo core *~ tests.out tests-shared.ok
realclean distclean:
	-rm -f *.o *.lo core *~ $(LIBBASE)[._][aps]* $(NSS_CDB)* cdb cdb-shared

test tests check: cdb
	sh ./tests.sh ./cdb > tests.out 2>&1
	diff tests.ok tests.out
	@echo All tests passed
test-shared tests-shared check-shared: cdb-shared
	sed 's/^cdb: /cdb-shared: /' <tests.ok >tests-shared.ok
	LD_LIBRARY_PATH=. sh ./tests.sh ./cdb-shared > tests.out 2>&1
	diff tests-shared.ok tests.out
	rm -f tests-shared.ok
	@echo All tests passed

do_install = \
 while [ "$$1" ] ; do \
   if [ .$$4 = .- ]; then f=$$1; else f=$$4; fi; \
   d=$(DESTDIR)$$3 ; echo installing $$1 to $$d/$$f; \
   [ -d $$d ] || mkdir -p $$d || exit 1 ; \
   $(CP) $$1 $$d/$$f || exit 1; \
   chmod 0$$2 $$d/$$f || exit 1; \
   shift 4; \
 done

install-all: all $(INSTALLPROG)
	set -- \
	 cdb.h 644 $(includedir) - \
	 cdb.3 644 $(mandir)/man3 - \
	 cdb.1 644 $(mandir)/man1 - \
	 cdb.5 644 $(mandir)/man5 - \
	 $(INSTALLPROG) 755 $(bindir) cdb \
	 libcdb.a 644 $(libdir) - \
	 ; \
	$(do_install)
install-nss: nss
	@set -- $(NSS_CDB) 644 $(syslibdir) - \
	        nss_cdb-Makefile 644 $(sysconfdir) cdb-Makefile ; \
	$(do_install)
install-sharedlib: sharedlib
	@set -- $(SHAREDLIB) 644 $(libdir) - ; \
	$(do_install) ; \
	ln -sf $(SHAREDLIB) $(DESTDIR)$(libdir)/$(LIBBASE).so
install-piclib: piclib
	@set -- $(PICLIB) 644 $(libdir) - ; \
	$(do_install)
install: install-all

DNAME = tinycdb-$(VERSION)
dist: $(DNAME).tar.gz
$(DNAME).tar.gz: $(DISTFILES) $(DEBIANFILES)
	mkdir $(DNAME) $(DNAME)/debian
	ln $(DISTFILES) $(DNAME)/
	ln $(DEBIANFILES) $(DNAME)/debian/
	tar cfz $@ $(DNAME)
	rm -fr $(DNAME)

.PHONY: all clean realclean dist spec
.PHONY: test tests check test-shared tests-shared check-shared
.PHONY: static staticlib shared sharedlib nss piclib
.PHONY: install install-all install-sharedlib install-piclib install-nss
