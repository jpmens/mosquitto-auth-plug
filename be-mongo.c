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
#include "mongoParam.h"

struct mongo_backend {
    mongoc_client_t *client;
    char *host;
    int port;
};

void *be_mongo_init()
{
    struct mongo_backend *conf;
    char *host, *p, *user, *password, *authSource;

    if ((host = p_stab("mongo_host")) == NULL)
        host = "localhost";
    if ((p = p_stab("mongo_port")) == NULL)
        p = "27017";
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
     printf("mongo: [%s]\n", uristr);

    conf = (struct mongo_backend *)malloc(sizeof(struct mongo_backend));
   mongoc_init ();

   conf->client = mongoc_client_new (uristr);

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
   char *result;

   bson_init (&query);

   bson_append_utf8 (&query, "username", -1, username, -1);

   collection = mongoc_client_get_collection (conf->client, dbName, colName);
   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    0,
                                    0,
                                    &query,
                                    NULL,  /* Fields, NULL for all. */
                                    NULL); /* Read Prefs, NULL for default */


   while (!mongoc_cursor_error (cursor, &error) &&
          mongoc_cursor_more (cursor)) {
      if (mongoc_cursor_next (cursor, &doc)) {

         bson_iter_init(&iter, doc);
         bson_iter_find(&iter, passLoc);

         char *src = (char *)bson_iter_utf8(&iter, NULL);
		 size_t tmp = strlen(src);
		 result = (char *) malloc(tmp);
		 memset(result, 0, tmp);
         memcpy(result, src, tmp);
      }
   }

   if (mongoc_cursor_error (cursor, &error)) {
      fprintf (stderr, "Cursor Failure: %s\n", error.message);
      return result;
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
	int result;

	bson_t query;
	bson_iter_t iter;
	bson_init (&query);
	bson_append_utf8(&query, "username", -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, dbName, colName);

	cursor = mongoc_collection_find(collection,
									MONGOC_QUERY_NONE,
									0,
									0,
									0,
									&query,
									NULL,
									NULL);

	while (!mongoc_cursor_error (cursor, &error) &&
			mongoc_cursor_more (cursor)) {
		if (mongoc_cursor_next (cursor, &doc)) {
				bson_iter_init(&iter, doc);
				bson_iter_find(&iter, superUser);

				result = (int64_t) bson_iter_as_int64(&iter);

				//_log(LOG_NOTICE, "SUPERUSER: %d", result);

		}
	}

	if (mongoc_cursor_error (cursor, &error)) {
      	fprintf (stderr, "Cursor Failure: %s\n", error.message);
    	  return result;
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

	bool check = false;
	int match = 0, foundFlag = 0;

	bson_t query;

	bson_init(&query);
	bson_append_utf8(&query, "username", -1, username, -1);

	collection = mongoc_client_get_collection(handle->client, dbName, colName);

	cursor = mongoc_collection_find(collection,
									MONGOC_QUERY_NONE,
									0,
									0,
									0,
									&query,
									NULL,
									NULL);

	while (!mongoc_cursor_error (cursor, &error) &&
			mongoc_cursor_more (cursor)) {
		if (foundFlag == 0 && mongoc_cursor_next (cursor, &doc)) {
				bson_iter_init(&iter, doc);
				bson_iter_find(&iter, topicLoc);

				int64_t topId = (int64_t) bson_iter_as_int64(&iter);//, NULL);

				bson_destroy(&query);
				mongoc_cursor_destroy(cursor);
				mongoc_collection_destroy(collection);

				bson_init(&query);
				bson_append_int64(&query, topicID, -1, topId);
				collection = mongoc_client_get_collection(handle->client, dbName, topicLoc);
				cursor = mongoc_collection_find(collection,
												MONGOC_QUERY_NONE,
												0,
												0,
												0,
												&query,
												NULL,
												NULL);
				foundFlag = 1;
		}
		if (foundFlag == 1 && mongoc_cursor_next(cursor, &doc)) {

			bson_iter_init(&iter, doc);
			bson_iter_find(&iter, topicLoc);
			uint32_t len;
			const uint8_t *arr;
			bson_iter_array(&iter, &len, &arr);
			bson_t b;



			if (bson_init_static(&b, arr, len))	{
				bson_iter_init(&iter, &b);
				while (bson_iter_next(&iter)) {

					char *str = bson_iter_dup_utf8(&iter, &len);

					mosquitto_topic_matches_sub(str, topic, &check);
					if (check) {
							match = 1;
							bson_free(str);
							break;
					}
					bson_free(str);
				}
			}

		}

	}


	if (mongoc_cursor_error (cursor, &error)) {
			fprintf (stderr, "Cursor Failure: %s\n", error.message);
			return 0;
	}


	bson_destroy(&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy(collection);






	return match;
}
#endif /* BE_MONGO */
