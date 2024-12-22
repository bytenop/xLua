/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <lua.h>
#include <luaconf.h>
#include <stdio.h>
#include <stdlib.h>

// field of NotifyNode
static const int field_name = 1;
static const int field_parent = 2;
static const int field_notify = 3;
static const int field_children = 4;
static const int field_lockNotify = 5;
static const int field_lockChildren = 6;

// field of NotifyTree
static const int field_root = 1;
static const int field_enabled = 2;

#define getuvfield(L, idx, name) lua_getiuservalue(L, (idx), field_##name)
#define setuvfield(L, idx, name) lua_setiuservalue(L, (idx), field_##name)

#ifdef NDEBUG
// field of Observer
static const int field___raw = 1;
static const int field___root = 2;
static const int field___notifyPool = 3;

#define getfield(L, idx, name) lua_rawgetp(L, (idx), &field_##name)
#define setfield(L, idx, name) lua_rawsetp(L, (idx), &field_##name)

#define ENTER_STACK(L)
#define LEAVE_STACK(L, n)
#else
#define getfield(L, idx, name) lua_getfield(L, (idx), #name)
#define setfield(L, idx, name) lua_setfield(L, (idx), #name)

#define ENTER_STACK(L) int __enterTop = lua_gettop(L)
#define LEAVE_STACK(L, n)                                                \
    int __leaveTop = lua_gettop(L);                                      \
    if (__leaveTop - __enterTop != (n)) {                                \
        luaL_error(L, "Stack corrupted at %s:%d\n", __FILE__, __LINE__); \
    }
#endif

static const char *ObserverLib = "NopByte.Observer";
static const int cachedKeyPathPlaceholder;

static int NotifyTreeDispatchNotify(lua_State *L);
static int ObserverNew(lua_State *L);
static int ObserverTest(lua_State *L, int index);

static int TableNext(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);

    if (lua_next(L, 1)) {
        return 2;
    }

    lua_pushnil(L);
    return 1;
}

static int BuildKeyPath(lua_State *L) {
    char buff[256];
    int size = sizeof(buff) / sizeof(buff[0]);
    int index = 0;

    int top = lua_gettop(L);

    for (int i = 1; i <= top; ++i) {
        int type = lua_type(L, i);

        if (type == LUA_TNUMBER) {
            lua_Integer v = luaL_checkinteger(L, i);

            int space = size - index;
            int n = snprintf(buff + index, space, "[%lli]", v);

            if (n < 0 || n >= space) {
                return luaL_error(L, "key length limit to 256 bytes");
            }

            index += n;
        } else if (type == LUA_TSTRING) {
            const char *s = lua_tostring(L, i);
            const char *f;

            if (index == 0 || *s == '[') {
                f = "%s";
            } else {
                f = ".%s";
            }

            int space = size - index;
            int n = snprintf(buff + index, space, f, s);

            if (n < 0 || n >= space) {
                return luaL_error(L, "key length limit to 256 bytes");
            }

            index += n;
        } else {
            return luaL_error(L, "unsupported key type %s", lua_typename(L, type));
        }
    }

    lua_pushlstring(L, buff, index);
    return 1;
}

static int ParseKeyPath(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    const char *last = path;
    const char *curr = path;
    int index = 0;

    lua_newtable(L);

    while (*curr) {
        char c = *curr;

        if (c == '.' || c == '[') {
            if (curr > last) {
                lua_pushlstring(L, last, curr - last);
                lua_rawseti(L, -2, ++index);
            }

            last = curr + 1;
        } else if (c == ']') {
            if (*last == '\'' || *last == '\"') {
                lua_pushlstring(L, last + 1, curr - last - 2);
            } else {
                lua_pushlstring(L, last, curr - last);
                lua_Integer n = lua_tointeger(L, -1);
                lua_pop(L, 1);
                lua_pushinteger(L, n);
            }

            lua_rawseti(L, -2, ++index);
            last = curr + 1;
        }

        ++curr;
    }

    if (curr > last) {
        lua_pushlstring(L, last, curr - last);
        lua_rawseti(L, -2, ++index);
    }

    return 1;
}

