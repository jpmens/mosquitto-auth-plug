/*
 * Copyright (c) 2013 Jan-Piet Mens <jpmens()gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of mosquitto nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include <fnmatch.h>

#include "log.h"
#include "hash.h"
#include "backends.h"

#include "be-psk.h"
#include "be-cdb.h"
#include "be-mysql.h"
#include "be-sqlite.h"
#include "be-redis.h"
#include "be-postgres.h"
#include "be-ldap.h"
#include "be-http.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define NBACKENDS	(5)

#if BE_PSK
# define PSKSETUP do { \
			if (!strcmp(psk_database, q)) { \
				(*pskbep)->conf =  (*bep)->conf; \
				(*pskbep)->superuser =  (*bep)->superuser; \
				(*pskbep)->aclcheck =  (*bep)->aclcheck; \
			} \
		   } while (0)
#else
# define PSKSETUP
#endif

struct backend_p {
	void *conf;			/* Handle to backend */
	char *name;
	f_kill *kill;
	f_getuser *getuser;
	f_superuser *superuser;
	f_aclcheck *aclcheck;
};

struct userdata {
	struct backend_p **be_list;
	char *superusers;		/* Static glob list */
	int authentication_be;		/* Back-end number user was authenticated in */
};

int pbkdf2_check(char *password, char *hash);

int mosquitto_auth_plugin_version(void)
{
	/* FIXME */
	fprintf(stderr, "*** auth-plug: backend=%s\n", TOSTRING(BACKEND));

	return MOSQ_AUTH_PLUGIN_VERSION;
}

