/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"


/*
** Some notes about garbage-collected objects: All objects in Lua must
** be kept somehow accessible until being freed, so all objects always
** belong to one (and only one) of these lists, using field 'next' of
** the 'CommonHeader' for the link:
**
** 'allgc': all objects not marked for finalization;
** 'finobj': all objects marked for finalization;
** 'tobefnz': all objects ready to be finalized;
** 'fixedgc': all objects that are not to be collected (currently
** only small strings, such as reserved words).
**
** For the generational collector, some of these lists have marks for
** generations. Each mark points to the first element in the list for
** that particular generation; that generation goes until the next mark.
**
** 'allgc' -> 'survival': new objects;
** 'survival' -> 'old': objects that survived one collection;
** 'old1' -> 'reallyold': objects that became old in last collection;
** 'reallyold' -> NULL: objects old for more than one cycle.
**
** 'finobj' -> 'finobjsur': new objects marked for finalization;
** 'finobjsur' -> 'finobjold1': survived   """";
** 'finobjold1' -> 'finobjrold': just old  """";
** 'finobjrold' -> NULL: really old       """".
**
** All lists can contain elements older than their main ages, due
** to 'luaC_checkfinalizer' and 'udata2finalize', which move
** objects between the normal lists and the "marked for finalization"
** lists. Moreover, barriers can age young objects in young lists as
** OLD0, which then become OLD1. However, a list never contains
** elements younger than their main ages.
**
** The generational collector also uses a pointer 'firstold1', which
** points to the first OLD1 object in the list. It is used to optimize
** 'markold'. (Potentially OLD1 objects can be anywhere between 'allgc'
** and 'reallyold', but often the list has no OLD1 objects or they are
** after 'old1'.) Note the difference between it and 'old1':
** 'firstold1': no OLD1 objects before this point; there can be all
**   ages after it.
** 'old1': no objects younger than OLD1 after this point.
*/

/*
** Moreover, there is another set of lists that control gray objects.
** These lists are linked by fields 'gclist'. (All objects that
** can become gray have such a field. The field is not the same
** in all objects, but it always has this name.)  Any gray object
** must belong to one of these lists, and all objects in these lists
** must be gray (with two exceptions explained below):
**
** 'gray': regular gray objects, still waiting to be visited.
** 'grayagain': objects that must be revisited at the atomic phase.
**   That includes
**   - black objects got in a write barrier;
**   - all kinds of weak tables during propagation phase;
**   - all threads.
** 'weak': tables with weak values to be cleared;
** 'ephemeron': ephemeron tables with white->white entries;
** 'allweak': tables with weak keys and/or weak values to be cleared.
**
** The exceptions to that "gray rule" are:
** - TOUCHED2 objects in generational mode stay in a gray list (because
** they must be visited again at the end of the cycle), but they are
** marked black because assignments to them must activate barriers (to
** move them back to TOUCHED1).
** - Open upvales are kept gray to avoid barriers, but they stay out
** of gray lists. (They don't even have a 'gclist' field.)
*/


struct lua_longjmp;  /* defined in ldo.c */


/*
** Atomic type (relative to signals) to better ensure that 'lua_sethook'
** is thread safe
*/
#if !defined(l_signalT)
#include <signal.h>
#define l_signalT	sig_atomic_t
#endif



/* kinds of Garbage Collection */
constexpr int KGC_INC = 0;  // incremental gc
constexpr int KGC_GEN = 1;  // generational gc


struct stringtable {
  TString **hash;
  int nuse;  /* number of elements */
  int size;
};


/*
** Information about a call.
*/
struct CallInfo {
  StkId func;  /* function index in the stack */
  StkId	top;  /* top for this function */
  struct CallInfo *previous, *next;  /* dynamic call link */
  union {
    struct {  /* only for Lua functions */
      const Instruction *savedpc;
      volatile l_signalT trap;
      int nextraargs;  /* # of extra arguments in vararg functions */
    } l;
    struct {  /* only for C functions */
      lua_KFunction k;  /* continuation in case of yields */
      ptrdiff_t old_errfunc;
      lua_KContext ctx;  /* context info. in case of yields */
    } c;
  } u;
  union {
    int funcidx;  /* called-function index */
    int nyield;  /* number of values yielded */
    struct {  /* info about transferred values (for call/return hooks) */
      unsigned short ftransfer;  /* offset of first value transferred */
      unsigned short ntransfer;  /* number of values transferred */
    } transferinfo;
  } u2;
  short nresults;  /* expected number of results from this function */
  unsigned short callstatus;
};


/*
** Bits in CallInfo status
*/
constexpr int CIST_OAH       = (1<<0);  /* original value of 'allowhook' */
constexpr int CIST_C         = (1<<1);  /* call is running a C function */
constexpr int CIST_FRESH     = (1<<2);  /* call is on a fresh "luaV_execute" frame */
constexpr int CIST_HOOKED    = (1<<3);  /* call is running a debug hook */
constexpr int CIST_YPCALL    = (1<<4);  /* call is a yieldable protected call */
constexpr int CIST_TAIL      = (1<<5);  /* call was tail called */
constexpr int CIST_HOOKYIELD = (1<<6);  /* last hook called yielded */
constexpr int CIST_FIN       = (1<<7);  /* call is running a finalizer */
constexpr int CIST_TRAN      = (1<<8);	/* 'ci' has transfer information */
#if defined(LUA_COMPAT_LT_LE)
constexpr int CIST_LEQ       = (1<<9);   /* using __lt for __le */
#endif

