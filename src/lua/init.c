#ifdef LUA

#include <lua.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

struct libmtable {
  const struct libmtable  * next;
  const char              * name;
  const luaL_Reg          * functions;
  unsigned  int             index_self : 1;
};

struct liblist {
  const struct liblist    * next;
  const char              * libname;
  const luaL_Reg          * functions;
  const luaL_Reg          * metafunctions;
  const struct libmtable  * mtables;
};

const struct liblist libnih = {
  .next = NULL,
  .libname = NULL,
  .functions = NULL,
  .metafunctions = NULL,
  .mtables = NULL,
};

#define LIBLIST_TAIL libnih

#include "lua/table-lua.c"

extern int luaopen_libnih(lua_State * L)
{
  const struct liblist * cur = &LIBLIST_TAIL;

  luaL_checkstack(L, 16, "not enough stack space");

  lua_newtable(L);
  int nihpos = lua_gettop(L);

  while (cur) {
    if (cur->libname) {
      lua_newtable(L);
      int libpos = lua_gettop(L);

      if (cur->functions) {
        for (unsigned int k = 0; cur->functions[k].name; k++) {
          lua_pushcfunction(L, cur->functions[k].func);
          lua_setfield(L, libpos, cur->functions[k].name);
        }
      }

      if (cur->metafunctions) {
        lua_newtable(L);
        int metapos = lua_gettop(L);

        for (unsigned int k = 0; cur->metafunctions[k].name; k++) {
          lua_pushcfunction(L, cur->metafunctions[k].func);
          lua_setfield(L, metapos, cur->metafunctions[k].name);
        }

        lua_setmetatable(L, libpos);
      }

      if (cur->mtables) {
        lua_newtable(L);
        int spos = lua_gettop(L);

        const struct libmtable * m = cur->mtables;
        while (m) {
          if (m->name) {
            luaL_newmetatable(L, m->name);
            int metapos = lua_gettop(L);

            if (m->functions) {
              for (unsigned int k = 0; m->functions[k].name; k++) {
                lua_pushcfunction(L, m->functions[k].func);
                lua_setfield(L, metapos, m->functions[k].name);
              }
            }

            if (m->index_self) {
              lua_pushvalue(L, metapos);
              lua_setfield(L, metapos, "__index");
            }

            lua_setfield(L, spos, m->name);
          }

          m = m->next;
        }

        lua_setfield(L, libpos, "__mtables");
      }

      lua_setfield(L, nihpos, cur->libname);
    }

    cur = cur->next;
  }

  return 1;
}

#endif /* LUA */
