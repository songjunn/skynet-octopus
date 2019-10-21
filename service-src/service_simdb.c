#include "skynet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define FILE_SIZE_MAX 1024*1024*5

struct simdb {
	char path[128];
};

int simdb_folder_create(struct skynet_service * ctx, const char * path) {
	if (access(path, 0) == 0) return 0;
	if (mkdir(path, 0777) == 0) return 0;
	skynet_logger_error(ctx, "[simdb]create floder failed: %s", path);
	return -1;
}

int simdb_file_exist(struct skynet_service * ctx, const char * file) {
	return (access(file, 0) == 0);
}

int simdb_file_delete(struct skynet_service * ctx, const char * file) {
	if (remove(file)) {
		skynet_logger_error(ctx, "[simdb]delete file failed: %s", file);
		return -1;
	}
	return 0;
}

char * simdb_file_read(struct skynet_service * ctx, const char * file) {
	FILE * fp = fopen(file, "r");
	if (fp == NULL) {
		skynet_logger_error(ctx, "[simdb]read file failed: %s", file);
		return NULL;
	}
	fseek(fp, 0, SEEK_END);
	fseek(fp, 0, SEEK_SET);
	long sz = ftell(fp);
	char * buffer = skynet_malloc(sz+1);
	fscanf(fp, "%s", buffer);
	return buffer;
}

void simdb_file_write(struct skynet_service * ctx, const char * file, const char * value, size_t sz) {
	FILE * fp = fopen(file, "wb");
	if (fp == NULL) {
		skynet_logger_error(ctx, "[simdb]write file failed: %s", file);
		return;
	}

	fprintf(fp, "%.*s", sz, value);
    fprintf(fp, "\n");
    fflush(fp);
    fclose(fp);
}

int simdb_connect(struct skynet_service * ctx, struct simdb * db) {
	return simdb_folder_create(ctx, db->path);
}

void simdb_insert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * value, size_t sz) {
	skynet_logger_error(ctx, "[simdb] insert failed, %s:%s, %.*s", dbname, collection, sz, value);
	skynet_logger_error(ctx, "[simdb] not support insert method.");
}

void simdb_remove(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, size_t sz) {
	struct simdb * db = ctx->hook;

	char dbpath[256], tablepath[512], filepath[1024], filename[512];
	snprintf(dbpath, sizeof(dbpath), "%s/%s", db->path, dbname);
	snprintf(tablepath, sizeof(tablepath), "%s/%s", dbpath, collection);
	snprintf(filepath, sizeof(filepath), "%s/%s", tablepath, query);

	if (simdb_file_exist(ctx, filepath)) {
		simdb_file_delete(ctx, filepath);
		skynet_logger_debug(ctx, "[simdb] remove success, %s:%s, %.*s", dbname, collection, query, sz);
	} else {
		skynet_logger_error(ctx, "[simdb] remove failed, %s:%s, %.*s", dbname, collection, sz, query);
		skynet_logger_error(ctx, "[simdb] file not found: %s", filepath);
	}
}

void simdb_update(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {
	struct simdb * db = ctx->hook;

	char dbpath[256], tablepath[512], filepath[1024], filename[512];
	snprintf(dbpath, sizeof(dbpath), "%s/%s", db->path, dbname);
	snprintf(tablepath, sizeof(tablepath), "%s/%s", dbpath, collection);
	snprintf(filepath, sizeof(filepath), "%s/%s", tablepath, query);

	if (simdb_file_exist(ctx, filepath)) {
		simdb_file_delete(ctx, filepath);
		simdb_file_write(ctx, filepath, value, sz);
		skynet_logger_debug(ctx, "[simdb] update success, %s:%s, %s", dbname, collection, query);
	} else {
		skynet_logger_error(ctx, "[simdb] update failed, %s:%s, %s", dbname, collection, query);
		skynet_logger_error(ctx, "[simdb] file not found: %s", filepath);
	}
}

void simdb_upsert(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, const char * value, size_t sz) {
	struct simdb * db = ctx->hook;

	char dbpath[256], tablepath[512], filepath[1024], filename[512];
	snprintf(dbpath, sizeof(dbpath), "%s/%s", db->path, dbname);
	snprintf(tablepath, sizeof(tablepath), "%s/%s", dbpath, collection);
	snprintf(filepath, sizeof(filepath), "%s/%s", tablepath, query);

	if (simdb_folder_create(ctx, dbpath)) return;
	if (simdb_folder_create(ctx, tablepath)) return;
	if (simdb_file_exist(ctx, filepath)) {
		simdb_file_delete(ctx, filepath);
	}
	simdb_file_write(ctx, filepath, value, sz);
	skynet_logger_debug(ctx, "[simdb] upsert success, %s:%s, %s", dbname, collection, query);
}

char * simdb_selectone(struct skynet_service * ctx, const char * dbname, const char * collection, const char * query, size_t sz, const char * opts) {
	struct simdb * db = ctx->hook;

	int size;
	char * buffer;
	char dbpath[256], tablepath[512], filepath[1024], filename[512];

	snprintf(dbpath, sizeof(dbpath), "%s/%s", db->path, dbname);
	snprintf(tablepath, sizeof(tablepath), "%s/%s", dbpath, collection);
	snprintf(filepath, sizeof(filepath), "%s/%s", tablepath, query);

	if (simdb_folder_create(ctx, dbpath)) return;
	if (simdb_folder_create(ctx, tablepath)) return;
	if (simdb_file_exist(ctx, filepath)) {
		return simdb_file_read(ctx, filepath);
	} else {
		return NULL;
	}
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
        	skynet_logger_debug(ctx, "[simdb] selectone: %s", value);
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
	ctx->hook = db;

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
