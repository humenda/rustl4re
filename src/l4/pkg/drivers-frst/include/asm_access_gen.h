/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Jakub Jermar <jakub.jermar@kernkonzept.com>
 */

#pragma once

#include <l4/sys/l4int.h>
#include <l4/cxx/type_traits>

namespace Asm_access {

template <typename T>
struct is_supported_type
{
  static const bool value = cxx::is_same<T, l4_uint8_t>::value
                            || cxx::is_same<T, l4_uint16_t>::value
                            || cxx::is_same<T, l4_uint32_t>::value
                            || cxx::is_same<T, l4_uint64_t>::value;
};

template <typename T>
inline
typename cxx::enable_if<is_supported_type<T>::value, T>::type
read(T const *mem)
{
  return *reinterpret_cast<volatile T const *>(mem);
}

template <typename T>
inline
typename cxx::enable_if<is_supported_type<T>::value, void>::type
write(T val, T *mem)
{
  *reinterpret_cast<volatile T *>(mem) = val;
}

}
