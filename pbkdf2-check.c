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
#include <string.h>
#include <openssl/evp.h>
#include "base64.h"

#define SEPARATOR       "$"
#define TRUE	(1)
#define FALSE	(0)


/*
 * Split PBKDF2$... string into their components. The caller must free()
 * the strings.
 */

static int detoken(char *pbkstr, char **sha, int *iter, char **salt, char **key)
{
	char *p, *s, *save;
	int rc = 1;

	save = s = strdup(pbkstr);

#if defined(SUPPORT_DJANGO_HASHERS)
{
	if ((p = strsep(&s, "_")) == NULL)
		goto out;
	if (strcmp(p, "pbkdf2") != 0)
		goto out;
}
#else
{
	if ((p = strsep(&s, SEPARATOR)) == NULL)
		goto out;
	if (strcmp(p, "PBKDF2") != 0)
		goto out;
}
#endif

	if ((p = strsep(&s, SEPARATOR)) == NULL)
		goto out;
	*sha = strdup(p);

	if ((p = strsep(&s, SEPARATOR)) == NULL)
		goto out;
	*iter = atoi(p);

	if ((p = strsep(&s, SEPARATOR)) == NULL)
		goto out;
	*salt = strdup(p);

	if ((p = strsep(&s, SEPARATOR)) == NULL)
		goto out;
	*key = strdup(p);

	rc = 0;

     out:
	free(save);
	return rc;
}

int pbkdf2_check(char *password, char *hash)
{
	char *sha, *salt, *h_pw;
	int iterations, saltlen, blen;
	char *b64, *keybuf;
	unsigned char *out;
	int match = FALSE;
	const EVP_MD *evpmd;
	int keylen, rc;

	if (detoken(hash, &sha, &iterations, &salt, &h_pw) != 0)
		return match;

	/* Determine key length by decoding base64 */
	if ((keybuf = malloc(strlen(h_pw) + 1)) == NULL) {
		fprintf(stderr, "Out of memory\n");
		return FALSE;
	}
	keylen = base64_decode(h_pw, keybuf);
	if (keylen < 1) {
		free(keybuf);
		return (FALSE);
	}
	free(keybuf);

	if ((out = malloc(keylen)) == NULL) {
		fprintf(stderr, "Cannot allocate out; out of memory\n");
		return (FALSE);
	}

#ifdef RAW_SALT
	char *rawSalt;

	if ((rawSalt = malloc(strlen(salt) + 1)) == NULL) {
		fprintf(stderr, "Out of memory\n");
		return FALSE;
	}

	saltlen = base64_decode(salt, rawSalt);
	if (saltlen < 1) {
		return (FALSE);
	}

	free(salt);
	salt = rawSalt;
	rawSalt = NULL;
#else
	saltlen = strlen((char *)salt);
#endif

#ifdef PWDEBUG
	fprintf(stderr, "sha        =[%s]\n", sha);
	fprintf(stderr, "iterations =%d\n", iterations);
	fprintf(stderr, "salt       =[%s]\n", salt);
	fprintf(stderr, "salt len   =[%d]\n", saltlen);
	fprintf(stderr, "h_pw       =[%s]\n", h_pw);
	fprintf(stderr, "kenlen     =[%d]\n", keylen);
#endif


	evpmd = EVP_sha256();
	if (strcmp(sha, "sha1") == 0) {
		evpmd = EVP_sha1();
	} else if (strcmp(sha, "sha512") == 0) {
		evpmd = EVP_sha512();
	}

	rc = PKCS5_PBKDF2_HMAC(password, strlen(password),
		(unsigned char *)salt, saltlen,
		iterations,
		evpmd, keylen, out);
	if (rc != 1) {
		goto out;
	}

	blen = base64_encode(out, keylen, &b64);
	if (blen > 0) {
		int i, diff = 0, hlen = strlen(h_pw);
#ifdef PWDEBUG
		fprintf(stderr, "HMAC b64   =[%s]\n", b64);
#endif

		/* "manual" strcmp() to ensure constant time */
		for (i = 0; (i < blen) && (i < hlen); i++) {
			diff |= h_pw[i] ^ b64[i];
		}

		match = diff == 0;
		if (hlen != blen)
			match = 0;

		free(b64);
	}

  out:
	free(sha);
	free(salt);
	free(h_pw);
	free(out);

	return match;
}

#if TEST
int main()
{
	char password[] = "password";
	char pbkstr[] = "PBKDF2$sha1$98$XaIs9vQgmLujKHZG4/B3dNTbeP2PyaVKySTirZznBrE=$2DX/HZDTojVbfgAIdozBi6CihjWP1+akYnh/h9uQfIVl6pLoAiwJe1ey2WW2BnT+";
	int match;

	printf("Checking password [%s] for %s\n", password, pbkstr);

	match = pbkdf2_check(password, pbkstr);
	printf("match == %d\n", match);
	return match;
}
#endif
