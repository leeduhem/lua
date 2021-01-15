/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"
#include "lmem.h"

#include <vector>
#include <cstddef>

/*
** Extra types for collectable non-values
*/
constexpr int LUA_TUPVAL = LUA_NUMTYPES;  /* upvalues */
constexpr int LUA_TPROTO = LUA_NUMTYPES+1;  /* function prototypes */
constexpr int LUA_TDEADKEY = LUA_NUMTYPES+2;  /* removed keys in tables */



/*
** number of all possible types (including LUA_TNONE but excluding DEADKEY)
*/
constexpr int LUA_TOTALTYPES = LUA_TPROTO + 2;


/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a LUA_T* constant)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/

/* add variant bits to a type */
constexpr inline lu_byte makevariant(lu_byte t, lu_byte v) { return t | v << 4; }


/*
** Union of all Lua values
*/
union Value {
  GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  lua_CFunction f; /* light C functions */
  lua_Integer i;   /* integer numbers */
  lua_Number n;    /* float numbers */
};


/*
** Tagged Values. This is the basic representation of values in Lua:
** an actual value plus a tag with its type.
*/
struct TValue {
  Value value_;
  lu_byte tt_;
};


inline const Value &val_(const TValue *o) { return o->value_; }
inline Value &val_(TValue *o) { return o->value_; }

inline const Value *valraw(const TValue *o) { return &val_(o); }
inline Value *valraw(TValue *o) { return &val_(o); }


/* raw type tag of a TValue */
inline lu_byte rawtt(const TValue *o) { return o->tt_; }

/* tag with no variants (bits 0-3) */
inline lu_byte novariant(const lu_byte t) { return t & 0x0F;}

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
inline lu_byte withvariant(const lu_byte t) { return t & 0x3F; }
inline lu_byte ttypetag(const TValue *o) { return withvariant(rawtt(o)); }

/* type of a TValue */
inline lu_byte ttype(const TValue *o) { return novariant(rawtt(o)); }

/* Functions to test type */
inline bool checktag(const TValue *o, const lu_byte t) { return rawtt(o) == t; }
inline bool checktype(const TValue *o, const lu_byte t) { return ttype(o) == t; }

/* set a value's tag */
inline void settt_(TValue *o, const lu_byte t) { o->tt_ = t; }


/*
** Entries in the Lua stack
*/
union StackValue {
  TValue val;
};


/* index to stack elements */
typedef StackValue *StkId;

/* convert a 'StackValue' to a 'TValue' */
inline TValue *s2v(const StkId o) { return &o->val; }


/*
** {==================================================================
** Nil
** ===================================================================
*/

/* Standard nil */
constexpr lu_byte LUA_VNIL = makevariant(LUA_TNIL, 0);

/* Empty slot (which might be different from a slot containing nil) */
constexpr lu_byte LUA_VEMPTY = makevariant(LUA_TNIL, 1);

/* Value returned for a key not found in a table (absent key) */
constexpr lu_byte LUA_VABSTKEY = makevariant(LUA_TNIL, 2);


/* test for (any kind of) nil */
inline bool ttisnil(const TValue *v) { return checktype(v, LUA_TNIL); }


/* test for a standard nil */
inline bool ttisstrictnil(const TValue *o) { return checktag(o, LUA_VNIL); }

inline void setnilvalue(TValue *o) { settt_(o, LUA_VNIL); }

inline bool isabstkey(const TValue *v) { return checktag(v, LUA_VABSTKEY); }

/*
** test for non-standard nils (used only in assertions)
*/
inline bool isnonstrictnil(const TValue *v) { return ttisnil(v) && !ttisstrictnil(v); }

/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/
inline bool isempty(const TValue *v) { return ttisnil(v); }

// A value corresponding to an absent key
extern TValue ABSTKEYCONSTANT;

/* mark an entry as empty */
inline void setempty(TValue *v) { settt_(v, LUA_VEMPTY); }


/* }================================================================== */


/*
** {==================================================================
** Booleans
** ===================================================================
*/


