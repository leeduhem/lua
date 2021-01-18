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

/* tag with no variants (bits 0-3) */
inline lu_byte novariant(lu_byte t) { return t & 0x0F;}

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
inline lu_byte withvariant(lu_byte t) { return t & 0x3F; }

/* Bit mark for collectable types */
constexpr lu_byte BIT_ISCOLLECTABLE  = 1 << 6;

/* mark a tag as collectable */
constexpr inline lu_byte ctb(lu_byte t) { return t | BIT_ISCOLLECTABLE; }


// Standard nil
constexpr lu_byte LUA_VNIL = makevariant(LUA_TNIL, 0);
// Empty slot (which might be different from a slot containing nil)
constexpr lu_byte LUA_VEMPTY = makevariant(LUA_TNIL, 1);
// Value returned for a key not found in a table (absent key)
constexpr lu_byte LUA_VABSTKEY = makevariant(LUA_TNIL, 2);
// False
constexpr lu_byte LUA_VFALSE = makevariant(LUA_TBOOLEAN, 0);
// True
constexpr lu_byte LUA_VTRUE = makevariant(LUA_TBOOLEAN, 1);
// Thread
constexpr lu_byte LUA_VTHREAD = makevariant(LUA_TTHREAD, 0);
// Integer number
constexpr lu_byte LUA_VNUMINT = makevariant(LUA_TNUMBER, 0);
// Float number
constexpr lu_byte LUA_VNUMFLT = makevariant(LUA_TNUMBER, 1);
// Short string
constexpr lu_byte LUA_VSHRSTR = makevariant(LUA_TSTRING, 0);
// Long string
constexpr lu_byte LUA_VLNGSTR = makevariant(LUA_TSTRING, 1);
// Light userdata should be a variant of userdata, but for compatibility
// reasons they are also different types.
constexpr lu_byte LUA_VLIGHTUSERDATA = makevariant(LUA_TLIGHTUSERDATA, 0);
// Userdata
constexpr lu_byte LUA_VUSERDATA = makevariant(LUA_TUSERDATA, 0);
// Prototypes
constexpr lu_byte LUA_VPROTO = makevariant(LUA_TPROTO, 0);
// Upvalue
constexpr lu_byte LUA_VUPVAL = makevariant(LUA_TUPVAL, 0);
// Lua closure
constexpr lu_byte LUA_VLCL = makevariant(LUA_TFUNCTION, 0);
// Light C function
constexpr lu_byte LUA_VLCF = makevariant(LUA_TFUNCTION, 1);
// C Closure
constexpr lu_byte LUA_VCCL = makevariant(LUA_TFUNCTION, 2);
// Table
constexpr lu_byte LUA_VTABLE = makevariant(LUA_TTABLE, 0);


// Collectable object
struct global_State;  // defined in lstate.h

// Common type for all collectable objects
struct GCObject {
  GCObject *next;
  lu_byte tt;
  lu_byte marked;

  GCObject() = default;
  GCObject(global_State *g, lu_byte tag);
};

/*
** Tagged Values. This is the basic representation of values in Lua:
** an actual value plus a tag with its type.
*/
class TValue {
public:
  TValue() = default;
  TValue(int t): tt_(t) {}
  TValue(GCObject *x): tt_(ctb(x->tt)), gc(x) {}
  TValue(void *x): tt_(LUA_VLIGHTUSERDATA), p(x) {}
  TValue(lua_CFunction x): tt_(LUA_VLCF), f(x) {}
  TValue(lua_Integer x): tt_(LUA_VNUMINT), i(x) {}
  TValue(lua_Number x): tt_(LUA_VNUMFLT), n(x) {}

  TValue &operator=(GCObject *x) {
    gc = x; tt_ = ctb(x->tt);
    return *this;
  }

  TValue &operator=(void *x) {
    p = x; tt_ = LUA_VLIGHTUSERDATA;
    return *this;
  }

  TValue &operator=(lua_CFunction x) {
    f = x; tt_ = LUA_VLCF;
    return *this;
  }

  TValue &operator=(lua_Integer x) {
    i = x; tt_ = LUA_VNUMINT;
    return *this;
  }

  TValue &operator=(lua_Number x) {
    n = x; tt_ = LUA_VNUMFLT;
    return *this;
  }


  explicit operator GCObject *() const { return check_exp(is_collectable(), gc); }
  explicit operator void *() const { return check_exp(is_light_userdata(), p); }
  explicit operator lua_CFunction() const { return check_exp(is_lcf(), f); }
  explicit operator lua_Integer() const { return check_exp(is_integer(), i); }
  explicit operator lua_Number() const { return check_exp(is_float(), n); }

