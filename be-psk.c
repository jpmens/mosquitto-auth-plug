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

#ifdef BE_PSK

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <mosquitto.h>
#include "backends.h"
#include "be-psk.h"
#include "log.h"
#include "hash.h"

#if 0
void *be_psk_init()
{
	struct psk_backend *conf;
	char *p;

	conf = malloc(sizeof(struct psk_backend));
	if (conf == NULL) {
		return (NULL);
	}

	p = p_stab("psk_database");
	if (p == NULL) {
		free(conf);
		_fatal("psk_database must be set");
	}

	conf->database = strdup(p);

	return (conf);
}

void be_psk_destroy(void *handle)
{
	struct psk_backend *conf = (struct psk_backend *)handle;

	if (conf) {
		free(conf);
	}
}

/*
 * Return PSK key string for the username
 */

char *be_psk_getuser(void *handle, const char *username, const char *password, int *authenticated)
{
	struct psk_backend *conf = (struct psk_backend *)handle;

	if (!conf || !username || !*username)
		return (NULL);

	return (NULL);
}

/*
 * Check access to topic for username. Look values for a key "acl:username"
 * and use mosquitto_topic_matches_sub() to validate the topic.
 */

int be_psk_access(void *handle, const char *username, char *topic)
{
	struct psk_backend *conf = (struct psk_backend *)handle;
	int found = 0;

	if (!conf || !username || !topic)
		return (0);

	return (found > 0);
}

int be_psk_superuser(void *handle, const char *username)
{
	return 0;
}

int be_psk_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	/* FIXME: implement. Currently TRUE */

	return 1;
}

#endif

#endif /* BE_PSK */
