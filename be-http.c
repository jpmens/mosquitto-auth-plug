/*
 * Copyright (c) 2013 Jan-Piet Mens <jpmens()gmail.com> wendal <wendal1985()gmai.com>
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

#ifdef BE_HTTP
#include "backends.h"
#include "be-http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"
#include "log.h"
#include <curl/curl.h>

static int http_post(void *handle, char *uri, const char *username,const char *password, const char *topic, int acc)
{
	struct http_backend *conf = (struct http_backend *)handle;
	struct curl_slist *headerlist=NULL;
	int re;
	int respCode = 0;
	int ok = FALSE;
	char *url;
	char *data;

	if (username == NULL) {
		return (FALSE);
	}
	if (password == NULL)
		password = "";

	if (topic == NULL)
		topic = "";
	
	CURL *curl = curl_easy_init();
	if (NULL == curl) {
		_fatal("create easy_handle fail");
		return (FALSE);
	}
	if (conf->hostheader != NULL)
		curl_slist_append(headerlist, conf->hostheader);
	curl_slist_append(headerlist, "Expect:");
		
	//_log(LOG_NOTICE, "u=%s p=%s t=%s acc=%d", username, password, topic, acc);
	
	url = (char *)malloc(1024);
	data = (char *)malloc(1024);
	sprintf(data, "username=%s&password=%s&topic=%s&acc=%d", 
			curl_easy_escape(curl, username, 0),
			curl_easy_escape(curl, password, 0),
			curl_easy_escape(curl, topic, 0),
			acc);
	
	sprintf(url, "http://%s:%d%s", conf->ip, conf->port, strdup(uri));

	_log(LOG_DEBUG, "url=%s", url);
	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
	
	re = curl_easy_perform(curl);
	if (re == CURLE_OK) {
		re = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);
		if (re == CURLE_OK && respCode == 200) {
			ok = TRUE;
		} else {
			//_log(LOG_NOTICE, "http auth fail re=%d respCode=%d", re, respCode);
		}
	} else {
		_log(LOG_DEBUG, "http req fail url=%s re=%s", url, curl_easy_strerror(re));
	}
	
	curl_easy_cleanup(curl);
	curl_slist_free_all (headerlist);
	free(url);
	free(data);
	return (ok);
}


void *be_http_init() {
	struct http_backend *conf;
	char *ip;
	char *getuser_uri;
	char *superuser_uri;
	char *aclcheck_uri;
	
	if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
		_fatal("init curl fail");
		return (NULL);
	}
	
	if ((ip = p_stab("http_ip")) == NULL) {
		_fatal("Mandatory parameter `http_ip' missing");
		return (NULL);
	}
	if ((getuser_uri = p_stab("http_getuser_uri")) == NULL) {
		_fatal("Mandatory parameter `http_getuser_uri' missing");
		return (NULL);
	}
	if ((superuser_uri = p_stab("http_superuser_uri")) == NULL) {
		_fatal("Mandatory parameter `http_superuser_uri' missing");
		return (NULL);
	}
	if ((aclcheck_uri = p_stab("http_aclcheck_uri")) == NULL) {
		_fatal("Mandatory parameter `http_aclcheck_uri' missing");
		return (NULL);
	}
	conf = (struct http_backend *)malloc(sizeof(struct http_backend));
	conf->ip = ip;
	conf->port = p_stab("http_port") == NULL ? 80 : atoi(p_stab("http_port"));
	if (p_stab("http_hostname") != NULL) {
		conf->hostheader = (char *)malloc(128);
		sprintf(conf->hostheader, "Host: %s", p_stab("http_hostname"));
	} else {
		conf->hostheader = NULL;
	}
	conf->getuser_uri = getuser_uri;
	conf->superuser_uri = superuser_uri;
	conf->aclcheck_uri = aclcheck_uri;
	
	_log(LOG_DEBUG, "getuser_uri=%s", getuser_uri);
	_log(LOG_DEBUG, "superuser_uri=%s", superuser_uri);
	_log(LOG_DEBUG, "aclcheck_uri=%s", aclcheck_uri);
	
	return conf;
};
void be_http_destroy(void *handle){
	struct http_backend *conf = (struct http_backend *)handle;
	if (conf) {
		curl_global_cleanup();
		free(conf);
	}
};
char *be_http_getuser(void *handle, const char *username, const char *password, int *authenticated) {
	struct http_backend *conf = (struct http_backend *)handle;
	int re;
	if (username == NULL) {
		return NULL;
	}
	re = http_post(handle, conf->getuser_uri, username, password, NULL, -1);
	if (re == 1) {
		*authenticated = 1;
	}
	return NULL;
};
int be_http_superuser(void *handle, const char *username){
	struct http_backend *conf = (struct http_backend *)handle;
	return http_post(handle, conf->superuser_uri, username, NULL, NULL, -1);
};
int be_http_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct http_backend *conf = (struct http_backend *)handle;

	/* FIXME: support clientid */
	return http_post(conf, conf->aclcheck_uri, username, NULL, topic, acc);
};
#endif /* BE_HTTP */
