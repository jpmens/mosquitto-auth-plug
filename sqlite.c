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
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

struct _be_conn {
	sqlite3 *sq;
	sqlite3_stmt *stmt;
};

struct _be_conn *sqlite_init(char *dbpath, char *userquery)
{
	int res;
	int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_SHAREDCACHE;

	struct _be_conn *bec;

	bec = malloc(sizeof(struct _be_conn));

	if (sqlite3_open_v2(dbpath, &bec->sq, flags, NULL) != SQLITE_OK) {
		perror("jp.db");
		free(bec);
		return (NULL);
	}

	if ((res = sqlite3_prepare(bec->sq, userquery, strlen(userquery), &bec->stmt, NULL)) != SQLITE_OK) {
		fprintf(stderr, "Can't prepare: %s\n", sqlite3_errmsg(bec->sq));
		sqlite3_close(bec->sq);
		free(bec);
		return (NULL);
	}

	return (bec);
}

void sqlite_destroy(struct _be_conn *bec)
{
	if (bec) {
		sqlite3_finalize(bec->stmt);
		sqlite3_close(bec->sq);
		free(bec);
	}
}

char *sqlite_getuser(struct _be_conn *bec, const char *username)
{
	int res;
	char *value = NULL, *v;

	if (!bec)
		return (NULL);

	sqlite3_exec(bec->sq, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	sqlite3_reset(bec->stmt);
	sqlite3_clear_bindings(bec->stmt);

	res = sqlite3_bind_text(bec->stmt, 1, username, -1, SQLITE_STATIC);
	if (res != SQLITE_OK) {
		puts("Can't bind");
		goto out;
	}

	res = sqlite3_step(bec->stmt);

	if (res == SQLITE_ROW) {
		v = (char *)sqlite3_column_text(bec->stmt, 0);
		if (v)
			value = strdup(v);
	}

    out:
	sqlite3_exec(bec->sq, "COMMIT TRANSACTION;", NULL, NULL, NULL);

	return (value);
}
