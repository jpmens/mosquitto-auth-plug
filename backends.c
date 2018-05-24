/*
 * Copyright (c) 2014 Jan-Piet Mens <jp@mens.de>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer. 2. Redistributions
 * in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution. 3. Neither the name of mosquitto
 * nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
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
#include "backends.h"

/*
 * Search through `in' for tokens %c (clientid) and %u (username); build a
 * new malloc'd string at `res' with those tokens interpolated into it.
 */

void t_expand(const char *clientid, const char *username, const char *in, char **res)
{
	const char *s;
	char *work, *wp;
	int c_specials = 0, u_specials = 0, len;
	const char *ct, *ut;

	ct = (clientid) ? clientid : "";
	ut = (username) ? username : "";

	for (s = in; s && *s; s++) {
		if (*s == '%' && (*(s + 1) == 'c'))
			c_specials++;
		if (*s == '%' && (*(s + 1) == 'u'))
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
		if (*s == '%' && (*(s + 1) == 'c')) {
			*--wp = 0;
			strcpy(wp, ct);
			wp += strlen(ct);
			s++;
		}
		if (*s == '%' && (*(s + 1) == 'u')) {
			*--wp = 0;
			strcpy(wp, ut);
			wp += strlen(ut);
			s++;
		}
	}
	*wp = 0;

	*res = work;
}
