/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"
#include "lmem.h"

#include <vector>

constexpr int EOZ = -1;  // end of stream

typedef struct Zio ZIO;

struct Mbuffer {
  std::vector<char, lua::lua_allocator<char>> buffer;

  Mbuffer(lua_State *L) :
    buffer(std::vector<char, lua::lua_allocator<char>>(lua::lua_allocator<char>(L))) {}

  const char *data() const { return buffer.data(); }
  size_t size() const { return buffer.size(); }
  void push_back(const char c) { buffer.push_back(c); }
  void clear() { buffer.clear(); }

  void remove(size_t n) {
    lua_assert(n <= buffer.size());
    buffer.resize(buffer.size() - n);
  }
};


/* --------- Private Part ------------------ */

struct Zio {
  size_t n = 0;			/* bytes still unread */
  const char *p = nullptr;	/* current position in buffer */
  lua_Reader reader;		/* reader function */
  void *data;			/* additional data */
  lua_State *L;			/* Lua state (for reader) */

  Zio(lua_State *L1, lua_Reader reader1, void *data1) : reader(reader1), data(data1), L(L1) {}

  size_t read(void *b, size_t n);  // read next n bytes
  int fill();

  int getc() { return n-- > 0 ? cast_uchar(*p++) : fill(); }
};

#endif
