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

#include <stdio.h>
#include <stdlib.h>
#include "hash.h"
#include "uthash.h"

static struct my_opts {
	char *name;		// key
	char *value;
	UT_hash_handle hh;
} *globalopts = NULL;

/*
 * Add a key/value pair to the hash.
 */

void p_add(char *name, char *value)
{
	struct my_opts *mo;

	mo = (struct my_opts *)malloc(sizeof(struct my_opts));
	if (mo == NULL) {
		return;
	}
	mo->name = strdup(name);
	mo->value = strdup(value);

	HASH_ADD_KEYPTR(hh, globalopts, mo->name, strlen(mo->name), mo);
}

/*
 * Recursively free the hash
 */

void p_freeall()
{
	struct my_opts *mo, *tmp;

	HASH_ITER(hh, globalopts, mo, tmp) {
		if (mo->value)
			free(mo->value);
		if (mo->name)
			free(mo->name);
		HASH_DEL(globalopts, mo);
	}
}

/*
 * Return value for key or NULL.
 * Returned value MUST NOT be freed by caller.
 */

char *p_stab(const char *key)
{
	struct my_opts *mo;

	HASH_FIND_STR(globalopts, key, mo);

	return ( (mo) ? mo->value : NULL);
}

void p_dump()
{
	struct my_opts *mo, *tmp;

	HASH_ITER(hh, globalopts, mo, tmp) {
		printf("-> %s=%s\n", mo->name, mo->value);
	}
}
