/*
 * Copyright (c) 2013 Jan-Piet Mens <jp@mens.de> All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer. 2. Redistributions
 * in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution. 3. Neither the name of mosquitto
 * nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
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

#ifdef BE_MEMCACHED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "hash.h"
#include "backends.h"
#include <libmemcached/memcached.h>

struct memcached_backend {
	memcached_st *memcached;
	char *host;
	char *userquery;
	char *aclquery;
	char *dbpass;
	int port;
	int db;
};


static int be_memcached_reconnect(struct memcached_backend *conf)
{
	if (conf->memcached != NULL) {
		memcached_free(conf->memcached);
		conf->memcached = NULL;
	}
	conf->memcached = memcached_create(NULL);

	memcached_return memcachedReply;
	memcached_server_st *servers;

	servers = memcached_server_list_append(NULL, conf->host, conf->port, &memcachedReply);
	memcachedReply = memcached_server_push(conf->memcached, servers);
	memcached_server_list_free(servers);

	//error message in memcached_st is called memcached_error_t but it is weird
	if (conf->memcached == NULL) {
		_log(LOG_NOTICE, "Memcached connection error for %s:%d\n",
		     conf->host, conf->port);
		return 1;
	}
	//there is no database password in memcached

		// check memcachced connection
	memcached_return rc;
	memcached_stat_st *stats = memcached_stat(conf->memcached, NULL, &rc);
	if (stats == NULL && rc != MEMCACHED_SUCCESS && rc != MEMCACHED_SOME_ERRORS) {
		return 2;
	}
	return 0;
}

void *be_memcached_init()
{
	struct memcached_backend *conf;
	char *host, *p, *db, *userquery, *password, *aclquery;

	_log(LOG_DEBUG, "}}}} Memcached");

	if ((host = p_stab("memcached_host")) == NULL)
		host = "localhost";
	if ((p = p_stab("memcached_port")) == NULL)
		p = "11211";
	if ((db = p_stab("memcached_db")) == NULL)
		db = "0";
	if ((password = p_stab("memcached_pass")) == NULL)
		password = "";
	if ((userquery = p_stab("memcached_userquery")) == NULL) {
		userquery = "";
	}
	if ((aclquery = p_stab("memcached_aclquery")) == NULL) {
		aclquery = "";
	}
	conf = (struct memcached_backend *)malloc(sizeof(struct memcached_backend));
	if (conf == NULL)
		_fatal("Out of memory");

	conf->host = strdup(host);
	conf->port = atoi(p);
	conf->db = atoi(db);
	conf->dbpass = strdup(password);
	conf->userquery = strdup(userquery);
	conf->aclquery = strdup(aclquery);

	conf->memcached = NULL;

	if (be_memcached_reconnect(conf)) {
		free(conf->host);
		free(conf->userquery);
		free(conf->dbpass);
		free(conf->aclquery);
		free(conf);
		return (NULL);
	}
	return (conf);
}

void be_memcached_destroy(void *handle)
{
	struct memcached_backend *conf = (struct memcached_backend *)handle;

	if (conf != NULL) {
		memcached_free(conf->memcached);
		conf->memcached = NULL;
		free(conf);
	}
}

int be_memcached_getuser(void *handle, const char *username, const char *password, char **phash)
{
	struct memcached_backend *conf = (struct memcached_backend *)handle;

	memcached_return rc;
	size_t value_length;
	uint32_t flags;
	char *value = NULL;

	if (conf == NULL || conf->memcached == NULL || username == NULL)
		return (BACKEND_ERROR);

	value = memcached_get(conf->memcached, username, strlen(username), &value_length, &flags, &rc);

	if (value == NULL || rc != MEMCACHED_SUCCESS) {
		be_memcached_reconnect(conf);
		return (BACKEND_DEFER);
	}
	if (rc == MEMCACHED_SUCCESS) {
		*phash = strdup(value);
	}
	return (BACKEND_DEFER);
}

int be_memcached_superuser(void *conf, const char *username)
{
	return 0;
}

int be_memcached_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct memcached_backend *conf = (struct memcached_backend *)handle;

	memcached_return rc;
	size_t value_length;
	uint32_t flags;
	char *value = NULL;

	if (conf == NULL || conf->memcached == NULL || username == NULL)
		return 0;

	if (strlen(conf->aclquery) == 0) {
		return 1;
	}
	char *query = malloc(strlen(conf->aclquery) + strlen(username) + strlen(topic) + 128);
	sprintf(query, "%s-%s", username, topic);
	value = memcached_get(conf->memcached, query, strlen(query), &value_length, &flags, &rc);

	if (value == NULL || rc != MEMCACHED_SUCCESS) {
		be_memcached_reconnect(conf);
		return BACKEND_ERROR;
	}
	free(query);

	int answer = 0;
	if (rc == MEMCACHED_SUCCESS) {
		int x = atoi(value);
		if (x >= acc)
			answer = 1;
	}
	free(value);
	return answer;
}
#endif				/* BE_MEMCACHED */
