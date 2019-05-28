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

void mongo_insert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * value) {
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson;
    
    bson = bson_new_from_json((const uint8_t*)value, strlen(value), &error);
    if (bson == NULL) {
        skynet_logger_error(ctx, "[MongoDB] Insert value error, %s:%s, %s", dbname, collection, value);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        client = mongoc_client_get_collection ((struct mongo_client *)ctx->hook, dbname, collection);
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

void mongo_remove(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_query;

    mc = ctx->hook;
    if (strlen(query) > 0) {
        bson_query = bson_new_from_json((const uint8_t*)query, strlen(query), &error);
    } else {
        bson_query = bson_new ();
    }
    client = mongoc_client_get_collection (mc, dbname, collection);

    if (!mongoc_collection_remove (client, MONGOC_REMOVE_SINGLE_REMOVE, bson_query, NULL, &error)) {
        skynet_logger_error(ctx, "[MongoDB] Delete Failed, %s:%s, %s", dbname, collection, query);
        skynet_logger_error(ctx, "[MongoDB] Exception: %s ", error.message);
    } else {
        skynet_logger_debug(ctx, "[MongoDB] Delete Success, %s:%s, %s", dbname, collection, query);
    }

    bson_destroy (bson_query);
    mongoc_collection_destroy (client);
}

void mongo_update(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_value, *bson_query;

    mc = ctx->hook;
    if (strlen(query) > 0) {
        bson_query = bson_new_from_json((const uint8_t*)query, strlen(query), &error);
    } else {
        bson_query = bson_new ();
    }
    bson_value = bson_new_from_json((const uint8_t*)value, strlen(value), &error);
    client = mongoc_client_get_collection (mc, dbname, collection);

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

void mongo_upsert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value) {
    struct mongo_client *mc;
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_BSON *bson_value, *bson_query;
    
    mc = ctx->hook;
    if (strlen(query) > 0) {
        bson_query = bson_new_from_json((const uint8_t*)query, strlen(query), &error);
    } else {
        bson_query = bson_new ();
    }
    bson_value = bson_new_from_json((const uint8_t*)value, strlen(value), &error);
    client = mongoc_client_get_collection (mc, dbname, collection);

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

/*void _selectAll(vector<string> &result, const char * dbname, const char * collection, const char * query, const char * opts) {
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_CURSOR *cursor;
    MONGO_BSON *bson_query;
    MONGO_BSON *bson_opts;
    const MONGO_BSON *doc;
    char *str;

    bson_opts = opts != NULL ? bson_new_from_json((const uint8_t*)opts, strlen(opts), &error) : bson_new();
    bson_query = query != NULL ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new();
    client = mongoc_client_get_collection (instance->client, dbname, collection);
    cursor = mongoc_collection_find (client, MONGOC_QUERY_NONE, 0, 0, 0, bson_query, bson_opts, NULL);

    while (mongoc_cursor_next (cursor, &doc)) {
        str = bson_as_json (doc, NULL);
        //skynet_logger_debug(instance->ctx, "[MongoDB] SelectAll Success, %s:%s, %s", dbname, collection, str);
        result.push_back(str);
        bson_free (str);
    }
    
    bson_destroy (bson_query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (client);
}

void _selectOne(string& result, const char * dbname, const char * collection, const char * query, const char * opts) {
    MONGO_COLLECTION *client;
    MONGO_ERROR error;
    MONGO_CURSOR *cursor;
    MONGO_BSON *bson_query;
    MONGO_BSON *bson_opts;
    const MONGO_BSON *doc;
    char *str;

    bson_opts = opts != NULL ? bson_new_from_json((const uint8_t*)opts, strlen(opts), &error) : bson_new();
    bson_query = query != NULL ? bson_new_from_json((const uint8_t*)query, strlen(query), &error) : bson_new();
    client = mongoc_client_get_collection (instance->client, dbname, collection);
    cursor = mongoc_collection_find (client, MONGOC_QUERY_NONE, 0, 0, 0, bson_query, bson_opts, NULL);

    if (mongoc_cursor_next (cursor, &doc)) {
        str = bson_as_json (doc, NULL);
        //skynet_logger_debug(instance->ctx, "[MongoDB] SelectOne Success, %s:%s, %s, %s", dbname, collection, query, str);
        result = str;
        bson_free (str);
    } else {
        skynet_logger_error(instance->ctx, "[MongoDB] SelectOne Failed, %s:%s, %s", dbname, collection, query);
    }
    
    bson_destroy (bson_opts);
    bson_destroy (bson_query);
    mongoc_cursor_destroy (cursor);
    mongoc_collection_destroy (client);
}*/

int mongo_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct mongo_client * mc = skynet_malloc(sizeof(struct mongo_client));
    mc->host = (char *)skynet_malloc(sizeof(char) * 1024);
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
        case SERVICE_TEXT: {
            int i;
            char * command = msg;

            for (i=0;i<sz;i++) {
                if (command[i]=='|') {
                    break;
                }
            }
            if (i >= sz || i+1 >= sz) {
                break;
            }

            if (memcmp(command, "insert", i) == 0) {
                char * param = command+i+1;
                char * dbname = strsep(&param, "|");
                char * collec = strsep(&param, "|");
                mongo_insert(ctx, dbname, collec, param);
            }
        } break;
        default: break;
    }

    return 0;
}
