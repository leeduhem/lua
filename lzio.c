/*
** $Id: lzio.c $
** Buffered streams
** See Copyright Notice in lua.h
*/

#define lzio_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"


int Zio::fill () {
  size_t size;
  lua_unlock(L);
  const char *buff = reader(L, data, &size);
  lua_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  n = size - 1;  /* discount char being returned */
  p = buff;
  return cast_uchar(*(p++));
}


/* --------------------------------------------------------------- read --- */
size_t Zio::read (void *b, size_t num) {
  while (num) {
    if (n == 0) {  /* no bytes in buffer? */
      if (fill() == EOZ)  /* try to read more */
        return num;  /* no more input; return number of missing bytes */
      else {
        n++;  /* luaZ_fill consumed first byte; put it back */
        p--;
      }
    }
    size_t m = (num <= n) ? num : n;  /* min. between num and n */
    memcpy(b, p, m);
    n -= m;
    p += m;
    b = (char *)b + m;
    num -= m;
  }
  return 0;
}

