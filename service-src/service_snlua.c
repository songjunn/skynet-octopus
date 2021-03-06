#include "skynet.h"
#include "skynet_malloc.h"

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include <string.h>

#define MEMORY_WARNING_REPORT (1024 * 1024 * 32)

struct snlua {
    lua_State * L;
    size_t mem;
    size_t mem_report;
    size_t mem_limit;
};

static int llogger_debug(lua_State* L) {
    int source = lua_tointeger(L, 1);
    const char * args = lua_tostring(L, 2);

    skynet_logger_debug(source, args);
    return 0;
}

static int llogger_warn(lua_State* L) {
    int source = lua_tointeger(L, 1);
    const char * args = lua_tostring(L, 2);

    skynet_logger_warn(source, args);
    return 0;
}

static int llogger_notice(lua_State* L) {
    int source = lua_tointeger(L, 1);
    const char * args = lua_tostring(L, 2);

    skynet_logger_notice(source, args);
    return 0;
}

static int llogger_error(lua_State* L) {
    int source = lua_tointeger(L, 1);
    const char * args = lua_tostring(L, 2);

    skynet_logger_error(source, args);
    return 0;
}

static int ltimer_register(lua_State* L) {
    int handle = lua_tointeger(L, 1);
    const char * args = lua_tostring(L, 2);
    int time = lua_tointeger(L, 3);

    skynet_timer_register(handle, 0, args, strlen(args), time);
    return 0;
}

static int lsend_string(lua_State* L) {
    const char * name, * msg;
    int t, type, source, session, target, size;

    t = lua_type(L, 1);
    if (t == LUA_TSTRING) {
        name = lua_tostring(L, 1);
    } 
    else if (t == LUA_TNUMBER) {
        target = lua_tointeger(L, 1);
    }

    source = lua_tointeger(L, 2);
    session = lua_tointeger(L, 3);
    type = lua_tointeger(L, 4);
    msg = lua_tostring(L, 5);
    size = lua_tointeger(L, 6);
    
    if (t == LUA_TSTRING) {
        skynet_sendname(name, source, session, type, msg, size);
    } 
    else if (t == LUA_TNUMBER) {
        skynet_sendhandle(target, source, session, type, msg, size);
    }

    return 0;
}

static int lsend_buffer(lua_State* L) {
    const char * name;
    void * msg;
    int t, type, source, session, target, size;

    t = lua_type(L, 1);
    if (t == LUA_TSTRING) {
        name = lua_tostring(L, 1);
    } 
    else if (t == LUA_TNUMBER) {
        target = lua_tointeger(L, 1);
    }

    source = lua_tointeger(L, 2);
    session = lua_tointeger(L, 3);
    type = lua_tointeger(L, 4);
    msg = lua_touserdata(L, 5);
    size = lua_tointeger(L, 6);
    
    if (t == LUA_TSTRING) {
        skynet_sendname(name, source, session, type, msg, size);
    } 
    else if (t == LUA_TNUMBER) {
        skynet_sendhandle(target, source, session, type, msg, size);
    }

    return 0;
}

static int lcreate_service(lua_State* L) {
    const char * name = lua_tostring(L, 1);
    const char * lib = lua_tostring(L, 2);
    const char * args = lua_tostring(L, 3);
    
    int harbor = skynet_harbor_local_id();
    uint32_t handle = skynet_service_create(name, harbor, lib, args, 0);
    lua_pushnumber(L, handle);
    return 1;
}

static int lclose_service(lua_State* L) {
    int t = lua_type(L, 1);
    if (t == LUA_TSTRING) {
        const char * name = lua_tostring(L, 1);
        struct skynet_service * ctx = skynet_service_findname(name);
        if (ctx != NULL) {
            skynet_service_close(ctx->handle);
        }
    } 
    else if (t == LUA_TNUMBER) {
        int handle = lua_tointeger(L, 1);
        skynet_service_close(handle);
    }
    return 0;
}

static int lprint_memory(lua_State* L) {
    skynet_malloc_print();
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
	//skynet_logger_error(ctx->handle, "[snlua]Memory free %.2f M", (float)l->mem / (1024 * 1024));
        skynet_malloc_remove(ptr);
        skynet_free(ptr);
        return NULL;
    } else {
        //skynet_logger_error(ctx->handle, "[snlua]Memory realloc %.2f M", (float)l->mem / (1024 * 1024));
        skynet_malloc_remove(ptr);
        void * ptr_new = skynet_realloc(ptr, nsize);
        skynet_malloc_insert(ptr_new, nsize, __FILE__, __LINE__);
        return ptr_new;
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
    char mainfile[128];
    sscanf(args, "%s", mainfile);

    struct snlua * l = skynet_malloc(sizeof(struct snlua));
    skynet_malloc_insert(l, sizeof(struct snlua), __FILE__, __LINE__);
    memset(l, 0, sizeof(*l));
    ctx->hook = l;
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
    lua_register(l->L, "skynet_send_string", lsend_string);
    lua_register(l->L, "skynet_create_service", lcreate_service);
    lua_register(l->L, "skynet_close_service", lclose_service);
    lua_register(l->L, "skynet_print_memory", lprint_memory);

    lua_gc(l->L, LUA_GCSTOP, 0);
    int ret = luaL_dofile(l->L, mainfile);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Load error:%s", lua_tostring(l->L, -1));
        return ret;
    }

    lua_getglobal(l->L, "create");  
    lua_pushinteger(l->L, harbor);
    lua_pushinteger(l->L, ctx->handle);
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

    lua_getglobal(l->L, "release");  
    int ret = lua_pcall(l->L, 0, 0, 0);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Runtime error:%s", lua_tostring(l->L, -1));
    }
    lua_settop(l->L, 0);
    lua_close(l->L);
    skynet_malloc_remove(l);
    skynet_free(l);
}

int snlua_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    struct snlua * l = (struct snlua *) ctx->hook;

    //lua_gc(l->L, LUA_GCSTOP, 0);
    lua_getglobal(l->L, "handle");  
    lua_pushinteger(l->L, ctx->handle);
    lua_pushinteger(l->L, source);
    lua_pushinteger(l->L, session);
    lua_pushinteger(l->L, type);
    if (msg && sz > 0) {
        lua_pushlstring(l->L, msg, sz);
    } else {
        lua_pushstring(l->L, "");
    }

    /*struct timeval tv;
    gettimeofday(&tv,NULL);
    long start = tv.tv_sec * 1000 + tv.tv_usec / 1000;*/

    int ret = lua_pcall(l->L, 5, 0, 0);
    if (ret != LUA_OK) {
        skynet_logger_error(ctx->handle, "[snlua]Runtime error:%s", lua_tostring(l->L, -1));
    }

    /*gettimeofday(&tv,NULL);
    long end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    if (end - start > 1) {
        skynet_logger_error(ctx->handle, "[snlua]lua_pcall cost:%d", (end-start));
    }*/

    lua_settop(l->L, 0);
    //lua_gc(l->L, LUA_GCRESTART, 0);
    return ret;
}