/* active function is a Lua function */
inline bool isLua(CallInfo *ci) { return !(ci->callstatus & CIST_C); }

/* call is running Lua code (not a hook) */
inline  bool isLuacode(CallInfo *ci) { return !(ci->callstatus & (CIST_C | CIST_HOOKED)); }

/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */
inline void setoah(unsigned short &st, lu_byte v) { st = (st & ~CIST_OAH) | v; }
inline unsigned short getoah(unsigned short st) { return st & CIST_OAH; }


/*
** 'global state', shared by all threads of this state
*/
struct global_State {
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to 'frealloc' */
  l_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
  lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */
  lu_mem lastatomic;  /* see function 'genstep' in file 'lgc.c' */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;
  TValue nilvalue;  /* a nil value */
  unsigned int seed;  /* randomized seed for hashes */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  lu_byte gckind;  /* kind of GC running */
  lu_byte genminormul;  /* control for minor generational collections */
  lu_byte genmajormul;  /* control for major generational collections */
  lu_byte gcrunning = 0;  /* true if GC is running */
  lu_byte gcemergency;  /* true if this is an emergency collection */
  lu_byte gcpause;  /* size of pause between successive GCs */
  lu_byte gcstepmul;  /* GC "speed" */
  lu_byte gcstepsize;  /* (log2 of) GC granularity */
  GCObject *allgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* current position of sweep in list */
  GCObject *finobj;  /* list of collectable objects with finalizers */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */
  GCObject *fixedgc;  /* list of objects not to be collected */
  /* fields for generational collector */
  GCObject *survival;  /* start of objects that survived one GC cycle */
  GCObject *old1;  /* start of old1 objects */
  GCObject *reallyold;  /* objects more than one cycle old ("really old") */
  GCObject *firstold1;  /* first OLD1 object in the list (if any) */
  GCObject *finobjsur;  /* list of survival objects with finalizers */
  GCObject *finobjold1;  /* list of old1 objects with finalizers */
  GCObject *finobjrold;  /* list of really old objects with finalizers */
  struct lua_State *twups;  /* list of threads with open upvalues */
  lua_CFunction panic;  /* to be called in unprotected errors */
  struct lua_State *mainthread;
  TString *memerrmsg;  /* message for memory-allocation errors */
  TString *tmname[TM_N];  /* array with tag-method names */
  struct Table *mt[LUA_NUMTAGS];  /* metatables for basic types */
  TString *strcache[STRCACHE_N][STRCACHE_M];  /* cache for strings in API */
  lua_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */

  global_State() = default;
};


/*
** 'per thread' state
*/
struct lua_State : public GCObject {
  lu_byte status = LUA_OK;
  lu_byte allowhook = 1;
  unsigned short nci;  /* number of items in 'ci' list */
  StkId top;  /* first free slot in the stack */
  global_State *l_G;
  CallInfo *ci;  /* call info for current function */
  StkId stack_last;  /* end of stack (last element + 1) */
  StkId stack;  /* stack base */
  UpVal *openupval;  /* list of open upvalues in this stack */
  GCObject *gclist;
  struct lua_State *twups;  /* list of threads with open upvalues */
  struct lua_longjmp *errorJmp;  /* current error recover point */
  CallInfo base_ci;  /* CallInfo for first level (C calling Lua) */
  volatile lua_Hook hook;
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  l_uint32 nCcalls;  /* number of nested (non-yieldable | C)  calls */
  int oldpc;  /* last pc traced */
  int basehookcount;
  int hookcount;
  volatile l_signalT hookmask;

