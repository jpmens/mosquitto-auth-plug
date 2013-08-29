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
#include "log.h"
#include "hash.h"
#include <hiredis/hiredis.h>

struct redis_backend {
	redisContext *redis;
	char *host;
	int port;
};

void *be_redis_init()
{
	struct redis_backend *conf;
	struct timeval timeout = { 2, 500000 }; // 2.5 seconds
	char *host, *p;

	_log(LOG_DEBUG, "}}}} Redis");

	if ((host = p_stab("redis_host")) == NULL)
		host = "localhost";
	if ((p = p_stab("redis_port")) == NULL)
		p = "6379";

	conf = (struct redis_backend *)malloc(sizeof(struct redis_backend));
	if (conf == NULL)
		_fatal("Out of memory");

	conf->host = strdup(host);
	conf->port = atoi(p);

	conf->redis = redisConnectWithTimeout(conf->host, conf->port, timeout);
	if (conf->redis->err) {
		_log(LOG_NOTICE, "Redis connection error: %s for %s:%d\n",
			conf->redis->errstr, conf->host, conf->port);
		free(conf->host);
		free(conf);
		return (NULL);
	}

	return (conf);
}

void be_redis_destroy(void *handle)
{
	struct redis_backend *conf = (struct redis_backend *)handle;

	if (conf != NULL) {
		redisFree(conf->redis);
		conf->redis = NULL;
	}
}

char *be_redis_getuser(void *handle, const char *username)
{
	struct redis_backend *conf = (struct redis_backend *)handle;
	redisReply *r;
	char *pwhash = NULL;

	if (conf == NULL || conf->redis == NULL || username == NULL)
		return (NULL);

	r = redisCommand(conf->redis, "GET %b", username, strlen(username));
	if (r == NULL || conf->redis->err != REDIS_OK) {
		/* FIXME: reconnect */
		return (NULL);
	}

	if (r->type == REDIS_REPLY_STRING) {
		pwhash = strdup(r->str);
	}
	freeReplyObject(r);

	return (pwhash);
}

int be_redis_superuser(void *conf, const char *username)
{
	return 0;
}

int be_redis_aclcheck(void *conf, const char *username, const char *topic, int acc)
{
	/* FIXME: implement. Currently TRUE */

	return 1;
}
