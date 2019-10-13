#include "skynet.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct snlua {
    lua_State * L;
    /*size_t mem;
      size_t mem_report;
      size_t mem_limit;*/
};

int snlua_create(struct skynet_service * ctx, int harbor, const char * args) {
    //char name[64], path[256];
    //sscanf(args, "%[^','],%s", path, name);

    struct snlua * l = skynet_malloc(sizeof(struct snlua));
    ctx->hook = l;
    memset(l, 0, sizeof(*l));
    l->L = luaL_newstate();
    luaL_openlibs(l->L);

    luaL_dofile(l, "./main.lua");
    lua_getglobal(l, "create");  
    lua_pushnumber(l, harbor);
    lua_pushnumber(l, ctx->handle);
    lua_pushstring(l, args);
    int ret = lua_pcall(l, 3, 1, 0);
    lua_settop(l, 0);
    return ret;
}

void snlua_release(struct skynet_service * ctx) {
    struct snlua * l = (struct snlua *) ctx->hook;

    luaL_dofile(l, "./main.lua");
    lua_getglobal(l, "release");  
    lua_pcall(l, 0, 0, 0);
    lua_settop(l, 0);

    lua_close(l->L);
    skynet_free(l);
}

int snlua_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {
    struct snlua * l = (struct snlua *) ctx->hook;

    luaL_dofile(l, "./main.lua");
    lua_getglobal(l, "handle");  
    lua_pushnumber(l, ctx->handle);
    lua_pushstring(l, source);
    lua_pushnumber(l, session);
    lua_pushnumber(l, type);
    lua_pushlstring(l, msg, sz);
    int ret = lua_pcall(l, 5, 1, 0);
    lua_settop(l, 0);
    return ret;
}

