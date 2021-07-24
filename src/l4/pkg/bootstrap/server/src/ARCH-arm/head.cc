#include "support.h"
#include "startup.h"

extern "C" int __aeabi_unwind_cpp_pr0(void);
extern "C" int __aeabi_unwind_cpp_pr1(void);

enum { _URC_FAILURE  = 9 };
int __aeabi_unwind_cpp_pr0(void) { return _URC_FAILURE; }
int __aeabi_unwind_cpp_pr1(void) { return _URC_FAILURE; }

struct boot_args boot_args;

extern "C" void __main(unsigned long r0, unsigned long r1,
                       unsigned long r2, unsigned long r3);
void __main(unsigned long r0, unsigned long r1,
            unsigned long r2, unsigned long r3)
{
  unsigned long r;

  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r) : : "memory");
  r &= ~1UL;
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");

  clear_bss();

  boot_args.r[0] = r0;
  boot_args.r[1] = r1;
  boot_args.r[2] = r2;
  boot_args.r[3] = r3;

  ctor_init();
  Platform_base::iterate_platforms();

  init_modules_infos();
  startup(mod_info_mbi_cmdline(mod_header));
  while(1)
    ;
}
