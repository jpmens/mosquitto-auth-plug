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

#ifdef BE_SQLITE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "be-sqlite.h"
#include "hash.h"
#include "log.h"

void *be_sqlite_init()
{
	struct sqlite_backend *conf;
	int res;
	int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_SHAREDCACHE;
	char *dbpath, *userquery;

	if ((dbpath = p_stab("dbpath")) == NULL) {
		_fatal("Mandatory parameter `dbpath' missing");
		return (NULL);
	}

	if ((userquery = p_stab("sqliteuserquery")) == NULL) {
		_fatal("Mandatory parameter `sqliteuserquery' missing");
		return (NULL);
	}

	conf = (struct sqlite_backend *)malloc(sizeof(struct sqlite_backend));

	if (sqlite3_open_v2(dbpath, &conf->sq, flags, NULL) != SQLITE_OK) {
		perror(dbpath);
		free(conf);
		return (NULL);
	}

	if ((res = sqlite3_prepare(conf->sq, userquery, strlen(userquery), &conf->stmt, NULL)) != SQLITE_OK) {
		fprintf(stderr, "Can't prepare: %s\n", sqlite3_errmsg(conf->sq));
		sqlite3_close(conf->sq);
		free(conf);
		return (NULL);
	}

	return (conf);
}

void be_sqlite_destroy(void *handle)
{
	struct sqlite_backend *conf = (struct sqlite_backend *)handle;

	if (conf) {
		sqlite3_finalize(conf->stmt);
		sqlite3_close(conf->sq);
		free(conf);
	}
}

char *be_sqlite_getuser(void *handle, const char *username, const char *password, int *authenticated)
{
	struct sqlite_backend *conf = (struct sqlite_backend *)handle;
	int res;
	char *value = NULL, *v;

	if (!conf)
		return (NULL);

	sqlite3_reset(conf->stmt);
	sqlite3_clear_bindings(conf->stmt);

	res = sqlite3_bind_text(conf->stmt, 1, username, -1, SQLITE_STATIC);
	if (res != SQLITE_OK) {
		puts("Can't bind");
		goto out;
	}

	res = sqlite3_step(conf->stmt);

	if (res == SQLITE_ROW) {
		v = (char *)sqlite3_column_text(conf->stmt, 0);
		if (v)
			value = strdup(v);
	}

    out:
	sqlite3_reset(conf->stmt);

	return (value);
}

int be_sqlite_superuser(void *handle, const char *username)
{
	return 0;
}

int be_sqlite_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	return 1;
}
#endif /* BE_SQLITE */
