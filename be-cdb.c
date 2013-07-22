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
#include <unistd.h>
#include <fcntl.h>
#include <cdb.h>
#include <mosquitto.h>
#include "be-cdb.h"

struct backend *be_cdb_init(char *cdbname)
{
	struct backend *be;
	int fd;

	if ((fd = open(cdbname, O_RDONLY)) == -1) {
		perror(cdbname);
		return (NULL);
	}

	be = malloc(sizeof(struct backend));
	if (be == NULL) {
		return (NULL);
	}

	be->cdbname	= strdup(cdbname);
	be->cdb		= (struct cdb *)malloc(sizeof(struct cdb));

	if (be->cdb == NULL) {
		free(be->cdbname);
		free(be);
		return (NULL);
	}

	cdb_init(be->cdb, fd);

	return (be);
}

void be_cdb_destroy(struct backend *be)
{
	if (be) {
		cdb_free(be->cdb);
		free(be->cdbname);
	}
}

char *be_cdb_getuser(struct backend *be, const char *username)
{
	char *k, *v = NULL;
	unsigned klen;

	if (!be || !username || !*username)
		return (NULL);

	k = (char *)username;
	klen = strlen(k);

	if (cdb_find(be->cdb, k, klen) > 0) {
		int vpos = cdb_datapos(be->cdb);
		int vlen = cdb_datalen(be->cdb);

		if ((v = malloc(vlen)) != NULL) {
			cdb_read(be->cdb, v, vlen, vpos);
		}
	}

	return (v);
}

/*
 * Check access to topic for username. Look values for a key "acl:username"
 * and use mosquitto_topic_matches_sub() to validate the topic.
 */

int be_cdb_access(struct backend *be, const char *username, char *topic)
{
	char *k;
	unsigned klen;
	int found = 0;
	struct cdb_find cdbf;
	bool bf;

	if (!be || !username || !topic)
		return (0);

	if ((k = malloc(strlen(username) + strlen("acl:") + 2)) == NULL)
		return (0);
	sprintf(k, "acl:%s", username);
	klen = strlen(k);

	cdb_findinit(&cdbf, be->cdb, k, klen);
	while ((cdb_findnext(&cdbf) > 0) && (!found)) {
		unsigned vpos = cdb_datapos(be->cdb);
		unsigned vlen = cdb_datalen(be->cdb);
		char *val;

		val = malloc(vlen);
		cdb_read(be->cdb, val, vlen, vpos);

		mosquitto_topic_matches_sub(val, topic, &bf);
		found |= bf;

		free(val);
	}

	free(k);

	return (found > 0);
}

#if TEST
int main(int argc, char **argv)
{
	char *p;
	static struct backend *be = NULL;
	char *username = argv[1];
	static char **topic, *topiclist[] = {
		"/location/a",
		"/location/uno/anton",
		"Devices/arduino/1",
		NULL };

	be = be_cdb_init("pwdb.cdb");

	if ((p = be_cdb_getuser(be, username)) != NULL) {
		printf("%s\n", p);
		free(p);
	}

	for (topic = topiclist; topic && *topic; topic++) {
		if (be_cdb_access(be, username, *topic) == 1) {
			printf("ALLOW  %s\n", *topic);
		} else {
			printf("DENIED %s\n", *topic);
		}
	}

	be_cdb_destroy(be);
	return (0);
}
#endif