  GCObject *gcvalue() const { return gc; }
  void *pvalue() const { return p; }
  lua_CFunction fvalue() const { return f; }
  lua_Integer ivalue() const { return i; }
  lua_Number nvalue() const { return n; }

  // raw type tag of a TValue
  lu_byte rawtt() const { return tt_; }
  // type tag of a TValue
  lu_byte typetag() const { return withvariant(rawtt()); }
  // type of a TValue
  lu_byte type() const { return novariant(rawtt()); }

  // Update raw type tag of a TValue
  void set_rawtt(lu_byte t) { tt_ = t; }

  // Test for (any kind of) nil
  bool is_nil() const { return checktype(LUA_TNIL); }
  // Test for standard nil
  bool is_strict_nil() const { return checktag(LUA_VNIL); }
  // Test for non-standard nil (used only in assertions)
  bool is_nonstrict_nil() const { return is_nil() && !is_strict_nil(); }
  // Test for absent key
  bool is_absent_key() const { return checktag(LUA_VABSTKEY); }

  bool is_boolean() const { return checktype(LUA_TBOOLEAN); }
  bool is_false() const { return checktag(LUA_VFALSE); }
  bool is_true() const { return checktag(LUA_VTRUE); }

  bool is_collectable() const { return rawtt() & BIT_ISCOLLECTABLE; }

  bool is_thread() const { return checktag(ctb(LUA_VTHREAD)); }

  bool is_number() const { return checktype(LUA_TNUMBER); }
  bool is_float() const { return checktag(LUA_VNUMFLT); }
  bool is_integer() const { return checktag(LUA_VNUMINT); }

  bool is_string() const { return checktype(LUA_TSTRING); }
  bool is_short_string() const { return checktag(ctb(LUA_VSHRSTR)); }
  bool is_long_string() const { return checktag(ctb(LUA_VLNGSTR)); }

  bool is_light_userdata() const { return checktag(LUA_VLIGHTUSERDATA); }
  bool is_full_userdata() const { return checktag(ctb(LUA_VUSERDATA)); }

  bool is_function() const { return checktype(LUA_TFUNCTION); }
  bool is_closure() const { return (rawtt() & 0x1F) == LUA_VLCL; }
  bool is_Lclosure() const { return checktag(ctb(LUA_VLCL)); }
  bool is_Cclosure() const { return checktag(ctb(LUA_VCCL)); }
  bool is_lcf() const { return checktag(LUA_VLCF); }
  bool is_Lfunction() const { return is_Lclosure(); }

  bool is_table() const { return checktag(ctb(LUA_VTABLE)); }

private:
  lu_byte tt_;

  // Union of all Lua values
  union {
    GCObject *gc;    /* collectable objects */
    void *p;         /* light userdata */
    lua_CFunction f; /* light C functions */
    lua_Integer i;   /* integer numbers */
    lua_Number n;    /* float numbers */
  };

  // Functions to test type
  bool checktag(lu_byte t) const { return rawtt() == t; }
  bool checktype(lu_byte t) const { return type() == t; }
};



/* raw type tag of a TValue */
inline lu_byte rawtt(const TValue *o) { return o->rawtt(); }

inline lu_byte ttypetag(const TValue *o) { return o->typetag(); }

/* type of a TValue */
inline lu_byte ttype(const TValue *o) { return o->type(); }

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

/* test for (any kind of) nil */
inline bool ttisnil(const TValue *v) { return v->is_nil(); }

/* test for a standard nil */
inline bool ttisstrictnil(const TValue *v) { return v->is_strict_nil(); }

inline void setnilvalue(TValue *o) { *o = TValue(LUA_VNIL); }

inline bool isabstkey(const TValue *v) { return v->is_absent_key(); }

/*
** test for non-standard nils (used only in assertions)
*/
inline bool isnonstrictnil(const TValue *v) { return v->is_nil() && !v->is_strict_nil(); }

/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/
inline bool isempty(const TValue *v) { return v->is_nil(); }

// A value corresponding to an absent key
extern TValue ABSTKEYCONSTANT;

/* mark an entry as empty */
inline void setempty(TValue *v) { *v = TValue(LUA_VEMPTY); }


/* }================================================================== */


/*
** {==================================================================
** Booleans
** ===================================================================
*/

inline bool ttisboolean(const TValue *o) { return o->is_boolean(); }
inline bool ttisfalse(const TValue *o) { return o->is_false(); }
inline bool ttistrue(const TValue *o) { return o->is_true(); }

inline bool l_isfalse(const TValue *o) { return o->is_false() || o->is_nil(); }

inline void setbfvalue(TValue *o) { *o = TValue(LUA_VFALSE); }
inline void setbtvalue(TValue *o) { *o = TValue(LUA_VTRUE); }

/* }================================================================== */


/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/

