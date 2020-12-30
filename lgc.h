/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"
#include "lstate.h"

/*
** Collectable objects may have one of three colors: white, which means
** the object is not marked; gray, which means the object is marked, but
** its references may be not marked; and black, which means that the
** object and all its references are marked.  The main invariant of the
** garbage collector, while marking objects, is that a black object can
** never point to a white one. Moreover, any gray object must be in a
** "gray list" (gray, grayagain, weak, allweak, ephemeron) so that it
** can be visited again before finishing the collection cycle. (Open
** upvalues are an exception to this rule.)  These lists have no meaning
** when the invariant is not being enforced (e.g., sweep phase).
*/


/*
** Possible states of the Garbage Collector
*/
constexpr lu_byte GCSpropagate   = 0;
constexpr lu_byte GCSenteratomic = 1;
constexpr lu_byte GCSatomic      = 2;
constexpr lu_byte GCSswpallgc    = 3;
constexpr lu_byte GCSswpfinobj   = 4;
constexpr lu_byte GCSswptobefnz  = 5;
constexpr lu_byte GCSswpend      = 6;
constexpr lu_byte GCScallfin     = 7;
constexpr lu_byte GCSpause       = 8;


inline bool issweepphase(global_State *g) {
  return GCSswpallgc <= g->gcstate && g->gcstate <= GCSswpend;
}


/*
** Function to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep
** phase may break the invariant, as objects turned white may point to
** still-black objects. The invariant is restored when sweep ends and
** all objects are white again.
*/

inline bool keepinvariant(global_State *g) { return g->gcstate <= GCSatomic; }

/*
** some useful bit tricks
*/
#define resetbits(x,m)		((x) &= cast_byte(~(m)))
#define setbits(x,m)		((x) |= (m))
#define testbits(x,m)		((x) & (m))
#define bitmask(b)		(1<<(b))
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2))
#define l_setbit(x,b)		setbits(x, bitmask(b))
#define resetbit(x,b)		resetbits(x, bitmask(b))
#define testbit(x,b)		testbits(x, bitmask(b))


/*
** Layout for bit use in 'marked' field. First three bits are used
** for object "age" in generational mode. Last bit is used by tests.
*/
constexpr int WHITE0BIT    = 3;  /* object is white (type 0) */
constexpr int WHITE1BIT    = 4;  /* object is white (type 1) */
constexpr int BLACKBIT     = 5;  /* object is black */
constexpr int FINALIZEDBIT = 6;  /* object has been marked for finalization */

constexpr int TESTBIT      = 7;


constexpr unsigned int WHITEBITS = bit2mask(WHITE0BIT, WHITE1BIT);


inline bool iswhite(const GCObject *x) { return testbits(x->marked, WHITEBITS); }
inline bool isblack(const GCObject *x) { return testbit(x->marked, BLACKBIT); }
inline bool isgray(const GCObject *x) { return !(testbits(x->marked, WHITEBITS | bitmask(BLACKBIT))); }

inline bool tofinalize(const GCObject *x) { return testbit(x->marked, FINALIZEDBIT); }

inline unsigned int otherwhite(const global_State *g) { return g->currentwhite ^ WHITEBITS; }
inline bool isdeadm(unsigned int ow, unsigned int m) { return m & ow; }
inline bool isdead(const global_State *g, const GCObject *v) {
  return isdeadm(otherwhite(g), v->marked);
}

inline void changewhite(GCObject *x) { x->marked ^= WHITEBITS; }
#define nw2black(x)  \
	check_exp(!iswhite(x), l_setbit((x)->marked, BLACKBIT))

inline lu_byte luaC_white(const global_State *g) { return g->currentwhite & WHITEBITS; }

/* object age in generational mode */
constexpr int G_NEW      = 0;  /* created in current cycle */
constexpr int G_SURVIVAL = 1;  /* created in previous cycle */
constexpr int G_OLD0     = 2;  /* marked old by frw. barrier in this cycle */
constexpr int G_OLD1     = 3;  /* first full cycle as old */
constexpr int G_OLD      = 4;  /* really old object (not to be visited) */
constexpr int G_TOUCHED1 = 5;  /* old object touched this cycle */
constexpr int G_TOUCHED2 = 6;  /* old object touched in previous cycle */

constexpr int AGEBITS = 7;  /* all age bits (111) */

inline unsigned int getage(GCObject *o) { return o->marked & AGEBITS; }

inline void setage(GCObject *o, unsigned int a) {
  o->marked = cast_byte((o->marked & (~AGEBITS)) | a);
}

inline bool isold(GCObject *o) { return getage(o) > G_SURVIVAL; }

inline void changeage(GCObject *o, unsigned int f, unsigned int t) {
  check_exp(getage(o) == f, o->marked ^= f ^ t);
}

/* Default Values for GC parameters */
constexpr int LUAI_GENMAJORMUL = 100;
constexpr int LUAI_GENMINORMUL = 20;

/* wait memory to double before starting new cycle */
constexpr int LUAI_GCPAUSE = 200;

/*
** some gc parameters are stored divided by 4 to allow a maximum value
** up to 1023 in a 'lu_byte'.
*/
inline lu_byte getgcparam(lu_byte p) { return p * 4; }
inline void setgcparam(lu_byte &p, lu_byte v) { p = v / 4; }

constexpr int LUAI_GCMUL = 100;

/* how much to allocate before next GC step (log2) */
constexpr int LUAI_GCSTEPSIZE = 13;      /* 8 KB */


/*
** Check whether the declared GC mode is generational. While in
** generational mode, the collector can go temporarily to incremental
** mode to improve performance. This is signaled by 'g->lastatomic != 0'.
*/
inline bool isdecGCmodegen(global_State *g) {
  return g->gckind ==KGC_GEN || g->lastatomic != 0;
}

/*
** Does one step of collection when debt becomes positive. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
*/
#define luaC_condGC(L,pre,pos) \
	{ if (G(L)->GCdebt > 0) { pre; luaC_step(L); pos;}; \
	  condchangemem(L,pre,pos); }


/* more often than not, 'pre'/'pos' are empty */
#define luaC_checkGC(L)		luaC_condGC(L,(void)0,(void)0)


LUAI_FUNC void luaC_fix (lua_State *L, GCObject *o);
LUAI_FUNC void luaC_freeallobjects (lua_State *L);
LUAI_FUNC void luaC_step (lua_State *L);
LUAI_FUNC void luaC_runtilstate (lua_State *L, int statesmask);
LUAI_FUNC void luaC_fullgc (lua_State *L, int isemergency);
LUAI_FUNC GCObject *luaC_newobj (lua_State *L, int tt, size_t sz);
LUAI_FUNC void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v);
LUAI_FUNC void luaC_barrierback_ (lua_State *L, GCObject *o);
LUAI_FUNC void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt);
LUAI_FUNC void luaC_changemode (lua_State *L, int newmode);

inline void luaC_barrier(lua_State *L, GCObject *p, const TValue *v) {
  if (iscollectable(v) && isblack(p) && iswhite(gcvalue(v)))
    luaC_barrier_(L, obj2gco(p), gcvalue(v));
}

inline void luaC_barrierback(lua_State *L, GCObject *p, const TValue *v) {
  if (iscollectable(v) && isblack(p) && iswhite(gcvalue(v)))
    luaC_barrierback_(L, p);
}

inline void luaC_objbarrier(lua_State *L, GCObject *p, const GCObject *o) {
  if (isblack(p) && iswhite(o))
    luaC_barrier_(L, obj2gco(p), obj2gco(o));
}

#endif
