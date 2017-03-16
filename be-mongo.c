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


struct mongo_backend {
	mongoc_client_t *client;
	char *database;
	char *users_coll;
	char *topics_coll;
	char *password_loc;
	char *topic_loc;
	char *topicId_loc;
	char *superuser_loc;
	char *user_embedded_topics_prop;
};

bool be_mongo_check_acl_topics_array(const bson_iter_t *topics, const char *req_topic);
bool be_mongo_check_acl_topics_map(const bson_iter_t *topics, const char *req_topic, int req_access);
mongoc_uri_t *be_mongo_new_uri_from_options();

void *be_mongo_init()
{
	struct mongo_backend *conf;
	char *database, *users_coll, *topics_coll, *password_loc, *topic_loc;
	char *topicId_loc, *superuser_loc, *user_embedded_topics_prop;

	conf = (struct mongo_backend *)malloc(sizeof(struct mongo_backend));

	if ((database = p_stab("mongo_database")) == NULL) {
		conf->database = "mqGate";
	} else {
		conf->database = database;
	}

	if ((users_coll  = p_stab("mongo_collection_users")) == NULL) {
		conf->users_coll = "users";
	} else {
		conf->users_coll = users_coll;
	}

	if ((topics_coll = p_stab("mongo_collection_topics")) == NULL) {
		conf->topics_coll = "topics";
	} else {
		conf->topics_coll = topics_coll;
	}

	if ((password_loc = p_stab("mongo_location_password")) == NULL) {
		conf->password_loc = "password";
	} else {
		conf->password_loc = password_loc;
	}

	if ((topic_loc = p_stab("mongo_location_topic")) == NULL) {
		conf->topic_loc = "topics";
	} else {
		conf->topic_loc = topic_loc;
	}

	if ((user_embedded_topics_prop = p_stab("mongo_user_embedded_topics_prop")) == NULL) {
		conf->user_embedded_topics_prop = "topics";
	} else {
		conf->user_embedded_topics_prop = user_embedded_topics_prop;
	}

	if ((topicId_loc = p_stab("mongo_location_topicId")) == NULL) {
		conf->topicId_loc = "_id";
	} else {
		conf->topicId_loc = topicId_loc;
	}

	if ((superuser_loc = p_stab("mongo_location_superuser")) == NULL) {
		conf->superuser_loc = "superuser";
	} else {
		conf->superuser_loc = superuser_loc;
	}

	mongoc_init();
	mongoc_uri_t *uri = be_mongo_new_uri_from_options();
	if (!uri) {
		_fatal("MongoDB connection options invalid");
	}
	conf->client = mongoc_client_new_from_uri(uri);
	mongoc_uri_destroy(uri);

	return (conf);
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

char *be_mongo_getuser(void *handle, const char *username, const char *password, int *authenticated)
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

	bson_append_utf8 (&query, "username", -1, username, -1);

	collection = mongoc_client_get_collection (conf->client, conf->database, conf->users_coll);
	cursor = mongoc_collection_find(collection,
							MONGOC_QUERY_NONE,
							0,
							0,
							0,
							&query,
							NULL,  /* Fields, NULL for all. */
							NULL); /* Read Prefs, NULL for default */

	if (!mongoc_cursor_error (cursor, &error) &&
		mongoc_cursor_next (cursor, &doc)) {

		bson_iter_init(&iter, doc);
		if (bson_iter_find(&iter, conf->password_loc)) {
			const char *password_src = bson_iter_utf8(&iter, NULL);
			size_t password_len = strlen(password_src) + 1;
			result = (char *) malloc(password_len);
			memcpy(result, password_src, password_len);
		} else {
			_log(LOG_NOTICE, "[mongo] (%s) missing for user (%s)", conf->password_loc, username);
		}
	}

	if (mongoc_cursor_error (cursor, &error)) {
		fprintf (stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy (&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy (collection);
	return result;
}


void be_mongo_destroy(void *handle)
{
	struct mongo_backend *conf = (struct mongo_backend *)handle;

	if (conf != NULL) {
		/* Free Settings */
		free(conf->database);
		free(conf->users_coll);
		free(conf->topics_coll);
		free(conf->password_loc);
		free(conf->topic_loc);
		free(conf->topicId_loc);
		free(conf->superuser_loc);
		free(conf->user_embedded_topics_prop);

		mongoc_client_destroy(conf->client);
		conf->client = NULL;
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
	bson_append_utf8(&query, "username", -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, handle->database, handle->users_coll);

	cursor = mongoc_collection_find(collection,
							MONGOC_QUERY_NONE,
							0,
							0,
							0,
							&query,
							NULL,
							NULL);

	if (!mongoc_cursor_error (cursor, &error) &&
		mongoc_cursor_next (cursor, &doc)) {
		bson_iter_init(&iter, doc);
		if (bson_iter_find(&iter, handle->superuser_loc)) {
			result = bson_iter_as_bool(&iter) ? 1 : 0;
		}
	}

	if (mongoc_cursor_error (cursor, &error)) {
		fprintf(stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy (&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy (collection);

	return result;
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
	bson_append_utf8(&query, "username", -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, handle->database, handle->users_coll);

	cursor = mongoc_collection_find(collection,
							MONGOC_QUERY_NONE,
							0,
							0,
							0,
							&query,
							NULL,
							NULL);

	if (!mongoc_cursor_error (cursor, &error) && mongoc_cursor_next (cursor, &doc)) {
		// First find any user[handle->topic_loc]
		if (bson_iter_init_find(&iter, doc, handle->topic_loc)) {
			bson_type_t loc_id_type = bson_iter_type(&iter);
			if (loc_id_type == BSON_TYPE_OID) {
				topic_lookup_oid = bson_iter_oid(&iter);
			} else if (loc_id_type == BSON_TYPE_INT32 || loc_id_type == BSON_TYPE_INT64) {
				topic_lookup_int64 = bson_iter_as_int64(&iter);
			} else if (loc_id_type == BSON_TYPE_UTF8) {
				topic_lookup_utf8 = bson_iter_utf8(&iter, NULL);
			}
		}

		// Look through the props from the beginning for user[handle->user_embedded_topics_prop]
		if (bson_iter_init_find(&iter, doc, handle->user_embedded_topics_prop)) {
			bson_type_t embedded_prop_type = bson_iter_type(&iter);
			if (embedded_prop_type == BSON_TYPE_ARRAY) {
				match = be_mongo_check_acl_topics_array(&iter, topic);
			} else if (embedded_prop_type == BSON_TYPE_DOCUMENT) {
				match = be_mongo_check_acl_topics_map(&iter, topic, acc);
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
			bson_append_oid(&query, handle->topicId_loc, -1, topic_lookup_oid);
		} else if (topic_lookup_int64 != 0) {
			bson_append_int64(&query, handle->topicId_loc, -1, topic_lookup_int64);
		} else if (topic_lookup_utf8 != NULL) {
			bson_append_utf8(&query, handle->topicId_loc, -1, topic_lookup_utf8, -1);
		}
		collection = mongoc_client_get_collection(handle->client, handle->database, handle->topics_coll);
		cursor = mongoc_collection_find(collection,
								MONGOC_QUERY_NONE,
								0,
								0,
								0,
								&query,
								NULL,
								NULL);


		if (!mongoc_cursor_error (cursor, &error) && mongoc_cursor_next(cursor, &doc)) {

			bson_iter_init(&iter, doc);
			if (bson_iter_find(&iter, handle->topic_loc)) {
				bson_type_t loc_prop_type = bson_iter_type(&iter);
				if (loc_prop_type == BSON_TYPE_ARRAY) {
					match = be_mongo_check_acl_topics_array(&iter, topic);
				} else if (loc_prop_type == BSON_TYPE_DOCUMENT) {
					match = be_mongo_check_acl_topics_map(&iter, topic, acc);
				}
			} else {
				_log(LOG_NOTICE, "[mongo] ACL check error - no topic list found for user (%s) in collection (%s)", username, handle->topics_coll);
			}
		}

		if ((mongoc_cursor_error (cursor, &error)) && (match != 1)) {
			fprintf (stderr, "Cursor Failure: %s\n", error.message);
		}

		bson_destroy(&query);
		mongoc_cursor_destroy(cursor);
		mongoc_collection_destroy(collection);
	}

	return match;
}

// Check an embedded array of the form [ "public/#", "private/myid/#" ]
bool be_mongo_check_acl_topics_array(const bson_iter_t *topics, const char *req_topic)
{
	bson_iter_t iter;
	bson_iter_recurse(topics, &iter);

	while (bson_iter_next(&iter)) {
		const char *permitted_topic = bson_iter_utf8(&iter, NULL);
		bool topic_matches = false;

		mosquitto_topic_matches_sub(permitted_topic, req_topic, &topic_matches);
		if (topic_matches) {
			return true;
		}
	}
	return false;
}

// Check an embedded document of the form { "article/#": "r", "article/+/comments": "rw", "ballotbox": "w" }
bool be_mongo_check_acl_topics_map(const bson_iter_t *topics, const char *req_topic, int req_access)
{
	bson_iter_t iter;
	bson_iter_recurse(topics, &iter);
	bool granted = false;

	// Loop through mapped topics, allowing for the fact that a two different ACLs may have complementary permissions.
	while (bson_iter_next(&iter) && !granted) {
		const char *permitted_topic = bson_iter_key(&iter);
		bool topic_matches = false;

		mosquitto_topic_matches_sub(permitted_topic, req_topic, &topic_matches);
		if (topic_matches) {
			bson_type_t val_type = bson_iter_type(&iter);
			if (val_type == BSON_TYPE_UTF8) {
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
	return granted;
}

#endif /* BE_MONGO */
