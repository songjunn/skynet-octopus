#include "skynet.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MEMORY_WARNING_REPORT (1024 * 1024 * 32)

struct snlua {
    lua_State * L;
    char mainfile[128];
    size_t mem;
    size_t mem_report;
    size_t mem_limit;
};

static int llogger_debug(lua_State* L) {
    int source = luaL_checkinteger(L, 1);
    const char* args = luaL_checkstring(L, 2);

    skynet_logger_debug(source, args);
    return 0;
}

static int llogger_warn(lua_State* L) {
    int source = luaL_checkinteger(L, 1);
    const char* args = luaL_checkstring(L, 2);

    skynet_logger_warn(source, args);
    return 0;
}

static int llogger_notice(lua_State* L) {
    int source = luaL_checkinteger(L, 1);
    const char* args = luaL_checkstring(L, 2);

    skynet_logger_notice(source, args);
    return 0;
}

static int llogger_error(lua_State* L) {
    int source = luaL_checkinteger(L, 1);
    const char* args = luaL_checkstring(L, 2);

    skynet_logger_error(source, args);
    return 0;
}

static int ltimer_register(lua_State* L) {
    int handle = luaL_checkinteger(L, 1);
    const char* args = luaL_checkstring(L, 2);
    int time = luaL_checkinteger(L, 3);

    skynet_timer_register(handle, args, strlen(args), time);
    return 0;
}

static int lsend_name(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int source = luaL_checkinteger(L, 2);
    int session = luaL_checkinteger(L, 3);
    int type = luaL_checkinteger(L, 4);
    const char* msg = luaL_checkstring(L, 5);

    skynet_sendname(name, source, session, type, msg, strlen(msg));
    return 0;
}

static int lsend_handle(lua_State* L) {
    int target = luaL_checkinteger(L, 1);
    int source = luaL_checkinteger(L, 2);
    int session = luaL_checkinteger(L, 3);
    int type = luaL_checkinteger(L, 4);
    const char* msg = luaL_checkstring(L, 5);

    skynet_sendhandle(target, source, session, type, msg, strlen(msg));
    return 0;
}

static void * lalloc(void * ud, void *ptr, size_t osize, size_t nsize) {
    struct skynet_service * ctx = ud;
    struct snlua *l = ctx->hook;
    size_t mem = l->mem;
    l->mem += nsize;
    if (ptr)
        l->mem -= osize;
    if (l->mem_limit != 0 && l->mem > l->mem_limit) {
        if (ptr == NULL || nsize > osize) {
            l->mem = mem;
            return NULL;
        }
    }
    if (l->mem > l->mem_report) {
        l->mem_report *= 2;
        skynet_logger_error(ctx->handle, "[snlua]Memory warning %.2f M", (float)l->mem / (1024 * 1024));
    }
    if (nsize == 0) {
        skynet_free(ptr);
        return NULL;
    } else {
        return skynet_realloc(ptr, nsize);
    }
}

static int traceback (lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg) {
        //fprintf(stderr, "lua error:%s\n", msg);
        luaL_traceback(L, L, msg, 1);
    } else {
        //fprintf(stderr, "lua error:no message\n");
        lua_pushliteral(L, "(no error message)");
    }
    return 1;
}

int snlua_create(struct skynet_service * ctx, int harbor, const char * args) {
    struct snlua * l = skynet_malloc(sizeof(struct snlua));
    ctx->hook = l;
    memset(l, 0, sizeof(*l));
    sscanf(args, "%s", l->mainfile);
    l->mem_report = MEMORY_WARNING_REPORT;
    l->mem_limit = 0;
    l->L = lua_newstate(lalloc, ctx);
    luaL_openlibs(l->L);
    lua_pushcfunction(l->L, traceback);

    lua_register(l->L, "skynet_logger_debug", llogger_debug);
    lua_register(l->L, "skynet_logger_warn", llogger_warn);
    lua_register(l->L, "skynet_logger_notice", llogger_notice);
    lua_register(l->L, "skynet_logger_error", llogger_error);
    lua_register(l->L, "skynet_timer_register", ltimer_register);
    lua_register(l->L, "skynet_send_name", lsend_name);
    lua_register(l->L, "skynet_send_handle", lsend_handle);

    lua_gc(l->L, LUA_GCSTOP, 0);
    int ret = luaL_dofile(l->L, l->mainfile);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Load error:%s", lua_tostring(l->L, -1));
        return ret;
    }
    lua_getglobal(l->L, "create");  
    lua_pushnumber(l->L, harbor);
    lua_pushnumber(l->L, ctx->handle);
    lua_pushstring(l->L, args);
    ret = lua_pcall(l->L, 3, 1, 1);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Runtime error:%s", lua_tostring(l->L, -1));
    }
    lua_settop(l->L, 0);
    lua_gc(l->L, LUA_GCRESTART, 0);
    return ret;
}

void snlua_release(struct skynet_service * ctx) {
    struct snlua * l = (struct snlua *) ctx->hook;

    int ret = luaL_dofile(l->L, l->mainfile);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Load error:%s", lua_tostring(l->L, -1));
    } else {
        lua_getglobal(l->L, "release");  
        ret = lua_pcall(l->L, 0, 0, 0);
        if (ret != LUA_OK) {
            skynet_logger_error(ctx->handle, "[snlua]Runtime error:%s", lua_tostring(l->L, -1));
        }
    }

    lua_settop(l->L, 0);
    lua_close(l->L);
    skynet_free(l);
}

int snlua_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    struct snlua * l = (struct snlua *) ctx->hook;

    lua_gc(l->L, LUA_GCSTOP, 0);
    int ret = luaL_dofile(l->L, l->mainfile);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Load error:%s", lua_tostring(l->L, -1));
    } else {
        lua_getglobal(l->L, "handle");  
        lua_pushnumber(l->L, ctx->handle);
        lua_pushstring(l->L, source);
        lua_pushnumber(l->L, session);
        lua_pushnumber(l->L, type);
        lua_pushlstring(l->L, msg, sz);
        ret = lua_pcall(l->L, 5, 1, 0);
        if (ret != LUA_OK) {
            skynet_logger_error(ctx->handle, "[snlua]Runtime error:%s", lua_tostring(l->L, -1));
        }
    }
    lua_settop(l->L, 0);
    lua_gc(l->L, LUA_GCRESTART, 0);
    return ret;
}
