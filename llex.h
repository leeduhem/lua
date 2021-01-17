/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include <limits.h>

#include "lobject.h"
#include "lstring.h"
#include "lzio.h"


/*
** Single-char tokens (terminal symbols) are represented by their own
** numeric code. Other tokens start at the following value.
*/
constexpr int FIRST_RESERVED = UCHAR_MAX + 1;

#if !defined(LUA_ENV)
#define LUA_ENV		"_ENV"
#endif


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_IDIV, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
  TK_SHL, TK_SHR,
  TK_DBCOLON, TK_EOS,
  TK_FLT, TK_INT, TK_NAME, TK_STRING
};

/* number of reserved words */
constexpr int NUM_RESERVED = cast_int(TK_WHILE-FIRST_RESERVED + 1);


class Token {
  friend inline bool operator==(const Token &a, const Token &b);
  friend inline bool operator==(const Token &a, int tok);

public:
  Token(): token(TK_INT), ival{0} {}
  explicit Token(int tok): token(tok) {}
  Token(lua_Integer i): token(TK_INT), ival(i) {}
  Token(lua_Number r): token(TK_FLT), rval(r) {}
  Token(TString *ts): token(TK_STRING), sval(ts) {}
  Token(TString *ts, int tok): token(tok), sval(ts) {}
  Token(const Token &t): token(t.token) { copyUnion(t); }

  Token &operator=(const Token &t) {
    token = t.token;
    copyUnion(t);
    return *this;
  }

  Token &operator=(lua_Integer i) {
    token = TK_INT;
    ival = i;
    return *this;
  }

  Token &operator=(lua_Number r) {
    token = TK_FLT;
    rval = r;
    return *this;
  }

  Token &operator=(TString *ts) {
    token = TK_STRING;
    sval = ts;
    return *this;
  }

  operator lua_Integer() const { assert(token == TK_INT); return ival; }
  operator lua_Number() const { assert(token == TK_FLT); return rval; }
  operator TString *() const { assert(token == TK_STRING || token == TK_NAME); return sval; }

  operator int() const { return token; }

private:
  int token;  // discriminant
  union {
    lua_Integer ival;
    lua_Number rval;
    TString *sval;
  };

  void copyUnion(const Token &t) {
    switch (t.token) {
    case TK_INT: ival = t.ival; break;
    case TK_FLT: rval = t.rval; break;
    case TK_STRING:
    case TK_NAME:
      sval = t.sval; break;
    }
  }

  bool equalUnion(const Token &t) const {
    switch (t.token) {
    case TK_INT: return ival == t.ival;
    case TK_FLT: return rval == t.rval;
    case TK_STRING:
    case TK_NAME:
      return eqstr(sval, t.sval);
    }
    return true;
  }
};

inline bool operator==(const Token &a, const Token &b) {
  return (a.token == b.token) && a.equalUnion(b);
}

inline bool operator!=(const Token &a, const Token &b) {
  return !(a == b);
}

inline bool operator==(const Token &a, int tok) {
  return a.token == tok;
}

inline bool operator!=(const Token &a, int tok) {
  return !(a == tok);
}

/* state of the lexer plus state of the parser when shared by all functions */
struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token 'consumed' */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* current function (parser) */
  struct lua_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  Table *h;  /* to avoid collection/reuse strings */
  struct Dyndata *dyd;  /* dynamic structures used by the parser */
  TString *source;  /* current source name */
  TString *envn;  /* environment variable name */

public:
  void set_input (lua_State *L, ZIO *z, TString *source, int firstchar);
  TString *new_string (const char *str, size_t l);
  void next_token ();
  int lookahead_token ();
  l_noret syntax_error (const char *msg) { lexerror(msg, t); }
  const char *token2str (int token);

private:
  void next();
  bool current_is_newline();
  void save_and_next();

  void save(int c);
  const char *txtToken(int token);
  l_noret lexerror(const char *msg, int token);
  void increment_line_number();

  int check_next1(int c);
  int check_next2(const char *set);

  Token read_numeral();
  size_t skip_sep();
  TString *read_long_string(size_t sep, bool is_string = false);
  void escape_check(int c, const char *msg);

  int gethexa();
  int readhexaesc();
  unsigned long readutf8esc();
  void utf8esc();
  int readdecesc();
  TString *read_string(int del);

  Token llex();
};


LUAI_FUNC void luaX_init (lua_State *L);

#endif
