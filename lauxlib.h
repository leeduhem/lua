/*
** $Id: lauxlib.h $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "lua.h"


/* global table */
#define LUA_GNAME	"_G"


struct luaL_Buffer;


/* extra error code for 'luaL_loadfilex' */
constexpr int LUA_ERRFILE = LUA_ERRERR + 1;


/* key, in the registry, for table of loaded modules */
constexpr const char *LUA_LOADED_TABLE = "_LOADED";


/* key, in the registry, for table of preloaded loaders */
constexpr const char *LUA_PRELOAD_TABLE = "_PRELOAD";


struct luaL_Reg {
  const char *name;
  lua_CFunction func;
};


constexpr size_t LUAL_NUMSIZES = (sizeof(lua_Integer)*16 + sizeof(lua_Number));

LUALIB_API void (luaL_checkversion_) (lua_State *L, lua_Number ver, size_t sz);

LUALIB_API inline void luaL_checkversion(lua_State *L) {
  luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES);
}

LUALIB_API int (luaL_getmetafield) (lua_State *L, int obj, const char *e);
LUALIB_API int (luaL_callmeta) (lua_State *L, int obj, const char *e);
LUALIB_API const char *(luaL_tolstring) (lua_State *L, int idx, size_t *len);
LUALIB_API int (luaL_argerror) (lua_State *L, int arg, const char *extramsg);
LUALIB_API int (luaL_typeerror) (lua_State *L, int arg, const char *tname);
LUALIB_API const char *(luaL_checklstring) (lua_State *L, int arg,
                                                          size_t *l);
LUALIB_API const char *(luaL_optlstring) (lua_State *L, int arg,
                                          const char *def, size_t *l);
LUALIB_API lua_Number (luaL_checknumber) (lua_State *L, int arg);
LUALIB_API lua_Number (luaL_optnumber) (lua_State *L, int arg, lua_Number def);

LUALIB_API lua_Integer (luaL_checkinteger) (lua_State *L, int arg);
LUALIB_API lua_Integer (luaL_optinteger) (lua_State *L, int arg,
                                          lua_Integer def);

LUALIB_API void (luaL_checkstack) (lua_State *L, int sz, const char *msg);
LUALIB_API void (luaL_checktype) (lua_State *L, int arg, int t);
LUALIB_API void (luaL_checkany) (lua_State *L, int arg);

LUALIB_API int   (luaL_newmetatable) (lua_State *L, const char *tname);
LUALIB_API void  (luaL_setmetatable) (lua_State *L, const char *tname);
LUALIB_API void *(luaL_testudata) (lua_State *L, int ud, const char *tname);
LUALIB_API void *(luaL_checkudata) (lua_State *L, int ud, const char *tname);

LUALIB_API void (luaL_where) (lua_State *L, int lvl);
LUALIB_API int (luaL_error) (lua_State *L, const char *fmt, ...);

LUALIB_API int (luaL_checkoption) (lua_State *L, int arg, const char *def,
                                   const char *const lst[]);

LUALIB_API int (luaL_fileresult) (lua_State *L, int stat, const char *fname);
LUALIB_API int (luaL_execresult) (lua_State *L, int stat);


/* predefined references */
constexpr int LUA_NOREF = -2;
constexpr int LUA_REFNIL = -1;

LUALIB_API int (luaL_ref) (lua_State *L, int t);
LUALIB_API void (luaL_unref) (lua_State *L, int t, int ref);

LUALIB_API int (luaL_loadfilex) (lua_State *L, const char *filename,
                                               const char *mode);

LUALIB_API inline int luaL_loadfile(lua_State *L, const char *f) {
  return luaL_loadfilex(L, f, nullptr);
}