  lua_State() = default;
  lua_State(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


/*
** About 'nCcalls':  This count has two parts: the lower 16 bits counts
** the number of recursive invocations in the C stack; the higher
** 16 bits counts the number of non-yieldable calls in the stack.
** (They are together so that we can change and save both with one
** instruction.)
*/


/* true if this thread does not have non-yieldable calls in the stack */

inline bool yieldable(lua_State *L) { return (L->nCcalls & 0xffff0000) == 0; }

/* real number of C calls */
inline l_uint32 getCcalls(lua_State *L) { return L->nCcalls & 0xffff; }

/* Increment the number of non-yieldable calls */
inline l_uint32 incnny(lua_State *L) { return L->nCcalls += 0x10000; }

/* Decrement the number of non-yieldable calls */
inline l_uint32 decnny(lua_State *L) { return L->nCcalls -= 0x10000; }

/* Non-yieldable call increment */
constexpr int nyci = 0x10000 | 1;

inline global_State *&G(lua_State *L) { return L->l_G; }
inline const global_State *G(const lua_State *L) { return L->l_G; }

/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/
constexpr int EXTRA_STACK = 5;


constexpr int BASIC_STACK_SIZE = 2 * LUA_MINSTACK;

inline int stacksize(lua_State *th) { return th->stack_last - th->stack; }

/*
** Union of all collectable objects (only for conversions)
** ISO C99, 6.5.2.3 p.5:
** "if a union contains several structures that share a common initial
** sequence [...], and if the union object currently contains one
** of these structures, it is permitted to inspect the common initial
** part of any of them anywhere that a declaration of the complete type
** of the union is visible."
*/
union GCUnion {
  GCObject gc;  /* common header */
  struct TString ts;
  struct Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct lua_State th;  /* thread */
  struct UpVal upv;
};


/*
** ISO C99, 6.7.2.1 p.14:
** "A pointer to a union object, suitably converted, points to each of
** its members [...], and vice versa."
*/
#define cast_u(o)	cast(union GCUnion *, (o))

/* Functions to convert a GCObject into a specific value */
inline TString *gco2ts(GCObject *o) {
  return check_exp(novariant(o->tt) == LUA_TSTRING, &(cast_u(o))->ts);
}

inline Udata *gco2u(GCObject *o) {
  return check_exp(o->tt == LUA_VUSERDATA, &(cast_u(o))->u);
}

inline LClosure *gco2lcl(GCObject *o) {
  return check_exp(o->tt == LUA_VLCL, &(cast_u(o))->cl.l);
}

inline CClosure *gco2ccl(GCObject *o) {
  return check_exp(o->tt == LUA_VCCL, &(cast_u(o))->cl.c);
}

inline Closure *gco2cl(GCObject *o) {
  return check_exp(novariant(o->tt) == LUA_TFUNCTION, &(cast_u(o))->cl);
}

inline Table *gco2t(GCObject *o) {
  return check_exp(o->tt == LUA_VTABLE, &(cast_u(o))->h);
}

inline Proto *gco2p(GCObject *o) {
  return check_exp(o->tt == LUA_VPROTO, &(cast_u(o))->p);
}

inline lua_State *gco2th(GCObject *o) {
  return check_exp(o->tt == LUA_VTHREAD, &(cast_u(o))->th);
}

inline UpVal *gco2upv(GCObject *o) {
  return check_exp(o->tt == LUA_VUPVAL, &(cast_u(o))->upv);
}

/*
** Functions to convert a Lua object into a GCObject
** (The access to 'tt' tries to ensure that 'v' is actually a Lua object.)
*/
template<typename T>
inline GCObject *obj2gco(const T *v) {
  return check_exp(v->tt >= LUA_TSTRING, &(cast_u(v))->gc);
}

/* actual number of total bytes allocated */
inline lu_mem gettotalbytes(global_State *g) {
  return g->totalbytes + g->GCdebt;
}

// Threads
inline void setthvalue(lua_State *L, TValue *obj, lua_State *x) {
  lua_assert(obj2gco(x)->tt == LUA_VTHREAD);
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

inline void setthvalue2s(lua_State *L, StkId o, lua_State *t) {
  setthvalue(L, s2v(o), t);
}

// TStrings
inline void setsvalue(lua_State *L, TValue *obj, TString *x) {
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

/* set a string to the stack */
inline void setsvalue2s(lua_State *L, StkId o, TString *s) {
  setsvalue(L, s2v(o), s);
}

/* set a string to a new object */
inline void setsvalue2n(lua_State *L, TValue *obj, TString *x) {
  setsvalue(L, obj, x);
}

// Udata
inline void setpvalue(TValue *o, void *x) {
  *o = x;
}

inline void setuvalue(lua_State *L, TValue *obj, Udata *x) {
  lua_assert(obj2gco(x)->tt == LUA_VUSERDATA);
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

// Closure
inline void setclLvalue(lua_State *L, TValue *obj, LClosure *x) {
  lua_assert(obj2gco(x)->tt == LUA_VLCL);
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

inline void setclLvalue2s(lua_State *L, StkId o, LClosure *cl) {
  setclLvalue(L, s2v(o), cl);
}

inline void setfvalue(TValue *o, lua_CFunction x) {
  *o = x;
}

inline void setclCvalue(lua_State *L, TValue *obj, CClosure *x) {
  lua_assert(obj2gco(x)->tt == LUA_VCCL);
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

// Table
inline void sethvalue(lua_State *L, TValue *obj, Table *x) {
  lua_assert(obj2gco(x)->tt == LUA_VTABLE);
  *obj = obj2gco(x);
  checkliveness(L, obj);
}

inline void sethvalue2s(lua_State *L, StkId o, Table *h) { sethvalue(L, s2v(o), h); }


LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);
LUAI_FUNC void luaE_shrinkCI (lua_State *L);
LUAI_FUNC void luaE_checkcstack (lua_State *L);
LUAI_FUNC void luaE_incCstack (lua_State *L);
LUAI_FUNC void luaE_warning (lua_State *L, const char *msg, int tocont);
LUAI_FUNC void luaE_warnerror (lua_State *L, const char *where);


#endif

