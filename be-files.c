/*
 * Copyright (c) 2017, Diehl Connectivity Solutions GmbH. All rights
 * reserved.
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

#ifdef BE_FILES

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include <mosquitto_broker.h>
#include "log.h"
#include "hash.h"
#include "backends.h"
#include "be-files.h"

#if (LIBMOSQUITTO_MAJOR > 1) || ((LIBMOSQUITTO_MAJOR == 1) && (LIBMOSQUITTO_MINOR >= 4))
#define LOG(lvl, fmt, ...) mosquitto_log_printf(lvl, fmt, ##__VA_ARGS__)
#else
#define LOG(lvl, fmt, ...) _log(lvl, fmt, ##__VA_ARGS__)
#endif

typedef struct dllist_entry {
	struct dllist_entry *next;
	struct dllist_entry *prev;
} dllist_entry;

typedef struct dllist {
	dllist_entry head;
} dllist;

static inline void dllist_entry_init(dllist_entry * thiz)
{
	thiz->next = thiz->prev = thiz;
}

static inline void dllist_init(dllist * thiz)
{
	dllist_entry_init(&thiz->head);
}

static inline void dllist_entry_remove(dllist_entry * thiz)
{
	thiz->next->prev = thiz->prev;
	thiz->prev->next = thiz->next;
	dllist_entry_init(thiz);
}

static inline void dllist_entry_insert(dllist_entry * thiz, dllist_entry * other)
{
	dllist_entry *tmpPrev = thiz;
	dllist_entry *tmpNext = thiz->next;
	dllist_entry_remove(other);
	tmpNext->prev = other;
	other->next = tmpNext;
	other->prev = tmpPrev;
	tmpPrev->next = other;
}

static inline void dllist_push_back(dllist * thiz, dllist_entry * entry)
{
	dllist_entry_insert(thiz->head.prev, entry);
}

static inline bool dllist_empty(const dllist * thiz)
{
	return (thiz->head.next == &thiz->head);
}

#if !(defined(offsetof))
#if defined(__GNUC__) && __GNUC__ >= 4
#define offsetof(type, member) __builtin_offsetof(type, member)
#else
#define offsetof(type, member) ((size_t)&((type*)NULL)->member)
#endif
#endif

#define dllist_entry_element(ptr, type, member) \
   (type*)((char*)(void*)ptr + offsetof(type, member))

#define dllist_for_each_element(thiz, iter, member) \
   for((iter) = dllist_entry_element((thiz)->head.next, typeof(*(iter)), member); \
       &(iter)->member != &(thiz)->head; \
       (iter) = dllist_entry_element((iter)->member.next, typeof(*(iter)), member))

typedef struct acl_entry {
	dllist_entry entry;
	int access;
	char *topic;
} acl_entry;

typedef struct pwd_entry {
	dllist_entry entry;
	char *username;
	char *password;
	dllist acl_entries;
} pwd_entry;

typedef struct be_files {
	dllist passwords;
	bool acl_checks;
} be_files;


static dllist acl_entries = {{&acl_entries.head, &acl_entries.head}};

static pwd_entry *find_pwd(be_files * conf, const char *username)
{
	pwd_entry *iter;

	dllist_for_each_element(&conf->passwords, iter, entry) {
		if (strcmp(username, iter->username) == 0)
			return iter;
	}
	return NULL;
}

static bool read_passwords(be_files * conf, FILE * file)
{
	char line[512];
	char *pos;
	char *username;
	char *password;
	pwd_entry *entry;

	while (fgets(line, sizeof(line), file) != NULL) {
		if (line[0] == '#' || line[0] == '\r' || line[0] == '\n')
			continue;
		pos = strchr(line, ':');
		if (pos) {
			*pos = '\0';
			++pos;
			username = line;
			password = pos;
			pos = strchr(password, '\r');
			if (pos != NULL)
				*pos = '\0';
			pos = strchr(password, '\n');
			if (pos != NULL)
				*pos = '\0';
			entry = (pwd_entry *) malloc(sizeof(pwd_entry));
			dllist_entry_init(&entry->entry);
			dllist_init(&entry->acl_entries);
			entry->username = strdup(username);
			entry->password = strdup(password);
			dllist_push_back(&conf->passwords, &entry->entry);
		}
	}
	return true;
}

/*
 * (read|write)?[ \t]+[^ \t]+
 */
