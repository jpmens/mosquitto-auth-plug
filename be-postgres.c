/*
 * Copyright (c) 2014 Jan-Piet Mens <jp@mens.de>
 * All rights reserved.
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
	char *userquery;
	   //MUST return 1 row, 1 column
	char *superquery;
	   //MUST return 1 row, 1 column,[0, 1]
	char *aclquery;
	   //MAY return n rows, 1 column, string
	char *sslcert;
	char *sslkey;
};

static int addKeyValue(char **keywords, char **values, char *key, char *value,
		const int MAX_KEYS);

void *be_pg_init()
{
	struct pg_backend *conf;
	char *host, *user, *pass, *dbname, *p, *port, *sslcert, *sslkey;
	char *userquery;
	char **keywords = NULL;
	char **values = NULL;

	_log(LOG_DEBUG, "}}}} POSTGRES");

	host = p_stab("host");
	p = p_stab("port");
	user = p_stab("user");
	pass = p_stab("pass");
	dbname = p_stab("dbname");
	sslcert = p_stab("sslcert");
	sslkey = p_stab("sslkey");

	host = (host) ? host : strdup("");
	port = (p) ? p : strdup("");

	userquery = p_stab("userquery");

	if (!userquery) {
		_fatal("Mandatory option 'userquery' is missing");
		return (NULL);
	}
	if ((conf = (struct pg_backend *)malloc(sizeof(struct pg_backend))) == NULL)
		return (NULL);

	conf->conn = NULL;
	conf->host = host;
	conf->port = port;
	conf->user = user;
	conf->pass = pass;
	conf->dbname = dbname;
	conf->userquery = userquery;
	conf->superquery = p_stab("superquery");
	conf->aclquery = p_stab("aclquery");
	conf->sslcert = sslcert;
	conf->sslkey = sslkey;

	_log(LOG_DEBUG, "HERE: %s", conf->superquery);
	_log(LOG_DEBUG, "HERE: %s", conf->aclquery);

	const uint8_t MAX_KEYS = 7;
	keywords = (char **) calloc(MAX_KEYS + 1, sizeof(char *));
	values = (char **) calloc(MAX_KEYS + 1, sizeof(char *));

	if (conf->host) {
		addKeyValue(keywords, values, "host", conf->host, MAX_KEYS);
	}
	if (conf->port) {
		addKeyValue(keywords, values, "port", conf->port, MAX_KEYS);
	}
	if (conf->dbname) {
		addKeyValue(keywords, values, "dbname", conf->dbname, MAX_KEYS);
	}
	if (conf->user) {
		addKeyValue(keywords, values, "user", conf->user, MAX_KEYS);
	}
	if (conf->pass) {
		addKeyValue(keywords, values, "password", conf->pass, MAX_KEYS);
	}
	if (conf->sslcert) {
		addKeyValue(keywords, values, "sslcert", conf->sslcert, MAX_KEYS);
	}
	if (conf->sslkey) {
		addKeyValue(keywords, values, "sslkey", conf->sslkey, MAX_KEYS);
	}

	conf->conn = PQconnectdbParams(
		(const char * const *)keywords, (const char * const *)values, 0);

	free(keywords);
	free(values);

	if (PQstatus(conf->conn) == CONNECTION_BAD) {
		free(conf);
		_fatal("We were unable to connect to the database");
		return (NULL);
	}

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

int be_pg_getuser(void *handle, const char *username, const char *password, char **phash)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *value = NULL, *v = NULL;
	long nrows;
	PGresult *res = NULL;

	_log(LOG_DEBUG, "GETTING USERS: %s", username);

	if (!conf || !conf->userquery || !username || !*username)
		return BACKEND_DEFER;

	const char *values[1] = {username};
	int lengths[1] = {strlen(username)};
	int binary[1] = {0};

	res = PQexecParams(conf->conn, conf->userquery, 1, NULL, values, lengths, binary, 0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		_log(LOG_DEBUG, "%s\n", PQresultErrorMessage(res));
		if(PQstatus(conf->conn) == CONNECTION_BAD){
			_log(LOG_NOTICE, "Noticed a postgres connection loss. Trying to reconnect ...\n");
			//try to reinitiate the database connection
			PQreset(conf->conn);
		}
		
		goto out;
	}
	if ((nrows = PQntuples(res)) != 1) {
		//DEBUG fprintf(stderr, "rowcount = %ld; not ok\n", nrows);
		goto out;
	}
	if (PQnfields(res) != 1) {
		//DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}
	if ((v = PQgetvalue(res, 0, 0)) == NULL) {
		goto out;
	}	
	value = (v) ? strdup(v) : NULL;


out:

	PQclear(res);

	*phash = value;
	return BACKEND_DEFER;
}

/*
 * Return T/F if user is superuser
 */

int be_pg_superuser(void *handle, const char *username)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *v = NULL;
	long nrows;
	int issuper = BACKEND_DEFER;
	PGresult *res = NULL;

	_log(LOG_DEBUG, "SUPERUSER: %s", username);

	if (!conf || !conf->superquery || !username || !*username)
		return BACKEND_DEFER;

	//query for postgres $1 instead of % s
	const char *values[1] = {username};
	int lengths[1] = {strlen(username)};
	int binary[1] = {0};

	res = PQexecParams(conf->conn, conf->superquery, 1, NULL, values, lengths, binary, 0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		fprintf(stderr, "%s\n", PQresultErrorMessage(res));
		issuper = BACKEND_ERROR;
		//try to reset connection if failing because of database connection lost
		if(PQstatus(conf->conn) == CONNECTION_BAD){
			_log(LOG_NOTICE, "Noticed a postgres connection loss. Trying to reconnect ...\n");
			//try to reinitiate the database connection
			PQreset(conf->conn);
		}

		goto out;
	}
	if ((nrows = PQntuples(res)) != 1) {
		goto out;
	}
	if (PQnfields(res) != 1) {
		//DEBUG fprintf(stderr, "numfields not ok\n");
		goto out;
	}
	if ((v = PQgetvalue(res, 0, 0)) == NULL) {
		goto out;
	}
	issuper = (atoi(v)) ? BACKEND_ALLOW : BACKEND_DEFER;


