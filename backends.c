#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "backends.h"

/*
 * Search through `in' for tokens %c (clientid) and %u (username);
 * build a new malloc'd string at `res' with those tokens interpolated
 * into it.
 */

void t_expand(const char *clientid, const char *username, char *in, char **res)
{
        char *s, *work, *wp;
        int c_specials = 0, u_specials = 0, len;
        const char *ct, *ut;

        ct = (clientid) ? clientid : "";
        ut = (username) ? username : "";

        for (s = in; s && *s; s++) {
                if (*s == '%' && (*(s+1) == 'c'))
                        c_specials++;
                if (*s == '%' && (*(s+1) == 'u'))
                        u_specials++;
        }
        len = strlen(in) + 1;
        len += strlen(clientid) * c_specials;
        len += strlen(username) * u_specials;

        if ((work = malloc(len)) == NULL) {
                *res = NULL;
                return;
        }
        for (s = in, wp = work; s && *s; s++) {
                *wp++ = *s;
                if (*s == '%' && (*(s+1) == 'c')) {
                        *--wp = 0;
                        strcpy(wp, ct);
                        wp += strlen(ct);
                        s++;
                }
                if (*s == '%' && (*(s+1) == 'u')) {
                        *--wp = 0;
                        strcpy(wp, ut);
                        wp += strlen(ut);
                        s++;
                }
        }
        *wp = 0;

        *res = work;
}