static acl_entry *read_acl_line(const char *line)
{
	acl_entry *entry;
	const char *pos;
	const char *i;
	size_t len;
	int access;

	if (strncmp("read", line, 4) == 0 && (line[4] == ' ' || line[4] == '\t')) {
		access = MOSQ_ACL_READ;
		pos = &line[5];
		for (; (*pos == ' ' || *pos == '\t') && *pos != '\0'; ++pos);
	} else if (strncmp("write", line, 5) == 0 && (line[5] == ' ' || line[5] == '\t')) {
		access = MOSQ_ACL_WRITE;
		pos = &line[6];
		for (; (*pos == ' ' || *pos == '\t') && *pos != '\0'; ++pos);
	} else {
		access = MOSQ_ACL_READ | MOSQ_ACL_WRITE;
		pos = &line[0];
	}
	for (len = 0, i = pos; *i != '\0' && *i != ' ' && *i != '\t' && *i != '\r' && *i != '\n'; ++len, ++i);
	entry = (acl_entry *) malloc(sizeof(acl_entry));
	dllist_entry_init(&entry->entry);
	entry->access = access;
	entry->topic = (char *)calloc(len + 1, sizeof(char));
	strncpy(entry->topic, pos, len);
	entry->topic[len] = '\0';
	return entry;
}

/*
 * (user)[ \t]+([^ \t]+) (topic|pattern)[ \t]+(read|write)?[ \t][^ \t]+
 */

static bool read_acl(be_files * conf, FILE * file)
{
	char line[512];
	char *pos;
	char *username;
	acl_entry *entry;
	pwd_entry *pwd = NULL;

	while (fgets(line, sizeof(line), file) != NULL) {
		if (line[0] == '#')
			continue;
		for (pos = line; *pos != '\0' && (*pos == ' ' || *pos == '\t'); ++pos);
		if (*pos == '\r' || *pos == '\n')
			continue;
		if (strncmp("user", pos, 4) == 0) {
			for (pos = pos + 4; (*pos == ' ' || *pos == '\t') && *pos != '\0'; ++pos);
			username = pos;
			pos = strchr(username, '\r');
			if (pos != NULL)
				*pos = '\0';
			pos = strchr(username, '\n');
			if (pos != NULL)
				*pos = '\0';
			pwd = find_pwd(conf, username);
			if (pwd == NULL) {
				pwd = (pwd_entry *) malloc(sizeof(pwd_entry));
				dllist_entry_init(&pwd->entry);
				dllist_init(&pwd->acl_entries);
				pwd->username = strdup(username);
				pwd->password = NULL;
				dllist_push_back(&conf->passwords, &pwd->entry);
			}
		} else if (strncmp("topic", pos, 5) == 0) {
			for (pos = pos + 5; (*pos == ' ' || *pos == '\t') && *pos != '\0'; ++pos);
			entry = read_acl_line(pos);
			if (pwd != NULL)
				dllist_push_back(&pwd->acl_entries, &entry->entry);
			else
				dllist_push_back(&acl_entries, &entry->entry);
		} else if (strncmp("pattern", pos, 7) == 0) {
			for (pos = pos + 7; (*pos == ' ' || *pos == '\t') && *pos != '\0'; ++pos);
			entry = read_acl_line(pos);
			dllist_push_back(&acl_entries, &entry->entry);
		} else {
			LOG(MOSQ_LOG_WARNING, "failed to parse line: %s", line);
		}
	}
	return true;
}

