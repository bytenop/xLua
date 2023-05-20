/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <stdint.h>
#include <string.h>

#define kChunkSize 0x2000
#define kBufferSize (kChunkSize + 0x10)

static void PackUInt16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xff;
    buf[1] = val & 0xff;
}

static int EncodePacket(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    uint32_t chunkSize = luaL_optinteger(L, 2, kChunkSize);

    if (chunkSize > kChunkSize) {
        chunkSize = kChunkSize;
    }

    uint8_t buf[kBufferSize];

    if (len <= chunkSize) {
        buf[2] = 0;

        PackUInt16(buf, len + 1);
        memcpy(buf + 3, str, len);

        lua_pushlstring(L, (const char *)buf, len + 3);
        lua_pushboolean(L, 0);

        return 2;
    }

    int chunkCount = (len - 1) / chunkSize + 1;
    lua_createtable(L, chunkCount, 0);

    for (int i = 1; i <= chunkCount; ++i) {
        uint16_t n;

        if (len > chunkSize) {
            n = chunkSize;
            buf[2] = 0xcc;
        } else {
            n = len;
            buf[2] = 0xff;
        }

        PackUInt16(buf, n + 1);
        memcpy(buf + 3, str, n);

        lua_pushlstring(L, (const char *)buf, n + 3);
        lua_rawseti(L, -2, i);

        str += n;
        len -= n;
    }

    lua_pushboolean(L, 1);
    return 2;
}

static int DecodePacket(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);
    const uint8_t *buf = (uint8_t *)str;

    lua_pushlstring(L, str + 1, len - 1);

    if (buf[0] == 0) {
        return 1;
    }

    lua_pushboolean(L, buf[0] == 0xff);
    return 2;
}

static int ParsePacket(lua_State *L) {
    const char *data = NULL;
    size_t size;

    int type = lua_type(L, 1);
    if (type == LUA_TSTRING) {
        data = lua_tolstring(L, 1, &size);
    } else {
        data = (const char *)lua_touserdata(L, 1);
        if (data == NULL) {
            return luaL_argerror(L, 1, "Need a string or userdata");
        }

        size = luaL_checkinteger(L, 2);
    }

    lua_pushlstring(L, data, 1);
    lua_pushlstring(L, data + 1, size - 1);

    return 2;
}

LUAMOD_API int luaopen_NopByte_Packet_Core(lua_State *L) {
    luaL_Reg l[] = {
        // clang-format off
        {"Encode", EncodePacket},
        {"Decode", DecodePacket},
        {"Parse", ParsePacket},
        {NULL, NULL},
        // clang-format on
    };

    luaL_newlib(L, l);
    return 1;
}
