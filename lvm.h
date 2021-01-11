/*
** $Id: lvm.h $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(LUA_NOCVTN2S)
inline bool cvt2str(const TValue *o) { return ttisnumber(o); }
#else
// no conversion from numbers to strings
inline bool cvt2str(const TValue *o) { return false; }
#endif


#if !defined(LUA_NOCVTS2N)
inline bool cvt2num(const TValue *o) { return ttisstring(o); }
#else
// no conversion from strings to numbers
inline bool cvt2num(const TValue *o) { return false; }
#endif

/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/*
** You can define LUA_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not integral values)
*/
#if !defined(LUA_FLOORN2I)
constexpr F2Imod LUA_FLOORN2I = F2Ieq;
#endif


LUAI_FUNC int luaV_equalobj (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r);
LUAI_FUNC int luaV_tonumber_ (const TValue *obj, lua_Number *n);
LUAI_FUNC int luaV_tointeger (const TValue *obj, lua_Integer *p, F2Imod mode);
LUAI_FUNC int luaV_tointegerns (const TValue *obj, lua_Integer *p, F2Imod mode);
LUAI_FUNC int luaV_flttointeger (lua_Number n, lua_Integer *p, F2Imod mode);
LUAI_FUNC void luaV_finishget (lua_State *L, const TValue *t, TValue *key, StkId val, const TValue *slot);
LUAI_FUNC void luaV_finishset (lua_State *L, const TValue *t, TValue *key, TValue *val, const TValue *slot);
LUAI_FUNC void luaV_finishOp (lua_State *L);
LUAI_FUNC void luaV_execute (lua_State *L, CallInfo *ci);
LUAI_FUNC void luaV_concat (lua_State *L, int total);
LUAI_FUNC lua_Integer luaV_idiv (lua_State *L, lua_Integer x, lua_Integer y);
LUAI_FUNC lua_Integer luaV_mod (lua_State *L, lua_Integer x, lua_Integer y);
LUAI_FUNC lua_Number luaV_modf (lua_State *L, lua_Number x, lua_Number y);
LUAI_FUNC lua_Integer luaV_shiftl (lua_Integer x, lua_Integer y);
LUAI_FUNC void luaV_objlen (lua_State *L, StkId ra, const TValue *rb);


/* convert an object to a float (including string coercion) */
inline int tonumber(const TValue *o, lua_Number *n) {
  if (ttisfloat(o)) {
    *n = fltvalue(o);
    return 1;
  }
  return luaV_tonumber_(o, n);
}

/* convert an object to a float (without string coercion) */
inline int tonumberns(const TValue *o, lua_Number &n) {
  if (ttisfloat(o)) {
    n = fltvalue(o);
    return 1;
  }
  if (ttisinteger(o)) {
    n = cast_num(ivalue(o));
    return 1;
  }
  return 0;
}


/* convert an object to an integer (including string coercion) */
inline int tointeger(const TValue *o, lua_Integer *i) {
  if (ttisinteger(o)) {
    *i = ivalue(o);
    return 1;
  }
  return luaV_tointeger(o, i, LUA_FLOORN2I);
}

/* convert an object to an integer (without string coercion) */
inline int tointegerns(const TValue *o, lua_Integer *i) {
  if (ttisinteger(o)) {
    *i = ivalue(o);
    return 1;
  }
  return luaV_tointegerns(o, i, LUA_FLOORN2I);
}

#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

inline bool luaV_rawequalobj(const TValue *t1, const TValue *t2) {
  return luaV_equalobj(nullptr, t1, t2);
}

/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define luaV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'luaV_fastget' for integers, inlining the fast case
** of 'luaH_getint'.
*/
#define luaV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : luaH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
inline void luaV_finishfastset(lua_State *L, const TValue *t, const TValue *slot, const TValue *v) {
  setobj2t(L, cast(TValue *, slot), v);
  luaC_barrierback(L, gcvalue(t), v);
}

#endif
