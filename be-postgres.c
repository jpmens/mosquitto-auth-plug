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

#ifdef BE_POSTGRES

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include "be-postgres.h"
#include "log.h"
#include "hash.h"
#include "backends.h"
#include <arpa/inet.h>

struct pg_backend {
	PGconn *conn;
	char *host;
	char *port;
	char *dbname;
	char *user;
	char *pass;
	char *userquery;        // MUST return 1 row, 1 column
	char *superquery;       // MUST return 1 row, 1 column, [0, 1]
	char *aclquery;         // MAY return n rows, 1 column, string
};

void *be_pg_init()
{
	struct pg_backend *conf;
	char *host, *user, *pass, *dbname, *p, *port;
	char *userquery;

	_log(LOG_DEBUG, "}}}} POSTGRES");

	host   = p_stab("host");
	p      = p_stab("port");
	user   = p_stab("user");
	pass   = p_stab("pass");
	dbname = p_stab("dbname");

	host = (host) ? host : strdup("localhost");
	port = (p) ? p : strdup("5432");

	userquery = p_stab("userquery");

	if (!userquery) {
		_fatal("Mandatory option 'userquery' is missing");
		return (NULL);
	}

	if ((conf = (struct pg_backend *)malloc(sizeof(struct pg_backend))) == NULL)
		return (NULL);

	conf->conn       = NULL;
	conf->host       = host;
	conf->port       = port;
	conf->user       = user;
	conf->pass       = pass;
	conf->dbname     = dbname;
	conf->userquery  = userquery;
	conf->superquery = p_stab("superquery");
	conf->aclquery   = p_stab("aclquery");

	_log( LOG_DEBUG, "HERE: %s", conf->superquery );
	_log( LOG_DEBUG, "HERE: %s", conf->aclquery );


	char *connect_string = NULL;

	conf->conn = PQsetdbLogin(conf->host, conf->port, NULL, NULL, conf->dbname, conf->user, conf->pass );

	if (PQstatus(conf->conn) == CONNECTION_BAD) {
		free(conf);
		free(connect_string);
		_fatal("We were unable to connect to the database");
		return (NULL);
	}

	free(connect_string);

	return ((void *)conf);
}

void be_pg_destroy(void *handle)
{
	struct pg_backend *conf = (struct pg_backend *)handle;

	if (conf) {
		PQfinish(conf->conn);
		if (conf->userquery)
			free(conf->userquery);
		if (conf->superquery)
			free(conf->superquery);
		if (conf->aclquery)
			free(conf->aclquery);
		free(conf);
	}
}

char *be_pg_getuser(void *handle, const char *username, const char *password, int *authenticated)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *value = NULL, *v = NULL;
	long nrows;
	PGresult *res = NULL;

	_log(LOG_DEBUG, "GETTING USERS: %s", username );

	if (!conf || !conf->userquery || !username || !*username)
		return (NULL);

	const char *values[1] = {username};
	int lengths[1] = {strlen(username)};
	int binary[1] = {0};

	res = PQexecParams(conf->conn, conf->userquery, 1, NULL, values, lengths, binary, 0);

	if ( PQresultStatus(res) != PGRES_TUPLES_OK )
	{
		_log(LOG_DEBUG, "%s\n", PQresultErrorMessage(res));
		goto out;
	}


	if ((nrows = PQntuples(res)) != 1) {
		// DEBUG fprintf(stderr, "rowcount = %ld; not ok\n", nrows);
		goto out;
	}

	if (PQnfields(res) != 1) {
		// DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	if ((v = PQgetvalue(res,0,0)) == NULL) {
		goto out;
	}

	value = (v) ? strdup(v) : NULL;


out:

	PQclear(res);

	return (value);
}

/*
 * Return T/F if user is superuser
 */

int be_pg_superuser(void *handle, const char *username)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *v = NULL;
	long nrows;
	int issuper = FALSE;
	PGresult *res = NULL;

	_log( LOG_DEBUG, "SUPERUSER: %s", username );

	if (!conf || !conf->superquery || !username || !*username)
		return (FALSE);

	// query for postgres $1 instead of %s
	const char *values[1] = {username};
	int lengths[1] = {strlen(username)};
	int binary[1] = {0};

	res = PQexecParams(conf->conn, conf->superquery, 1, NULL, values, lengths, binary, 0);

	if ( PQresultStatus(res) != PGRES_TUPLES_OK )
	{
		fprintf(stderr, "%s\n", PQresultErrorMessage(res));
		goto out;
	}

	if ((nrows = PQntuples(res)) != 1) {
		goto out;
	}

	if (PQnfields(res) != 1) {
		// DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	if ((v = PQgetvalue(res,0,0)) == NULL) {
		goto out;
	}

	issuper = atoi(v);


out:
	_log(LOG_DEBUG, "user is %d", issuper );

	PQclear(res);

	return (issuper);
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

int be_pg_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *v = NULL;
	int match = 0;
	bool bf;
	PGresult *res = NULL;

	_log( LOG_DEBUG, "USERNAME: %s, TOPIC: %s, acc: %d", username, topic, acc );


	if (!conf || !conf->aclquery)
		return (FALSE);

	int localacc = htonl(acc);

	const char *values[2] = {username,(char*)&localacc};
	int lengths[2] = {strlen(username),sizeof(localacc)};
	int binary[2] = {0,1};

	res = PQexecParams(conf->conn, conf->aclquery, 2, NULL, values, lengths, binary, 0);

	if ( PQresultStatus(res) != PGRES_TUPLES_OK )
	{
		fprintf(stderr, "%s\n", PQresultErrorMessage(res));
		goto out;
	}

	if (PQnfields(res) != 1) {
		fprintf(stderr, "numfields not ok\n");
		goto out;
	}

	int rec_count = PQntuples(res);
	int row = 0;
	for ( row = 0; row < rec_count; row++ ) {
		if ( (v = PQgetvalue(res,row,0) ) != NULL) {

			/* Check mosquitto_match_topic. If true,
			 * if true, set match and break out of loop. */

                        char *expanded;

                        t_expand(clientid, username, v, &expanded);
                        if (expanded && *expanded) {
                                mosquitto_topic_matches_sub(expanded, topic, &bf);
                                match |= bf;
                                _log(LOG_DEBUG, "  postgres: topic_matches(%s, %s) == %d",
                                        expanded, v, bf);

				free(expanded);
                        }
		}

		if ( match != 0 )
		{
			break;
		}
	}

out:

	PQclear(res);

	return (match);
}
#endif /* BE_POSTGRES */
