#include "skynet.h"
#include "mongoc.h"
#include "bson.h"

#define MONGO_BSON bson_t
#define MONGO_ERROR bson_error_t
#define MONGO_CURSOR mongoc_cursor_t
#define MONGO_CLIENT mongoc_client_t
#define MONGO_COLLECTION mongoc_collection_t

struct mongo_event {
    char * ev_opts;
    char * ev_query;
    char * ev_value;
    char * ev_dbname;
    char * ev_collection;
};

struct mongo_client {
    char * host;
    MONGO_CLIENT * client;
    struct skynet_service * ctx;
};

int mongo_connect(struct skynet_service * ctx, struct mongo_client * mc) {
    char host[1024];
    snprintf(host, 1023, "mongodb://%s", mc->host);

    mc->client = mongoc_client_new(host);
    if (mc->client == NULL) {
        skynet_logger_error(ctx, "[MongoDB] Connect %s Error", mc->host);
        return 1;
    }
    
    skynet_logger_notice(ctx, "[MongoDB] Connect %s Success", mc->host);
    return 0;
}

void mongo_insert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * value, size_t sz) {
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson;
    
    bson = bson_new_from_json((const uint8_t*)value, sz, &error);
    if (bson == NULL) {
        skynet_logger_error(ctx, "[MongoDB] Insert value error, %s:%s, %s", dbname, collection, value);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        client = mongoc_client_get_collection (((struct mongo_client *)ctx->hook)->client, dbname, collection);
        if (!mongoc_collection_insert (client, MONGOC_INSERT_NONE, bson, NULL, &error)) {
            skynet_logger_error(ctx, "[MongoDB] Insert Failed, %s:%s, %s", dbname, collection, value);
            skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
        } else {
            skynet_logger_debug(ctx, "[MongoDB] Insert Success, %s:%s, %s", dbname, collection, value);
        }

        bson_destroy (bson);
        mongoc_collection_destroy (client);
    }
}

void mongo_remove(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, size_t sz) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_query;

    mc = ctx->hook;
    bson_query = strlen(query) ? bson_new_from_json((const uint8_t*)query, sz, &error) : bson_new ();
    client = mongoc_client_get_collection (mc->client, dbname, collection);

    if (!mongoc_collection_remove (client, MONGOC_REMOVE_SINGLE_REMOVE, bson_query, NULL, &error)) {
        skynet_logger_error(ctx, "[MongoDB] Delete Failed, %s:%s, %s", dbname, collection, query);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        skynet_logger_debug(ctx, "[MongoDB] Delete Success, %s:%s, %s", dbname, collection, query);
    }

    bson_destroy (bson_query);
    mongoc_collection_destroy (client);
}

void mongo_update(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_value, *bson_query;

    mc = ctx->hook;
    bson_query = strlen(query) ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new ();
    bson_value = bson_new_from_json((const uint8_t*)value, sz, &error);
    client = mongoc_client_get_collection (mc->client, dbname, collection);

    if (!mongoc_collection_update (client, MONGOC_UPDATE_NONE, bson_query, bson_value, NULL, &error)) {
        skynet_logger_error(ctx, "[MongoDB] Update Failed, %s:%s, %s, %s", dbname, collection, query, value);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        //skynet_logger_debug(ctx, "[MongoDB] Update Success, %s:%s, %s, %s", dbname, collection, query, value);
        skynet_logger_debug(ctx, "[MongoDB] Update Success, %s:%s, %s", dbname, collection, query);
    }

    bson_destroy (bson_query);
    bson_destroy (bson_value);
    mongoc_collection_destroy (client);
}

void mongo_upsert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_value, *bson_query;
    
    mc = ctx->hook;
    bson_query = strlen(query) ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new ();
    bson_value = bson_new_from_json((const uint8_t*)value, sz, &error);
    client = mongoc_client_get_collection (mc->client, dbname, collection);

    if (!mongoc_collection_update (client, MONGOC_UPDATE_UPSERT, bson_query, bson_value, NULL, &error)) {
        skynet_logger_error(ctx, "[MongoDB] Upsert Failed, %s:%s, %s, %s", dbname, collection, query, value);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        //skynet_logger_debug(ctx, "[MongoDB] Upsert Success, %s:%s, %s, %s", dbname, collection, query, value);
        skynet_logger_debug(ctx, "[MongoDB] Upsert Success, %s:%s, %s", dbname, collection, query);
    }

    bson_destroy (bson_query);
    bson_destroy (bson_value);
    mongoc_collection_destroy (client);
}

