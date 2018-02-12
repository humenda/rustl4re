#include "support.h"
#include "startup.h"

#include <l4/drivers/of.h>

extern "C" void __main(unsigned long p1, unsigned long p2, unsigned long p3);
void __main(unsigned long, unsigned long, unsigned long p3)
{
  clear_bss();
  ctor_init();
  L4_drivers::Of::set_prom(p3); //p3 is OF prom pointer
  Platform_base::iterate_platforms();

  printf("PPC platform initialized\n");
  startup(_mbi_cmdline);
  while(1)
    ;
}