int mosquitto_auth_plugin_init(void **userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
	int i;
	char *backends = NULL, *p, *q;
	struct mosquitto_auth_opt *o;
	struct userdata *ud;
	int ret = MOSQ_ERR_SUCCESS;
	int nord;
	struct backend_p **bep;
#ifdef BE_PSK
	struct backend_p **pskbep;
	char *psk_database = NULL;
#endif

	*userdata = (struct userdata *)malloc(sizeof(struct userdata));
	if (*userdata == NULL) {
		perror("allocting userdata");
		return MOSQ_ERR_UNKNOWN;
	}

	memset(*userdata, 0, sizeof(struct userdata));
	ud = *userdata;
	ud->superusers	= NULL;
	ud->authentication_be = -1;

	/*
	 * Shove all options Mosquitto gives the plugin into a hash,
	 * and let the back-ends figure out if they have all they
	 * need upon init()
	 */

	for (i = 0, o = auth_opts; i < auth_opt_count; i++, o++) {
		// _log(LOG_DEBUG, "AuthOptions: key=%s, val=%s", o->key, o->value);

		p_add(o->key, o->value);

		if (!strcmp(o->key, "superusers"))
			ud->superusers = strdup(o->value);
#if 0
		if (!strcmp(o->key, "topic_prefix"))
			ud->topicprefix = strdup(o->value);
#endif
	}

	/*
	 * Set up back-ends, and tell them to initialize themselves.
	 */


	backends = p_stab("backends");
	if (backends == NULL) {
		_fatal("No backends configured.");
	}

        p = strdup(backends);

        printf("** Configured order: %s\n", p);

	ud->be_list = (struct backend_p **)malloc((sizeof (struct backend_p *)) * (NBACKENDS + 1));

	bep = ud->be_list;
	nord = 0;

#if BE_PSK
	/*
	 * Force adding PSK back-end, which must be indexed at 0
	 * The PSK back-end is a little special in that it will use
	 * a database from another back-end (e.g. mysql or sqlite)
	 * for authorization.
	 */

	if ((psk_database = p_stab("psk_database")) == NULL) {
		_fatal("PSK is configured so psk_database needs to be set");
	}

	pskbep = bep;
	*pskbep = (struct backend_p *)malloc(sizeof(struct backend_p));
	memset(*pskbep, 0, sizeof(struct backend_p));
	(*pskbep)->name = strdup("psk");

	bep = pskbep;
	bep++;
	nord++;
#endif /* BE_PSK */

        for (q = strsep(&p, ","); q && *q && (nord < NBACKENDS); q = strsep(&p, ",")) {
                int found = 0;
#if BE_MYSQL
		if (!strcmp(q, "mysql")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("mysql");
			(*bep)->conf = be_mysql_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_mysql_destroy;
			(*bep)->getuser =  be_mysql_getuser;
			(*bep)->superuser =  be_mysql_superuser;
			(*bep)->aclcheck =  be_mysql_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_POSTGRES
		if (!strcmp(q, "postgres")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("postgres");
			(*bep)->conf = be_pg_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill = be_pg_destroy;
			(*bep)->getuser = be_pg_getuser;
			(*bep)->superuser = be_pg_superuser;
			(*bep)->aclcheck = be_pg_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_LDAP
		if (!strcmp(q, "ldap")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("ldap");
			(*bep)->conf = be_ldap_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_ldap_destroy;
			(*bep)->getuser =  be_ldap_getuser;
			(*bep)->superuser =  be_ldap_superuser;
			(*bep)->aclcheck =  be_ldap_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_CDB
		if (!strcmp(q, "cdb")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("cdb");
			(*bep)->conf = be_cdb_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_cdb_destroy;
			(*bep)->getuser =  be_cdb_getuser;
			(*bep)->superuser =  be_cdb_superuser;
			(*bep)->aclcheck =  be_cdb_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_SQLITE
		if (!strcmp(q, "sqlite")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("sqlite");
			(*bep)->conf = be_sqlite_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_sqlite_destroy;
			(*bep)->getuser =  be_sqlite_getuser;
			(*bep)->superuser =  be_sqlite_superuser;
			(*bep)->aclcheck =  be_sqlite_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_REDIS
		if (!strcmp(q, "redis")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("redis");
			(*bep)->conf = be_redis_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_redis_destroy;
			(*bep)->getuser =  be_redis_getuser;
			(*bep)->superuser =  be_redis_superuser;
			(*bep)->aclcheck =  be_redis_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif

#if BE_HTTP
		if (!strcmp(q, "http")) {
			*bep = (struct backend_p *)malloc(sizeof(struct backend_p));
			memset(*bep, 0, sizeof(struct backend_p));
			(*bep)->name = strdup("http");
			(*bep)->conf = be_http_init();
			if ((*bep)->conf == NULL) {
				_fatal("%s init returns NULL", q);
			}
			(*bep)->kill =  be_http_destroy;
			(*bep)->getuser =  be_http_getuser;
			(*bep)->superuser =  be_http_superuser;
			(*bep)->aclcheck =  be_http_aclcheck;
			found = 1;
			PSKSETUP;
		}
#endif
                if (!found) {
                        _fatal("ERROR: configured back-end `%s' is not compiled in this plugin", q);
                }

		ud->be_list[++nord] = NULL;
		bep++;
        }

        free(p);

	return (ret);
}

int mosquitto_auth_plugin_cleanup(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
	// struct userdata *ud = (struct userdata *)userdata;

	/* FIXME: free other elements */

	return MOSQ_ERR_SUCCESS;
}

int mosquitto_auth_security_init(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count, bool reload)
{
	return MOSQ_ERR_SUCCESS;
}

int mosquitto_auth_security_cleanup(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count, bool reload)
{
	return MOSQ_ERR_SUCCESS;
}


int mosquitto_auth_unpwd_check(void *userdata, const char *username, const char *password)
{
	struct userdata *ud = (struct userdata *)userdata;
	struct backend_p **bep;
	char *phash = NULL, *backend_name = NULL;
	int match, authenticated = FALSE, nord;

	if (!username || !*username || !password || !*password)
		return MOSQ_ERR_AUTH;

	ud->authentication_be = -1;

	_log(LOG_DEBUG, "mosquitto_auth_unpwd_check(%s)", (username) ? username : "<nil>");

	for (nord = 0, bep = ud->be_list; bep && *bep; bep++, nord++) {
		struct backend_p *b = *bep;

		_log(LOG_DEBUG, "** checking backend %s", b->name);

		/*
		 * The ->getuser() routine can decide to authenticate by setting
		 * either `authenticated = TRUE' or by returning a pointer to
		 * the user's PBKDF2 password hash
		 */

		phash = b->getuser(b->conf, username, password, &authenticated);
		if (authenticated == TRUE) {
			ud->authentication_be = nord;
			break;
		}
		if (phash != NULL) {
			match = pbkdf2_check((char *)password, phash);
			if (match == 1) {
				authenticated = TRUE;
				/* Mark backend index in userdata so we can check
				 * authorization in this back-end only.
				 */
				ud->authentication_be = nord;
				break;
			}
		}
	}

	/* Set name of back-end which authenticated */
	backend_name = (authenticated) ? (*bep)->name : "none";

	_log(DEBUG, "getuser(%s) AUTHENTICATED=%d by %s",
		username, authenticated, backend_name);

	if (phash != NULL) {
		free(phash);
	}

	return (authenticated) ? MOSQ_ERR_SUCCESS : MOSQ_ERR_AUTH;
}

int mosquitto_auth_acl_check(void *userdata, const char *clientid, const char *username, const char *topic, int access)
{
	struct userdata *ud = (struct userdata *)userdata;
	struct backend_p **bep;
	char *backend_name = NULL;
	int match = 0, authorized = FALSE, nord;

	_log(DEBUG, "mosquitto_auth_acl_check(..., %s, %s, %s, %d)",
		clientid ? clientid : "NULL",
		username ? username : "NULL",
		topic ? topic : "NULL",
		access);


	if (!username || !*username || !topic || !*topic)
		return MOSQ_ERR_ACL_DENIED;

	/* Check for usernames exempt from ACL checking, first */

	if (ud->superusers) {
		if (fnmatch(ud->superusers, username, 0) == 0) {
			_log(DEBUG, "aclcheck(%s, %s, %d) GLOBAL SUPERUSER=Y",
				username, topic, access);
			return MOSQ_ERR_SUCCESS;
		}
	}

	for (bep = ud->be_list; bep && *bep; bep++) {
		struct backend_p *b = *bep;

		match = b->superuser(b->conf, username);
		if (match == 1) {
			_log(DEBUG, "aclcheck(%s, %s, %d) SUPERUSER=Y by %s",
				username, topic, access, b->name);
			return MOSQ_ERR_SUCCESS;
		}
	}

	/*
	 * Check authorization in the back-end used to authenticate the user.
	 */

	nord = ud->authentication_be;
	backend_name = (nord >= 0 && nord < NBACKENDS) ?  ud->be_list[nord]->name : "<nil>";

	if ((nord < 0) || (nord >= NBACKENDS)) {
		_log(LOG_NOTICE, "nord is %d: unpossible!", nord);
		return (MOSQ_ERR_ACL_DENIED);
	}

	/* FIXME: |-- user bridge was authenticated in back-end 16 (<nil>)  */
	_log(LOG_NOTICE, "user %s was authenticated in back-end %d (%s)",
		username, nord, (backend_name) ? backend_name : "<nil>");


	bep = &ud->be_list[nord];
	if (nord == -1 || !bep)
		return (MOSQ_ERR_ACL_DENIED);


	match = (*bep)->aclcheck((*bep)->conf, username, topic, access);
	if (match == 1) {
		authorized = TRUE;
	}

	_log(DEBUG, "aclcheck(%s, %s, %d) AUTHORIZED=%d by %s",
		username, topic, access, authorized, backend_name);

	return (authorized) ?  MOSQ_ERR_SUCCESS : MOSQ_ERR_ACL_DENIED;
}


int mosquitto_auth_psk_key_get(void *userdata, const char *hint, const char *identity, char *key, int max_key_len)
{
#if BE_PSK
	struct userdata *ud = (struct userdata *)userdata;
	struct backend_p **bep;
	char *database = p_stab("psk_database");
	char *psk_key = NULL, *username;
	int psk_found = FALSE;

	// username = malloc(strlen(hint) + strlen(identity) + 12);
	// sprintf(username, "%s-%s", hint, identity);
	username = (char *)identity;

	for (bep = ud->be_list; bep && *bep; bep++) {
		struct backend_p *b = *bep;
		if (!strcmp(database, b->name)) {
			psk_key = b->getuser(b->conf, username);
			break;
		}

	}

	_log(DEBUG, "psk_key_get(%s, %s) from [%s] finds PSK: %d",
		hint, identity, database,
		psk_key ? 1 : 0);

	if (psk_key != NULL) {
		strncpy(key, psk_key, max_key_len);
		free(psk_key);
		psk_found = TRUE;
	}

	ud->authentication_be = 0;		/* PSK */

	// free(username);

	return (psk_found) ? MOSQ_ERR_SUCCESS : MOSQ_ERR_AUTH;

#else /* !BE_PSK */
	return MOSQ_ERR_AUTH;
#endif /* BE_PSK */
}
#ifdef CONFIG_USE_BINDED_TOPIC_MATCH
/* copy from mosquitto 1.3.4 */
/* Does a topic match a subscription? */
int mosquitto_topic_matches_sub(const char *sub, const char *topic, bool *result)
{
	int slen, tlen;
	int spos, tpos;
	bool multilevel_wildcard = false;

	if(!sub || !topic || !result) return MOSQ_ERR_INVAL;

	slen = strlen(sub);
	tlen = strlen(topic);

	if(slen && tlen){
		if((sub[0] == '$' && topic[0] != '$')
				|| (topic[0] == '$' && sub[0] != '$')){

			*result = false;
			return MOSQ_ERR_SUCCESS;
		}
	}

	spos = 0;
	tpos = 0;

	while(spos < slen && tpos < tlen){
		if(sub[spos] == topic[tpos]){
			if(tpos == tlen-1){
				/* Check for e.g. foo matching foo/# */
				if(spos == slen-3 
						&& sub[spos+1] == '/'
						&& sub[spos+2] == '#'){
					*result = true;
					multilevel_wildcard = true;
					return MOSQ_ERR_SUCCESS;
				}
			}
			spos++;
			tpos++;
			if(spos == slen && tpos == tlen){
				*result = true;
				return MOSQ_ERR_SUCCESS;
			}else if(tpos == tlen && spos == slen-1 && sub[spos] == '+'){
				spos++;
				*result = true;
				return MOSQ_ERR_SUCCESS;
			}
		}else{
			if(sub[spos] == '+'){
				spos++;
				while(tpos < tlen && topic[tpos] != '/'){
					tpos++;
				}
				if(tpos == tlen && spos == slen){
					*result = true;
					return MOSQ_ERR_SUCCESS;
				}
			}else if(sub[spos] == '#'){
				multilevel_wildcard = true;
				if(spos+1 != slen){
					*result = false;
					return MOSQ_ERR_SUCCESS;
				}else{
					*result = true;
					return MOSQ_ERR_SUCCESS;
				}
			}else{
				*result = false;
				return MOSQ_ERR_SUCCESS;
			}
		}
	}
	if(multilevel_wildcard == false && (tpos < tlen || spos < slen)){
		*result = false;
	}

	return MOSQ_ERR_SUCCESS;
}
#endif