char * mongo_selectmore(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * opts, size_t sz) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_CURSOR *cursor;
    MONGO_BSON *bson_query;
    MONGO_BSON *bson_opts;
    MONGO_BSON bson_result;
    const MONGO_BSON *doc;
    char *str, *result = NULL;
    char key[64];
    int num = 0;

    mc = ctx->hook;
    bson_opts = strlen(opts) ? bson_new_from_json((const uint8_t*)opts, sz, &error) : bson_new();
    bson_query = strlen(query) ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new ();
    client = mongoc_client_get_collection (mc->client, dbname, collection);
    cursor = mongoc_collection_find (client, MONGOC_QUERY_NONE, 0, 0, 0, bson_query, bson_opts, NULL);

    bson_init (&bson_result);
    while (mongoc_cursor_next (cursor, &doc)) {
        sprintf(key, "%d", num++);
        bson_append_document (&bson_result, key, strlen(key), doc);
        //skynet_logger_debug(ctx, "[MongoDB] SelectMore Success, %s:%s, %s, %s", dbname, collection, query, str);
    }

    if (num > 0) {
        str = bson_array_as_json(&bson_result, NULL);
        result = skynet_strdup(str);
        bson_free (str);
    }
    
    bson_destroy (bson_opts);
    bson_destroy (bson_query);
    bson_destroy (&bson_result);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (client);

    return result;
}

char * mongo_selectone(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * opts, size_t sz) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_CURSOR *cursor;
    MONGO_BSON *bson_query;
    MONGO_BSON *bson_opts;
    const MONGO_BSON *doc;
    char *str, *result = NULL;

    mc = ctx->hook;
    bson_opts = strlen(opts) ? bson_new_from_json((const uint8_t*)opts, sz, &error) : bson_new();
    bson_query = strlen(query) ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new ();
    client = mongoc_client_get_collection (mc->client, dbname, collection);
    cursor = mongoc_collection_find (client, MONGOC_QUERY_NONE, 0, 0, 0, bson_query, bson_opts, NULL);

    if (mongoc_cursor_next (cursor, &doc)) {
        str = bson_as_json (doc, NULL);
        result = skynet_strdup(str);
        bson_free (str);
        //skynet_logger_debug(ctx, "[MongoDB] SelectOne Success, %s:%s, %s, %s", dbname, collection, query, str);
    } else {
        skynet_logger_error(ctx, "[MongoDB] SelectOne Failed, %s:%s, %s", dbname, collection, query);
    }
    
    bson_destroy (bson_opts);
    bson_destroy (bson_query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (client);

    return result;
}

void mongo_dispatch_cmd(struct skynet_service * ctx, uint32_t source, uint32_t session, const char * msg, size_t sz) {
    int i;
    char * command = msg;

    skynet_logger_debug(ctx, "[MongoDB] dispatch command: %.*s", sz, msg);

    for (i=0;i<sz;i++) {
        if (command[i]=='|') {
            break;
        }
    }
    if (i >= sz || i+1 >= sz) {
        return;
    }

    char * param = command+i+1;
    if (param == NULL) return;

#define GET_CMD_ARGS(name, args) \
    char * name = strsep(&args, "|"); \
    if (name == NULL) return; \
//end define

    if (memcmp(command, "update", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        GET_CMD_ARGS(query, param)
        mongo_update(ctx, dbname, collec, query, param, sz-(param-msg));
    } else if (memcmp(command, "upsert", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        GET_CMD_ARGS(query, param)
        mongo_upsert(ctx, dbname, collec, query, param, sz-(param-msg));
    } else if (memcmp(command, "insert", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        mongo_insert(ctx, dbname, collec, param, sz-(param-msg));
    } else if (memcmp(command, "findone", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        GET_CMD_ARGS(query, param)

        char * value = mongo_selectone(ctx, dbname, collec, query, param, sz-(param-msg));
        if (value != NULL) {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, value, strlen(value));
            skynet_free(value);
        } else {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, "", 0);
        }
    } else if (memcmp(command, "findmore", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        GET_CMD_ARGS(query, param)

        char * value = mongo_selectmore(ctx, dbname, collec, query, param, sz-(param-msg));
        if (value != NULL) {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, value, strlen(value));
            skynet_free(value);
        } else {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, "", 0);
        }
    } else if (memcmp(command, "remove", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        mongo_remove(ctx, dbname, collec, param, sz-(param-msg));
    }
}

int mongo_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct mongo_client * mc = SKYNET_MALLOC(sizeof(struct mongo_client));
    mc->host = (char *)SKYNET_MALLOC(sizeof(char) * 1024);
    sscanf(args, "%s", mc->host);

    ctx->hook = mc;
    mongoc_init();
    return mongo_connect(ctx, mc);
}

void mongo_release(struct skynet_service * ctx) {
    struct mongo_client * mc = ctx->hook;
    mongoc_client_destroy(mc->client);
    mongoc_cleanup();
    skynet_free(mc->host);
    skynet_free(mc);
}

int mongo_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    switch(type) {
    case SERVICE_TEXT:
        mongo_dispatch_cmd(ctx, source, session, msg, sz);
        break;
    default: 
        break;
    }

    return 0;
}
