/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <lua.h>
#include <luaconf.h>
#include <stdlib.h>
#include <zconf.h>
#include <zlib.h>

#if MAX_MEM_LEVEL >= 8
#define DEF_MEM_LEVEL 8
#else
#define DEF_MEM_LEVEL MAX_MEM_LEVEL
#endif

#define BUFFER_SIZE 0x1000

static const char *ModeOptions[] = {"zlib", "deflate", "gzip", NULL};
static const int WindowBitsOptions[] = {MAX_WBITS, -MAX_WBITS, MAX_WBITS | 0x10};

static void RaiseError(lua_State *L, int err) {
    switch (err) {
        case Z_ERRNO:
            luaL_error(L, "file stream error");
            break;
        case Z_STREAM_ERROR:
            luaL_error(L, "invalid compression level");
            break;
        case Z_DATA_ERROR:
            luaL_error(L, "invalid or incomplete deflate data");
            break;
        case Z_MEM_ERROR:
            luaL_error(L, "out of memory");
            break;
        case Z_VERSION_ERROR:
            luaL_error(L, "zlib version mismatch");
            break;
        default:
            luaL_error(L, "zlib error=%d", err);
    }
}

static const void *CheckData(lua_State *L, int *pindex, size_t *size) {
    const void *data = NULL;
    int index = *pindex;
    int type = lua_type(L, index);

    if (type == LUA_TSTRING) {
        data = lua_tolstring(L, index, size);
    } else {
        data = lua_touserdata(L, index);
        if (data == NULL) {
            luaL_argerror(L, index, "Need a string or userdata");
            return NULL;
        }

        *size = luaL_checkinteger(L, index + 1);
        ++(*pindex);
    }

    return data;
}

static int Deflate(lua_State *L) {
    size_t size = 0;
    int index = 1;

    const char *data = (const char *)CheckData(L, &index, &size);
    int mode = luaL_checkoption(L, index + 1, NULL, ModeOptions);
    int level = (int)luaL_optinteger(L, index + 2, Z_DEFAULT_COMPRESSION);

    z_stream stream;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int err = deflateInit2(&stream, level, Z_DEFLATED, WindowBitsOptions[mode],
                           DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        return luaL_error(L, "deflateInit: %d", err);
    }

    stream.avail_in = size;
    stream.next_in = (Bytef *)data;

    Bytef out[BUFFER_SIZE];

    luaL_Buffer b;
    luaL_buffinit(L, &b);

    do {
        stream.avail_out = BUFFER_SIZE;
        stream.next_out = out;

        err = deflate(&stream, Z_FINISH);
        if (err < 0) {
            deflateEnd(&stream);
            RaiseError(L, err);
        }

        size_t have = BUFFER_SIZE - stream.avail_out;
        lua_pushlstring(L, (const char *)out, have);
        luaL_addvalue(&b);
    } while (stream.avail_out == 0);

    lua_assert(err == Z_STREAM_END);
    lua_assert(stream.avail_in == 0);

    deflateEnd(&stream);
    luaL_pushresult(&b);

    return 1;
}

static int Inflate(lua_State *L) {
    size_t size = 0;
    int index = 1;

    const char *data = (const char *)CheckData(L, &index, &size);
    int mode = luaL_checkoption(L, index + 1, NULL, ModeOptions);
    int partial = lua_toboolean(L, index + 2);

    z_stream stream;

    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int err = inflateInit2(&stream, WindowBitsOptions[mode]);
    if (err != Z_OK) {
        return luaL_error(L, "inflateInit: %d", err);
    }

    stream.avail_in = size;
    stream.next_in = (Bytef *)data;

    Bytef out[BUFFER_SIZE];

    luaL_Buffer b;
    luaL_buffinit(L, &b);

    do {
        stream.avail_out = BUFFER_SIZE;
        stream.next_out = out;

        err = inflate(&stream, Z_NO_FLUSH);
        if (err < 0) {
            inflateEnd(&stream);
            RaiseError(L, err);
        }

        size_t have = BUFFER_SIZE - stream.avail_out;
        lua_pushlstring(L, (const char *)out, have);
        luaL_addvalue(&b);
    } while (stream.avail_out == 0);

    if (!partial && err != Z_STREAM_END) {
        inflateEnd(&stream);
        RaiseError(L, Z_DATA_ERROR);
    }

    lua_assert(stream.avail_in == 0);

    inflateEnd(&stream);
    luaL_pushresult(&b);

    return 1;
}

LUAMOD_API int luaopen_NopByte_ZLib(lua_State *L) {
    luaL_Reg l[] = {
        // clang-format off
        { "Deflate", Deflate },
        { "Inflate", Inflate },
        { NULL, NULL },
        // clang-format on
    };

    luaL_newlib(L, l);
    return 1;
}
