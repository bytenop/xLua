/*
 * Copyright (C) NopByte, All rights reserved!
 *
 * Author: Zaxbbun.Du
 */

#include <lauxlib.h>
#include <lua.h>
#include <luaconf.h>
#include <stdlib.h>

typedef unsigned int utf8_t;

enum {
    kTableInitCapa = 1,
};

typedef enum {
    kFlagNone = 0x00,
    kFlagUsed = 0x01,
    kFlagLeaf = 0x02,
} Flag;

typedef struct {
    Flag flag;
    utf8_t key;
    int prev;
    int next;
    void *value;
} HashNode;

typedef struct {
    int capa;
    int size;
    int free;
    HashNode *nodes;
} HashTable;

static HashTable *wordFilter = NULL;

static HashTable *HashTableCreate(int capa) {
    HashTable *self = (HashTable *)malloc(sizeof(*self));
    if (self == NULL) {
        return NULL;
    }

    HashNode *nodes = (HashNode *)malloc(capa * sizeof(*nodes));
    if (nodes == NULL) {
        free(self);
        return NULL;
    }

    for (int i = 0; i < capa; ++i) {
        nodes[i].flag = kFlagNone;
    }

    self->capa = capa;
    self->size = 0;
    self->free = 0;
    self->nodes = nodes;

    return self;
}

static void HashTableRelease(HashTable *self) {
    for (int i = 0; i < self->capa; ++i) {
        HashNode *node = &self->nodes[i];
        if (node->flag != kFlagNone) {
            HashTable *value = (HashTable *)node->value;
            if (value) {
                HashTableRelease(value);
            }
        }
    }

    free(self->nodes);
    free(self);
}

static HashNode *HashTableFind(HashTable *self, utf8_t key) {
    int slot = (int)(key % self->capa);
    HashNode *node = &self->nodes[slot];

    while (node->flag != kFlagNone) {
        if (node->key == key) {
            return node;
        }

        int next = node->next;
        if (next < 0) {
            return NULL;
        }

        node = self->nodes + next;
    }

    return NULL;
}

static HashNode *HashTableSpace(HashTable *self) {
    while (self->free < self->capa) {
        HashNode *node = &self->nodes[self->free++];
        if (node->flag == kFlagNone) {
            return node;
        }
    }

    return NULL;
}

static HashNode *HashTableInsert(HashTable *self, utf8_t key);

static int HashTableGrow(HashTable *self) {
    int capa = self->capa << 1;
    HashNode *nodes = (HashNode *)malloc(capa * sizeof(*nodes));

    if (nodes == NULL) {
        return -1;
    }

    for (int i = 0; i < capa; ++i) {
        nodes[i].flag = kFlagNone;
    }

    int capaLast = self->capa;
    HashNode *nodesLast = self->nodes;

    self->capa = capa;
    self->size = 0;
    self->free = 0;
    self->nodes = nodes;

    for (int i = 0; i < capaLast; ++i) {
        HashNode *lastNode = &nodesLast[i];
        if (lastNode->flag != kFlagNone) {
            HashNode *currNode = HashTableInsert(self, lastNode->key);
            currNode->flag = lastNode->flag;
            currNode->value = lastNode->value;
        }
    }

    free(nodesLast);
    return 0;
}

static HashNode *HashTableInsert(HashTable *self, utf8_t key) {
    HashNode *node = HashTableFind(self, key);
    if (node) {
        return node;
    }

    if (self->size == self->capa) {
        if (HashTableGrow(self)) {
            return NULL;
        }
    }

    int slot = (int)(key % self->capa);
    node = self->nodes + slot;

    if (node->flag == kFlagNone) {
        node->prev = -1;
        node->next = -1;
    } else {
        HashNode *freeNode = HashTableSpace(self);
        int freeSlot = (int)(freeNode - self->nodes);
        int mainSlot = (int)(node->key % self->capa);

        if (mainSlot == slot) {
            freeNode->prev = mainSlot;
            freeNode->next = node->next;

            if (node->next != -1) {
                HashNode *nnode = &self->nodes[node->next];
                nnode->prev = freeSlot;
            }

            node->next = freeSlot;
            node = freeNode;
        } else {
            HashNode *pnode = &self->nodes[node->prev];
            pnode->next = freeSlot;

            if (node->next != -1) {
                HashNode *nnode = &self->nodes[node->next];
                nnode->prev = freeSlot;
            }

            *freeNode = *node;
            node->prev = -1;
            node->next = -1;
        }
    }

    node->key = key;
    node->flag = kFlagUsed;
    node->value = NULL;

    ++self->size;
    return node;
}

static void CheckWordFilter(lua_State *L, const HashTable *wordFilter) {
    if (wordFilter == NULL) {
        luaL_error(L, "WordFilter has not been initialized");
    }
}

static int WordFilterInitialize(lua_State *L) {
    (void)L;

    if (wordFilter) {
        return luaL_error(L, "WordFilter already initialized");
    }

    wordFilter = HashTableCreate(kTableInitCapa);
    if (wordFilter == NULL) {
        return luaL_error(L, "oom");
    }

    return 0;
}

static int WordFilterRelease(lua_State *L) {
    (void)L;

    if (wordFilter) {
        HashTableRelease(wordFilter);
        wordFilter = NULL;
    }

    return 0;
}

static int WordFilterBlock(lua_State *L) {
    CheckWordFilter(L, wordFilter);

    int top = lua_gettop(L);
    HashTable *table = wordFilter;
    HashNode *node = NULL;

    for (int i = 1; i <= top; ++i) {
        if (node && table == NULL) {
            table = HashTableCreate(kTableInitCapa);
            if (table == NULL) {
                return luaL_error(L, "oom");
            }
            node->value = table;
        }

        utf8_t key = (utf8_t)luaL_checkinteger(L, i);
        node = HashTableInsert(table, key);

        if (node == NULL) {
            return luaL_error(L, "oom");
        }

        table = (HashTable *)node->value;
    }

    if (node) {
        node->flag = kFlagLeaf;
    }

    return 0;
}

static int WordFilterFilter(lua_State *L) {
    CheckWordFilter(L, wordFilter);

    int top = lua_gettop(L);
    int replace = 0;

    for (int i = 1; i <= top; ++i) {
        HashTable *table = wordFilter;
        int leaf = 0;

        for (int j = i; j <= top; ++j) {
            utf8_t key = (utf8_t)luaL_checkinteger(L, j);
            HashNode *node = HashTableFind(table, key);

            if (node == NULL) {
                break;
            }

            if (node->flag == kFlagLeaf) {
                leaf = j;
            }

            table = (HashTable *)node->value;
            if (table == NULL) {
                break;
            }
        }

        if (leaf > 0) {
            for (int k = i; k <= leaf; ++k) {
                lua_pushinteger(L, '*');
                lua_replace(L, k);
            }

            i = leaf;
            replace = 1;
        }
    }

    return replace ? top : 0;
}

LUAMOD_API int luaopen_NopByte_WordFilter_Core(lua_State *L) {
    luaL_Reg l[] = {
        // clang-format off
        { "Initialize", WordFilterInitialize },
        { "Release", WordFilterRelease },
        { "Block", WordFilterBlock },
        { "Filter", WordFilterFilter },
        { NULL, NULL },
        // clang-format on
    };

    luaL_newlib(L, l);
    return 1;
}
