/*
 * Copyright (c) 2014 Jan-Piet Mens <jpmens()gmail.com>
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
#include "be-ldap.h"
#include "log.h"
#include "hash.h"
#include "backends.h"

struct ldap_backend {
	char *ldap_uri;
	LDAPURLDesc *lud;	
	LDAP *ld;
        char *user_uri;        // MUST return 1 row, 1 column
        char *superquery;       // MUST return 1 row, 1 column, [0, 1]
        char *aclquery;         // MAY return n rows, 1 column, string
};

void *be_ldap_init()
{
	struct ldap_backend *conf;
	char *uri;
	char connstr[512];
	char *binddn, *bindpw;
	int rc, opt;

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

	conf->ldap_uri = strdup(uri);
	if (ldap_url_parse(uri, &conf->lud) != 0) {
		_fatal("Cannot parse ldap_uri");
		return (NULL);
	}

	/* FIXME: _initialize() allows schema://host:port only */
	sprintf(connstr, "%s://%s:%d", conf->lud->lud_scheme, conf->lud->lud_host, conf->lud->lud_port);
	printf("INIT TO %s\n", connstr);

	if (ldap_initialize(&conf->ld, connstr) != LDAP_SUCCESS) {
		ldap_free_urldesc(conf->lud);
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


	return ((void *)conf);
}

void be_ldap_destroy(void *handle)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;

	if (conf) {
		ldap_free_urldesc(conf->lud);
		free(conf->ldap_uri);

		/* FIXME: ldap_close */
		free(conf);
	}
}

char *be_ldap_getuser(void *handle, const char *username, const char *password, int *authenticated)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;
	LDAPMessage *msg,*entry;
	int rc, len;
	char *filter, *bp, *fp, *up, *dn;

	printf("+++++++++++ GET %s USERNAME [%s] (%s)\n", conf->ldap_uri, username, password);

	*authenticated = FALSE;

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
		return (NULL);
	}

	free(filter);

	if (ldap_count_entries(conf->ld, msg) != 1) {
		_log(1, "LDAP search for %s returns != 1 entry", username);
		return (NULL);
	}

	entry = ldap_first_entry(conf->ld, msg);
	dn = ldap_get_dn(conf->ld, entry);

	_log(1, "Attempt to bind as %s\n", dn);

	if ((rc = ldap_simple_bind_s(conf->ld, dn, password)) != LDAP_SUCCESS) {
		_log(1, "Cannot bind to LDAP as %s: %s", dn, ldap_err2string(rc));
		ldap_memfree(dn);
		return (NULL);
	}
	ldap_memfree(dn);

	*authenticated = TRUE;
	
	return (NULL);
}

/*
 * Return T/F if user is superuser
 */

int be_ldap_superuser(void *handle, const char *username)
{
	struct ldap_backend *conf = (struct ldap_backend *)handle;
	printf("%s\n", conf->ldap_uri);

	return (0);
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

int be_ldap_aclcheck(void *handle, const char *username, const char *topic, int acc)
{
	return (2);
}
#endif /* BE_LDAP */
