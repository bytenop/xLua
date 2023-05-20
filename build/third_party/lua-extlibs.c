/*
 * Copyright (C) Lemix, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

int luaopen_cjson(lua_State *L);
int luaopen_cjson_safe(lua_State *L);
int luaopen_cmsgpack(lua_State *L);
int luaopen_cmsgpack_safe(lua_State *L);
int luaopen_lpeg(lua_State *L);
int luaopen_NopByte_Core(lua_State *L);
int luaopen_NopByte_HashLib(lua_State *L);
int luaopen_NopByte_Observer(lua_State *L);
int luaopen_NopByte_Packet_Core(lua_State *L);
int luaopen_NopByte_Random(lua_State *L);
int luaopen_NopByte_WordFilter_Core(lua_State *L);
int luaopen_NopByte_ZLib(lua_State *L);
int luaopen_sproto_core(lua_State *L);
int luaopen_xxtea(lua_State *L);

LUAMOD_API int luaopen_extlibs(lua_State *L) {
    luaL_Reg libs[] = {
        {"cjson", luaopen_cjson},
        {"cjson.safe", luaopen_cjson_safe},
        {"cmsgpack", luaopen_cmsgpack},
        {"cmsgpack.safe", luaopen_cmsgpack_safe},
        {"lpeg", luaopen_lpeg},
        {"NopByte.Core", luaopen_NopByte_Core},
        {"NopByte.HashLib", luaopen_NopByte_HashLib},
        {"NopByte.Observer", luaopen_NopByte_Observer},
        {"NopByte.Packet.Core", luaopen_NopByte_Packet_Core},
        {"NopByte.Random", luaopen_NopByte_Random},
        {"NopByte.WordFilter.Core", luaopen_NopByte_WordFilter_Core},
        {"NopByte.ZLib", luaopen_NopByte_ZLib},
        {"sproto.core", luaopen_sproto_core},
        {"xxtea", luaopen_xxtea},
        {NULL, NULL},
    };

    for (luaL_Reg *lib = libs; lib->name; ++lib) {
        luaL_requiref(L, lib->name, lib->func, 0);
        lua_pop(L, 1); /* remove lib */
    }

    return 0;
}
