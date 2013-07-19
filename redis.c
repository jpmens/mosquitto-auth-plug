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

#include <hiredis/hiredis.h>

redisContext *redis_init(char *host, int port)
{
	redisContext *c;
	struct timeval timeout = { 2, 500000 }; // 2.5 seconds

	c = redisConnectWithTimeout(host, port, timeout);
	if (c->err) {
		fprintf(stderr, "Connection error: %s\n", c->errstr);
		return (NULL);
	}

	return (c);
}

void redis_destroy(redisContext *redis)
{
	if (redis != NULL) {
		redisFree(redis);
		redis = NULL;
	}
}

/* 
 * Set *io to 1 on Redis error, 0 otherwise
 */

char *redis_getuser(redisContext *redis, char *usernameprefix, const char *username, int *io)
{
	redisReply *r;
	char *pwhash = NULL;
	char *up = usernameprefix;

	*io = 0;

	if (redis == NULL || username == NULL)
		return (NULL);

	if (!up || !*up)
		up = "";

	r = redisCommand(redis, "GET %s%b", up, username, strlen(username));
	if (r == NULL || redis->err != REDIS_OK) {
		*io = 1;
		return (NULL);
	}

	if (r->type == REDIS_REPLY_STRING) {
		pwhash = strdup(r->str);
	}
	freeReplyObject(r);

	return (pwhash);
}

#if TEST
int main(int argc, char **argv)
{
	char *p;
	static redisContext *redis = NULL;

	redis = redis_init("localhost", 6379);

	if ((p = redis_getuser(redis, argv[1])) != NULL) {
		printf("%s\n", p);
		free(p);
	}

	redis_destroy(redis);
}
#endif
