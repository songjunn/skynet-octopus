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
    char name[64], path[256];
    sscanf(args, "%[^','],%s", path, name);

    struct snlua * l = skynet_malloc(sizeof(struct snlua));
    ctx->hook = l;
    memset(l, 0, sizeof(*l));
    l->L = luaL_newstate();
    luaL_openlibs(l->L);

    return 0;
}

void snlua_release(struct skynet_service * ctx) {
    struct snlua * l = (struct snlua *) ctx->hook;
    lua_close(l->L);
    skynet_free(l);
}

int snlua_callback(struct skynet_service * ctx, uint32_t source, uint32_t session, int type, void * msg, size_t sz) {

}

