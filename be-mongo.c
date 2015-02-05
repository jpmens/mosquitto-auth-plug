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
#include <mongoc.h>
#include "hash.h"

struct mongo_backend {
    mongoc_client_t *client;
    char *host;
    int port;
};

void *be_mongo_init()
{
    struct mongo_backend *conf;
    char *host, *p;

    if ((host = p_stab("mongo_host")) == NULL)
        host = "localhost";
    if ((p = p_stab("mongo_port")) == NULL)
        p = "27017";

     char uristr[128] = {0};
     strcpy(uristr, "mongodb://");
     strcat(uristr, host);
     strcat(uristr, ":");
     strcat(uristr, p);
     printf("mongo: [%s]\n", uristr);
   //"mongodb://127.0.0.1:27017/";

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
   const char *collection_name = "passport";
   bson_t query;
   char *str = NULL;
   char *result = malloc(33);
   memset(result, 0, 33);

   bson_init (&query);

   bson_append_utf8 (&query, "username", -1, username, -1);

   collection = mongoc_client_get_collection (conf->client, "cas", collection_name);
   cursor = mongoc_collection_find (collection,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    0,
                                    0,
                                    &query,
                                    NULL,  /* Fields, NULL for all. */
                                    NULL); /* Read Prefs, NULL for default */


   bson_iter_t iter;
   while (!mongoc_cursor_error (cursor, &error) &&
          mongoc_cursor_more (cursor)) {
      if (mongoc_cursor_next (cursor, &doc)) {

         bson_iter_init(&iter, doc);
         bson_iter_find(&iter, "pwd");
         //fprintf (stdout, "%s\n", bson_iter_utf8(&iter, NULL));
         str = bson_as_json (doc, NULL);
         //fprintf (stdout, "%s\n", str);
         bson_free (str);
         char *src = (char *)bson_iter_utf8(&iter, NULL);
         memcpy(result, src, strlen(src));
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
	return 0;
}

int be_mongo_aclcheck(void *conf, const char *clientid, const char *username, const char *topic, int acc)
{
	/* FIXME: implement. Currently TRUE */

	return 1;
}
#endif /* BE_MONGO */
