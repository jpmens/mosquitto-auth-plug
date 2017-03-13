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
	char *host;
	int port;
	char *database;
	char *users_coll;
	char *topics_coll;
	char *password_loc;
	char *topic_loc;
	char *topicId_loc;
	char *superuser_loc;
};

void *be_mongo_init()
{
	struct mongo_backend *conf;
	char *host, *p, *user, *password, *authSource;
	char *database, *users_coll, *topics_coll, *password_loc, *topic_loc;
	char *topicId_loc, *superuser_loc;

	conf = (struct mongo_backend *)malloc(sizeof(struct mongo_backend));

	if ((host = p_stab("mongo_host")) == NULL) {
		host = "localhost";
	}

	if ((p = p_stab("mongo_port")) == NULL) {
		p = "27017";
	}

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

	user = p_stab("mongo_user");
	password = p_stab("mongo_password");
	authSource = p_stab("mongo_authSource");

	char uristr[128] = {0};
	strcpy(uristr, "mongodb://");
	if (user != NULL) {
		strcat(uristr, user);
		if (password != NULL) {
		   strcat(uristr, ":");
		   strcat(uristr, password);
		}
		strcat(uristr, "@");
	}
	strcat(uristr, host);
	strcat(uristr, ":");
	strcat(uristr, p);
	if (authSource != NULL) {
		strcat(uristr, "?authSource=");
		strcat(uristr, authSource);
	}

	mongoc_init();
	conf->client = mongoc_client_new(uristr);

	if (!conf->client) {
		fprintf (stderr, "Failed to parse URI.\n");
		return NULL;
	}

	return (conf);
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
	bson_type_t loc_id_type;

	bool check = false;
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

		bson_iter_init(&iter, doc);
		if (bson_iter_find(&iter, handle->topic_loc)) {
			loc_id_type = bson_iter_type(&iter);
			if (loc_id_type == BSON_TYPE_OID) {
				topic_lookup_oid = bson_iter_oid(&iter);
			} else if (loc_id_type == BSON_TYPE_INT32 || loc_id_type == BSON_TYPE_INT64) {
				topic_lookup_int64 = bson_iter_as_int64(&iter);
			} else if (loc_id_type == BSON_TYPE_UTF8) {
				topic_lookup_utf8 = bson_iter_utf8(&iter, NULL);
			}
		}
	}

	if ((mongoc_cursor_error (cursor, &error)) && (match != 1)) {
		fprintf (stderr, "Cursor Failure: %s\n", error.message);
	}

	bson_destroy(&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy(collection);

	if (topic_lookup_oid != NULL || topic_lookup_int64 != 0 || topic_lookup_utf8 != NULL) {
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
				uint32_t len;
				const uint8_t *arr;
				bson_iter_array(&iter, &len, &arr);
				bson_t b;

				if (bson_init_static(&b, arr, len)) {
					bson_iter_init(&iter, &b);
					while (bson_iter_next(&iter)) {
						const char *str = bson_iter_utf8(&iter, NULL);
						mosquitto_topic_matches_sub(str, topic, &check);
						if (check) {
							match = 1;
							break;
						}
					}
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
	} else {
		_log(LOG_NOTICE, "[mongo] ACL check error - user (%s) does not have a topic list", username);
	}

	return match;
}
#endif /* BE_MONGO */
