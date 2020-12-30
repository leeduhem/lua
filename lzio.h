/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/


#ifndef lzio_h
#define lzio_h

#include "lua.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;

  void init_buffer(lua_State *) {
    buffer = nullptr;
    buffsize = 0;
  }

  char *get_buffer()  { return buffer; }
  size_t buffer_size() { return buffsize; }
  size_t &buff_len() { return n; }
  void reset_buffer() { n = 0; }
  void buff_remove(size_t i) { n -= i; }

  void resize_buffer(lua_State *L, size_t size) {
    buffer = luaM_reallocvchar(L, buffer, buffsize, size);
    buffsize = size;
  }

  void free_buffer(lua_State *L) { resize_buffer(L, 0); }
};

inline void luaZ_initbuffer(lua_State *L, Mbuffer *buff) {
  buff->init_buffer(L);
}

inline char *luaZ_buffer(Mbuffer *buff) { return buff->get_buffer(); }
inline size_t luaZ_sizebuffer(Mbuffer *buff) { return buff->buffer_size(); }
inline size_t &luaZ_bufflen(Mbuffer *buff) { return buff->buff_len(); }

inline void luaZ_buffremove(Mbuffer *buff, size_t i) { buff->buff_remove(i); }
inline void luaZ_resetbuffer(Mbuffer *buff) { buff->reset_buffer(); }

inline void luaZ_resizebuffer(lua_State *L, Mbuffer *buff, size_t size) { buff->resize_buffer(L, size); }
inline void luaZ_freebuffer(lua_State *L, Mbuffer *buff) { buff->resize_buffer(L, 0); }


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
