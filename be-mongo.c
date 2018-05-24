/*
 * Original Work:
 *      Copyright (c) 2014 zhangxj <zxj_2007_happy@163.com>
 * Modified and Adapted for Mosquitto_auth_plug by:
 *         Gopalakrishna Palem < http://gk.palem.in/ >
 */

#ifdef BE_MONGO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>
#include <mongoc.h>
#include "hash.h"
#include "log.h"
#include "backends.h"


struct mongo_backend {
	mongoc_client_t *client;
	char *database;
	char *user_coll;
	char *topiclist_coll;
	char *user_username_prop;
	char *user_password_prop;
	char *user_superuser_prop;
	char *user_topics_prop;
	char *user_topiclist_fk_prop;
	char *topiclist_key_prop;
	char *topiclist_topics_prop;
};

const char *be_mongo_get_option(const char *opt_name, const char *dep_opt_name, const char *default_val);
mongoc_uri_t *be_mongo_new_uri_from_options();
bool be_mongo_check_acl_topics_array(const bson_iter_t *topics, const char *req_topic, const char *clientid, const char *username);
bool be_mongo_check_acl_topics_map(const bson_iter_t *topics, const char *req_topic, int req_access, const char *clientid, const char *username);

void *be_mongo_init()
{
	struct mongo_backend *conf;
	conf = (struct mongo_backend *)malloc(sizeof(struct mongo_backend));

	conf->database = strdup(be_mongo_get_option("mongo_database", NULL, "mqGate"));
	conf->user_coll = strdup(be_mongo_get_option("mongo_user_coll", "mongo_collection_users", "users"));
	conf->topiclist_coll = strdup(be_mongo_get_option("mongo_topiclist_coll", "mongo_collection_topics", "topics"));
	conf->user_username_prop = strdup(be_mongo_get_option("mongo_user_username_prop", NULL, "username"));
	conf->user_password_prop = strdup(be_mongo_get_option("mongo_user_password_prop", "mongo_location_password", "password"));
	conf->user_superuser_prop = strdup(be_mongo_get_option("mongo_user_superuser_prop", "mongo_location_superuser", "superuser"));
	conf->user_topics_prop = strdup(be_mongo_get_option("mongo_user_topics_prop", NULL, "topics"));
	conf->user_topiclist_fk_prop = strdup(be_mongo_get_option("mongo_user_topiclist_fk_prop", "mongo_location_topic", "topics"));
	conf->topiclist_key_prop = strdup(be_mongo_get_option("mongo_topiclist_key_prop", "mongo_location_superuser", "_id"));
	conf->topiclist_topics_prop = strdup(be_mongo_get_option("mongo_topiclist_topics_prop", "mongo_location_topic", "topics"));

	mongoc_init();
	mongoc_uri_t *uri = be_mongo_new_uri_from_options();
	if (!uri) {
		_fatal("MongoDB connection options invalid");
	}
	conf->client = mongoc_client_new_from_uri(uri);
	mongoc_uri_destroy(uri);

	return (conf);
}

// Get an option value via p_stab, fallback to a deprecated option (log a warning if present), fallback to a default
const char *be_mongo_get_option(const char *opt_name, const char *dep_opt_name, const char *default_val) {
	const char *value;
	if ((value = p_stab(opt_name)) == NULL) {
		if (dep_opt_name == NULL || (value = p_stab(dep_opt_name)) == NULL) {
			value = default_val;
		} else {
			_log(LOG_NOTICE, "[mongo] Warning: Option '%s' is deprecated. Use '%s' instead.", opt_name, dep_opt_name);
		}
	}
	return value;
}

// Return a new mongoc_uri_t which should be freed with mongoc_uri_destroy
mongoc_uri_t *be_mongo_new_uri_from_options() {
	const char *uristr = p_stab("mongo_uri");
	const char *host = p_stab("mongo_host");
	const char *port = p_stab("mongo_port");
	const char *user = p_stab("mongo_user");
	const char *password = p_stab("mongo_password");
	const char *authSource = p_stab("mongo_authSource");
	mongoc_uri_t *uri;

	if (uristr) {
		// URI string trumps everything else. Let the driver parse it.
		uri = mongoc_uri_new(uristr);
	} else if (host || port || user || password || authSource) {
		// Using legacy piecemeal connection options. Assemble the URI.
		uri = mongoc_uri_new_for_host_port(
			host ? host : "localhost",
			(port && atoi(port)) ? atoi(port) : 27017
		);

		// NB: Option setters require mongo-c-driver >= 1.4.0 (Aug 2016)
		if (user != NULL) {
			mongoc_uri_set_username(uri, user);
			if (password != NULL) {
				mongoc_uri_set_password(uri, password);
			}
		}
		if (authSource != NULL) {
			mongoc_uri_set_auth_source(uri, authSource);
		}
	} else {
		// No connection options given at all, use defaults.
		uri = mongoc_uri_new_for_host_port("localhost", 27017);
	}

	return uri;
}