constexpr lu_byte LUA_VFALSE = makevariant(LUA_TBOOLEAN, 0);
constexpr lu_byte LUA_VTRUE = makevariant(LUA_TBOOLEAN, 1);

inline bool ttisboolean(const TValue *o) { return checktype(o, LUA_TBOOLEAN); }
inline bool ttisfalse(const TValue *o) { return checktag(o, LUA_VFALSE); }
inline bool ttistrue(const TValue *o) { return checktag(o, LUA_VTRUE); }

inline bool l_isfalse(const TValue *o) { return ttisfalse(o) || ttisnil(o); }

inline void setbfvalue(TValue *o) { settt_(o, LUA_VFALSE); }
inline void setbtvalue(TValue *o) { settt_(o, LUA_VTRUE); }

/* }================================================================== */


/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/

struct global_State;  // defined in lstate.h

/* Common type for all collectable objects */
struct GCObject {
  GCObject *next;
  lu_byte tt;
  lu_byte marked;

  GCObject() = default;
  GCObject(global_State *g, lu_byte tag);
};


/* Bit mark for collectable types */
constexpr lu_byte BIT_ISCOLLECTABLE  = 1 << 6;

inline lu_byte iscollectable(const TValue *o) { return rawtt(o) & BIT_ISCOLLECTABLE; }

/* mark a tag as collectable */
constexpr inline lu_byte ctb(lu_byte t) { return t | BIT_ISCOLLECTABLE; }

inline GCObject *gcvalue(const TValue *o) { return check_exp(iscollectable(o), val_(o).gc); }
inline GCObject *gcvalueraw(const Value &v) { return v.gc; }

inline void setgcovalue(lua_State *, TValue *o, GCObject *x) {
  val_(o).gc = x; settt_(o, ctb(x->tt));
}

/* Functions for internal tests */

/* collectable object has the same tag as the original value */
inline bool righttt(const TValue *o) { return ttypetag(o) == gcvalue(o)->tt; }

/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/
const global_State *G(const lua_State*);
bool isdead(const global_State *, const GCObject *);

inline void checkliveness(const lua_State *L, const TValue *o) {
  lua_longassert(!iscollectable(o)
		 || (righttt(o) && (L == nullptr || !isdead(G(L), gcvalue(o)))));
}

/* }================================================================== */


/*
** {==================================================================
** Threads
** ===================================================================
*/

constexpr lu_byte LUA_VTHREAD = makevariant(LUA_TTHREAD, 0);

inline bool ttisthread(const TValue *o) { return checktag(o, ctb(LUA_VTHREAD)); }

lua_State *gco2th(GCObject *);

inline lua_State *thvalue(const TValue *o) {
  return check_exp(ttisthread(o), gco2th(val_(o).gc));
}

/* }================================================================== */


/*
** {==================================================================
** Numbers
** ===================================================================
*/

/* Variant tags for numbers */
constexpr lu_byte LUA_VNUMINT = makevariant(LUA_TNUMBER, 0);  /* integer numbers */
constexpr lu_byte LUA_VNUMFLT = makevariant(LUA_TNUMBER, 1);  /* float numbers */

inline bool ttisnumber(const TValue *o) { return checktype(o, LUA_TNUMBER); }
inline bool ttisfloat(const TValue *o) { return checktag(o, LUA_VNUMFLT); }
inline bool ttisinteger(const TValue *o) { return checktag(o, LUA_VNUMINT); }

inline lua_Number fltvalue(const TValue *o) { return check_exp(ttisfloat(o), val_(o).n); }
inline lua_Integer ivalue(const TValue *o) { return check_exp(ttisinteger(o), val_(o).i); }
inline lua_Number nvalue(const TValue *o) {
  return check_exp(ttisnumber(o),
		   ttisinteger(o) ? ivalue(o) : fltvalue(o));
}

inline const lua_Number &fltvalueraw(const Value &v) { return v.n; }
inline const lua_Integer &ivalueraw(const Value &v) { return v.i; }

