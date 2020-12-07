/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include <limits.h>

#include "lobject.h"
#include "lzio.h"


/*
** Single-char tokens (terminal symbols) are represented by their own
** numeric code. Other tokens start at the following value.
*/
#define FIRST_RESERVED	(UCHAR_MAX + 1)


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
#define NUM_RESERVED	(cast_int(TK_WHILE-FIRST_RESERVED + 1))


typedef union {
  lua_Number r;
  lua_Integer i;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


/* state of the lexer plus state of the parser when shared by all
   functions */
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

  // Public interface
 public:
  void set_input (lua_State *L, ZIO *z, TString *source, int firstchar);
  TString *new_string (const char *str, size_t l);
  void next_token ();
  int look_ahead ();
  l_noret syntax_error (const char *msg) { lexerror(msg, t.token); }
  const char *token2str (int token);

 private:
  void next() { current = zgetc(z); }
  bool current_is_newline() { return current == '\n' || current == '\r'; }
  void save_and_next() { save(current); next(); }

  void save(int c);
  const char *txtToken(int token);
  l_noret lexerror(const char *msg, int token);
  void increment_line_number();

  int check_next1(int c);
  int check_next2(const char *set);

  int read_numeral(SemInfo *seminfo);
  size_t skip_sep();
  void read_long_string(SemInfo *seminfo, size_t sep);
  void escape_check(int c, const char *msg);

  int gethexa();
  int readhexaesc();
  unsigned long readutf8esc();
  void utf8esc();
  int readdecesc();
  void read_string(int del, SemInfo *seminfo);

  int llex(SemInfo *seminfo);
};


LUAI_FUNC void luaX_init (lua_State *L);

LUAI_FUNC inline void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
				     TString *source, int firstchar)
{
  ls->set_input(L, z, source, firstchar);
}

LUAI_FUNC inline TString *luaX_newstring (LexState *ls, const char *str, size_t l)
{
  return ls->new_string(str, l);
}

LUAI_FUNC inline void luaX_next (LexState *ls)
{
  ls->next_token();
}

LUAI_FUNC inline int luaX_lookahead (LexState *ls)
{
  return ls->look_ahead();
}

LUAI_FUNC inline l_noret luaX_syntaxerror (LexState *ls, const char *s)
{
  ls->syntax_error(s);
}

LUAI_FUNC inline const char *luaX_token2str (LexState *ls, int token)
{
  return ls->token2str(token);
}


#endif
