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
#include "backends.h"

#if 0
#include "uthash.h"

static struct backends {
	char *name;		// key
	void *conf;		/* Opaque config blob */
	UT_hash_handle hh;
} *backends = NULL;

void be_add(char *name)
{
	struct backends *b;

	b = (struct backends *)malloc(sizeof(struct backends));
	b->name = strdup(name);

	HASH_ADD_KEYPTR(hh, backends, b->name, strlen(b->name), b);
}

/*
 * Recursively free the hash
 */

void be_freeall()
{
	struct backends *b, *tmp;

	HASH_ITER(hh, backends, b, tmp) {
		if (b->name)
			free(b->name);
		HASH_DEL(backends, b);
	}
}

/*
 * Return value for key or NULL.
 * Returned value MUST NOT be freed by caller.
 */

void *be_stab(char *key)
{
	struct backends *b;

	HASH_FIND_STR(backends, key, b);

	return ( (b) ? b : NULL);
}

void be_dump()
{
	struct backends *b, *tmp;

	HASH_ITER(hh, backends, b, tmp) {
		printf("BACKEND-> %s\n", b->name);
	}
}

#endif /* 0 */