inline void setfltvalue(TValue *o, const lua_Number x) {
  val_(o).n = x; settt_(o, LUA_VNUMFLT);
}

inline void chgfltvalue(TValue *o, const lua_Number x) {
  lua_assert(ttisfloat(o)); val_(o).n = x;
}

inline void setivalue(TValue *o, const lua_Integer x) {
  val_(o).i = x; settt_(o, LUA_VNUMINT);
}

inline void chgivalue(TValue *o, const lua_Integer x) {
  lua_assert(ttisinteger(o)); val_(o).i = x;
}

/* }================================================================== */


/*
** {==================================================================
** Strings
** ===================================================================
*/

/* Variant tags for strings */
constexpr lu_byte LUA_VSHRSTR = makevariant(LUA_TSTRING, 0);  /* short strings */
constexpr lu_byte LUA_VLNGSTR = makevariant(LUA_TSTRING, 1);  /* long strings */

inline bool ttisstring(const TValue *o) { return checktype(o, LUA_TSTRING); }
inline bool ttisshrstring(const TValue *o) { return checktag(o, ctb(LUA_VSHRSTR)); }
inline bool ttislngstring(const TValue *o) { return checktag(o, ctb(LUA_VLNGSTR)); }

/*
** Header for a string value.
*/
struct TString : public GCObject {
  lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
  lu_byte shrlen;  /* length for short strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char contents[1];

 TString(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};

/*
** Get the actual string (array of bytes) from a 'TString'.
*/
inline const char *getstr(const TString *ts) { return ts->contents; }
inline char *getstr(TString *ts) { return ts->contents; }

TString *gco2ts(GCObject *);
TString *gco2ts(const GCObject *);

inline TString *tsvalue(const TValue *o) {
  return check_exp(ttisstring(o), gco2ts(val_(o).gc));
}

inline TString *tsvalueraw(Value &v) { return gco2ts(v.gc); }
inline TString *tsvalueraw(const Value &v) { return gco2ts(v.gc); }

/* get the actual string (array of bytes) from a Lua value */
inline const char *svalue(const TValue *o) { return getstr(tsvalue(o)); }

/* get string length from 'TString *s' */
inline size_t tsslen(const TString *ts) {
  return ts->tt == LUA_VSHRSTR ? ts->shrlen : ts->u.lnglen;
}

/* get string length from 'TValue *o' */
inline size_t vslen(const TValue *o) {
  return tsslen(tsvalue(o));
}

/* }================================================================== */


/*
** {==================================================================
** Userdata
** ===================================================================
*/


/*
** Light userdata should be a variant of userdata, but for compatibility
** reasons they are also different types.
*/
constexpr lu_byte LUA_VLIGHTUSERDATA = makevariant(LUA_TLIGHTUSERDATA, 0);
constexpr lu_byte LUA_VUSERDATA = makevariant(LUA_TUSERDATA, 0);

inline bool ttislightuserdata(const TValue *o) { return checktag(o, LUA_VLIGHTUSERDATA); }
inline bool ttisfulluserdata(const TValue *o) { return checktag(o, ctb(LUA_VUSERDATA)); }


/* Ensures that addresses after this type are always fully aligned. */
union alignas(std::max_align_t) UValue {
  TValue uv;
};


/*
** Header for userdata with user values;
** memory area follows the end of this structure.
*/
struct Udata : public GCObject {
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  GCObject *gclist;
  UValue uv[1];  /* user values */

