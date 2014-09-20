# Choose one or more back-ends Allowed values are
#	BE_CDB
#	BE_MYSQL
#	BE_SQLITE
#	BE_REDIS
#	BE_POSTGRES
BE_CDB=0
BE_MYSQL=1
BE_SQLITE=0
BE_REDIS=0
BE_POSTGRES=0
BE_LDAP=0
BE_HTTP=1
USE_BINDED_TOPIC_MATCH=1
MOSQUITTO_SRC=/home/Administrator/mosquitto-1.3.4
OPENSSLDIR=/usr/local/stow/openssl-1.0.0c/

ifeq ($(BE_MYSQL), 1)
	BACKENDS += -DBE_MYSQL
	BE_CFLAGS+=`mysql_config --cflags`
	BE_LDFLAGS+=`mysql_config --libs`
endif
ifeq ($(BE_POSTGRES), 1)
	BACKENDS+= -DBE_POSTGRES
	BE_CFLAGS+= -I`pg_config --includedir`
	BE_LDFLAGS+=-lpq
endif
ifeq ($(BE_LDAP), 1)
	BACKENDS+= -DBE_LDAP
	BE_CFLAGS += -I/usr/include
	BE_LDFLAGS += -L/usr/lib -lldap -llber
endif


ifeq ($(BE_CDB), 1)
	BACKENDS+= -DBE_CDB
	CDBDIR=contrib/tinycdb-0.78
	CDB=$(CDBDIR)/cdb
	CDBINC=$(CDBDIR)/
	CDBLIB=$(CDBDIR)/libcdb.a
	BE_CFLAGS += -I$(CDBINC)/
	BE_LDFLAGS += -L$(CDBDIR) -lcdb
	BE_DEPS += $(CDBLIB)
endif

ifeq ($(BE_SQLITE), 1)
	BACKENDS+= -DBE_SQLITE
	BE_LDFLAGS += -lsqlite3
endif

ifeq ($(BE_REDIS), 1)
	BACKENDS+= -DBE_REDIS
	BE_CFLAGS += -I/usr/local/include/hiredis
	BE_LDFLAGS += -L/usr/local/lib -lhiredis
endif

ifeq ($(BE_HTTP), 1)
	BACKENDS+= -DBE_HTTP
	BE_LDFLAGS += -lcurl
endif

ifeq ($(USE_BINDED_TOPIC_MATCH), 1)
	BE_CFLAGS += -DCONFIG_USE_BINDED_TOPIC_MATCH
else
	MOS_LIB = -lmosquitto
endif

OSSLINC=-I$(OPENSSLDIR)/include
OSSLIBS=-L$(OPENSSLDIR)/lib -lcrypto 

OBJS=auth-plug.o base64.o pbkdf2-check.o log.o hash.o be-psk.o be-cdb.o be-mysql.o be-sqlite.o be-redis.o be-postgres.o be-ldap.o be-http.o
CFLAGS = -I$(MOSQUITTO_SRC)/src/
CFLAGS += -I$(MOSQUITTO_SRC)/lib/
CFLAGS += -fPIC -Wall  $(BACKENDS) $(BE_CFLAGS) -I$(MOSQ)/src -DDEBUG=1 $(OSSLINC)
LDFLAGS=$(BE_LDFLAGS) $(MOS_LIB) $(OSSLIBS)
LDFLAGS += -L$(MOSQUITTO_SRC)/lib/
# LDFLAGS += -Wl,-rpath,$(../../../../pubgit/MQTT/mosquitto/lib) -lc
# LDFLAGS += -export-dynamic


all: auth-plug.so np 

auth-plug.so : $(OBJS) $(BE_DEPS)
	$(CC) -fPIC -shared $(OBJS) -o $@  $(OSSLIBS) $(BE_DEPS) $(LDFLAGS) 

be-redis.o: be-redis.c be-redis.h log.h hash.h Makefile
be-sqlite.o: be-sqlite.c be-sqlite.h Makefile
auth-plug.o: auth-plug.c be-cdb.h be-mysql.h be-sqlite.h Makefile
be-psk.o: be-psk.c be-psk.h Makefile
be-cdb.o: be-cdb.c be-cdb.h Makefile
be-mysql.o: be-mysql.c be-mysql.h Makefile
be-ldap.o: be-ldap.c be-ldap.h Makefile
be-sqlite.o: be-sqlite.c be-sqlite.h Makefile
pbkdf2-check.o: pbkdf2-check.c base64.h Makefile
base64.o: base64.c base64.h Makefile
log.o: log.c log.h Makefile
hash.o: hash.c hash.h uthash.h Makefile
be-postgres.o: be-postgres.c be-postgres.h Makefile
be-http.o: be-http.c be-http.h Makefile

np: np.c base64.o
	$(CC) $(CFLAGS) $^ -o $@ $(OSSLIBS)

$(CDBLIB):
	(cd $(CDBDIR); make libcdb.a cdb )

pwdb.cdb: pwdb.in
	$(CDB) -c -m  pwdb.cdb pwdb.in
clean :
	rm -f *.o *.so 
	(cd contrib/tinycdb-0.78; make realclean )
