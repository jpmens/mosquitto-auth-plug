/*
 * Copyright (c) 2013 Jan-Piet Mens <jp@mens.de>
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

#ifdef BE_CDB

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <cdb.h>
#include <mosquitto.h>
#include "backends.h"
#include "be-cdb.h"
#include "log.h"
#include "hash.h"

void *be_cdb_init()
{
	struct cdb_backend *conf;
	char *cdbname;
	int fd;

	if ((cdbname = p_stab("cdbname")) == NULL)
		_fatal("Mandatory parameter `cdbname' missing");

	if ((fd = open(cdbname, O_RDONLY)) == -1) {
		perror(cdbname);
		return (NULL);
	}

	conf = malloc(sizeof(struct cdb_backend));
	if (conf == NULL) {
		return (NULL);
	}

	conf->cdbname	= strdup(cdbname);
	conf->cdb	= (struct cdb *)malloc(sizeof(struct cdb));

	if (conf->cdb == NULL) {
		free(conf->cdbname);
		free(conf);
		return (NULL);
	}

	cdb_init(conf->cdb, fd);

	return (conf);
}

void be_cdb_destroy(void *handle)
{
	struct cdb_backend *conf = (struct cdb_backend *)handle;

	if (conf) {
		cdb_free(conf->cdb);
		free(conf->cdbname);
		free(conf);
	}
}

int be_cdb_getuser(void *handle, const char *username, const char *password, char **phash)
{
	struct cdb_backend *conf = (struct cdb_backend *)handle;
	char *k, *v = NULL;
	unsigned klen;

	if (!conf || !username || !*username)
		return (FALSE);

	k = (char *)username;
	klen = strlen(k);

	if (cdb_find(conf->cdb, k, klen) > 0) {
		int vpos = cdb_datapos(conf->cdb);
		int vlen = cdb_datalen(conf->cdb);

		if ((v = malloc(vlen + 1)) != NULL) {
			cdb_read(conf->cdb, v, vlen, vpos);
			v[vlen] = 0;
		}
	}

	*phash = v;
	return BACKEND_DEFER;
}

/*
 * Check access to topic for username. Look values for a key "acl:username"
 * and use mosquitto_topic_matches_sub() to validate the topic.
 */

int be_cdb_access(void *handle, const char *username, char *topic)
{
	struct cdb_backend *conf = (struct cdb_backend *)handle;
	char *k;
	unsigned klen;
	int found = 0;
	struct cdb_find cdbf;
	bool bf;

	if (!conf || !username || !topic)
		return (0);

	if ((k = malloc(strlen(username) + strlen("acl:") + 2)) == NULL)
		return (0);
	sprintf(k, "acl:%s", username);
	klen = strlen(k);

	cdb_findinit(&cdbf, conf->cdb, k, klen);
	while ((cdb_findnext(&cdbf) > 0) && (!found)) {
		unsigned vpos = cdb_datapos(conf->cdb);
		unsigned vlen = cdb_datalen(conf->cdb);
		char *val;

		val = malloc(vlen);
		cdb_read(conf->cdb, val, vlen, vpos);

		mosquitto_topic_matches_sub(val, topic, &bf);
		found |= bf;

		free(val);
	}

	free(k);

	return (found > 0);
}

int be_cdb_superuser(void *handle, const char *username)
{
	return BACKEND_DEFER;
}

int be_cdb_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	/* FIXME: implement. Currently TRUE */

	return BACKEND_ALLOW;
}
#endif /* BE_CDB */