inline lu_byte iscollectable(const TValue *o) { return o->is_collectable(); }

inline GCObject *gcvalue(const TValue *o) { return static_cast<GCObject *>(*o); }
inline GCObject *gcvalueraw(const TValue *o) { return o->gcvalue(); }

inline void setgcovalue(lua_State *, TValue *o, GCObject *x) { *o = x; }

/* Functions for internal tests */

/* collectable object has the same tag as the original value */
inline bool righttt(const TValue *o) {
  return o->typetag() == static_cast<GCObject *>(*o)->tt;
}

/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/
const global_State *G(const lua_State*);
bool isdead(const global_State *, const GCObject *);

inline void checkliveness(const lua_State *L, const TValue *o) {
  lua_longassert(!o->is_collectable()
		 || (righttt(o) && (L == nullptr || !isdead(G(L), static_cast<GCObject *>(*o)))));
}

/* }================================================================== */


/*
** {==================================================================
** Threads
** ===================================================================
*/

inline bool ttisthread(const TValue *o) { return o->is_thread(); }

lua_State *gco2th(GCObject *);

inline lua_State *thvalue(const TValue *o) {
  return check_exp(o->is_thread(), gco2th(static_cast<GCObject *>(*o)));
}

/* }================================================================== */


/*
** {==================================================================
** Numbers
** ===================================================================
*/

inline bool ttisnumber(const TValue *o) { return o->is_number(); }
inline bool ttisfloat(const TValue *o) { return o->is_float(); }
inline bool ttisinteger(const TValue *o) { return o->is_integer(); }

inline lua_Number fltvalue(const TValue *o) { return static_cast<lua_Number>(*o); }
inline lua_Number fltvalueraw(const TValue *o) { return o->nvalue(); }
inline lua_Integer ivalue(const TValue *o) { return static_cast<lua_Integer>(*o); }
inline lua_Integer ivalueraw(const TValue *o) { return o->ivalue(); }
inline lua_Number nvalue(const TValue *o) {
  return check_exp(o->is_number(),
		   o->is_integer() ? static_cast<lua_Integer>(*o) : static_cast<lua_Number>(*o));
}

inline void setfltvalue(TValue *o, const lua_Number x) { *o = x; }
inline void chgfltvalue(TValue *o, const lua_Number x) { lua_assert(o->is_float()); *o = x; }

inline void setivalue(TValue *o, const lua_Integer x) { *o = x; }
inline void chgivalue(TValue *o, const lua_Integer x) { lua_assert(o->is_integer()); *o = x; }

/* }================================================================== */


/*
** {==================================================================
** Strings
** ===================================================================
*/

inline bool ttisstring(const TValue *o) { return o->is_string(); }
inline bool ttisshrstring(const TValue *o) { return o->is_short_string(); }
inline bool ttislngstring(const TValue *o) { return o->is_long_string(); }

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
  return check_exp(o->is_string(), gco2ts(static_cast<GCObject *>(*o)));
}
inline TString *tsvalueraw(const TValue *o) { return gco2ts(o->gcvalue()); }

/* get the actual string (array of bytes) from a Lua value */
inline const char *svalue(const TValue *o) { return getstr(tsvalue(o)); }

/* get string length from 'TString *s' */
inline size_t tsslen(const TString *ts) {
  return ts->tt == LUA_VSHRSTR ? ts->shrlen : ts->u.lnglen;
}

/* get string length from 'TValue *o' */
inline size_t vslen(const TValue *o) { return tsslen(tsvalue(o)); }

/* }================================================================== */


/*
** {==================================================================
** Userdata
** ===================================================================
*/

inline bool ttislightuserdata(const TValue *o) { return o->is_light_userdata(); }
inline bool ttisfulluserdata(const TValue *o) { return o->is_full_userdata(); }


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

inline Udata *uvalue(const TValue *o) {
  return check_exp(o->is_full_userdata(), gco2u(static_cast<GCObject *>(*o)));
}

inline void *pvalue(const TValue *o) { return static_cast<void *>(*o); }
inline void *pvalueraw(const TValue *o) { return o->pvalue(); }

/* }================================================================== */


/*
** {==================================================================
** Prototypes
** ===================================================================
*/

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
  int linedefined = 0;  /* debug information  */
  int lastlinedefined = 0;  /* debug information  */
  TValue *k = nullptr;  /* constants used by the function */
  Instruction *code = nullptr;  /* opcodes */
  std::vector<Proto *> p; // functions defined inside the function
  std::vector<Upvaldesc, lua::allocator<Upvaldesc>> upvalues;  // upvalue information
  std::vector<ls_byte, lua::allocator<ls_byte>> lineinfo;  // information about source lines (debug information)
  std::vector<AbsLineInfo> abslineinfo;  // idem
  std::vector<LocVar> locvars;  // information about local variables (debug information)
  TString  *source = nullptr;  /* used for debug information */
  GCObject *gclist = nullptr;

  Proto(lua_State *L, lu_byte tag)
    : GCObject(G(L), tag)
    , upvalues(lua::allocator<Upvaldesc>(L))
    , lineinfo(lua::allocator<ls_byte>(L))
  {}
};

