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

    if ((host = p_stab("mongo_host")) == NULL){
        host = "localhost";
	}
	
    if ((p = p_stab("mongo_port")) == NULL){
        p = "27017";
	}
	
    if ((database = p_stab("mongo_database")) == NULL){
        conf->database = "mqGate";
    }else{
	conf->database = database;
    }
	
    if ((users_coll  = p_stab("mongo_collection_users")) == NULL){
        conf->users_coll = "users";
    }else{
	conf->users_coll = users_coll;
    }
	
    if ((topics_coll = p_stab("mongo_collection_topics")) == NULL){
        conf->topics_coll = "topics";
    }else{
	conf->topics_coll = topics_coll;
    }
	
    if ((password_loc = p_stab("mongo_location_password")) == NULL){
        conf->password_loc = "password";
    }else{
	conf->password_loc = password_loc;
    }
	
    if ((topic_loc = p_stab("mongo_location_topic")) == NULL){
        conf->topic_loc = "topics";
    }else{
        conf->topic_loc = topic_loc;
    }
	
    if ((topicId_loc = p_stab("mongo_location_topicId")) == NULL){
        conf->topicId_loc = "_id";
    }else{
        conf->topicId_loc = topicId_loc;
    }
	
    if ((superuser_loc = p_stab("mongo_location_superuser")) == NULL){
        conf->superuser_loc = "superuser";
    }else{
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
   char *result = NULL;

   bson_init (&query);

   bson_append_utf8 (&query, "username", -1, username, -1);

   collection = mongoc_client_get_collection (conf->client, conf->database, conf->users_coll);
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
         bson_iter_find(&iter, conf->password_loc);

         char *src = (char *)bson_iter_utf8(&iter, NULL);
		 size_t tmp = strlen(src) + 1;
		 result = (char *) malloc(tmp);
		 memset(result, 0, tmp);
		 memcpy(result, src, tmp);
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
	int result;

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

	while (!mongoc_cursor_error (cursor, &error) &&
			mongoc_cursor_more (cursor)) {
		if (mongoc_cursor_next (cursor, &doc)) {
				bson_iter_init(&iter, doc);
				bson_iter_find(&iter, handle->superuser_loc);

				result = (int64_t) bson_iter_as_int64(&iter);

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

	collection = mongoc_client_get_collection(handle->client, handle->database, handle->users_coll);

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
				bson_iter_find(&iter, handle->topic_loc);

				int64_t topId = (int64_t) bson_iter_as_int64(&iter);//, NULL);

				bson_destroy(&query);
				mongoc_cursor_destroy(cursor);
				mongoc_collection_destroy(collection);

				bson_init(&query);
				bson_append_int64(&query, handle->topicId_loc, -1, topId);
				collection = mongoc_client_get_collection(handle->client, handle->database, handle->topics_coll);
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
			bson_iter_find(&iter, handle->topic_loc);
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
	
	if ( (mongoc_cursor_error (cursor, &error)) && (match != 1) ) {
			fprintf (stderr, "Cursor Failure: %s\n", error.message);
	}
	
	bson_destroy(&query);
	mongoc_cursor_destroy (cursor);
	mongoc_collection_destroy(collection);
	return match;

}
#endif /* BE_MONGO */
