/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <typeinfo>
#include <l4/cxx/hlist>
#include <l4/cxx/type_traits>

template< typename BASE, typename MATCH_RESULT = bool >
struct Type_matcher : cxx::H_list_item_t<BASE>
{
  explicit Type_matcher(std::type_info const *type);
  typedef MATCH_RESULT Match_result;

  template< typename ...Args > static
  Match_result match(Args && ...args);

private:
  std::type_info const *_type;

  typedef cxx::H_list_t<BASE> List;
  typedef typename List::Iterator Iterator;

  static List _for_type;
};

template< typename BASE, typename MR >
typename Type_matcher<BASE, MR>::List Type_matcher<BASE, MR>::_for_type(true);

template< typename BASE, typename MR >
Type_matcher<BASE, MR>::Type_matcher(std::type_info const *type)
: _type(type)
{
  if (!type)
    return;

  Iterator i = _for_type.end();
  for (Iterator c = _for_type.begin(); c != _for_type.end(); ++c)
    {
      void *x = 0;
      // use the compiler catch logic to figure out if TYPE
      // is a base class of c->_type, if it is we must put
      // this behind c in the list.
      if (type->__do_catch(c->_type, &x, 0))
        i = c;
    }

  _for_type.insert(static_cast<BASE*>(this), i);
}

/**
 * Iterate over all registered type matchers and call c->do_match until
 * a result that evaluates to true is returned.
 */
template< typename BASE, typename MR >
template< typename ...Args >
typename Type_matcher<BASE, MR>::Match_result
Type_matcher<BASE, MR>::match(Args && ...args)
{
  for (Iterator c = _for_type.begin(); c != _for_type.end(); ++c)
    if (Match_result r = c->do_match(cxx::forward<Args>(args)...))
      return r;

  return Match_result();
}