static int CachedKeyPath(lua_State *L) {
    lua_pushcfunction(L, ParseKeyPath);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 1);

    lua_pushvalue(L, 2);
    lua_pushvalue(L, -2);
    lua_rawset(L, 1);

    return 1;
}

static void PushKeyPath(lua_State *L, int index) {
    index = lua_absindex(L, index);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cachedKeyPathPlaceholder);
    lua_pushvalue(L, index);
    lua_gettable(L, -2);
    lua_replace(L, -2);
}

static int GetValueWithPath(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_Unsigned len = lua_rawlen(L, 2);
    lua_Unsigned start = luaL_optinteger(L, 3, 1);

    lua_pushvalue(L, 1);

    for (lua_Unsigned i = start; i <= len; ++i) {
        if (lua_isnil(L, -1)) {
            break;
        }

        lua_rawgeti(L, 2, (lua_Integer)i);
        lua_gettable(L, -2);
        lua_replace(L, -2);
    }

    return 1;
}

static int SetValueWithPath(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checkany(L, 3);

    lua_Unsigned len = lua_rawlen(L, 2);
    lua_pushvalue(L, 1);

    for (lua_Unsigned i = 1; i <= len; ++i) {
        if (i < len) {
            lua_rawgeti(L, 2, (lua_Integer)i);
            lua_gettable(L, -2);
            lua_replace(L, -2);
        } else {
            int type = lua_type(L, -1);
            if (type != LUA_TTABLE) {
                return luaL_error(L, "find %s value in path", lua_typename(L, type));
            }

            lua_rawgeti(L, 2, (lua_Integer)i);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
        }
    }

    return 0;
}

static int JoinNotify(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_settop(L, 3);

    getfield(L, 1, __notifyPool);

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        lua_pushvalue(L, -2);
        if (lua_rawget(L, -4) != LUA_TNIL) {
            const char *last = lua_tostring(L, -1);
            const char *curr = lua_tostring(L, -2);
            return luaL_error(L, "Cannot mount on multi path: %s and %s", last, curr);
        }

        lua_pop(L, 1);
        lua_pushvalue(L, -2);

        if (lua_isnil(L, 3)) {
            lua_pushvalue(L, -2);
        } else {
            lua_pushcfunction(L, BuildKeyPath);
            lua_pushvalue(L, -3);
            lua_pushvalue(L, 3);
            lua_call(L, 2, 1);
        }

        lua_rawset(L, -5);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    getfield(L, 1, __raw);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        if (!ObserverTest(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        if (lua_isnil(L, 3)) {
            lua_pushvalue(L, -2);
        } else {
            lua_pushcfunction(L, BuildKeyPath);
            lua_pushvalue(L, 3);
            lua_pushvalue(L, -4);
            lua_call(L, 2, 1);
        }

        lua_pushcfunction(L, JoinNotify);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, -4);
        lua_call(L, 3, 0);

        lua_pop(L, 2);
    }

    return 0;
}

static int LeaveNotify(lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);

    getfield(L, 1, __notifyPool);

    lua_pushnil(L);
    while (lua_next(L, 2)) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        lua_rawset(L, -4);
    }

    lua_pop(L, 1);
    getfield(L, 1, __raw);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        if (!ObserverTest(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        lua_pushcfunction(L, LeaveNotify);
        lua_pushvalue(L, -2);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 0);

        lua_pop(L, 1);
    }

    return 0;
}

static int NotifyParent(lua_State *L) {
    luaL_checkany(L, 4);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cachedKeyPathPlaceholder);
    getfield(L, 1, __notifyPool);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pushcfunction(L, NotifyTreeDispatchNotify);
        lua_pushvalue(L, -3);

        lua_pushvalue(L, -3);
        lua_gettable(L, -7);

        lua_pushvalue(L, 3);
        lua_pushvalue(L, 4);
        lua_pushvalue(L, 2);
        lua_call(L, 5, 0);

        lua_pop(L, 1);
    }

    return 0;
}

