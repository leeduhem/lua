/*
** $Id: lparser.h $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"

#include <vector>

/*
** Expression and variable descriptor.
** Code generation for variables and expressions can be delayed to allow
** optimizations; An 'expdesc' structure describes a potentially-delayed
** variable/expression. It has a description of its "main" value plus a
** list of conditional jumps that can also produce its value (generated
** by short-circuit operators 'and'/'or').
*/

/* kinds of variables/expressions */
enum expkind {
  VVOID,  /* when 'expdesc' describes the last expression of a list,
             this kind means an empty list (so, no expression) */
  VNIL,  /* constant nil */
  VTRUE,  /* constant true */
  VFALSE,  /* constant false */
  VK,  /* constant in 'k'; info = index of constant in 'k' */
  VKFLT,  /* floating constant; nval = numerical float value */
  VKINT,  /* integer constant; ival = numerical integer value */
  VKSTR,  /* string constant; strval = TString address;
             (string is fixed by the lexer) */
  VNONRELOC,  /* expression has its value in a fixed register;
                 info = result register */
  VLOCAL,  /* local variable; var.sidx = stack index (local register);
              var.vidx = relative index in 'actvar'  */
  VUPVAL,  /* upvalue variable; info = index of upvalue in 'upvalues' */
  VCONST,  /* compile-time <const> variable;
              info = absolute index in 'actvar'  */
  VINDEXED,  /* indexed variable;
                ind.t = table register;
                ind.idx = key's R index */
  VINDEXUP,  /* indexed upvalue;
                ind.t = table upvalue;
                ind.idx = key's K index */
  VINDEXI, /* indexed variable with constant integer;
                ind.t = table register;
                ind.idx = key's value */
  VINDEXSTR, /* indexed variable with literal string;
                ind.t = table register;
                ind.idx = key's K index */
  VJMP,  /* expression is a test/comparison;
            info = pc of corresponding jump instruction */
  VRELOC,  /* expression can put result in any register;
              info = instruction pc */
  VCALL,  /* expression is a function call; info = instruction pc */
  VVARARG  /* vararg expression; info = instruction pc */
};


inline bool vkisvar(expkind k)     { return (VLOCAL <= k && k <= VINDEXSTR); }
inline bool vkisindexed(expkind k) { return (VINDEXED <= k && k <= VINDEXSTR); }


struct expdesc {
  expkind k;
  union {
    lua_Integer ival;    /* for VKINT */
    lua_Number nval;  /* for VKFLT */
    TString *strval;  /* for VKSTR */
    int info;  /* for generic use */
    struct {  /* for indexed variables */
      short idx;  /* index (R or "long" K) */
      lu_byte t;  /* table (register or upvalue) */
    } ind;
    struct {  /* for local variables */
      lu_byte sidx;  /* index in the stack */
      unsigned short vidx;  /* compiler index (in 'actvar')  */
    } var;
  } u;
  int t;  /* patch list of 'exit when true' */
  int f;  /* patch list of 'exit when false' */
};


/* kinds of variables */
constexpr lu_byte VDKREG     = 0;   /* regular */
constexpr lu_byte RDKCONST   = 1;   /* constant */
constexpr lu_byte RDKTOCLOSE = 2;   /* to-be-closed */
constexpr lu_byte RDKCTC     = 3;   /* compile-time constant */

/* description of an active local variable */
struct Vardesc {
  TValue k;      // constant value (if it is a compile-time constant)
  lu_byte kind;
  lu_byte sidx;  // index of the variable in the stack
  short pidx;    // index of the variable in the Proto's 'locvars' array
  TString *name; // variable name

  Vardesc() = default;
  Vardesc(TString *name1, int kind1)
    : kind(static_cast<lu_byte>(kind1)), name(name1)
  {}
};


/* description of pending goto statements and label statements */
struct Labeldesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  lu_byte nactvar;  /* number of active variables in that position */
  lu_byte close;  /* goto that escapes upvalues */

  Labeldesc() = default;
  Labeldesc(TString *name1, int pc1, int line1, lu_byte nactvar1, lu_byte close1)
    : name(name1), pc(pc1), line(line1), nactvar(nactvar1), close(close1) {}
};


/* list of labels or gotos */
typedef std::vector<Labeldesc, lua::allocator<Labeldesc>> Labellist;

/* dynamic structures used by the parser */
struct Dyndata {
  std::vector<Vardesc, lua::allocator<Vardesc>> actvar;  // list of all active local variables
  Labellist gt;  /* list of pending gotos */
  Labellist label;   /* list of active labels */

  Dyndata(lua_State *L)
    : actvar(lua::allocator<Vardesc>(L))
    , gt(lua::allocator<Labeldesc>(L))
    , label(lua::allocator<Labeldesc>(L))
  {}
};


/* control of blocks */
struct BlockCnt;  /* defined in lparser.c */


/* state needed to generate code for a given function */
struct FuncState {
  Proto *f = nullptr;  /* current function header */
  struct FuncState *prev = nullptr;  /* enclosing function */
  struct LexState *ls = nullptr;  /* lexical state */
  std::vector<BlockCnt *, lua::allocator<BlockCnt *>> blocks; // chain of current blocks
  int pc = 0;  /* next position to code (equivalent to 'ncode') */
  int lasttarget = 0;   /* 'label' of last 'jump label' */
  int previousline = 0;  /* last line that was saved in 'lineinfo' */
  int nk = 0;  /* number of elements in 'k' */
  int nabslineinfo = 0;  /* number of elements in 'abslineinfo' */
  int firstlocal = 0;  /* index of first local var (in Dyndata array) */
  int firstlabel = 0;  /* index of first label (in 'dyd->label->arr') */
  lu_byte nactvar = 0;  /* number of active local variables */
  lu_byte freereg = 0;  /* first free register */
  lu_byte iwthabs = 0;  /* instructions issued since last absolute line info */
  lu_byte needclose = 0;  /* function needs to close upvalues when returning */

  FuncState(lua_State *L)
    : blocks(lua::allocator<BlockCnt *>(L))
  {}
};


LUAI_FUNC int luaY_nvarstack (FuncState *fs);
LUAI_FUNC LClosure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                 Dyndata *dyd, const char *name, int firstchar);


#endif
