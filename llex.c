/*
** $Id: llex.c $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#define llex_c
#define LUA_CORE

#include "lprefix.h"


#include <locale.h>
#include <string.h>

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lzio.h"

/* ORDER RESERVED */
static const char *const luaX_tokens [] = {
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "goto", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while",
    "//", "..", "...", "==", ">=", "<=", "~=",
    "<<", ">>", "::", "<eof>",
    "<number>", "<integer>", "<name>", "<string>"
};

void luaX_init (lua_State *L) {
  TString *e = luaS_newliteral(L, LUA_ENV);  /* create env name */
  luaC_fix(L, obj2gco(e));  /* never collect this name */
  for (int i=0; i<NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, luaX_tokens[i]);
    luaC_fix(L, obj2gco(ts));  /* reserved words are never collected */
    ts->extra = cast_byte(i+1);  /* reserved word */
  }
}


void LexState::save (int c) {
  buff->push_back(c);
}


const char *LexState::token2str (int token) {
  if (token < FIRST_RESERVED) {  /* single-byte symbols? */
    if (lisprint(token))
      return luaO_pushfstring(L, "'%c'", token);
    else  /* control character */
      return luaO_pushfstring(L, "'<\\%d>'", token);
  }
  else {
    const char *s = luaX_tokens[token - FIRST_RESERVED];
    if (token < TK_EOS)  /* fixed format (symbols and reserved words)? */
      return luaO_pushfstring(L, "'%s'", s);
    else  /* names, strings, and numerals */
      return s;
  }
}


const char *LexState::txtToken (int token) {
  switch (token) {
    case TK_NAME: case TK_STRING:
    case TK_FLT: case TK_INT:
      save('\0');
      return luaO_pushfstring(L, "'%s'", buff->get_buffer());
    default:
      return token2str(token);
  }
}


l_noret LexState::lexerror (const char *msg, int token) {
  msg = luaG_addinfo(L, msg, source, linenumber);
  if (token)
    luaO_pushfstring(L, "%s near %s", msg, txtToken(token));
  luaD_throw(L, LUA_ERRSYNTAX);
}


/*
** creates a new string and anchors it in scanner's table so that
** it will not be collected until the end of the compilation
** (by that time it should be anchored somewhere else)
*/

TString *LexState::new_string (const char *str, size_t l)
{
  TString *ts = luaS_newlstr(L, str, l);  // create new string
  setsvalue2s(L, L->top++, ts);  // temporarily anchor it in stack
  TValue *o = luaH_set(L, h, s2v(L->top - 1));  // entry for 'str'
  if (isempty(o)) {  // not in use yet?
    // boolean value does not need GC barrier;
    // table is not metatable, so it does not need to invalidate cache
    setbtvalue(o);  // t[string] = true
    luaC_checkGC(L);
  } else {  // string already present
    ts = keystrval(nodefromval(o));  // re-use value previously stored
  }
  L->top--;  // remove string from stack
  return ts;
}


/*
** increment line number and skips newline sequence (any of
** \n, \r, \n\r, or \r\n)
*/
void LexState::increment_line_number () {
  int old = current;
  lua_assert(current_is_newline());
  next();  /* skip '\n' or '\r' */
  if (current_is_newline() && current != old)
    next();  /* skip '\n\r' or '\r\n' */
  if (++linenumber >= MAX_INT)
    lexerror("chunk has too many lines", 0);
}


