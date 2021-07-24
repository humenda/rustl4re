#include "support.h"
#include "startup.h"

extern "C" void __main();
void __main()
{
  clear_bss();
  ctor_init();
  Platform_base::iterate_platforms();

  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));
  while(1)
    ;
}
