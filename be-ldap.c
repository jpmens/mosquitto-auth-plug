/*
 * Copyright (c) 2014 Jan-Piet Mens <jp@mens.de>
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

#ifdef BE_LDAP

#define   LDAP_DEPRECATED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include "backends.h"
#include "be-ldap.h"
#include "log.h"
#include "hash.h"

struct ldap_backend {
	char *ldap_uri;
	char *connstr;		/* ldap_initialize() wants scheme://host:port  only */
	LDAPURLDesc *lud;	
	LDAP *ld;
	char *user_uri;
	char *superquery;
	char *aclquery;
	int acldeny;
};

static char *get_bool(char *option, char *defval)
{
	char *flag = p_stab(option);
	flag = flag ? flag : defval;
	if (!strcmp("true", flag) || !strcmp("false", flag)) {
		return flag;
	}
	_log(LOG_NOTICE, "WARN: %s is unexpected value -> %s", option, flag);
	return defval;
}

void *be_ldap_init()
{
	struct ldap_backend *conf;
	char *uri;
	char *binddn, *bindpw;
	char *opt_flag;
	int rc, opt, len;

	_log(LOG_DEBUG, "}}}} LDAP");

	uri = p_stab("ldap_uri");
	binddn = p_stab("binddn");
	bindpw = p_stab("bindpw");

	if (!uri) {
		_fatal("Mandatory option 'ldap_uri' is missing");
		return (NULL);
	}

	if (!ldap_is_ldap_url(uri)) {
		_fatal("Mandatory option 'ldap_uri' doesn't look like an LDAP URI");
		return (NULL);
	}

	if ((conf = (struct ldap_backend *)malloc(sizeof(struct ldap_backend))) == NULL)
		return (NULL);

	conf->ldap_uri	= NULL;
	conf->connstr	= NULL;
	conf->lud	= NULL;
	conf->ld	= NULL;
	conf->user_uri	= NULL;
	conf->superquery = NULL;
	conf->aclquery	= NULL;
	conf->acldeny = 0;

	conf->ldap_uri = strdup(uri);
	if (ldap_url_parse(uri, &conf->lud) != 0) {
		_fatal("Cannot parse ldap_uri");
		return (NULL);
	}

	/* ldap_initialize() allows schema://host:port only; build
	 * an appropriate string from what we have, to use later also.
	 */

	len = strlen(conf->lud->lud_scheme) + strlen(conf->lud->lud_host) + 15;
	if ((conf->connstr = malloc(len)) == NULL) {
		_fatal("Out of memory");
		return (NULL);
	}
	sprintf(conf->connstr, "%s://%s:%d", conf->lud->lud_scheme, conf->lud->lud_host, conf->lud->lud_port);
	if (ldap_initialize(&conf->ld, conf->connstr) != LDAP_SUCCESS) {
		ldap_free_urldesc(conf->lud);
		free(conf->connstr);
		free(conf->ldap_uri);

		_fatal("Cannot ldap_initialize");
		return (NULL);
	}

	opt = LDAP_VERSION3;
	ldap_set_option(conf->ld, LDAP_OPT_PROTOCOL_VERSION, &opt);

	if ((rc = ldap_simple_bind_s(conf->ld, binddn, bindpw)) != LDAP_SUCCESS) {
		_fatal("Cannot bind to LDAP: %s", ldap_err2string(rc));
		return (NULL);
	}

	// conf->superquery	= p_stab("superquery");
	// conf->aclquery		= p_stab("aclquery");

	opt_flag = get_bool("ldap_acl_deny", "false");
	if (!strcmp("true", opt_flag))
		conf->acldeny = 1;

	return ((void *)conf);
}

void be_ldap_destroy(void *handle)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;

	if (conf) {
		ldap_free_urldesc(conf->lud);
		free(conf->ldap_uri);

		if (conf->connstr)
			free(conf->connstr);
		if (conf->ld)
			ldap_unbind(conf->ld);
		free(conf);
	}
}

/*
 * Open a new connection to LDAP so that we don't lose the exising
 * binddn/pw. Check if the user's `dn' can bind with `password'.
 * Return T/F. `connstr' is a scheme://host:port thing.
 */

static int user_bind(char *connstr, char *dn, const char *password)
{
	LDAP *ld;
	int opt, rc;

	if (ldap_initialize(&ld, connstr) != LDAP_SUCCESS) {
		_log(1, "Cannot ldap_initialize-2");
		return (FALSE);
	}

	opt = LDAP_VERSION3;
	ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &opt);

	if ((rc = ldap_simple_bind_s(ld, dn, password)) != LDAP_SUCCESS) {
		_log(1, "Cannot bind to LDAP as %s: %s", dn, ldap_err2string(rc));
		return (FALSE);
	}

	ldap_unbind(ld);
	return (TRUE);

}

int be_ldap_getuser(void *handle, const char *username, const char *password, char **phash)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;
	LDAPMessage *msg,*entry;
	int rc, len;
	char *filter, *bp, *fp, *up, *dn;

	// printf("+++++++++++ GET %s USERNAME [%s] (%s)\n", conf->ldap_uri, username, password);

	/*
	 * Replace '@' in filter with `username'
	 */

	len = strlen(conf->lud->lud_filter) + strlen(username) + 10;
	filter = (char *)malloc(len);

	for (fp = filter, bp = conf->lud->lud_filter; bp && *bp;) {
		if (*bp == '@') {
			++bp;
			for (up = (char *)username; up && *up; up++) {
				*fp++ = *up;
			}
		} else {
			*fp++ = *bp++;
		}
		*fp = 0;
	}

	rc = ldap_search_s(conf->ld,
		conf->lud->lud_dn,
		conf->lud->lud_scope,
		filter,
		conf->lud->lud_attrs,
		0,
		&msg);
	if (rc != LDAP_SUCCESS) {
		_fatal("Cannot search LDAP for user %s: %s", username, ldap_err2string(rc));
		return BACKEND_ERROR;
	}

	free(filter);

	if (ldap_count_entries(conf->ld, msg) != 1) {
		_log(1, "LDAP search for %s returns != 1 entry", username);
		return BACKEND_DEFER;
	}

	rc = BACKEND_DEFER;
	if ((entry = ldap_first_entry(conf->ld, msg)) != NULL) {
		dn = ldap_get_dn(conf->ld, entry);

		_log(1, "Attempt to bind as %s\n", dn);

		if (user_bind(conf->connstr, dn, password)) {
			rc = BACKEND_ALLOW;
		}

		ldap_memfree(dn);
	}
	
	return rc;
}

/*
 * Return T/F if user is superuser
 */

int be_ldap_superuser(void *handle, const char *username)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;
	printf("%s\n", conf->ldap_uri);

	return BACKEND_DEFER;
}

/*
 * Check ACL.
 * username is the name of the connected user attempting
 * to access
 * topic is the topic user is trying to access (may contain
 * wildcards)
 * acc is desired type of access: read/write
 *	for subscriptions (READ) (1)
 *	for publish (WRITE) (2)
 *
 * SELECT topic FROM table WHERE username = '%s' AND (acc & %d)		// may user SUB or PUB topic?
 * SELECT topic FROM table WHERE username = '%s'              		// ignore ACC
 */

int be_ldap_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;

	return (conf->acldeny ? BACKEND_DENY : BACKEND_ALLOW);
}
#endif /* BE_LDAP */