void LexState::set_input (lua_State *L1, ZIO *z1, TString *source1, int firstchar) {
  t.token = 0;
  L = L1;
  current = firstchar;
  lookahead.token = TK_EOS;  /* no look-ahead token */
  z = z1;
  fs = NULL;
  linenumber = 1;
  lastline = 1;
  source = source1;
  envn = luaS_newliteral(L, LUA_ENV);  /* get env name */
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


int LexState::check_next1 (int c) {
  if (current == c) {
    next();
    return 1;
  }
  else return 0;
}


/*
** Check whether current char is in set 'set' (with two chars) and
** saves it
*/
int LexState::check_next2 (const char *set) {
  lua_assert(set[2] == '\0');
  if (current == set[0] || current == set[1]) {
    save_and_next();
    return 1;
  }
  else return 0;
}


/* LUA_NUMBER */
/*
** This function is quite liberal in what it accepts, as 'luaO_str2num'
** will reject ill-formed numerals. Roughly, it accepts the following
** pattern:
**
**   %d(%x|%.|([Ee][+-]?))* | 0[Xx](%x|%.|([Pp][+-]?))*
**
** The only tricky part is to accept [+-] only after a valid exponent
** mark, to avoid reading '3-4' or '0xe+1' as a single number.
**
** The caller might have already read an initial dot.
*/
int LexState::read_numeral (SemInfo *seminfo) {
  TValue obj;
  const char *expo = "Ee";
  int first = current;
  lua_assert(lisdigit(current));
  save_and_next();
  if (first == '0' && check_next2("xX"))  /* hexadecimal? */
    expo = "Pp";
  for (;;) {
    if (check_next2(expo))  /* exponent mark? */
      check_next2("-+");  /* optional exponent sign */
    else if (lisxdigit(current) || current == '.')  /* '%x|%.' */
      save_and_next();
    else break;
  }
  if (lislalpha(current))  /* is numeral touching a letter? */
    save_and_next();  /* force an error */
  save('\0');
  if (luaO_str2num(buff->get_buffer(), &obj) == 0)  /* format error? */
    lexerror("malformed number", TK_FLT);
  if (ttisinteger(&obj)) {
    seminfo->i = ivalue(&obj);
    return TK_INT;
  }
  else {
    lua_assert(ttisfloat(&obj));
    seminfo->r = fltvalue(&obj);
    return TK_FLT;
  }
}


/*
** read a sequence '[=*[' or ']=*]', leaving the last bracket. If
** sequence is well formed, return its number of '='s + 2; otherwise,
** return 1 if it is a single bracket (no '='s and no 2nd bracket);
** otherwise (an unfinished '[==...') return 0.
*/
size_t LexState::skip_sep () {
  size_t count = 0;
  int s = current;
  lua_assert(s == '[' || s == ']');
  save_and_next();
  while (current == '=') {
    save_and_next();
    count++;
  }
  return (current == s) ? count + 2
         : (count == 0) ? 1
         : 0;
}


void LexState::read_long_string (SemInfo *seminfo, size_t sep) {
  int line = linenumber;  /* initial line (for error message) */
  save_and_next();  /* skip 2nd '[' */
  if (current_is_newline())  /* string starts with a newline? */
    increment_line_number();  /* skip it */
  for (;;) {
    switch (current) {
      case EOZ: {  /* error */
        const char *what = (seminfo ? "string" : "comment");
        const char *msg = luaO_pushfstring(L,
                     "unfinished long %s (starting at line %d)", what, line);
        lexerror(msg, TK_EOS);
        break;  /* to avoid warnings */
      }
      case ']': {
        if (skip_sep() == sep) {
          save_and_next();  /* skip 2nd ']' */
          goto endloop;
        }
        break;
      }
      case '\n': case '\r': {
        save('\n');
	increment_line_number();
        if (!seminfo) buff->reset_buffer();  /* avoid wasting space */
        break;
      }
      default: {
        if (seminfo) save_and_next();
        else next();
      }
    }
  } endloop:
  if (seminfo)
    seminfo->ts = new_string(buff->get_buffer() + sep, buff->buffer_size() - 2 * sep);
}

void LexState::escape_check(int c, const char *msg) {
  if (!c) {
    if (current != EOZ)
      save_and_next();  /* add current to buffer for error message */
    lexerror(msg, TK_STRING);
  }
}


int LexState::gethexa () {
  save_and_next();
  escape_check (lisxdigit(current), "hexadecimal digit expected");
  return luaO_hexavalue(current);
}


int LexState::readhexaesc () {
  int r = gethexa();
  r = (r << 4) + gethexa();
  buff->buff_remove(2);  /* remove saved chars from buffer */
  return r;
}


unsigned long LexState::readutf8esc () {
  int i = 4;  /* chars to be removed: '\', 'u', '{', and first digit */
  save_and_next();  /* skip 'u' */
  escape_check(current == '{', "missing '{'");
  unsigned long r = gethexa();  /* must have at least one digit */
  while (cast_void(save_and_next()), lisxdigit(current)) {
    i++;
    escape_check(r <= (0x7FFFFFFFu >> 4), "UTF-8 value too large");
    r = (r << 4) + luaO_hexavalue(current);
  }
  escape_check(current == '}', "missing '}'");
  next();  /* skip '}' */
  buff->buff_remove(i);  /* remove saved chars from buffer */
  return r;
}


void LexState::utf8esc () {
  char buf[UTF8BUFFSZ];
  int n = luaO_utf8esc(buf, readutf8esc());
  for (; n > 0; n--)  /* add 'buff' to string */
    save(buf[UTF8BUFFSZ - n]);
}


int LexState::readdecesc () {
  int i, r = 0;  /* result accumulator */
  for (i = 0; i < 3 && lisdigit(current); i++) {  /* read up to 3 digits */
    r = 10*r + current - '0';
    save_and_next();
  }
  escape_check(r <= UCHAR_MAX, "decimal escape too large");
  buff->buff_remove(i);  /* remove read digits from buffer */
  return r;
}


void LexState::read_string (int del, SemInfo *seminfo) {
  save_and_next();  /* keep delimiter (for error messages) */
  while (current != del) {
    switch (current) {
      case EOZ:
        lexerror("unfinished string", TK_EOS);
        break;  /* to avoid warnings */
      case '\n':
      case '\r':
        lexerror("unfinished string", TK_STRING);
        break;  /* to avoid warnings */
      case '\\': {  /* escape sequences */
        int c;  /* final character to be saved */
        save_and_next();  /* keep '\\' for error messages */
        switch (current) {
          case 'a': c = '\a'; goto read_save;
          case 'b': c = '\b'; goto read_save;
          case 'f': c = '\f'; goto read_save;
          case 'n': c = '\n'; goto read_save;
          case 'r': c = '\r'; goto read_save;
          case 't': c = '\t'; goto read_save;
          case 'v': c = '\v'; goto read_save;
          case 'x': c = readhexaesc(); goto read_save;
          case 'u': utf8esc();  goto no_save;
          case '\n': case '\r':
            increment_line_number(); c = '\n'; goto only_save;
          case '\\': case '\"': case '\'':
            c = current; goto read_save;
          case EOZ: goto no_save;  /* will raise an error next loop */
          case 'z': {  /* zap following span of spaces */
            buff->buff_remove(1);  /* remove '\\' */
            next();  /* skip the 'z' */
            while (lisspace(current)) {
              if (current_is_newline()) increment_line_number();
              else next();
            }
            goto no_save;
          }
          default: {
            escape_check(lisdigit(current), "invalid escape sequence");
            c = readdecesc();  /* digital escape '\ddd' */
            goto only_save;
          }
        }
	read_save:
	next();
	/* go through */
	only_save:
	buff->buff_remove(1);  /* remove '\\' */
	save(c);
	/* go through */
	no_save: break;
      }
      default:
        save_and_next();
    }
  }
  save_and_next();  /* skip delimiter */
  seminfo->ts = new_string(buff->get_buffer() + 1, buff->buffer_size() - 2);
}


int LexState::llex (SemInfo *seminfo) {
  buff->reset_buffer();
  for (;;) {
    switch (current) {
      case '\n': case '\r': {  /* line breaks */
	increment_line_number();
        break;
      }
      case ' ': case '\f': case '\t': case '\v': {  /* spaces */
        next();
        break;
      }
      case '-': {  /* '-' or '--' (comment) */
        next();
        if (current != '-') return '-';
        /* else is a comment */
        next();
        if (current == '[') {  /* long comment? */
          size_t sep = skip_sep();
          buff->reset_buffer();  /* 'skip_sep' may dirty the buffer */
          if (sep >= 2) {
            read_long_string(NULL, sep);  /* skip long comment */
            buff->reset_buffer();  /* previous call may dirty the buff. */
            break;
          }
        }
        /* else short comment */
        while (!current_is_newline() && current != EOZ)
          next();  /* skip until end of line (or end of file) */
        break;
      }
      case '[': {  /* long string or simply '[' */
        size_t sep = skip_sep();
        if (sep >= 2) {
          read_long_string(seminfo, sep);
          return TK_STRING;
        }
        else if (sep == 0)  /* '[=...' missing second bracket? */
          lexerror("invalid long string delimiter", TK_STRING);
        return '[';
      }
      case '=': {
        next();
        if (check_next1('=')) return TK_EQ;  /* '==' */
        else return '=';
      }
      case '<': {
        next();
        if (check_next1('=')) return TK_LE;  /* '<=' */
        else if (check_next1('<')) return TK_SHL;  /* '<<' */
        else return '<';
      }
      case '>': {
        next();
        if (check_next1('=')) return TK_GE;  /* '>=' */
        else if (check_next1('>')) return TK_SHR;  /* '>>' */
        else return '>';
      }
      case '/': {
        next();
        if (check_next1('/')) return TK_IDIV;  /* '//' */
        else return '/';
      }
      case '~': {
        next();
        if (check_next1('=')) return TK_NE;  /* '~=' */
        else return '~';
      }
      case ':': {
        next();
        if (check_next1(':')) return TK_DBCOLON;  /* '::' */
        else return ':';
      }
      case '"': case '\'': {  /* short literal strings */
        read_string(current, seminfo);
        return TK_STRING;
      }
      case '.': {  /* '.', '..', '...', or number */
        save_and_next();
        if (check_next1('.')) {
          if (check_next1('.'))
            return TK_DOTS;   /* '...' */
          else return TK_CONCAT;   /* '..' */
        }
        else if (!lisdigit(current)) return '.';
        else return read_numeral(seminfo);
      }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9': {
        return read_numeral(seminfo);
      }
      case EOZ: {
        return TK_EOS;
      }
      default: {
        if (lislalpha(current)) {  /* identifier or reserved word? */
          TString *ts;
          do {
            save_and_next();
          } while (lislalnum(current));
          ts = new_string(buff->get_buffer(), buff->buffer_size());
          seminfo->ts = ts;
          if (isreserved(ts))  /* reserved word? */
            return ts->extra - 1 + FIRST_RESERVED;
          else {
            return TK_NAME;
          }
        }
        else {  /* single-char tokens ('+', '*', '%', '{', '}', ...) */
          int c = current;
          next();
          return c;
        }
      }
    }
  }
}


void LexState::next_token () {
  lastline = linenumber;
  if (lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    t = lookahead;  /* use this one */
    lookahead.token = TK_EOS;  /* and discharge it */
  }
  else
    t.token = llex(&t.seminfo);  /* read next token */
}


int LexState::lookahead_token () {
  lua_assert(lookahead.token == TK_EOS);
  lookahead.token = llex(&lookahead.seminfo);
  return lookahead.token;
}
