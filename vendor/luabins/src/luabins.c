/*
* luabins.c
* Luabins Lua module code
* See copyright notice in luabins.h
*/

#include "luaheaders.h"

#include "luabins.h"
#include "saveload.h"

/*
* On success returns data string.
* On failure returns nil and error message.
*/
static int l_save(lua_State * L)
{
  int error = luabins_save(L, 1, lua_gettop(L));
  if (error == 0)
  {
    return 1;
  }

  lua_pushnil(L);
  lua_replace(L, -3); /* Put nil before error message on stack */
  return 2;
}

/*
* On success returns true and loaded data tuple.
* On failure returns nil and error message.
*/
static int l_load(lua_State * L)
{
  int count = 0;
  int error = 0;
  size_t len = 0;
  const unsigned char * data = (const unsigned char *)luaL_checklstring(
      L, 1, &len
    );

  lua_pushboolean(L, 1);

  error = luabins_load(L, data, len, &count);
  if (error == 0)
  {
    return count + 1;
  }

  lua_pushnil(L);
  lua_replace(L, -3); /* Put nil before error message on stack */

  return 2;
}

/* luabins Lua module API */
static const struct luaL_Reg R[] =
{
  { "save", l_save },
  { "load", l_load },
  { NULL, NULL }
};

#ifdef __cplusplus
extern "C" {
#endif

LUALIB_API int luaopen_luabins(lua_State * L)
{
  /*
  * Compile-time checks for size constants.
  * Consult PORTABILITY WARNING in saveload.h before changing constants.
  */

  /* int is too small on your platform, fix LUABINS_LINT */
  luabins_static_assert(sizeof(int) >= LUABINS_LINT);
  /* size_t is too small on your platform, fix LUABINS_LSIZET */
  luabins_static_assert(sizeof(size_t) >= LUABINS_LSIZET);
  /* unexpected lua_Number size, fix LUABINS_LNUMBER */
  luabins_static_assert(sizeof(lua_Number) == LUABINS_LNUMBER);

  /*
  * Register module
  */
  luaL_register(L, "luabins", R);

  /*
  * Register module information
  */
  lua_pushliteral(L, LUABINS_VERSION);
  lua_setfield(L, -2, "_VERSION");

  lua_pushliteral(L, LUABINS_COPYRIGHT);
  lua_setfield(L, -2, "_COPYRIGHT");

  lua_pushliteral(L, LUABINS_DESCRIPTION);
  lua_setfield(L, -2, "_DESCRIPTION");

  return 1;
}

#ifdef __cplusplus
}
#endif
