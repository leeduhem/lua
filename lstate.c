/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in lua.h
*/

#define lstate_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



/*
** thread state + extra space
*/
struct LX {
  lu_byte extra_[LUA_EXTRASPACE];
  lua_State l;
};


/*
** Main thread combines a thread state and the global state
*/
struct LG {
  LX l;
  global_State g;
};



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
#if !defined(luai_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int luai_makeseed (lua_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(nullptr));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &lua_newstate);  /* public function */
  lua_assert(p == sizeof(buff));
  return luaS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void luaE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  lua_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


LUA_API int lua_setcstacklimit (lua_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return LUAI_MAXCCALLS;  /* warning?? */
}


CallInfo *luaE_extendCI (lua_State *L) {
  lua_assert(!L->ci->next);
  CallInfo *ci = luaM_new(L, CallInfo);
  lua_assert(!L->ci->next);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = nullptr;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
void luaE_freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = nullptr;
  while ((ci = next)) {
    next = ci->next;
    luaM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void luaE_shrinkCI (lua_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (!ci)
    return;  /* no extra elements */
  while ((next = ci->next)) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    luaM_free(L, next);  /* free next */
    if (!next2)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to LUAI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** LUAI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void luaE_checkcstack (lua_State *L) {
  if (getCcalls(L) == LUAI_MAXCCALLS)
    luaG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (LUAI_MAXCCALLS / 10 * 11))
    luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
}


LUAI_FUNC void luaE_incCstack (lua_State *L) {
  L->nCcalls++;
  if (unlikely(getCcalls(L) >= LUAI_MAXCCALLS))
    luaE_checkcstack(L);
}


static void stack_init (lua_State *L1, lua_State *L) {
  /* initialize stack array */
  L1->stack = luaM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  for (int i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack + i));  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + BASIC_STACK_SIZE;
  /* initialize first ci */
  CallInfo *ci = &L1->base_ci;
  ci->next = ci->previous = nullptr;
  ci->callstatus = CIST_C;
  ci->func = L1->top;
  ci->u.c.k = nullptr;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top));  /* 'function' entry for this 'ci' */
  L1->top++;
  ci->top = L1->top + LUA_MINSTACK;
  L1->ci = ci;
}


static void freestack (lua_State *L) {
  if (!L->stack)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  luaE_freeCI(L);
  lua_assert(L->nci == 0);
  luaM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (lua_State *L, global_State *g) {
  TValue temp;
  /* create registry */
  Table *registry = luaH_new(L);
  sethvalue(L, &g->l_registry, registry);
  luaH_resize(L, registry, LUA_RIDX_LAST, 0);
  /* registry[LUA_RIDX_MAINTHREAD] = L */
  setthvalue(L, &temp, L);  /* temp = L */
  luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &temp);
  /* registry[LUA_RIDX_GLOBALS] = table of globals */
  sethvalue(L, &temp, luaH_new(L));  /* temp = new table (global table) */
  luaH_setint(L, registry, LUA_RIDX_GLOBALS, &temp);
}


/*
** open parts of the state that may cause memory-allocation errors.
** ('g->nilvalue' being a nil value flags that the state was completely
** build.)
*/
static void f_luaopen (lua_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  luaS_init(L);
  luaT_init(L);
  luaX_init(L);
  g->gcrunning = 1;  /* allow gc */
  setnilvalue(&g->nilvalue);
  luai_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (lua_State *L, global_State *g) {
  G(L) = g;
  L->twups = L;  /* thread has no upvalues */
}


static void close_state (lua_State *L) {
  global_State *g = G(L);
  luaF_close(L, L->stack, CLOSEPROTECT);  /* close all upvalues */
  luaC_freeallobjects(L);  /* collect all objects */
  if (ttisnil(&g->nilvalue))  /* closing a fully built state? */
    luai_userstateclose(L);
  luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  cast(LG *, fromstate(L))->~LG();
  lua_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


LUA_API lua_State *lua_newthread (lua_State *L) {
  lua_lock(L);
  global_State *g = G(L);
  luaC_checkGC(L);
  /* create new thread */
  LX *lx = new (cast(LX *, luaM_newobject(L, LUA_TTHREAD, sizeof(LX)))) LX();
  lua_State *L1 = &lx->l;
  L1->marked = luaC_white(g);
  L1->tt = LUA_VTHREAD;
  /* link it on list 'allgc' */
  L1->next = g->allgc;
  g->allgc = L1;
  /* anchor it on L stack */
  setthvalue2s(L, L->top, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->nCcalls = 0;
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(lua_getextraspace(L1), lua_getextraspace(g->mainthread), LUA_EXTRASPACE);
  luai_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  lua_unlock(L);
  return L1;
}


void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_close(L1, L1->stack, NOCLOSINGMETH);  /* close all upvalues */
  lua_assert(!L1->openupval);
  luai_userstatefree(L, L1);
  freestack(L1);
  l->~LX();
  luaM_free(L, l);
}


int lua_resetthread (lua_State *L) {
  CallInfo *ci;
  lua_lock(L);
  L->ci = ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  int status = luaF_close(L, L->stack, CLOSEPROTECT);
  if (status != CLOSEPROTECT)  /* real errors? */
    luaD_seterrorobj(L, status, L->stack + 1);
  else {
    status = LUA_OK;
    L->top = L->stack + 1;
  }
  ci->top = L->top + LUA_MINSTACK;
  L->status = status;
  lua_unlock(L);
  return status;
}


LUA_API lua_State *lua_newstate (lua_Alloc f, void *ud) {
  LG *l = cast(LG *, (*f)(ud, nullptr, LUA_TTHREAD, sizeof(LG)));
  if (!l) return nullptr;
  l = new (l) LG();
  lua_State *L = &l->l.l;
  global_State *g = &l->g;
  L->tt = LUA_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = luaC_white(g);
  preinit_thread(L, g);
  g->allgc = L;  /* by now, only object is the main thread */
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->mainthread = L;
  g->seed = luai_makeseed(L);
  setnilvalue(&g->l_registry);
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->totalbytes = sizeof(LG);
  setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
  setgcparam(g->gcpause, LUAI_GCPAUSE);
  setgcparam(g->gcstepmul, LUAI_GCMUL);
  g->gcstepsize = LUAI_GCSTEPSIZE;
  setgcparam(g->genmajormul, LUAI_GENMAJORMUL);
  g->genminormul = LUAI_GENMINORMUL;
  for (int i = 0; i < LUA_NUMTAGS; i++) g->mt[i] = nullptr;
  if (luaD_rawrunprotected(L, f_luaopen, nullptr) != LUA_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = nullptr;
  }
  return L;
}


LUA_API void lua_close (lua_State *L) {
  lua_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void luaE_warning (lua_State *L, const char *msg, int tocont) {
  lua_WarnFunction wf = G(L)->warnf;
  if (wf)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void luaE_warnerror (lua_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  luaE_warning(L, "error in ", 1);
  luaE_warning(L, where, 1);
  luaE_warning(L, " (", 1);
  luaE_warning(L, msg, 1);
  luaE_warning(L, ")", 0);
}