out:
	_log(LOG_DEBUG, "user is %d", issuper);

	PQclear(res);

	return (issuper);
}

/*
 * Check ACL. username is the name of the connected user attempting to access
 * topic is the topic user is trying to access (may contain wildcards) acc is
 * desired type of access: read/write for subscriptions (READ) (1) for
 * publish (WRITE) (2)
 *
 * SELECT topic FROM table WHERE username = '%s' AND (acc & %d)
 * may user SUB or PUB topic?
 *
 * SELECT topic FROM table WHERE username = '%s'
 * ignore ACC
 */

int be_pg_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct pg_backend *conf = (struct pg_backend *)handle;
	char *v = NULL;
	int match = BACKEND_DEFER;
	bool bf;
	PGresult *res = NULL;

	_log(LOG_DEBUG, "USERNAME: %s, TOPIC: %s, acc: %d", username, topic, acc);


	if (!conf || !conf->aclquery)
		return BACKEND_DEFER;

	const int buflen = 11;
	//10 for 2^32 + 1
	char accbuffer[buflen];
	snprintf(accbuffer, buflen, "%d", acc);

	const char *values[2] = {username, accbuffer};
	int lengths[2] = {strlen(username), buflen};

	res = PQexecParams(conf->conn, conf->aclquery, 2, NULL, values, lengths, NULL, 0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		fprintf(stderr, "%s\n", PQresultErrorMessage(res));
		match = BACKEND_ERROR;

		//try to reset connection if failing because of database connection lost
		if(PQstatus(conf->conn) == CONNECTION_BAD){
			_log(LOG_NOTICE, "Noticed a postgres connection loss. Trying to reconnect ...\n");
			//try to reinitiate the database connection
			PQreset(conf->conn);
		}

		goto out;
	}
	if (PQnfields(res) != 1) {
		fprintf(stderr, "numfields not ok\n");
		goto out;
	}
	int rec_count = PQntuples(res);
	int row = 0;
	for (row = 0; row < rec_count; row++) {
		if ((v = PQgetvalue(res, row, 0)) != NULL) {

			/*
			 * Check mosquitto_match_topic. If true, if true, set
			 * match and break out of loop.
			 */

			char *expanded;

			t_expand(clientid, username, v, &expanded);
			if (expanded && *expanded) {
				mosquitto_topic_matches_sub(expanded, topic, &bf);
				if (bf) match = BACKEND_ALLOW;
				_log(LOG_DEBUG, "  postgres: topic_matches(%s, %s) == %d",
				     expanded, v, bf);

				free(expanded);
			}
		}
		if (match != BACKEND_DEFER) {
			break;
		}
	}

out:

	PQclear(res);

	return (match);
}

/*
 * addKeyValue - Adds key-value pair to index-linked 'dictionary'.
 *
 * - Keys, values and 'dictionary' are zero terminated
 * - 'Dictionary' must have MAX_KEYS + 1 elements
 * - 'Dictionary' must be initialized with zeros
 *
 * Prototype:
 * ----------
 * int addKeyValue(char **keywords, char **values, char *key, char *value,
 *                 const int MAX_KEYS)
 *
 * Arguments:
 * ----------
 * 		keywords: Array of strings, zero terminated
 * 		  values: Array of strings, zero terminated
 * 		     key: Char array, zero terminated
 * 		   value: Char array, zero terminated
 * 		MAX_KEYS: Number of key-value entries the dictionary can hold
 *
 * Returns:
 * --------
 *    n > 0: Number of keys in dictionary
 *    n < 0: Error
 *       -1: Dictionary is not initialized
 *       -2: Dictionary is not initialized properly or unexpectedly full
 *
 * Example:
 * --------
 *    char **keys = NULL, **values = NULL;
 *    const uint8_t MAX_KEYS = 1;
 *    keys = (char **) calloc(MAX_KEYS + 1, sizeof(char *));
 *    values = (char **) calloc(MAX_KEYS + 1, sizeof(char *));
 *    int n = addKeyValue(keys, values, "foo", "bar", MAX_KEYS);
 *    for (int i = 0; (i < n) && (n > 0); i++)
 *    {
 *       printf("key=%s value=%s\n", keys[i], values[i]);
 *    }
 *    free(keys);
 *    free(values);
 */
int addKeyValue(char **keywords, char **values, char *key, char *value,
		const int MAX_KEYS)
{
	int n = 0;

	// Check if dictionary is initialized
	if ((keywords == NULL) || (values == NULL))
	{
		return -1;
	}

	// Get length of dictionary
	while (keywords[n] != NULL)
	{
		n++;
	}

	// Abort if dictionary is full
	if (n == MAX_KEYS)
	{
		return n;
	}

	// Check for dictionary end
	if ((keywords[n] != NULL) || (values[n] != NULL)){
		// Dictionary wasn't initialized properly or it is the
		// dictionaries end. --> Abort
		return -2;
	}

	// Add pair to dictionary and zero-terminate it
	keywords[n] = key;
	values[n] = value;
	n++;

	// In case of not zero-terminated dictionary
	keywords[n] = 0;
	values[n] = 0;

	return n;
}

#endif /* BE_POSTGRES */