static int NotifyChild(lua_State *L) {
    luaL_checkany(L, 4);

    lua_rawgetp(L, LUA_REGISTRYINDEX, &cachedKeyPathPlaceholder);
    getfield(L, 1, __notifyPool);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pushcfunction(L, BuildKeyPath);
        lua_pushvalue(L, -2);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 1);

        lua_gettable(L, -5);

        lua_pushcfunction(L, NotifyTreeDispatchNotify);
        lua_pushvalue(L, -4);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, 3);
        lua_pushvalue(L, 4);
        lua_call(L, 4, 0);

        lua_pop(L, 2);
    }

    return 0;
}

static void NotifyNodeNew(lua_State *L, int name, int parent) {
    ENTER_STACK(L);

    lua_newuserdatauv(L, 0, 6);

    if (name) {
        lua_pushvalue(L, name);
        setuvfield(L, -2, name);
    }

    if (parent) {
        lua_pushvalue(L, parent);
        setuvfield(L, -2, parent);
    }

    LEAVE_STACK(L, 1);
}

static void NotifyNodeAddChild(lua_State *L, int node, int name) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    name = lua_absindex(L, name);

    if (getuvfield(L, node, children) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        setuvfield(L, node, children);
    }

    lua_pushvalue(L, name);
    if (lua_rawget(L, -2) != LUA_TNIL) {
        lua_remove(L, -2);
        LEAVE_STACK(L, 1);
        return;
    }

    lua_pop(L, 1);

    getuvfield(L, node, lockChildren);
    if (lua_toboolean(L, -1)) {
        luaL_error(L, "Cannot AddChild while DispatchNotify");
        return;
    }

    lua_pop(L, 1);
    NotifyNodeNew(L, name, node);

    lua_pushvalue(L, name);
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);

    lua_remove(L, -2);
    LEAVE_STACK(L, 1);
}

static int NotifyNodeFindChild(lua_State *L, int node, int name) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    name = lua_absindex(L, name);

    if (getuvfield(L, node, children) == LUA_TNIL) {
        lua_pop(L, 1);
        LEAVE_STACK(L, 0);
        return 0;
    }

    lua_pushvalue(L, name);
    if (lua_rawget(L, -2) == LUA_TNIL) {
        lua_pop(L, 2);
        LEAVE_STACK(L, 0);
        return 0;
    }

    lua_remove(L, -2);
    LEAVE_STACK(L, 1);
    return 1;
}

static void NotifyNodeRegisterCallback(lua_State *L, int node, int path, int callback) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    path = lua_absindex(L, path);
    callback = lua_absindex(L, callback);

    getuvfield(L, node, lockNotify);
    if (lua_toboolean(L, -1)) {
        luaL_error(L, "Cannot RegisterCallback while DispatchNotify");
        return;
    }

    lua_pop(L, 1);

    if (getuvfield(L, node, notify) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        setuvfield(L, node, notify);
    }

    lua_pushvalue(L, callback);
    lua_pushvalue(L, path);
    lua_rawset(L, -3);

    lua_pop(L, 1);
    LEAVE_STACK(L, 0);
}

static void NotifyNodeRemoveCallback(lua_State *L, int node, int callback) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    callback = lua_absindex(L, callback);

    getuvfield(L, node, lockNotify);
    if (lua_toboolean(L, -1)) {
        luaL_error(L, "Cannot RemoveCallback while DispatchNotify");
        return;
    }

    lua_pop(L, 1);

    if (getuvfield(L, node, notify) == LUA_TNIL) {
        lua_pop(L, 1);
        LEAVE_STACK(L, 0);
        return;
    }

    lua_pushvalue(L, callback);
    lua_pushnil(L);
    lua_rawset(L, -3);

    lua_pushnil(L);
    if (lua_next(L, -2)) {
        lua_pop(L, 3);
        LEAVE_STACK(L, 0);
        return;
    }

    lua_pushnil(L);
    setuvfield(L, node, notify);
    lua_pop(L, 1);

    while (1) {
        if (getuvfield(L, node, parent) == LUA_TNIL) {
            lua_pop(L, 1);
            break;
        }

        if (getuvfield(L, node, notify) != LUA_TNIL) {
            lua_pop(L, 2);
            break;
        }

        if (getuvfield(L, node, children) != LUA_TNIL) {
            lua_pop(L, 3);
            break;
        }

        lua_pop(L, 2);

        getuvfield(L, -1, children);
        getuvfield(L, node, name);
        lua_pushnil(L);
        lua_rawset(L, -3);

        lua_pushnil(L);
        if (lua_next(L, -2)) {
            lua_pop(L, 4);
            break;
        }

        lua_pop(L, 1);
        lua_pushnil(L);
        setuvfield(L, -2, children);

        lua_replace(L, node);
    }

    LEAVE_STACK(L, 0);
}

