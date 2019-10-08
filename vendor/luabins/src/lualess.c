/*
* lualess.h
* Lua-related definitions for lua-less builds (based on Lua manual)
* See copyright notice in luabins.h
*/

#include <stdlib.h>

/*
* lua_Alloc-compatible allocator to use in Lua-less applications
* with lbs_SaveBuffer. Based on sample code from Lua 5.1 manual.
*/
void * lbs_simplealloc(
    void * ud,
    void * ptr,
    size_t osize,
    size_t nsize
  )
{
  (void) ud;
  (void) osize;  /* not used */

  if (nsize == 0)
  {
    free(ptr);
    return NULL;
  }
  else
  {
    return realloc(ptr, nsize);
  }
}
