#include "gpio"

void
Gpio_resource::dump(int indent) const
{
  l4_uint64_t s, e;
  s = start();
  e = end();

  printf("%*.s%s%c [%014llx-%014llx %llx] (dev=%p:%s)\n",
         indent, " ",
         "GPIO", provided() ? '*' : ' ',
         s, e, (l4_uint64_t)size(),
         _hw, _hw ? _hw->name() : "(NULL)");
}