static void NotifyNodeInvokeCallback(lua_State *L, int node, int base, int argc) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    base = lua_absindex(L, base);

    if (getuvfield(L, node, notify) != LUA_TNIL) {
        lua_pushboolean(L, 1);
        setuvfield(L, node, lockNotify);

        lua_pushnil(L);
        while (lua_next(L, -2)) {
            lua_pop(L, 1);
            lua_pushvalue(L, -1);

            for (int i = 0; i < argc; ++i) {
                lua_pushvalue(L, base + i);
            }

            lua_call(L, argc, 0);
        }

        lua_pushnil(L);
        setuvfield(L, node, lockNotify);
    }

    lua_pop(L, 1);

    if (argc > 2) {
        LEAVE_STACK(L, 0);
        return;
    }

    if (getuvfield(L, node, children) == LUA_TNIL) {
        lua_pop(L, 1);
        LEAVE_STACK(L, 0);
        return;
    }

    lua_pushboolean(L, 1);
    setuvfield(L, node, lockChildren);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        lua_pushvalue(L, base + 0);
        if (!lua_isnil(L, -1)) {
            lua_pushvalue(L, -3);
            lua_gettable(L, -2);
            lua_replace(L, -2);
        }

        lua_pushvalue(L, base + 1);
        if (!lua_isnil(L, -1)) {
            lua_pushvalue(L, -4);
            lua_gettable(L, -2);
            lua_replace(L, -2);
        }

        if (!lua_compare(L, -1, -2, LUA_OPEQ)) {
            NotifyNodeInvokeCallback(L, -3, -2, 2);
        }

        lua_pop(L, 3);
    }

    lua_pushnil(L);
    setuvfield(L, node, lockChildren);

    lua_pop(L, 1);
    LEAVE_STACK(L, 0);
}

static void NotifyNodeDispatchNotify(lua_State *L, int node, int path, int index,
                                     int argc) {
    ENTER_STACK(L);

    node = lua_absindex(L, node);
    path = lua_absindex(L, path);

    if (lua_rawgeti(L, path, index) == LUA_TNIL) {
        lua_pop(L, 1);
        NotifyNodeInvokeCallback(L, node, path + 1, argc);
        LEAVE_STACK(L, 0);
        return;
    }

    if (!NotifyNodeFindChild(L, node, -1)) {
        lua_pop(L, 1);
        LEAVE_STACK(L, 0);
        return;
    }

    lua_replace(L, node);
    lua_pop(L, 1);

    NotifyNodeDispatchNotify(L, node, path, index + 1, argc);
    LEAVE_STACK(L, 0);
}

static int NotifyTreeNew(lua_State *L) {
    lua_newuserdatauv(L, 0, 2);

    NotifyNodeNew(L, 0, 0);
    setuvfield(L, 1, root);

    lua_pushboolean(L, 1);
    setuvfield(L, 1, enabled);

    return 1;
}

static int NotifyTreeActiveNotify(lua_State *L) {
    int enabled = lua_toboolean(L, 2);

    getuvfield(L, 1, enabled);
    if (lua_toboolean(L, -1) == enabled) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, enabled);
    setuvfield(L, 1, enabled);

    lua_pushboolean(L, 1);
    return 1;
}

