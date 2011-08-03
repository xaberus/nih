#include "table.h"

#if 0

#define UTABLE_MT "nih@utable"

int l_utable_new(lua_State * L)
{
  table_t * table = lua_newuserdata(L, sizeof(table_t));
  mem_allocator_t a;

  a.realloc = lua_getallocf(L, &a.ud);

  table_init(table, a);

  luaL_getmetatable(L, UTABLE_MT);
  lua_setmetatable(L, -2);

  return 1;
}

int l_pushtvalue(lua_State * L, struct tvalue_base * value)
{
  switch ((enum tvalue_type) value->tag) {
    case TVALUE_NONE:
      lua_pushnil(L);
      return 1;
    case TVALUE_TABLE: {
      table_t * table = ((struct tvalue_table *) value)->table;

    }
    case TVALUE_INTEGER:
      lua_pushinteger(L, ((struct tvalue_integer *) value)->integer);
      return 1;
    case TVALUE_UINTEGER:
      lua_pushinteger(L, ((struct tvalue_uinteger *) value)->uinteger);
      return 1;
    case TVALUE_REFERENCE:
      lua_rawgeti(L, LUA_REGISTRYINDEX, ((struct tvalue_reference *) value)->reference);
      return 1;
    default:
      return luaL_error(L, "N/I");
  }
}

int l_utable_index(lua_State * L)
{
  table_t * table = luaL_checkudata(L, 1, UTABLE_MT);

  if (lua_isnumber(L, 2)) {
    uint32_t key = luaL_checkinteger(L, 2);

    tget_t get = table_geti(table, key);

    switch (get.error) {
      case ERR_SUCCESS:
        return l_pushtvalue(L, get.value);
      case ERR_NOT_FOUND:
        lua_pushnil(L);
        break;
      default:
        return luaL_error(L, "error while getting value");
    }
  }

  return 1;
}


const luaL_Reg utable_functions[] = {
  {"new", l_utable_new},
};

/*const luaL_Reg utable_metafunctions[] = {
  {NULL, NULL},
};*/


const luaL_Reg utable_meta_functions[] = {
  {"__index", l_utable_index},
};

const struct libmtable utable_meta = {
  .next = NULL,
  .name = UTABLE_MT,
  .functions = utable_meta_functions,
  .index_self = 1,
};

const struct liblist utable = {
  .next = &LIBLIST_TAIL,
  .libname = "utable",
  .functions = utable_functions,
  .metafunctions = /*utable_metafunctions*/ NULL,
  .mtables = &utable_meta,
};

#undef LIBLIST_TAIL
#define LIBLIST_TAIL utable

#endif