 Udata(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


/*
** Header for userdata with no user values. These userdata do not need
** to be gray during GC, and therefore do not need a 'gclist' field.
** To simplify, the code always use 'Udata' for both kinds of userdata,
** making sure it never accesses 'gclist' on userdata with no user values.
** This structure here is used only to compute the correct size for
** this representation. (The 'bindata' field in its end ensures correct
** alignment for binary data following this header.)
*/
struct Udata0 : public GCObject {
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  std::max_align_t bindata;

 Udata0(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


/* compute the offset of the memory area of a userdata */
inline size_t udatamemoffset(unsigned short nuv) {
  return nuv == 0
    ? offsetof(Udata0, bindata)
    : offsetof(Udata, uv) + sizeof(UValue) * (nuv);
}

/* get the address of the memory block inside 'Udata' */
inline char *getudatamem(Udata *u) {
  return cast_charp(u) + udatamemoffset(u->nuvalue);
}

/* compute the size of a userdata */
inline size_t sizeudata(unsigned short nuv, size_t nb) { return udatamemoffset(nuv) + nb; }

Udata *gco2u(GCObject *);

inline Udata *uvalue(const TValue *o) { return check_exp(ttisfulluserdata(o), gco2u(val_(o).gc)); }

inline void *pvalue(const TValue *o) { return check_exp(ttislightuserdata(o), val_(o).p); }
inline void *pvalueraw(const Value &v) { return v.p; }

/* }================================================================== */


/*
** {==================================================================
** Prototypes
** ===================================================================
*/

constexpr lu_byte LUA_VPROTO = makevariant(LUA_TPROTO, 0);


/*
** Description of an upvalue for function prototypes
*/
struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  lu_byte instack;  /* whether it is in stack (register) */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
  lu_byte kind;  /* kind of corresponding variable */

  Upvaldesc() = default;
  Upvaldesc(TString *name1, lu_byte instack1, lu_byte idx1, lu_byte kind1) :
    name(name1), instack(instack1), idx(idx1), kind(kind1) {}
};


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */

  LocVar() = default;
  LocVar(TString *name, int start, int end = 0) : varname(name), startpc(start), endpc(end) {}
};


/*
** Associates the absolute line source for a given instruction ('pc').
** The array 'lineinfo' gives, for each instruction, the difference in
** lines from the previous instruction. When that difference does not
** fit into a byte, Lua saves the absolute line for that instruction.
** (Lua also saves the absolute line periodically, to speed up the
** computation of a line number: we can use binary search in the
** absolute-line array, but we must traverse the 'lineinfo' array
** linearly to compute a line.)
*/
struct AbsLineInfo {
  int pc;
  int line;

  AbsLineInfo() = default;
  AbsLineInfo(int pc1, int line1) : pc(pc1), line(line1) {}
};

global_State *&G(lua_State*);

/*
** Function Prototypes
*/
struct Proto : public GCObject {
  lu_byte numparams = 0;  /* number of fixed (named) parameters */
  lu_byte is_vararg = 0;
  lu_byte maxstacksize = 0;  /* number of registers needed by this function */
  int sizek = 0;  /* size of 'k' */
  int sizecode = 0;
  int sizep = 0;  /* size of 'p' */
  int linedefined = 0;  /* debug information  */
  int lastlinedefined = 0;  /* debug information  */
  TValue *k = nullptr;  /* constants used by the function */
  Instruction *code = nullptr;  /* opcodes */
  Proto **p = nullptr;  /* functions defined inside the function */
  std::vector<Upvaldesc, lua::allocator<Upvaldesc>> upvalues;  // upvalue information
  std::vector<ls_byte, lua::allocator<ls_byte>> lineinfo;  // information about source lines (debug information)
  std::vector<AbsLineInfo> abslineinfo;  // idem
  std::vector<LocVar> locvars;  // information about local variables (debug information)
  TString  *source = nullptr;  /* used for debug information */
  GCObject *gclist = nullptr;

 Proto(lua_State *L, lu_byte tag) :
  GCObject(G(L), tag),
    upvalues(lua::allocator<Upvaldesc>(L)),
    lineinfo(lua::allocator<ls_byte>(L))
    {}
};

/* }================================================================== */


/*
** {==================================================================
** Functions
** ===================================================================
*/

constexpr lu_byte LUA_VUPVAL = makevariant(LUA_TUPVAL, 0);


/* Variant tags for functions */
constexpr lu_byte LUA_VLCL = makevariant(LUA_TFUNCTION, 0);  /* Lua closure */
constexpr lu_byte LUA_VLCF = makevariant(LUA_TFUNCTION, 1);  /* light C function */
constexpr lu_byte LUA_VCCL = makevariant(LUA_TFUNCTION, 2);  /* C closure */

inline bool ttisfunction(const TValue *o) { return checktype(o, LUA_TFUNCTION); }
inline bool ttisclosure(const TValue *o) { return (rawtt(o) & 0x1F) == LUA_VLCL; }
inline bool ttisLclosure(const TValue *o) { return checktag(o, ctb(LUA_VLCL)); }
inline bool ttislcf(const TValue *o) { return checktag(o, LUA_VLCF); }
inline bool ttisCclosure(const TValue *o) { return checktag(o, ctb(LUA_VCCL)); }

inline bool isLfunction(const TValue *o) { return ttisLclosure(o); }


/*
** Upvalues for Lua closures
*/
struct UpVal : public GCObject {
  lu_byte tbc;  /* true if it represents a to-be-closed variable */
  TValue *v;  /* points to stack or to its own value */
  union {
    struct {  /* (when open) */
      struct UpVal *next;  /* linked list */
      struct UpVal **previous;
    } open;
    TValue value;  /* the value (when closed) */
  } u;

 UpVal(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


struct CClosure : public GCObject {
  lu_byte nupvalues;
  GCObject *gclist;
  lua_CFunction f;
  TValue upvalue[1];  /* list of upvalues */

  CClosure(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


struct LClosure : public GCObject {
  lu_byte nupvalues;
  GCObject *gclist;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */

 LClosure(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};


union Closure {
  CClosure c;
  LClosure l;
};

Closure *gco2cl(GCObject *);
LClosure *gco2lcl(GCObject *);
CClosure *gco2ccl(GCObject *);

inline Closure *clvalue(const TValue *o) { return check_exp(ttisclosure(o), gco2cl(val_(o).gc)); }
inline LClosure* clLvalue(const TValue *o) { return check_exp(ttisLclosure(o), gco2lcl(val_(o).gc)); }
inline CClosure *clCvalue(const TValue *o) { return check_exp(ttisCclosure(o), gco2ccl(val_(o).gc)); }

inline lua_CFunction fvalue(const TValue *o) { return check_exp(ttislcf(o), val_(o).f); }
inline lua_CFunction fvalueraw(const Value &v) { return v.f; }

inline Proto *getproto(const TValue *o) { return clLvalue(o)->p; }

/* }================================================================== */


/*
** {==================================================================
** Tables
** ===================================================================
*/

constexpr lu_byte LUA_VTABLE = makevariant(LUA_TTABLE, 0);

inline bool ttistable(const TValue *o) { return checktag(o, ctb(LUA_VTABLE)); }


/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
union Node {
  struct NodeKey {
    Value value_;  // fields for value
    lu_byte tt_;
    lu_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
};


/* copy a value into a key */
inline void setnodekey(lua_State *L, Node *node, const TValue *obj) {
  node->u.key_val = obj->value_; node->u.key_tt = obj->tt_;
  checkliveness(L, obj);
}

/* copy a value from a key */
inline void getnodekey(lua_State *L, TValue *obj, const Node *node) {
  obj->value_ = node->u.key_val; obj->tt_ = node->u.key_tt;
  checkliveness(L, obj);
}

/*
** About 'alimit': if 'isrealasize(t)' is true, then 'alimit' is the
** real size of 'array'. Otherwise, the real size of 'array' is the
** smallest power of two not smaller than 'alimit' (or zero iff 'alimit'
** is zero); 'alimit' is then used as a hint for #t.
*/

struct Table : public GCObject {
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;

  Table() = default;
  Table(global_State *g, lu_byte tag) : GCObject(g, tag) {}
};

constexpr lu_byte BITRAS = 1 << 7;
inline bool isrealasize(const Table *t) { return !(t->flags & BITRAS); }
inline void setrealasize(Table *t) { t->flags &= ~BITRAS; }
inline void setnorealasize(Table *t) { t->flags |= BITRAS; }

/*
** Functions to manipulate keys inserted in nodes
*/
inline const lu_byte &keytt(const Node *node) { return node->u.key_tt; }
inline lu_byte &keytt(Node *node) { return node->u.key_tt; }
inline const Value &keyval(const Node *node) { return node->u.key_val; }
inline Value &keyval(Node *node) { return node->u.key_val; }

inline bool keyisnil(const Node *node) { return keytt(node) == LUA_TNIL; }
inline bool keyisinteger(const Node *node) { return keytt(node) == LUA_VNUMINT; }
inline lua_Integer keyival(Node *node) { return keyval(node).i; }
inline lua_Integer keyival(const Node *node) { return keyval(node).i; }
inline bool keyisshrstr(const Node *node) { return keytt(node) == ctb(LUA_VSHRSTR); }
inline TString *keystrval(const Node *node) { return gco2ts(keyval(node).gc); }

inline void setnilkey(Node *node) { keytt(node) = LUA_TNIL; }

inline bool keyiscollectable(const Node *node) { return keytt(node) & BIT_ISCOLLECTABLE; }

inline GCObject *gckey(const Node *node) { return keyval(node).gc; }
inline GCObject *gckeyN(const Node *node) { return keyiscollectable(node) ? gckey(node) : nullptr; }

/*
** Dead keys in tables have the tag DEADKEY but keep their original
** gcvalue. This distinguishes them from regular keys but allows them to
** be found when searched in a special way. ('next' needs that to find
** keys removed from a table during a traversal.)
*/
inline void setdeadkey(Node *node) { keytt(node) = LUA_TDEADKEY; }
inline bool keyisdead(const Node *node) { return keytt(node) == LUA_TDEADKEY; }

Table *gco2t(GCObject *);

inline Table *hvalue(const TValue *o) { return check_exp(ttistable(o), gco2t(val_(o).gc)); }

/* }================================================================== */


/* main function to copy values (from 'obj2' to 'obj1') */
inline void setobj(lua_State *L, TValue *obj1, const TValue *obj2) {
  obj1->value_ = obj2->value_; settt_(obj1, obj2->tt_);
  checkliveness(L, obj1); lua_assert(!isnonstrictnil(obj1));
}

/*
** Different types of assignments, according to source and destination.
** (They are mostly equal now, but may be different in the future.)
*/

/* from stack to stack */
inline void setobjs2s(lua_State *L, StkId o1, const StkId o2) { setobj(L, s2v(o1), s2v(o2)); }
/* to stack (not from same stack) */
inline void setobj2s(lua_State *L, StkId o1, const TValue *o2) { setobj(L, s2v(o1), o2); }
/* from table to same table */
inline void setobjt2t(lua_State *L, TValue *o1, const TValue *o2) { setobj(L, o1, o2); }
/* to new object */
inline void setobj2n(lua_State *L, TValue *o1, const TValue *o2) { setobj(L, o1, o2); }
/* to table */
inline void setobj2t(lua_State *L, TValue *o1, const TValue *o2) { setobj(L, o1, o2); }


/*
** 'module' operation for hashing (size is always a power of 2)
*/
inline int lmod(unsigned int s, int size) {
  return check_exp((size & (size-1)) == 0, (cast_int(s & (size-1))));
}

inline int twoto(int x) { return 1 << x; }
inline int sizenode(const Table *t) { return twoto(t->lsizenode); }

/* size of buffer for 'luaO_utf8esc' function */
constexpr int UTF8BUFFSZ = 8;

LUAI_FUNC int luaO_utf8esc (char *buff, unsigned long x);
LUAI_FUNC int luaO_ceillog2 (unsigned int x);
LUAI_FUNC int luaO_rawarith (lua_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
LUAI_FUNC void luaO_arith (lua_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
LUAI_FUNC size_t luaO_str2num (const char *s, TValue *o);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC void luaO_tostring (lua_State *L, TValue *obj);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t srclen);


#endif

