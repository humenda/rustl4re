#include "support.h"
#include "startup.h"

extern "C" void __main();
void __main()
{
  clear_bss();
  ctor_init();
  Platform_base::iterate_platforms();

  startup(_mbi_cmdline);
  while(1)
    ;
}
