/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <stdint.h>
#include <time.h>

typedef uint32_t Rand32;

typedef struct {
    Rand32 s[4];
} RandState;

static const char RandomLib[] = "NopByte.Random";

static inline lua_Number r2d(Rand32 x) {
    return x * 1.0 / (Rand32)~0;
}

static inline Rand32 rotl(Rand32 x, int k) {
    return (x << k) | (x >> (32 - k));
}

static Rand32 next(Rand32 *s) {
    Rand32 v = rotl(s[0] + s[3], 7) + s[0];
    Rand32 t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = rotl(s[3], 11);

    return v;
}

static Rand32 project(Rand32 ran, Rand32 n, RandState *state) {
    if ((n & (n + 1)) == 0) { /* is 'n + 1' a power of 2? */
        return ran & n; /* no bias */
    }

    Rand32 lim = n;

    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);

    lua_assert((lim & (lim + 1)) == 0 /* 'lim + 1' is a power of 2, */
               && lim >= n /* not smaller than 'n', */
               && (lim >> 1) < n); /* and it is the smallest one */

    while ((ran &= lim) > n) { /* project 'ran' into [0..lim] */
        ran = next(state->s); /* not inside [0..n]? try again */
    }

    return ran;
}

static int RandomNew(lua_State *L) {
    Rand32 s0, s1, s2, s3;

    if (lua_isnone(L, 2)) {
        s0 = time(NULL);
        s1 = (size_t)L;
        s2 = 0xff;
        s3 = 0x1b;
    } else {
        s0 = luaL_checkinteger(L, 2);
        s1 = luaL_optinteger(L, 3, 0xcc);
        s2 = luaL_optinteger(L, 4, 0xff);
        s3 = luaL_optinteger(L, 5, 0x1b);
    }

    RandState *state = (RandState *)lua_newuserdata(L, sizeof(RandState));
    Rand32 *s = state->s;

    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;

    lua_pushvalue(L, 1);
    lua_setmetatable(L, -2);

    return 1;
}

static int RandomRandom(lua_State *L) {
    RandState *state = (RandState *)luaL_checkudata(L, 1, RandomLib);
    Rand32 v = next(state->s); /* next pseudo-random value */
    lua_Integer low, up, p;

    switch (lua_gettop(L)) { /* check number of arguments */
        case 1: { /* no arguments */
            lua_pushnumber(L, r2d(v)); /* float between 0 and 1 */
            return 1;
        }
        case 2: { /* only upper limit */
            low = 1;
            up = luaL_checkinteger(L, 2);
            if (up == 0) { /* single 0 as argument? */
                lua_pushinteger(L, v); /* full random integer */
                return 1;
            }
            break;
        }
        case 3: { /* lower and upper limits */
            low = luaL_checkinteger(L, 2);
            up = luaL_checkinteger(L, 3);
            break;
        }
        default: {
            return luaL_error(L, "wrong number of arguments");
        }
    }

    /* random integer in the interval [low, up] */
    luaL_argcheck(L, low <= up, 1, "interval is empty");

    /* project random integer into the interval [0, up - low] */
    p = project(v, up - low, state);
    lua_pushinteger(L, p + low);

    state->s[0] ^= (up << 16) | low;
    state->s[3] ^= (low << 16) | up;

    return 1;
}

static int RandomJump(lua_State *L) {
    static const Rand32 JUMP[] = {0x8764000b, 0xf542d2d3, 0x6fa035c3,
                                  0x77f2db5b};
    const int size = sizeof(JUMP) / sizeof(*JUMP);

    RandState *state = (RandState *)luaL_checkudata(L, 1, RandomLib);
    Rand32 *s = state->s;

    Rand32 s0 = 0;
    Rand32 s1 = 0;
    Rand32 s2 = 0;
    Rand32 s3 = 0;

    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < 32; j++) {
            if (JUMP[i] & (Rand32)1 << j) {
                s0 ^= s[0];
                s1 ^= s[1];
                s2 ^= s[2];
                s3 ^= s[3];
            }
            next(s);
        }
    }

    s[0] = s0;
    s[1] = s1;
    s[2] = s2;
    s[3] = s3;

    return 0;
}

static int RandomDump(lua_State *L) {
    RandState *state = (RandState *)luaL_checkudata(L, 1, RandomLib);
    Rand32 *s = state->s;

    lua_pushinteger(L, s[0]);
    lua_pushinteger(L, s[1]);
    lua_pushinteger(L, s[2]);
    lua_pushinteger(L, s[3]);

    return 4;
}

static int RandomHash(lua_State *L) {
    RandState *state = (RandState *)luaL_checkudata(L, 1, RandomLib);
    Rand32 *s = state->s;
    Rand32 c[4] = {s[0], s[1], s[2], s[3]};

    lua_pushinteger(L, next(c));
    return 1;
}

LUAMOD_API int luaopen_NopByte_Random(lua_State *L) {
    luaL_Reg l[] = {
        // clang-format off
        {"__index", NULL},
        {"Random", RandomRandom},
        {"Jump", RandomJump},
        {"Dump", RandomDump},
        {"Hash", RandomHash},
        {NULL, NULL},
        // clang-format on
    };

    luaL_newmetatable(L, RandomLib);
    luaL_setfuncs(L, l, 0);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_newtable(L);
    lua_pushcfunction(L, RandomNew);
    lua_setfield(L, -2, "__call");

    lua_setmetatable(L, -2);
    return 1;
}
