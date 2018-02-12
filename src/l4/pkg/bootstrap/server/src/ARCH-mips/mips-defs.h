#pragma once

#include <l4/sys/l4int.h>

namespace Mips {

constexpr l4_addr_t sign_ext(l4_uint32_t base)
{ return (l4_addr_t) ((l4_mword_t) ((l4_int32_t) base)); }

enum : l4_addr_t {
  KSEG0 = sign_ext(0x80000000),
  KSEG1 = sign_ext(0xa0000000)
};

}