static int NotifyTreeRegisterCallback(lua_State *L) {
    int i = 0;
    getuvfield(L, 1, root);

    while (lua_rawgeti(L, 2, ++i) != LUA_TNIL) {
        NotifyNodeAddChild(L, -2, -1);
        lua_replace(L, -3);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    NotifyNodeRegisterCallback(L, -1, 2, 3);

    return 0;
}

static int NotifyTreeRemoveCallback(lua_State *L) {
    int i = 0;
    getuvfield(L, 1, root);

    while (lua_rawgeti(L, 2, ++i) != LUA_TNIL) {
        if (!NotifyNodeFindChild(L, -2, -1)) {
            return 0;
        }

        lua_replace(L, -3);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    NotifyNodeRemoveCallback(L, -1, 3);

    return 0;
}

static int NotifyTreeDispatchNotify(lua_State *L) {
    getuvfield(L, 1, enabled);
    if (!lua_toboolean(L, -1)) {
        return 0;
    }

    lua_pop(L, 1);

    int argc = lua_gettop(L) - 2;
    getuvfield(L, 1, root);

    NotifyNodeDispatchNotify(L, -1, 2, 1, argc);
    return 0;
}

static int MetatableIndex(lua_State *L) {
    getfield(L, 1, __raw);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    return 1;
}

static int MetatableIndexAndReport(lua_State *L) {
    getfield(L, 1, __notifyPool);

    lua_pushvalue(L, lua_upvalueindex(1));
    if (lua_rawget(L, -2) != LUA_TNIL && lua_toboolean(L, lua_upvalueindex(3))) {
        lua_pushvalue(L, lua_upvalueindex(2));

        lua_pushcfunction(L, BuildKeyPath);
        lua_pushvalue(L, -3);
        lua_pushvalue(L, 2);
        lua_call(L, 2, 1);

        lua_call(L, 1, 0);
    }

    getfield(L, 1, __raw);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    return 1;
}

static int MetatableNewIndex(lua_State *L) {
    getfield(L, 1, __raw);

    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    if (lua_compare(L, -1, 3, LUA_OPEQ)) {
        return 0;
    }

    if (lua_type(L, -1) == LUA_TTABLE && ObserverTest(L, -1)) {
        lua_pushcfunction(L, LeaveNotify);
        lua_pushvalue(L, -2);
        getfield(L, 1, __notifyPool);
        lua_call(L, 2, 0);
    }

    if (lua_type(L, 3) == LUA_TTABLE) {
        lua_pushcfunction(L, ObserverNew);
        lua_pushvalue(L, 3);
        lua_pushvalue(L, 2);
        getfield(L, 1, __notifyPool);
        lua_call(L, 3, 1);

        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
        } else {
            lua_replace(L, 3);
        }
    }

    lua_pushvalue(L, 2);
    lua_pushvalue(L, 3);
    lua_rawset(L, -4);

#define InvokeNotify(f)      \
    lua_pushcfunction(L, f); \
    lua_pushvalue(L, 1);     \
    lua_pushvalue(L, 2);     \
    lua_pushvalue(L, 3);     \
    lua_pushvalue(L, -5);    \
    lua_call(L, 4, 0);

    if (lua_isnil(L, -1)) {
        InvokeNotify(NotifyChild);
        InvokeNotify(NotifyParent);
    } else if (lua_isnil(L, 3)) {
        InvokeNotify(NotifyParent);
        InvokeNotify(NotifyChild);
    } else {
        InvokeNotify(NotifyChild);
    }

#undef InvokeNotify

    return 0;
}

static int MetatablePairs(lua_State *L) {
    lua_pushcfunction(L, TableNext);
    getfield(L, 1, __raw);
    return 2;
}

static int MetatableLen(lua_State *L) {
    getfield(L, 1, __raw);
    lua_pushinteger(L, (lua_Integer)lua_rawlen(L, -1));
    return 1;
}

static int ObserverTest(lua_State *L, int index) {
    if (!lua_getmetatable(L, index)) {
        return 0;
    }

    luaL_getmetatable(L, ObserverLib);
    int equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);

    return equal;
}

