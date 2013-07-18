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
#include "redis.h"

struct userdata {
	redisContext *redis;
	char *usernameprefix;		/* e.g. "u:" */
	char *topicprefix;
	char *superusers;		/* fnmatch-style glob */
};

int pbkdf2_check(char *password, char *hash);

int mosquitto_auth_plugin_version(void)
{
	return MOSQ_AUTH_PLUGIN_VERSION;
}

int mosquitto_auth_plugin_init(void **userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
	int i;
	struct mosquitto_auth_opt *o;
	struct userdata *ud;
	char *redis_host = NULL;
	int redis_port = 6379;
	int ret = MOSQ_ERR_SUCCESS;

	*userdata = (struct userdata *)malloc(sizeof(struct userdata));
	if (*userdata == NULL) {
		perror("allocting userdata");
		return MOSQ_ERR_UNKNOWN;
	}

	ud = *userdata;
	ud->usernameprefix	= NULL;
	ud->topicprefix		= NULL;
	ud->superusers		= NULL;
	ud->redis		= NULL;

	for (i = 0, o = auth_opts; i < auth_opt_count; i++, o++) {
#ifdef DEBUG
		fprintf(stderr, "AuthOptions: key=%s, val=%s\n", o->key, o->value);
#endif
		if (!strcmp(o->key, "redis_username_prefix"))
			ud->usernameprefix = strdup(o->value);
		if (!strcmp(o->key, "redis_topic_prefix"))
			ud->topicprefix = strdup(o->value);
		if (!strcmp(o->key, "redis_superusers"))
			ud->superusers = strdup(o->value);
		if (!strcmp(o->key, "redis_host"))
			redis_host = strdup(o->value);
		if (!strcmp(o->key, "redis_port"))
			redis_port = atoi(o->value);
	}

	if (redis_host == NULL)
		redis_host = strdup("localhost");

	ud->redis = redis_init(redis_host, redis_port);
	if (ud->redis == NULL) {
		fprintf(stderr, "Cannot connect to Redis on %s:%d\n", redis_host, redis_port);
		ret = MOSQ_ERR_UNKNOWN;
	}

	free(redis_host);

	return (ret);
}

int mosquitto_auth_plugin_cleanup(void *userdata, struct mosquitto_auth_opt *auth_opts, int auth_opt_count)
{
	struct userdata *ud = (struct userdata *)userdata;

	if (ud) {
		if (ud->redis) {
			redis_destroy(ud->redis);
		}
		if (ud->usernameprefix)
			free(ud->usernameprefix);
		if (ud->topicprefix)
			free(ud->topicprefix);
		if (ud->superusers)
			free(ud->superusers);
	}

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

int mosquitto_auth_acl_check(void *userdata, const char *clientid, const char *username, const char *topic, int access)
{
	struct userdata *ud = (struct userdata *)userdata;
	char *tname;
	int tlen, match = 0;

#ifdef DEBUG
	fprintf(stderr, "acl_check u=%s, t=%s, a=%d\n",
		(username) ? username : "NIL",
		(topic) ? topic : "NIL",
		access);
#endif

	if (!username || !*username)
		return MOSQ_ERR_ACL_DENIED;
	if (ud->redis == NULL)
		return MOSQ_ERR_ACL_DENIED;

/*
	if (strcmp(username, "S1") == 0) {
		return MOSQ_ERR_SUCCESS;
	}
	*/

	/* Check for usernames exempt from ACL checking, first */

	if (ud->superusers) {
		if (fnmatch(ud->superusers, username, 0) == 0) {
#ifdef DEBUG
			fprintf(stderr, "** !!! %s is superuser\n", username);
#endif
			return MOSQ_ERR_SUCCESS;
		}
	}

	if (ud->topicprefix) {
		char *s, *t;
		int n;

#ifdef DEBUG
		fprintf(stderr, "** topicprefix=%s\n", ud->topicprefix);
#endif

		/* Count number of '%' in topicprefix */
		for (n = 0, s = ud->topicprefix; s && *s; s++) {
			if (*s == '%')
				++n;
		}

		tlen = strlen(ud->topicprefix) + (strlen(username) * n) + 1;
		tname = malloc(tlen);

		/* Create new topic in tname with all '%' replaced by username */
		*tname = 0;
		for (t = tname, s = ud->topicprefix; s && *s; ) {
			if (*s != '%') {
				*t++ = *s++;
				*t = 0;
			} else {
				strcat(tname, username);
				t = tname + strlen(tname);
				s++;
			}
		}

#ifdef DEBUG
		fprintf(stderr, "**-> tname=[%s]\n", tname);
#endif
		if (strcmp(topic, tname) == 0)
			match = 1;

		free(tname);
	}

#ifdef DEBUG
	fprintf(stderr, "** ACL match == %d\n", match);
#endif

	if (match == 1) {
		return MOSQ_ERR_SUCCESS;
	} 
	return MOSQ_ERR_ACL_DENIED;
}

int mosquitto_auth_unpwd_check(void *userdata, const char *username, const char *password)
{
	struct userdata *ud = (struct userdata *)userdata;
	char *phash;
	int match;

#ifdef DEBUG
	fprintf(stderr, "auth_unpwd_check u=%s, p=%s, redisuser: %s%s\n",
		(username) ? username : "NIL",
		(password) ? password : "NIL",
		(ud->usernameprefix) ? ud->usernameprefix : "",
		username);
#endif


	if (!username || !*username || !password || !*password)
		return MOSQ_ERR_AUTH;

	if (ud->redis == NULL)
		return MOSQ_ERR_AUTH;

	if ((phash = redis_getuser(ud->redis, ud->usernameprefix, username)) == NULL) {
#ifdef DEBUG
		fprintf(stderr, "User %s%s not found in Redis\n", 
			ud->usernameprefix? ud->usernameprefix : "",
			username);
#endif
		return MOSQ_ERR_AUTH;
	}

	match = pbkdf2_check((char *)password, phash);

#ifdef DEBUG
	fprintf(stderr, "unpwd_check: for user=%s, got: %s\n", username, phash);
	fprintf(stderr, "unpwd_check: PBKDF2 match == %d\n", match);
#endif

	free(phash);
	return (match == 1) ? MOSQ_ERR_SUCCESS : MOSQ_ERR_AUTH;
}

int mosquitto_auth_psk_key_get(void *userdata, const char *hint, const char *identity, char *key, int max_key_len)
{
	return MOSQ_ERR_AUTH;
}

