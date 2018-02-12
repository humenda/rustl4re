// vi:ft=cpp
/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

%{
#define SWIG_RECURSIVE_DEFINED defined
%}

%define SWIG_INTEGER_TYPEMAP(TYPE)
%typemap(typecheck,precedence=SWIG_TYPECHECK_INTEGER) TYPE
{$1 = lua_isinteger(L, $input);}
%typemap(in,checkfn="lua_isinteger")     TYPE
%{ $1 = ($type)lua_tointeger(L, $input); %}
%typemap(in,checkfn="lua_isinteger")	TYPE *INPUT($*ltype temp), TYPE &INPUT($*ltype temp)
%{ temp = ($*ltype)lua_tointeger(L,$input);
   $1 = &temp; %}
%typemap(in, numinputs=0) TYPE *OUTPUT ($*ltype temp)
%{ $1 = &temp; %}
%typemap(argout) TYPE *OUTPUT
%{  lua_pushinteger(L, (lua_Integer) *$1); SWIG_arg++;%}
%typemap(in) TYPE *INOUT = TYPE *INPUT;
%typemap(argout) TYPE *INOUT = TYPE *OUTPUT;
%typemap(in) TYPE &OUTPUT = TYPE *OUTPUT;
%typemap(argout) TYPE &OUTPUT = TYPE *OUTPUT;
%typemap(in) TYPE &INOUT = TYPE *INPUT;
%typemap(argout) TYPE &INOUT = TYPE *OUTPUT;
// const version (the $*ltype is the basic number without ptr or const's)
%typemap(in,checkfn="lua_isinteger")	const TYPE *INPUT($*ltype temp)
%{ temp = ($*ltype)lua_tointeger(L,$input);
   $1 = &temp; %}
%typemap(out) TYPE
%{  lua_pushinteger(L, (lua_Integer)($1)); SWIG_arg++;  %}
%enddef


SWIG_INTEGER_TYPEMAP(l4_mword_t); SWIG_INTEGER_TYPEMAP(l4_umword_t);
SWIG_INTEGER_TYPEMAP(l4_int32_t); SWIG_INTEGER_TYPEMAP(l4_uint32_t);
SWIG_INTEGER_TYPEMAP(l4_int64_t); SWIG_INTEGER_TYPEMAP(l4_uint64_t);
SWIG_INTEGER_TYPEMAP(int); SWIG_INTEGER_TYPEMAP(unsigned);
SWIG_INTEGER_TYPEMAP(long int); SWIG_INTEGER_TYPEMAP(unsigned long);
SWIG_INTEGER_TYPEMAP(long long int); SWIG_INTEGER_TYPEMAP(unsigned long long);
SWIG_INTEGER_TYPEMAP(short int); SWIG_INTEGER_TYPEMAP(unsigned short);
SWIG_INTEGER_TYPEMAP(char); SWIG_INTEGER_TYPEMAP(unsigned char); SWIG_INTEGER_TYPEMAP(signed char);
SWIG_INTEGER_TYPEMAP(l4_addr_t);


/* Refined typemaps for our L4 types and our lua lib with integer support */
/*
%typemap(in,checkfn="lua_isnumber") l4_int32_t, l4_int64_t, int, short, long, signed char, long long
%{$1 = ($type)lua_tointeger(L, $input);%}

%typemap(in,checkfn="lua_isnumber") l4_umword_t, l4_uint64_t,unsigned long long,
        unsigned int, unsigned short,
        unsigned long, unsigned char
%{SWIG_contract_assert((lua_tointeger(L,$input)>=0),"number must not be negative")
$1 = ($type)lua_tointeger(L, $input);%}
*/

%typemap(in,checkfn="lua_isinteger") enum SWIGTYPE
%{$1 = ($type)(int)lua_tointeger(L, $input);%}

%typemap(out) enum SWIGTYPE
%{  lua_pushinteger(L, (lua_Integer)($1)); SWIG_arg++;%}

/*
%typemap(out) long long, l4_umword_t,l4_int32_t,l4_int64_t,int,short,long,
             unsigned long long, l4_uint32_t, l4_uint64_t,
             unsigned int,unsigned short,unsigned long,
             signed char,unsigned char
%{  lua_pushinteger(L, $1); SWIG_arg++;%}

%typemap(out) l4_int32_t*,l4_int64_t*,int*,short*,long*,
             l4_uint32_t *, l4_uint64_t *, long long *, unsigned long long*,
             unsigned int*,unsigned short*,unsigned long*,
             signed char*,unsigned char*
%{ lua_pushinteger(L, *$1); SWIG_arg++;%}
*/

%typemap(typecheck,precedence=SWIG_TYPECHECK_STRING) cxx::String, cxx::String const & {
   $1 = lua_isstring(L, $input);
}

%typemap(in,checkfn="lua_isstring") cxx::String
%{$1 = cxx::String(lua_tostring(L, $input), lua_rawlen(L, $input));%}

%typemap(in,checkfn="lua_isstring") cxx::String const & (cxx::String tmp)
%{tmp = cxx::String(lua_tostring(L, $input), lua_rawlen(L, $input)); $1 = &tmp;%}