LUALIB_API int (luaL_loadbufferx) (lua_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
LUALIB_API int (luaL_loadstring) (lua_State *L, const char *s);

LUALIB_API lua_State *(luaL_newstate) (void);

LUALIB_API lua_Integer (luaL_len) (lua_State *L, int idx);

LUALIB_API void luaL_addgsub (luaL_Buffer *b, const char *s,
                                     const char *p, const char *r);
LUALIB_API const char *(luaL_gsub) (lua_State *L, const char *s,
                                    const char *p, const char *r);

LUALIB_API void (luaL_setfuncs) (lua_State *L, const luaL_Reg *l, int nup);

LUALIB_API int (luaL_getsubtable) (lua_State *L, int idx, const char *fname);

LUALIB_API void (luaL_traceback) (lua_State *L, lua_State *L1,
                                  const char *msg, int level);

LUALIB_API void (luaL_requiref) (lua_State *L, const char *modname,
                                 lua_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define luaL_newlibtable(L,l)	\
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)  \
  (luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

LUALIB_API inline bool luaL_argcheck(lua_State *L, bool cond, int arg, const char *extramsg) {
  return cond || luaL_argerror(L, arg, extramsg);
}

LUALIB_API inline bool luaL_argexpected(lua_State *L, bool cond, int arg, const char *tname) {
  return cond || luaL_typeerror(L, arg, tname);
}

LUALIB_API inline const char *luaL_checkstring(lua_State *L, int n) {
  return luaL_checklstring(L, n, nullptr);
}

LUALIB_API inline const char *luaL_optstring(lua_State *L, int n, const char *d) {
  return luaL_optlstring(L, n, d, nullptr);
}

LUALIB_API inline const char *luaL_typename(lua_State *L, int i) {
  return lua_typename(L, lua_type(L, i));
}

LUALIB_API inline bool luaL_dofile(lua_State *L, const char *fn) {
  return luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0);
}

LUALIB_API inline bool luaL_dostring(lua_State *L, const char *s) {
  return luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0);
}

LUALIB_API inline int luaL_getmetatable(lua_State *L, const char *n) {
  return lua_getfield(L, LUA_REGISTRYINDEX, n);
}

#define luaL_opt(L,f,n,d)	(lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))

LUALIB_API inline int luaL_loadbuffer(lua_State *L, const char *s, size_t sz, const char *n) {
  return luaL_loadbufferx(L, s, sz, n, nullptr);
}

/* push the value used to represent failure/error */
LUALIB_API inline void luaL_pushfail(lua_State *L) {
  return lua_pushnil(L);
}

/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct luaL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  lua_State *L;
  union {
    LUAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[LUAL_BUFFERSIZE];  /* initial buffer */
  } init;
};


LUALIB_API inline size_t luaL_bufflen(luaL_Buffer *bf) { return bf->n; }
LUALIB_API inline char *luaL_buffaddr(luaL_Buffer *bf) { return bf->b; }

LUALIB_API inline void luaL_addsize(luaL_Buffer *bf, size_t s) { bf->n += s; }
LUALIB_API inline void luaL_buffsub(luaL_Buffer *bf, size_t s) { bf->n -= s; }

LUALIB_API void (luaL_buffinit) (lua_State *L, luaL_Buffer *B);
LUALIB_API char *(luaL_prepbuffsize) (luaL_Buffer *B, size_t sz);
LUALIB_API void (luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l);
LUALIB_API void (luaL_addstring) (luaL_Buffer *B, const char *s);
LUALIB_API void (luaL_addvalue) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresult) (luaL_Buffer *B);
LUALIB_API void (luaL_pushresultsize) (luaL_Buffer *B, size_t sz);
LUALIB_API char *(luaL_buffinitsize) (lua_State *L, luaL_Buffer *B, size_t sz);

LUALIB_API inline void luaL_addchar(luaL_Buffer *bf, char c) {
  if (bf->n >= bf->size)
    luaL_prepbuffsize(bf, 1);
  bf->b[bf->n++] = c;
}

LUALIB_API inline char *luaL_prepbuffer(luaL_Buffer *bf) {
  return luaL_prepbuffsize(bf, LUAL_BUFFERSIZE);
}

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'LUA_FILEHANDLE' and
** initial structure 'luaL_Stream' (it may contain other fields
** after that initial structure).
*/

constexpr const char *LUA_FILEHANDLE = "FILE*";


struct luaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  lua_CFunction closef;  /* to close stream (NULL for closed streams) */
};

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(lua_writestring)
#define lua_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(lua_writenewline)
#define lua_writenewline()        (lua_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(lua_writestringerror)
#define lua_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(LUA_COMPAT_APIINTCASTS)

#define luaL_checkunsigned(L,a)	((lua_Unsigned)luaL_checkinteger(L,a))
#define luaL_optunsigned(L,a,d)	\
	((lua_Unsigned)luaL_optinteger(L,a,(lua_Integer)(d)))

#define luaL_checkint(L,n)	((int)luaL_checkinteger(L, (n)))
#define luaL_optint(L,n,d)	((int)luaL_optinteger(L, (n), (d)))

#define luaL_checklong(L,n)	((long)luaL_checkinteger(L, (n)))
#define luaL_optlong(L,n,d)	((long)luaL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


