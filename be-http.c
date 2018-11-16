/*
 * Copyright (c) 2013 Jan-Piet Mens <jp@mens.de> wendal <wendal1985()gmai.com>
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
#include "envs.h"
#include <curl/curl.h>

static int get_string_envs(CURL *curl, const char *required_env, char *querystring)
{
	char *data = NULL;
	char *escaped_key = NULL;
	char *escaped_val = NULL;
	char *env_string = NULL;

	char *params_key[MAXPARAMSNUM];
	char *env_names[MAXPARAMSNUM];
	char *env_value[MAXPARAMSNUM];
	int i, num = 0;

	//_log(LOG_DEBUG, "sys_envs=%s", sys_envs);

	env_string = (char *)malloc( strlen(required_env) + 20);
	if (env_string == NULL) {
		_fatal("ENOMEM");
		return (-1);
	}
	sprintf(env_string, "%s", required_env);

	//_log(LOG_DEBUG, "env_string=%s", env_string);

	num = get_sys_envs(env_string, ",", "=", params_key, env_names, env_value);
	//sprintf(querystring, "");
	for( i = 0; i < num; i++ ){
		escaped_key = curl_easy_escape(curl, params_key[i], 0);
		escaped_val = curl_easy_escape(curl, env_value[i], 0);

		//_log(LOG_DEBUG, "key=%s", params_key[i]);
		//_log(LOG_DEBUG, "escaped_key=%s", escaped_key);
		//_log(LOG_DEBUG, "escaped_val=%s", escaped_envvalue);

		data = (char *)malloc(strlen(escaped_key) + strlen(escaped_val) + 1);
		if ( data == NULL ) {
			_fatal("ENOMEM");
			return (-1);
		}
		sprintf(data, "%s=%s&", escaped_key, escaped_val);
		if ( i == 0 ) {
			sprintf(querystring, "%s", data);
		} else {
			strcat(querystring, data);
		}
		free(data);
	}

	if (escaped_key) free(escaped_key);
	if (escaped_val) free(escaped_val);
	free(env_string);
	return (num);
}

static int http_post(void *handle, char *uri, const char *clientid, const char *username, const char *password, const char *topic, int acc, int method)
{
	struct http_backend *conf = (struct http_backend *)handle;
	CURL *curl;
	struct curl_slist *headerlist=NULL;
	int re;
	int respCode = 0;
	int ok = BACKEND_DEFER;
	char *url;
	char *data;

	if (username == NULL) {
		return BACKEND_DEFER;
	}

	clientid = (clientid && *clientid) ? clientid : "";
	password = (password && *password) ? password : "";
	topic    = (topic && *topic) ? topic : "";

	if ((curl = curl_easy_init()) == NULL) {
		_fatal("create curl_easy_handle fails");
		return BACKEND_ERROR;
	}
	if (conf->hostheader != NULL)
		headerlist = curl_slist_append(headerlist, conf->hostheader);
	headerlist = curl_slist_append(headerlist, "Expect:");

	if(conf->basic_auth !=NULL){
		headerlist = curl_slist_append(headerlist, conf->basic_auth);
	}

	//_log(LOG_NOTICE, "u=%s p=%s t=%s acc=%d", username, password, topic, acc);

	url = (char *)malloc(strlen(conf->hostname) + strlen(uri) + 20);
	if (url == NULL) {
		_fatal("ENOMEM");
		return BACKEND_ERROR;
	}

	// enable the https
	if (strcmp(conf->with_tls, "true") == 0){
		sprintf(url, "https://%s:%d%s", conf->hostname, conf->port, uri);
	}else{
		sprintf(url, "http://%s:%d%s", conf->hostname, conf->port, uri);
	}

	char* escaped_username = curl_easy_escape(curl, username, 0);
	char* escaped_password = curl_easy_escape(curl, password, 0);
	char* escaped_topic = curl_easy_escape(curl, topic, 0);
	char* escaped_clientid = curl_easy_escape(curl, clientid, 0);

	char string_acc[20];
	snprintf(string_acc, 20, "%d", acc);

	char *string_envs = (char *)malloc(MAXPARAMSLEN);
	if (string_envs == NULL) {
		_fatal("ENOMEM");
		return BACKEND_ERROR;
	}

	memset(string_envs, 0, MAXPARAMSLEN);

	//get the sys_env from here
	int env_num = 0;
	if ( method == METHOD_GETUSER && conf->getuser_envs != NULL ){
		env_num = get_string_envs(curl, conf->getuser_envs, string_envs);
	}else if ( method == METHOD_SUPERUSER && conf->superuser_envs != NULL ){
		env_num = get_string_envs(curl, conf->superuser_envs, string_envs);
	} else if ( method == METHOD_ACLCHECK && conf->aclcheck_envs != NULL ){
		env_num = get_string_envs(curl, conf->aclcheck_envs, string_envs);
	}
	if( env_num == -1 ){
		return BACKEND_ERROR;
	}
	//---- over ----

	data = (char *)malloc(strlen(string_envs) + strlen(escaped_username) + strlen(escaped_password) + strlen(escaped_topic) + strlen(string_acc) + strlen(escaped_clientid) + 50);
	if (data == NULL) {
		_fatal("ENOMEM");
		return BACKEND_ERROR;
	}
	sprintf(data, "%susername=%s&password=%s&topic=%s&acc=%s&clientid=%s",
		string_envs,
		escaped_username,
		escaped_password,
		escaped_topic,
		string_acc,
		clientid);

	_log(LOG_DEBUG, "url=%s", url);
	_log(LOG_DEBUG, "data=%s", data);
	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	curl_easy_setopt(curl, CURLOPT_USERNAME, username);
	curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

	re = curl_easy_perform(curl);
	if (re == CURLE_OK) {
		re = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respCode);
		if (re == CURLE_OK && respCode >= 200 && respCode < 300) {
			ok = BACKEND_ALLOW;
		} else if (re == CURLE_OK && respCode >= 500) {
			ok = BACKEND_ERROR;
		} else {
			//_log(LOG_NOTICE, "http auth fail re=%d respCode=%d", re, respCode);
		}
	} else {
		_log(LOG_DEBUG, "http req fail url=%s re=%s", url, curl_easy_strerror(re));
		ok = BACKEND_ERROR;
	}

	curl_easy_cleanup(curl);
	curl_slist_free_all (headerlist);
	free(url);
	free(data);
	free(string_envs);
	free(escaped_username);
	free(escaped_password);
	free(escaped_topic);
	free(escaped_clientid);
	return (ok);
}

void *be_http_init()
{
	struct http_backend *conf;
	char *hostname;
	char *getuser_uri;
	char *superuser_uri;
	char *aclcheck_uri;

	if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
		_fatal("init curl fail");
		return (NULL);
	}

	if ((hostname = p_stab("http_ip")) == NULL && (hostname = p_stab("http_hostname")) == NULL) {
		_fatal("Mandatory parameter: one of either `http_ip' or `http_hostname' required");
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
	conf->hostname = hostname;
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

	conf->getuser_envs = p_stab("http_getuser_params");
	conf->superuser_envs = p_stab("http_superuser_params");
	conf->aclcheck_envs = p_stab("http_aclcheck_params");
	if(p_stab("http_basic_auth_key")!= NULL){
		conf->basic_auth = (char *)malloc( strlen("Authorization: Basic %s") + strlen(p_stab("http_basic_auth_key")));
		sprintf(conf->basic_auth, "Authorization: Basic %s",p_stab("http_basic_auth_key"));
	} else {
		conf->basic_auth = NULL;
	}

	if (p_stab("http_with_tls") != NULL) {
		conf->with_tls = p_stab("http_with_tls");
	} else {
		conf->with_tls = "false";
	}

	conf->retry_count = p_stab("http_retry_count") == NULL ? 3 : atoi(p_stab("http_retry_count"));

	_log(LOG_DEBUG, "with_tls=%s", conf->with_tls);
	_log(LOG_DEBUG, "getuser_uri=%s", getuser_uri);
	_log(LOG_DEBUG, "superuser_uri=%s", superuser_uri);
	_log(LOG_DEBUG, "aclcheck_uri=%s", aclcheck_uri);

	_log(LOG_DEBUG, "getuser_params=%s", conf->getuser_envs);
	_log(LOG_DEBUG, "superuser_params=%s", conf->superuser_envs);
	_log(LOG_DEBUG, "aclcheck_params=%s", conf->aclcheck_envs);
	_log(LOG_DEBUG, "retry_count=%d", conf->retry_count);

	return (conf);
};
void be_http_destroy(void *handle)
{
	struct http_backend *conf = (struct http_backend *)handle;

	if (conf) {
		curl_global_cleanup();
		free(conf);
	}
};

int be_http_getuser(void *handle, const char *username, const char *password, char **phash) {
	struct http_backend *conf = (struct http_backend *)handle;
	int re, try;
	if (username == NULL) {
		return BACKEND_DEFER;
	}

	re = BACKEND_ERROR;
	try = 0;

	while (re == BACKEND_ERROR && try <= conf->retry_count) {
		try++;
		re = http_post(handle, conf->getuser_uri, NULL, username, password, NULL, -1, METHOD_GETUSER);
	}
	return re;
};

int be_http_superuser(void *handle, const char *username)
{
	struct http_backend *conf = (struct http_backend *)handle;
	int re, try;

	re = BACKEND_ERROR;
	try = 0;
	while (re == BACKEND_ERROR && try <= conf->retry_count) {
		try++;
		re = http_post(handle, conf->superuser_uri, NULL, username, NULL, NULL, -1, METHOD_SUPERUSER);
	}
	return re;
};

int be_http_aclcheck(void *handle, const char *clientid, const char *username, const char *topic, int acc)
{
	struct http_backend *conf = (struct http_backend *)handle;
	int re, try;

	re = BACKEND_ERROR;
	try = 0;

	while (re == BACKEND_ERROR && try <= conf->retry_count) {
		try++;
		re = http_post(conf, conf->aclcheck_uri, clientid, username, NULL, topic, acc, METHOD_ACLCHECK);
	}
	return re;
};
#endif /* BE_HTTP */
