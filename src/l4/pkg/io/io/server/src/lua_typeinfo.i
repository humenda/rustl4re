%native(swig_class) int swig_class(lua_State *L);
%native(swig_instance_of) int swig_instance_of(lua_State *L);

%{
inline int swig_class(lua_State *L)
{
  char const *name = lua_tostring(L, 1);
  SWIG_Lua_get_class_metatable(L, name);
  return 1;
}

inline int swig_instance_of(lua_State *L)
{
  swig_lua_userdata* usr;
  if (lua_isnil(L,1) || !lua_isstring(L,2))
    {
      lua_pushboolean(L, false);
      return 1;
    }

  char const *type = lua_tostring(L,2);

  usr=(swig_lua_userdata*)lua_touserdata(L,1);  /* get data */
  if (!usr)
  {
    lua_pushboolean(L, false);
    return 1;
  }

  swig_module_info* module=SWIG_GetModule(L);
  swig_type_info *to = SWIG_TypeQueryModule(module, module, type);
  if (!to)
    {
      lua_pushboolean(L, false);
      return 1;
    }

  swig_cast_info *iter = to->cast;
  while (iter)
    {
      if (iter->type == usr->type)
        {
          lua_pushboolean(L, true);
          return 1;
        }
      iter = iter->next;
    }
  lua_pushboolean(L, false);
  return 1;
}
%}
