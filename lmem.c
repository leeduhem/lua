/*
** $Id: lmem.c $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#define lmem_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


#if defined(EMERGENCYGCTESTS)
/*
** First allocation will fail whenever not building initial state
** and not shrinking a block. (This fail will trigger 'tryagain' and
** a full GC cycle at every allocation.)
*/
static void *firsttry (global_State *g, void *block, size_t os, size_t ns) {
  if (ttisnil(&g->nilvalue) && ns > os)
    return nullptr;  /* fail */
  /* normal allocation */
  return (*g->frealloc)(g->ud, block, os, ns);
}
#else
static void *firsttry (global_State *g, void *block, size_t os, size_t ns) {
  return (*g->frealloc)(g->ud, block, os, ns);
}
#endif





/*
** About the realloc function:
** void *frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** ('osize' is the old size, 'nsize' is the new size)
**
** - frealloc(ud, p, x, 0) frees the block 'p' and returns NULL.
** Particularly, frealloc(ud, NULL, 0, 0) does nothing,
** which is equivalent to free(NULL) in ISO C.
**
** - frealloc(ud, NULL, x, s) creates a new block of size 's'
** (no matter 'x'). Returns NULL if it cannot create the new block.
**
** - otherwise, frealloc(ud, b, x, y) reallocates the block 'b' from
** size 'x' to size 'y'. Returns NULL if it cannot reallocate the
** block to the new size.
*/




/*
** {==================================================================
** Functions to allocate/deallocate arrays for the Parser
** ===================================================================
*/

/*
** Minimum size for arrays during parsing, to avoid overhead of
** reallocating to size 1, then 2, and then 4. All these arrays
** will be reallocated to exact sizes or erased when parsing ends.
*/
constexpr int MINSIZEARRAY = 4;


void *luaM_growaux_ (lua_State *L, void *block, int nelems, int *psize,
                     int size_elems, int limit, const char *what) {
  int size = *psize;
  if (nelems + 1 <= size)  /* does one extra element still fit? */
    return block;  /* nothing to be done */
  if (size >= limit / 2) {  /* cannot double it? */
    if (unlikely(size >= limit))  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    size = limit;  /* still have at least one free place */
  }
  else {
    size *= 2;
    if (size < MINSIZEARRAY)
      size = MINSIZEARRAY;  /* minimum size */
  }
  lua_assert(nelems + 1 <= size && size <= limit);
  /* 'limit' ensures that multiplication will not overflow */
  void *newblock = luaM_saferealloc_(L, block, cast_sizet(*psize) * size_elems,
				     cast_sizet(size) * size_elems);
  *psize = size;  /* update only when everything else is OK */
  return newblock;
}


/*
** In prototypes, the size of the array is also its number of
** elements (to save memory). So, if it cannot shrink an array
** to its number of elements, the only option is to raise an
** error.
*/
void *luaM_shrinkvector_ (lua_State *L, void *block, int *size,
                          int final_n, int size_elem) {
  size_t oldsize = cast_sizet((*size) * size_elem);
  size_t newsize = cast_sizet(final_n * size_elem);
  lua_assert(newsize <= oldsize);
  void *newblock = luaM_saferealloc_(L, block, oldsize, newsize);
  *size = final_n;
  return newblock;
}

/* }================================================================== */


l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}


/*
** Free memory
*/
void luaM_free_ (lua_State *L, void *block, size_t osize) {
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == NULL));
  (*g->frealloc)(g->ud, block, osize, 0);
  g->GCdebt -= osize;
}


/*
** In case of allocation fail, this function will call the GC to try
** to free some memory and then try the allocation again.
** (It should not be called when shrinking a block, because then the
** interpreter may be in the middle of a collection step.)
*/
static void *tryagain (lua_State *L, void *block,
                       size_t osize, size_t nsize) {
  global_State *g = G(L);
  if (ttisnil(&g->nilvalue)) {  /* is state fully build? */
    luaC_fullgc(L, 1);  /* try to free some memory... */
    return (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
  }
  return nullptr;  /* cannot free any memory without a full state */
}


/*
** Generic allocation routine.
** If allocation fails while shrinking a block, do not try again; the
** GC shrinks some blocks and it is not reentrant.
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  global_State *g = G(L);
  lua_assert((osize == 0) == (block == nullptr));
  void *newblock = firsttry(g, block, osize, nsize);
  if (unlikely(newblock == nullptr && nsize > 0)) {
    if (nsize > osize)  /* not shrinking a block? */
      newblock = tryagain(L, block, osize, nsize);
    if (newblock == nullptr)  /* still no memory? */
      return nullptr;  /* do not update 'GCdebt' */
  }
  lua_assert((nsize == 0) == (newblock == nullptr));
  g->GCdebt = (g->GCdebt + nsize) - osize;
  return newblock;
}


void *luaM_saferealloc_ (lua_State *L, void *block, size_t osize,
                                                    size_t nsize) {
  void *newblock = luaM_realloc_(L, block, osize, nsize);
  if (unlikely(newblock == nullptr && nsize > 0))  /* allocation failed? */
    luaM_error(L);
  return newblock;
}


void *luaM_malloc_ (lua_State *L, size_t size, int tag) {
  if (size == 0)
    return nullptr;  /* that's all */

  global_State *g = G(L);
  void *newblock = firsttry(g, nullptr, tag, size);
  if (unlikely(newblock == nullptr)) {
    newblock = tryagain(L, nullptr, tag, size);
    if (newblock == nullptr)
      luaM_error(L);
  }
  g->GCdebt += size;
  return newblock;
}