static void ObserverCheck(lua_State *L, int index) {
    if (!ObserverTest(L, index)) {
        luaL_typeerror(L, index, ObserverLib);
    }
}

static void ObserverCheckRoot(lua_State *L, int index) {
    ObserverCheck(L, index);

    if (getfield(L, index, __root) == LUA_TNIL) {
        luaL_argerror(L, index, "root observer expected");
    }

    lua_pop(L, 1);
}

static void ObserverPushNotify(lua_State *L, int index) {
    getfield(L, index, __notifyPool);

    lua_pushnil(L);
    if (lua_next(L, -2)) {
        lua_pop(L, 1);
        lua_replace(L, -2);
        return;
    }

    luaL_error(L, "notify node expected in observer");
}

static int ObserverNew(lua_State *L) {
    if (lua_isnone(L, 1)) {
        lua_newtable(L);
    } else {
        luaL_checktype(L, 1, LUA_TTABLE);
    }

    int root = lua_gettop(L) == 1;

    if (root) {
        lua_settop(L, 3);
    } else {
        luaL_checkany(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);
    }

    if (lua_isnil(L, 3)) {
        lua_newtable(L);

        lua_pushcfunction(L, NotifyTreeNew);
        lua_call(L, 0, 1);

        lua_pushliteral(L, "");
        lua_rawset(L, -3);
    } else {
        lua_newtable(L);

        lua_pushnil(L);
        while (lua_next(L, 3)) {
            lua_pushvalue(L, -2);

            lua_pushcfunction(L, BuildKeyPath);
            lua_pushvalue(L, -3);
            lua_pushvalue(L, 2);
            lua_call(L, 2, 1);

            lua_rawset(L, -5);
            lua_pop(L, 1);
        }
    }

    lua_replace(L, 3);

    if (lua_getmetatable(L, 1)) {
        if (!ObserverTest(L, 1)) {
            return 0;
        }

        if (getfield(L, 1, __root) != LUA_TNIL) {
            luaL_argerror(L, 1, "child observer expected");
        }

        lua_pushcfunction(L, JoinNotify);
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 3);
        lua_call(L, 2, 0);

        lua_pushvalue(L, 1);
        return 1;
    }

    lua_newtable(L);

    lua_pushnil(L);
    while (lua_next(L, 1)) {
        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_pushcfunction(L, ObserverNew);
            lua_pushvalue(L, -2);
            lua_pushvalue(L, -4);
            lua_pushvalue(L, 3);
            lua_call(L, 3, 1);

            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
            } else {
                lua_replace(L, -2);
            }
        }

        lua_pushvalue(L, -2);
        lua_pushvalue(L, -2);
        lua_rawset(L, -5);

        lua_pushvalue(L, -2);
        lua_pushnil(L);
        lua_rawset(L, 1);

        lua_pop(L, 1);
    }

    setfield(L, 1, __raw);

    if (root) {
        lua_pushboolean(L, 1);
        setfield(L, 1, __root);
    }

    lua_pushvalue(L, 3);
    setfield(L, 1, __notifyPool);

    lua_pushvalue(L, 1);
    luaL_setmetatable(L, ObserverLib);

    return 1;
}

static int ObserverRaw(lua_State *L) {
    ObserverCheck(L, 1);

    if (lua_isnone(L, 2)) {
        getfield(L, 1, __raw);
        return 1;
    }

    getfield(L, 1, __raw);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    return 1;
}

static int ObserverGet(lua_State *L) {
    ObserverCheck(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);

    lua_pushcfunction(L, GetValueWithPath);
    lua_pushvalue(L, 1);
    PushKeyPath(L, 2);
    lua_call(L, 2, 1);

    return 1;
}

