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
#include "be-mysql.h"
#include <mosquitto.h>

struct backend *be_mysql_init(char *host, int port, char *user, char *passwd,
			char *dbname,
			char *userquery, char *superquery, char *aclquery)
{
	struct backend *be;

	if (!userquery)
		return (NULL);

	if ((be = malloc(sizeof(struct backend))) == NULL)
		return (NULL);

	be->mysql = mysql_init(NULL);
	be->userquery	= NULL;
	be->superquery	= NULL;
	be->aclquery	= NULL;

	if (!mysql_real_connect(be->mysql, host, user, passwd, dbname, port, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(be->mysql));
		free(be);
		mysql_close(be->mysql);
		return (NULL);
	}

	be->userquery = strdup(userquery);
	if (superquery)
		be->superquery = strdup(superquery);
	if (aclquery)
		be->aclquery = strdup(aclquery);

	return (be);
}

void be_mysql_destroy(struct backend *be)
{
	if (be) {
		mysql_close(be->mysql);
		if (be->userquery)
			free(be->userquery);
		if (be->superquery)
			free(be->superquery);
		if (be->aclquery)
			free(be->aclquery);
		free(be);
	}
}

static char *escape(struct backend *be, char *value, long *vlen)
{
	char *v;


	*vlen = strlen(value) * 2 + 1;
	if ((v = malloc(*vlen)) == NULL)
		return (NULL);
	mysql_real_escape_string(be->mysql, v, value, strlen(value));
	return (v);
}

char *be_mysql_userpw(struct backend *be, char *username)
{
	char *query = NULL, *u = NULL, *value = NULL, *v;
	long nrows, ulen;
	MYSQL_RES *res = NULL;
	MYSQL_ROW rowdata;

	if (!be)
		return (NULL);

	if ((u = escape(be, username, &ulen)) == NULL)
		return (NULL);

	if ((query = malloc(strlen(be->userquery) + ulen + 128)) == NULL) {
		free(u);
		return (NULL);
	}
	sprintf(query, be->userquery, u);
	free(u);

	// DEBUG puts(query);

	if (mysql_query(be->mysql, query)) {
		fprintf(stderr, "%s\n", mysql_error(be->mysql));
		goto out;
	}

	res = mysql_store_result(be->mysql);
	if ((nrows = mysql_num_rows(res)) != 1) {
		// DEBUG fprintf(stderr, "rowcount = %ld; not ok\n", nrows);
		goto out;
	}

	if (mysql_num_fields(res) != 1) {
		// DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	if ((rowdata = mysql_fetch_row(res)) == NULL) {
		goto out;
	}

	v = rowdata[0];
	value = (v) ? strdup(v) : NULL;


   out:

	mysql_free_result(res);
	free(query);

	return (value);
}

/*
 * Return T/F if user is superuser
 */

int be_mysql_superuser(struct backend *be, char *username)
{
	char *query = NULL, *u = NULL;
	long nrows, ulen;
	int issuper = FALSE;
	MYSQL_RES *res = NULL;
	MYSQL_ROW rowdata;

	if (!be)
		return (FALSE);

	if ((u = escape(be, username, &ulen)) == NULL)
		return (FALSE);

	if ((query = malloc(strlen(be->superquery) + ulen + 128)) == NULL) {
		free(u);
		return (FALSE);
	}
	sprintf(query, be->superquery, u);
	free(u);

	// puts(query);

	if (mysql_query(be->mysql, query)) {
		fprintf(stderr, "%s\n", mysql_error(be->mysql));
		goto out;
	}

	res = mysql_store_result(be->mysql);
	if ((nrows = mysql_num_rows(res)) != 1) {
		goto out;
	}

	if (mysql_num_fields(res) != 1) {
		// DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	if ((rowdata = mysql_fetch_row(res)) == NULL) {
		goto out;
	}

	issuper = atoi(rowdata[0]);

   out:

	mysql_free_result(res);
	free(query);

	return (issuper);
}

/*
 * Check ACL.
 * username is the name of the connected user attempting
 * to access
 * topic is the topic user is trying to access (may contain
 * wildcards)
 * acc is desired type of access: read/write		// FIXME 
 *	for subscriptions (READ) (1)
 *	for publish (WRITE) (2)
 *
 * SELECT topic FROM table WHERE username = '%s' AND acc = %d		// may user SUB or PUB topic?
 * SELECT topic FROM table WHERE username = '%s'              		// ignore ACC
 */

int be_mysql_aclcheck(struct backend *be, char *username, char *topic, int acc)
{
	char *query = NULL, *u = NULL, *v;
	long ulen;
	int match = 0;
	bool bf;
	MYSQL_RES *res = NULL;
	MYSQL_ROW rowdata;

	if (!be)
		return (FALSE);

	if ((u = escape(be, username, &ulen)) == NULL)
		return (FALSE);

	if ((query = malloc(strlen(be->aclquery) + ulen + 128)) == NULL) {
		free(u);
		return (FALSE);
	}
	sprintf(query, be->aclquery, u, acc);
	free(u);

	// puts(query);

	if (mysql_query(be->mysql, query)) {
		fprintf(stderr, "%s\n", mysql_error(be->mysql));
		goto out;
	}

	res = mysql_store_result(be->mysql);
	if (mysql_num_fields(res) != 1) {
		fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	while (match == 0 && (rowdata = mysql_fetch_row(res)) != NULL) {
		if ((v = rowdata[0]) != NULL) {
			// printf("--> %s\n", v);

			/* Check mosquitto_match_topic. If true,
			 * if true, set match and break out of loop. */

			mosquitto_topic_matches_sub(v, topic, &bf);
			match |= bf;
		}
	}

   out:

	mysql_free_result(res);
	free(query);

	return (match);
}

#ifdef TESTING

int main()
{
	struct backend *be = NULL;
	char *p;
	int match;
	static char **tu, *testusers[] = {
		"jjolie", "nop", "a", "S1", NULL };

	static char **top, *topics[] = {
		"loc/a",
		"loc/jjolie",
		"mega/secret",
		"loc/test",
		"$SYS/broker/log/N",
		NULL };

	char *user = NULL;
	char *pass = NULL;
	char *dbname = "test";

	be = be_mysql_init("localhost", 3306, user, pass, dbname,
			"SELECT pw FROM users WHERE username = '%s'",
			"SELECT COUNT(*) FROM users WHERE username = '%s' AND super = 1",
			"SELECT topic FROM acls WHERE username = '%s'");
		
	if (be == NULL) {
		fprintf(stderr, "Cannot init\n");
		exit(1);
	}

	for (tu = testusers; tu && *tu; tu++) {
		int superuser = FALSE;

		p = be_mysql_userpw(be, *tu);

		superuser = be_mysql_superuser(be, *tu);

		printf("%c %-10s %s\n",
			(superuser) ? '*' : ' ',
			*tu,
			p ? p : "<nil>");
		if (p)
			free(p);

		for (top = topics; top && *top; top++) {
				/* FIXME: read/write ??? */
				match = be_mysql_aclcheck(be, *tu, *top, 1);

				match |= superuser;
				printf("\t%-40s %s\n",
					*top,
					(match) ? "PERMIT" : "DENY");
		}
	}

	be_mysql_destroy(be);
	return (0);
}
#endif
