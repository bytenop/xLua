add_link_options($<$<CONFIG:RELEASE>:-s>)

set(LPEG_SRC
    third_party/lpeg/lpcap.c
    third_party/lpeg/lpcode.c
    third_party/lpeg/lpcset.c
    third_party/lpeg/lpprint.c
    third_party/lpeg/lptree.c
    third_party/lpeg/lpvm.c
)

set(LUA_CJSON_SRC
    third_party/lua-cjson/fpconv.c
    third_party/lua-cjson/lua_cjson.c
    third_party/lua-cjson/strbuf.c
)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-Dinline=__inline)
    add_definitions(-Dstrncasecmp=_strnicmp)
endif()

set(LUA_CMSGPACK_SRC
    third_party/lua-cmsgpack/lua_cmsgpack.c
)

set(NOPBYTE_SRC
    third_party/nopbyte/lua_hashlib.c
    third_party/nopbyte/lua_nopbyte.c
    third_party/nopbyte/lua_observer.c
    third_party/nopbyte/lua_packet.c
    third_party/nopbyte/lua_random.c
    third_party/nopbyte/lua_wordfilter.c
    third_party/nopbyte/lua_zlib.c
)

set(SPROTO_SRC
    third_party/sproto/lsproto.c
    third_party/sproto/sproto.c
)

set(XXTEA_SRC
    third_party/xxtea-lua/src/lua_xxtea.c
    third_party/xxtea-lua/src/xxtea.c
)

set(ZLIB_SRC
    third_party/zlib/adler32.c
    third_party/zlib/compress.c
    third_party/zlib/crc32.c
    third_party/zlib/deflate.c
    third_party/zlib/gzclose.c
    third_party/zlib/gzlib.c
    third_party/zlib/gzread.c
    third_party/zlib/gzwrite.c
    third_party/zlib/infback.c
    third_party/zlib/inffast.c
    third_party/zlib/inflate.c
    third_party/zlib/inftrees.c
    third_party/zlib/trees.c
    third_party/zlib/uncompr.c
    third_party/zlib/zutil.c
)

set(THIRDPART_INC
    third_party/lpeg
    third_party/lua-cjson
    third_party/lua-cmsgpack
    third_party/nopbyte
    third_party/sproto
    third_party/xxtea-lua/src
    third_party/zlib
)

set(THIRDPART_SRC
    ${LPEG_SRC}
    ${LUA_CJSON_SRC}
    ${LUA_CMSGPACK_SRC}
    ${NOPBYTE_SRC}
    ${SPROTO_SRC}
    ${XXTEA_SRC}
    ${ZLIB_SRC}
    third_party/lua-extlibs.c
)

set_property(
    SOURCE ${THIRDPART_SRC}
    APPEND
    PROPERTY COMPILE_DEFINITIONS
    LUA_LIB
)