static int ObserverSet(lua_State *L) {
    ObserverCheck(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checkany(L, 3);

    lua_pushcfunction(L, SetValueWithPath);
    lua_pushvalue(L, 1);
    PushKeyPath(L, 2);
    lua_pushvalue(L, 3);
    lua_call(L, 3, 0);

    return 0;
}

static int ObserverNext(lua_State *L) {
    lua_settop(L, 2);

    if (luaL_getmetafield(L, 1, "__pairs") == LUA_TNIL) {
        return TableNext(L);
    }

    lua_pushvalue(L, 1);
    lua_call(L, 1, 2);

    lua_pushvalue(L, 2);
    lua_call(L, 2, 2);

    return 2;
}

static int ObserverActiveNotify(lua_State *L) {
    ObserverCheckRoot(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    lua_pushcfunction(L, NotifyTreeActiveNotify);
    ObserverPushNotify(L, 1);
    lua_pushvalue(L, 2);
    lua_call(L, 2, 1);

    return 1;
}

static int ObserverReportPath(lua_State *L) {
    if (lua_isnone(L, 1)) {
        luaL_getmetatable(L, ObserverLib);
        lua_pushcfunction(L, MetatableIndex);
        lua_setfield(L, -2, "__index");
        return 0;
    }

    ObserverCheckRoot(L, 1);
    ObserverPushNotify(L, 1);

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);

    lua_pushboolean(L, 1);
    lua_pushcclosure(L, MetatableIndexAndReport, 3);

    luaL_getmetatable(L, ObserverLib);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "__index");

    return 0;
}

static int ObserverToggleReport(lua_State *L) {
    luaL_checktype(L, 1, LUA_TBOOLEAN);

    luaL_getmetatable(L, ObserverLib);
    lua_getfield(L, -1, "__index");

    if (lua_tocfunction(L, -1) == MetatableIndex) {
        return 0;
    }

    lua_pushvalue(L, 1);
    lua_setupvalue(L, -2, 3);

    return 0;
}

static int ObserverSetupWatch(lua_State *L) {
    ObserverCheckRoot(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushcfunction(L, NotifyTreeRegisterCallback);
    ObserverPushNotify(L, 1);
    PushKeyPath(L, 2);
    lua_pushvalue(L, 3);
    lua_call(L, 3, 0);

    lua_pushvalue(L, 3);
    return 1;
}

static int ObserverRemoveWatch(lua_State *L) {
    ObserverCheckRoot(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    lua_pushcfunction(L, NotifyTreeRemoveCallback);
    ObserverPushNotify(L, 1);
    PushKeyPath(L, 2);
    lua_pushvalue(L, 3);
    lua_call(L, 3, 0);

    return 0;
}

LUAMOD_API int luaopen_NopByte_Observer(lua_State *L) {
    luaL_Reg metatable[] = {
        // clang-format off
        { "__index", MetatableIndex },
        { "__newindex", MetatableNewIndex },
        { "__pairs", MetatablePairs },
        { "__len", MetatableLen },
        { NULL, NULL },
        // clang-format on
    };

    luaL_Reg Observer[] = {
        // clang-format off
        { "BuildKeyPath", BuildKeyPath },
        { "ParseKeyPath", ParseKeyPath },
        { "CachedKeyPath", NULL },

        { "New", ObserverNew },
        { "Raw", ObserverRaw },
        { "Get", ObserverGet },
        { "Set", ObserverSet },
        { "Next", ObserverNext },

        { "ActiveNotify", ObserverActiveNotify },
        { "ReportPath", ObserverReportPath },
        { "ToggleReport", ObserverToggleReport },
        { "SetupWatch", ObserverSetupWatch },
        { "RemoveWatch", ObserverRemoveWatch },
        { NULL, NULL },
        // clang-format on
    };

    luaL_newmetatable(L, ObserverLib);
    luaL_setfuncs(L, metatable, 0);

    luaL_newlib(L, Observer);

    lua_newtable(L);
    lua_newtable(L);

    lua_pushliteral(L, "__mode");
    lua_pushliteral(L, "kv");
    lua_rawset(L, -3);

    lua_pushliteral(L, "__index");
    lua_pushcfunction(L, CachedKeyPath);
    lua_rawset(L, -3);

    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "CachedKeyPath");
    lua_rawsetp(L, LUA_REGISTRYINDEX, &cachedKeyPathPlaceholder);

    return 1;
}
