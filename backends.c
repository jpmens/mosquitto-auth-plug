#include <stdio.h>
#include <stdlib.h>
#include "backends.h"
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
