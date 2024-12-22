/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

static int QuoteString(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);

    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addchar(&b, '"');

    while (len--) {
        unsigned char c = *(str++);

        if (c == '"' || c == '\\') {
            luaL_addchar(&b, '\\');
            luaL_addchar(&b, c);
        } else if (c == '\n') {
            luaL_addchar(&b, '\\');
            luaL_addchar(&b, 'n');
        } else if (c == '\r') {
            luaL_addchar(&b, '\\');
            luaL_addchar(&b, 'r');
        } else if (c == '\t') {
            luaL_addchar(&b, '\\');
            luaL_addchar(&b, 't');
        } else if (c < ' ' || c >= 0x7f) {
            char buf[5];
            int n = l_sprintf(buf, sizeof(buf), "\\x%02x", c);
            luaL_addlstring(&b, buf, n);
        } else {
            luaL_addchar(&b, c);
        }
    }

    luaL_addchar(&b, '"');
    luaL_pushresult(&b);

    return 1;
}

static int RawCopy(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);

    char *buf = (char *)malloc(len);
    memcpy(buf, str, len);

    lua_pushlightuserdata(L, buf);
    lua_pushinteger(L, len);

    return 2;
}

LUAMOD_API int luaopen_NopByte_Core(lua_State *L) {
    luaL_Reg l[] = {
        // clang-format off
        {"QuoteString", QuoteString},
        {"RawCopy", RawCopy},
        {NULL, NULL},
        // clang-format on
    };

    luaL_newlib(L, l);
    return 1;
}