void *be_files_init()
{
	const char *path;
	FILE *file;
	be_files *const conf = (be_files *) malloc(sizeof(be_files));

	dllist_init(&conf->passwords);
	conf->acl_checks = false;

	path = p_stab("password_file");
	file = (path == NULL) ? NULL : fopen(path, "r");
	if (path != NULL && file == NULL) {
		LOG(MOSQ_LOG_ERR, "failed to open password file: %s", path);
		be_files_destroy(conf);
		return NULL;
	}
	if (file != NULL) {
		read_passwords(conf, file);
		fclose(file);
	}
	path = p_stab("acl_file");
	conf->acl_checks = path != NULL;
	file = (path == NULL) ? NULL : fopen(path, "r");
	if (path != NULL && file == NULL) {
		LOG(MOSQ_LOG_ERR, "failed to open ACL file: %s", path);
		be_files_destroy(conf);
		return NULL;
	}
	if (file != NULL) {
		read_acl(conf, file);
		fclose(file);
	}
	return conf;
}

static void free_acl(dllist * list)
{
	acl_entry *acl;

	while (!dllist_empty(list)) {
		acl = dllist_entry_element(list->head.next, acl_entry, entry);
		dllist_entry_remove(&acl->entry);
		if (acl->topic)
			free(acl->topic);
		free(acl);
	}
}

void be_files_destroy(void *handle)
{
	be_files *const conf = (be_files *) handle;
	pwd_entry *pwd;

	while (!dllist_empty(&conf->passwords)) {
		pwd = dllist_entry_element(conf->passwords.head.next, pwd_entry, entry);
		dllist_entry_remove(&pwd->entry);
		if (pwd->username)
			free(pwd->username);
		if (pwd->password)
			free(pwd->password);
		free_acl(&pwd->acl_entries);
		free(pwd);
	}

	free_acl(&acl_entries);

	free(conf);
}

int be_files_getuser(void *handle,
		            const char *username,
		            const char *password,
		            char **phash)
{
	be_files *const conf = (be_files *) handle;
	pwd_entry *entry = find_pwd(conf, username);

	*phash = (entry == NULL || entry->password == NULL) ? NULL : strdup(entry->password);
	return BACKEND_DEFER;
}

int be_files_superuser(void *handle, const char *username)
{
	return 0;
}

static int do_aclcheck(dllist * acl_list,
		           const char *clientid,
		           const char *username,
		           const char *topic,
		           int access)
{
	char buf[512];
	bool ret;
	acl_entry *acl;
	const char *t;
	const char *si;
	char *di;

	dllist_for_each_element(acl_list, acl, entry) {
		for (si = acl->topic, di = buf; *si != '\0';) {
			switch (*si) {
			case '%':
				++si;
				switch (*si) {
				case 'c':
					++si;
					for (t = clientid; *t != '\0'; ++di, ++t)
						*di = *t;
					break;
				case 'u':
					++si;
					for (t = username; *t != '\0'; ++di, ++t)
						*di = *t;
					break;
				default:
					*di++ = *si;
					break;
				}
				break;
			default:
				*di++ = *si++;
				break;
			}
		}
		*di = '\0';
		if (mosquitto_topic_matches_sub(buf, topic, &ret) != MOSQ_ERR_SUCCESS) {
			LOG(MOSQ_LOG_ERR, "invalid topic '%s'", buf);
		} else if (ret && (access & acl->access) != 0) {
			return BACKEND_ALLOW;
		}
	}
	return BACKEND_DEFER;
}

int be_files_aclcheck(void *handle,
		          const char *clientid,
		          const char *username,
		          const char *topic,
		          int access)
{
	be_files *const conf = (be_files *) handle;
	pwd_entry *pwd = find_pwd(conf, username);
	int ret = 0;

	if (!conf->acl_checks)
		return BACKEND_ALLOW;

	if (pwd != NULL) {
		ret = do_aclcheck(&pwd->acl_entries, clientid, username, topic, access);
	}

	if (ret == BACKEND_DEFER)
		ret = do_aclcheck(&acl_entries, clientid, username, topic, access);
	return ret;
}

int be_files_aclpatterns_available(void)
{
	return !dllist_empty(&acl_entries);
}

int be_files_aclpatterns_check(const char *clientid,
			           const char *username,
			           const char *topic,
			           int access)
{
	return do_aclcheck(&acl_entries, clientid, username, topic, access);
}

#endif	/* // BE_FILES */