int be_mongo_getuser(void *handle, const char *username, const char *password, char **phash)
{
	struct mongo_backend *conf = (struct mongo_backend *)handle;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *doc;
	bson_iter_t iter;
	bson_t query;
	char *result = NULL;

	bson_init (&query);

	bson_append_utf8 (&query, conf->user_username_prop, -1, username, -1);

	collection = mongoc_client_get_collection (conf->client, conf->database, conf->user_coll);
	cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

	if (!mongoc_cursor_error (cursor, &error) &&
		mongoc_cursor_next (cursor, &doc)) {

		bson_iter_init(&iter, doc);
		if (bson_iter_find(&iter, conf->user_password_prop)) {
			const char *password_src = bson_iter_utf8(&iter, NULL);
			size_t password_len = strlen(password_src) + 1;
			result = (char *) malloc(password_len);
			memcpy(result, password_src, password_len);
		} else {
			_log(LOG_NOTICE, "[mongo] (%s) missing for user (%s)", conf->user_password_prop, username);
		}
	}

	if (mongoc_cursor_error (cursor, &error)) {
		fprintf (stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy (&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy (collection);

	*phash = result;
	return BACKEND_DEFER;
}


void be_mongo_destroy(void *handle)
{
	struct mongo_backend *conf = (struct mongo_backend *)handle;

	if (conf != NULL) {
		/* Free Settings */
		free(conf->database);
		free(conf->user_coll);
		free(conf->topiclist_coll);
		free(conf->user_username_prop);
		free(conf->user_password_prop);
		free(conf->user_superuser_prop);
		free(conf->user_topics_prop);
		free(conf->user_topiclist_fk_prop);
		free(conf->topiclist_key_prop);
		free(conf->topiclist_topics_prop);

		mongoc_client_destroy(conf->client);
		conf->client = NULL;
		free(conf);
	}
}

int be_mongo_superuser(void *conf, const char *username)
{
	struct mongo_backend *handle = (struct mongo_backend *) conf;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *doc;
	int result = 0;

	bson_t query;
	bson_iter_t iter;
	bson_init (&query);
	bson_append_utf8(&query, handle->user_username_prop, -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, handle->database, handle->user_coll);

	cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

	if (!mongoc_cursor_error (cursor, &error) &&
		mongoc_cursor_next (cursor, &doc)) {
		bson_iter_init(&iter, doc);
		if (bson_iter_find(&iter, handle->user_superuser_prop)) {
			result = bson_iter_as_bool(&iter) ? 1 : 0;
		}
	}

	if (mongoc_cursor_error (cursor, &error)) {
		fprintf(stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy (&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy (collection);

	return (result) ? BACKEND_ALLOW : BACKEND_DEFER;
}

int be_mongo_aclcheck(void *conf, const char *clientid, const char *username, const char *topic, int acc)
{
	struct mongo_backend *handle = (struct mongo_backend *) conf;
	mongoc_collection_t *collection;
	mongoc_cursor_t *cursor;
	bson_error_t error;
	const bson_t *doc;
	bson_iter_t iter;
	int match = 0;
	const bson_oid_t *topic_lookup_oid = NULL;
	const char *topic_lookup_utf8 = NULL;
	int64_t topic_lookup_int64 = 0;

	bson_t query;

	bson_init(&query);
	bson_append_utf8(&query, handle->user_username_prop, -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, handle->database, handle->user_coll);

	cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);

	if (!mongoc_cursor_error (cursor, &error) && mongoc_cursor_next (cursor, &doc)) {
		// First find any user[handle->user_topiclist_fk_prop]
		if (bson_iter_init_find(&iter, doc, handle->user_topiclist_fk_prop)) {
			bson_type_t loc_id_type = bson_iter_type(&iter);
			if (loc_id_type == BSON_TYPE_OID) {
				topic_lookup_oid = bson_iter_oid(&iter);
			} else if (loc_id_type == BSON_TYPE_INT32 || loc_id_type == BSON_TYPE_INT64) {
				topic_lookup_int64 = bson_iter_as_int64(&iter);
			} else if (loc_id_type == BSON_TYPE_UTF8) {
				topic_lookup_utf8 = bson_iter_utf8(&iter, NULL);
			}
		}

		// Look through the props from the beginning for user[handle->user_topics_prop]
		if (bson_iter_init_find(&iter, doc, handle->user_topics_prop)) {
			bson_type_t embedded_prop_type = bson_iter_type(&iter);
			if (embedded_prop_type == BSON_TYPE_ARRAY) {
				match = be_mongo_check_acl_topics_array(&iter, topic, clientid, username);
			} else if (embedded_prop_type == BSON_TYPE_DOCUMENT) {
				match = be_mongo_check_acl_topics_map(&iter, topic, acc, clientid, username);
			}
		}
	}

	if ((mongoc_cursor_error (cursor, &error)) && (match != 1)) {
		fprintf (stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy(&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy(collection);

	if (!match && (topic_lookup_oid != NULL || topic_lookup_int64 != 0 || topic_lookup_utf8 != NULL)) {
		bson_init(&query);
		if (topic_lookup_oid != NULL) {
			bson_append_oid(&query, handle->topiclist_key_prop, -1, topic_lookup_oid);
		} else if (topic_lookup_int64 != 0) {
			bson_append_int64(&query, handle->topiclist_key_prop, -1, topic_lookup_int64);
		} else if (topic_lookup_utf8 != NULL) {
			bson_append_utf8(&query, handle->topiclist_key_prop, -1, topic_lookup_utf8, -1);
		}
		collection = mongoc_client_get_collection(handle->client, handle->database, handle->topiclist_coll);
		cursor = mongoc_collection_find_with_opts(collection, &query, NULL, NULL);


		if (!mongoc_cursor_error (cursor, &error) && mongoc_cursor_next(cursor, &doc)) {

			bson_iter_init(&iter, doc);
			if (bson_iter_find(&iter, handle->topiclist_topics_prop)) {
				bson_type_t loc_prop_type = bson_iter_type(&iter);
				if (loc_prop_type == BSON_TYPE_ARRAY) {
					match = be_mongo_check_acl_topics_array(&iter, topic, clientid, username);
				} else if (loc_prop_type == BSON_TYPE_DOCUMENT) {
					match = be_mongo_check_acl_topics_map(&iter, topic, acc, clientid, username);
				}
			} else {
				_log(LOG_NOTICE, "[mongo] ACL check error - no topic list found for user (%s) in collection (%s)", username, handle->topiclist_coll);
			}
		}

		if ((mongoc_cursor_error (cursor, &error)) && (match != 1)) {
			fprintf (stderr, "Cursor Failure: %s\n", error.message);
		}

		bson_destroy(&query);
		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);
	}

	return (match) ? BACKEND_ALLOW : BACKEND_DEFER;
}

// Check an embedded array of the form [ "public/#", "private/myid/#" ]
bool be_mongo_check_acl_topics_array(const bson_iter_t *topics, const char *req_topic, const char *clientid, const char *username)
{
	bson_iter_t iter;
	bson_iter_recurse(topics, &iter);

	while (bson_iter_next(&iter)) {
		const char *permitted_topic = bson_iter_utf8(&iter, NULL);
		bool topic_matches = false;

		char *expanded;

		t_expand(clientid, username, permitted_topic, &expanded);
		if (expanded && *expanded) {
			mosquitto_topic_matches_sub(expanded, req_topic, &topic_matches);
			free(expanded);

			if (topic_matches) {
				return true;
			}
		}
	}
	return false;
}

// Check an embedded document of the form { "article/#": "r", "article/+/comments": "rw", "ballotbox": "w" }
bool be_mongo_check_acl_topics_map(const bson_iter_t *topics, const char *req_topic, int req_access, const char *clientid, const char *username)
{
	bson_iter_t iter;
	bson_iter_recurse(topics, &iter);
	bool granted = false;

	// Loop through mapped topics, allowing for the fact that a two different ACLs may have complementary permissions.
	while (bson_iter_next(&iter) && !granted) {
		const char *permitted_topic = bson_iter_key(&iter);
		bool topic_matches = false;

		char *expanded;

		t_expand(clientid, username, permitted_topic, &expanded);
		if (expanded && *expanded) {
			mosquitto_topic_matches_sub(expanded, req_topic, &topic_matches);
			free(expanded);

			if (topic_matches) {
				bson_type_t val_type = bson_iter_type(&iter);
				if (val_type == BSON_TYPE_UTF8) {
					// NOTE: can req_access be any other value than 1 or 2?
					// in that case this may not be correct:
					// e.g. req_access == 3 (rw) -> granted = (3 & 1 > 0) == true
					const char *permission = bson_iter_utf8(&iter, NULL);
					if (strcmp(permission, "r") == 0) {
						granted = (req_access & 1) > 0;
					} else if (strcmp(permission, "w") == 0) {
						granted = (req_access & 2) > 0;
					} else if (strcmp(permission, "rw") == 0) {
						granted = true;
					}
				}
			}
		}
	}
	return granted;
}

#endif /* BE_MONGO */
