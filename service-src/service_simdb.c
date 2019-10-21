#include "skynet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

struct simdb {
	char path[128];
};

#define FILE_EXIST(file)  (access(file, 0) == 0)
#define FILE_REMOVE(file) (remove(file) == 0)
#define FOLDER_EXIST(folder) (access(folder, 0) == 0)
#define FOLDER_CREATE(folder) (mkdir(folder) == 0)
#define FOLDER_REMOVE(folder) (rmdir(folder) == 0)

int simdb_folder_create(struct skynet_service * ctx, const char * path) {
	if (FOLDER_EXIST(path)) return 0;
	if (FOLDER_CREATE(path)) return 0;
	skynet_logger_error(ctx, "[simdb]create floder failed: %s", path);
	return -1;
}

int simdb_file_create(struct skynet_service * ctx, const char * path) {

}

void simdb_file_delete() {

}

void simdb_file_read() {

}

void simdb_file_write(struct skynet_service * ctx, const char * file, const char * value, size_t sz) {
	FILE * handle = fopen(file, "wb");
	if (handle == NULL) {
		skynet_logger_error(ctx, "[simdb]write file failed: %s", file);
		return;
	}

	fprintf(handle, "%.*s", sz, value);
    fprintf(handle, "\n");
    fflush(handle);
    fclose(handle);
}

int simdb_connect(struct skynet_service * ctx, struct simdb * db) {
	return simdb_folder_create(ctx, db);
}

void simdb_insert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * value, size_t sz) {

}

void simdb_remove(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, size_t sz) {

}

void simdb_update(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {

}

void simdb_upsert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {
	struct simdb * db = ctx->hook;
printf("aaaaaaaaa");
	char dbpath[256], tablepath[512], filepath[1024], filename[512];
	snprintf(dbpath, sizeof(dbpath), "%s/%s", db->path, dbname);
	snprintf(tablepath, sizeof(tablepath), "%s/%s", dbpath, collection);
	snprintf(filepath, sizeof(filepath), "%s/%s", tablepath, query);
printf("bbbbbbbb");
	if (simdb_folder_create(ctx, dbpath)) return;
	if (simdb_folder_create(ctx, tablepath)) return;
	if (FILE_EXIST(filepath)) {
		FILE_REMOVE(filepath);
	}
	simdb_file_write(ctx, filepath, value, sz);
}

char * simdb_selectone(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, size_t sz, const char * opts) {
	return NULL;
}

void simdb_dispatch_cmd(struct skynet_service * ctx, uint32_t source, uint32_t session, const char * msg, size_t sz) {
    int i;
    char * command = msg;

    skynet_logger_debug(ctx, "[simdb] dispatch command: %s", msg);

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
        simdb_update(ctx, dbname, collec, query, param, sz-(param-msg));
    } else if (memcmp(command, "upsert", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        GET_CMD_ARGS(query, param)
        simdb_upsert(ctx, dbname, collec, query, param, sz-(param-msg));
    } else if (memcmp(command, "insert", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        simdb_insert(ctx, dbname, collec, param, sz-(param-msg));
    } else if (memcmp(command, "findone", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)

        char * value = simdb_selectone(ctx, dbname, collec, param, sz-(param-msg), "");
        if (value != NULL) {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, value, strlen(value));
            skynet_free(value);
        } else {
            skynet_sendhandle(source, ctx->handle, session, SERVICE_RESPONSE, "", 0);
        }
    } else if (memcmp(command, "remove", i) == 0) {
        GET_CMD_ARGS(dbname, param)
        GET_CMD_ARGS(collec, param)
        simdb_remove(ctx, dbname, collec, param, sz-(param-msg));
    }
}

int simdb_create(struct skynet_service * ctx, int harbor, const char * args) {
	struct simdb * db = skynet_malloc(sizeof(struct simdb));

	sscanf(args, "%s", db->path);

    return simdb_connect(ctx, db);
}

void simdb_release(struct skynet_service * ctx) {
	struct simdb * db = ctx->hook;
	skynet_free(db);
}

int simdb_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    switch(type) {
    case SERVICE_TEXT:
        simdb_dispatch_cmd(ctx, source, session, msg, sz);
        break;
    default: 
        break;
    }

    return 0;
}
