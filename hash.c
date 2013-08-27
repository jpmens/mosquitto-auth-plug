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
 * FIXME: must bail-out on ENOMEM
 */

void p_add(char *name, char *value)
{
	struct my_opts *mo;

	mo = (struct my_opts *)malloc(sizeof(struct my_opts));
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

char *p_stab(char *key)
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
