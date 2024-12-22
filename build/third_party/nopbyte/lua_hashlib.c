/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

// http://sfsrealm.hopto.org/inside_mopaq/chapter2.htm#hash_tables

#include <ctype.h>
#include <lauxlib.h>
#include <stdint.h>

static uint32_t seeds[0x500];

static void Initialize() {
    uint32_t seed = 0x00100001, tmp1, tmp2;

    for (int i = 0; i < 0x100; ++i) {
        for (int j = i, k = 0; k < 5; ++k, j += 0x100) {
            seed = (seed * 125 + 3) % 0x2AAAAB;
            tmp1 = (seed & 0xFFFFu) << 0x10u;
            seed = (seed * 125 + 3) % 0x2AAAAB;
            tmp2 = (seed & 0xFFFFu);
            seeds[j] = tmp1 | tmp2;
        }
    }
}

static int HashString(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);

    uint32_t seed1 = 0x7FED7FED;
    uint32_t seed2 = 0xEEEEEEEE;
    uint8_t *key = (uint8_t *)str;

    for (size_t i = 0; i < len; ++i) {
        uint8_t ord = toupper(*key++);
        seed1 = seeds[(2u << 8u) + ord] ^ (seed1 + seed2);
        seed2 = ord + seed1 + seed2 + (seed2 << 5u) + 3;
    }

    lua_pushinteger(L, seed1);
    return 1;
}

LUAMOD_API int luaopen_NopByte_HashLib(lua_State *L) {
    Initialize();

    luaL_Reg l[] = {
        {"HashString", HashString},
        {NULL, NULL},
    };

    luaL_newlib(L, l);
    return 1;
}
