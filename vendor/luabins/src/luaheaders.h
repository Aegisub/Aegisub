#ifndef LUABINS_LUAHEADERS_H_INCLUDED_
#define LUABINS_LUAHEADERS_H_INCLUDED_

#if defined (__cplusplus) && !defined (LUABINS_LUABUILTASCPP)
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>

#if !defined LUA_VERSION_NUM
#define luaL_Reg luaL_reg
#endif

#if LUA_VERSION_NUM > 501
#define luaL_register(L,n,R) (luaL_newlib(L,R))
#define lua_objlen(L,i) lua_rawlen(L, (i))
#endif

#if defined (__cplusplus) && !defined (LUABINS_LUABUILTASCPP)
}
#endif

#endif /* LUABINS_LUAHEADERS_H_INCLUDED_ */