/* }================================================================== */


/*
** {==================================================================
** Functions
** ===================================================================
*/

inline bool ttisfunction(const TValue *o) { return o->is_function(); }
inline bool ttisclosure(const TValue *o) { return o->is_closure(); }
inline bool ttisLclosure(const TValue *o) { return o->is_Lclosure(); }
inline bool ttislcf(const TValue *o) { return o->is_lcf(); }
inline bool ttisCclosure(const TValue *o) { return o->is_Cclosure(); }

inline bool isLfunction(const TValue *o) { return o->is_Lfunction(); }


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

inline Closure *clvalue(const TValue *o) {
  return check_exp(o->is_closure(), gco2cl(static_cast<GCObject *>(*o)));
}
inline LClosure* clLvalue(const TValue *o) {
  return check_exp(o->is_Lclosure(), gco2lcl(static_cast<GCObject *>(*o)));
}
inline CClosure *clCvalue(const TValue *o) {
  return check_exp(o->is_Cclosure(), gco2ccl(static_cast<GCObject *>(*o)));
}

inline lua_CFunction fvalue(const TValue *o) { return static_cast<lua_CFunction>(*o); }
inline lua_CFunction fvalueraw(const TValue *o) { return o->fvalue(); }

inline Proto *getproto(const TValue *o) { return clLvalue(o)->p; }

/* }================================================================== */


/*
** {==================================================================
** Tables
** ===================================================================
*/

inline bool ttistable(const TValue *o) { return o->is_table(); }


/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries.
*/
struct Node {
  TValue val;
  TValue key;
  int next;   // for chaining
};


/* copy a value into a key */
inline void setnodekey(lua_State *L, Node *node, const TValue *obj) {
  node->key = *obj;
  checkliveness(L, obj);
}

/* copy a value from a key */
inline void getnodekey(lua_State *L, TValue *obj, const Node *node) {
  *obj = node->key;
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
inline lu_byte keytt(const Node *node) { return node->key.rawtt(); }

inline bool keyisnil(const Node *node) { return node->key.is_nil(); }
inline bool keyisinteger(const Node *node) { return node->key.is_integer(); }
inline lua_Integer keyival(const Node *node) { return node->key.ivalue(); }
inline bool keyisshrstr(const Node *node) { return node->key.is_short_string(); }
inline TString *keystrval(const Node *node) { return gco2ts(node->key.gcvalue()); }
inline lua_Number keyfltvalue(const Node *node) { return node->key.nvalue(); }
inline void *keypvalue(const Node *node) { return node->key.pvalue(); }
inline lua_CFunction keyfvalue(const Node *node) { return node->key.fvalue(); }
inline GCObject *keygcvalue(const Node *node) { return node->key.gcvalue(); }

inline void setnilkey(Node *node) { node->key = TValue(LUA_TNIL); }

inline bool keyiscollectable(const Node *node) { return node->key.is_collectable(); }

inline GCObject *gckey(const Node *node) { return node->key.gcvalue(); }
inline GCObject *gckeyN(const Node *node) {
  return keyiscollectable(node) ? gckey(node) : nullptr;
}

/*
** Dead keys in tables have the tag DEADKEY but keep their original
** gcvalue. This distinguishes them from regular keys but allows them to
** be found when searched in a special way. ('next' needs that to find
** keys removed from a table during a traversal.)
*/
inline void setdeadkey(Node *node) { node->key.set_rawtt(LUA_TDEADKEY); }
inline bool keyisdead(const Node *node) { return keytt(node) == LUA_TDEADKEY; }

Table *gco2t(GCObject *);

inline Table *hvalue(const TValue *o) {
  return check_exp(o->is_table(), gco2t(static_cast<GCObject *>(*o)));
}

/* }================================================================== */


/* main function to copy values (from 'obj2' to 'obj1') */
inline void setobj(lua_State *L, TValue *obj1, const TValue *obj2) {
  *obj1 = *obj2;
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
LUAI_FUNC int luaO_rawarith (lua_State *L, int op, const TValue *p1, const TValue *p2, TValue *res);
LUAI_FUNC void luaO_arith (lua_State *L, int op, const TValue *p1, const TValue *p2, StkId res);
LUAI_FUNC size_t luaO_str2num (const char *s, TValue *o);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC void luaO_tostring (lua_State *L, TValue *obj);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t srclen);

#endif

