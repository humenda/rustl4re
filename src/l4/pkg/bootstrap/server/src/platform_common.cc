#include "support.h"
#include <l4/cxx/minmax>
#include <cassert>

#ifdef RAM_SIZE_MB

static unsigned long
scan_ram_size(unsigned long base_addr, unsigned long max_scan_size_mb)
{
  // scan the RAM to find out the RAM size, note that at this point we have
  // two regions in RAM that we cannot touch, &_start - &_end and the
  // modules

  // assume that the image is loaded correctly and we need to start
  // probing for memory wraparounds beyond the end of our image
  extern char _end[];
#ifdef IMAGE_MODE
  extern char _module_data_end[];
  l4_addr_t lower_bound
    = l4_round_size((l4_addr_t)cxx::max(_end, _module_data_end), 20);
#else
  l4_addr_t lower_bound = l4_round_size((l4_addr_t)_end, 20);
#endif

  assert(base_addr <= lower_bound);
  lower_bound -= base_addr;

  // must be a power of 2 and >= (1 << 20)
  unsigned long increment;
  unsigned long max_scan_size = max_scan_size_mb << 20;

  // push the initial probe point beyond lower_bound
  for (increment = 1 << 20;
       increment < lower_bound && increment < max_scan_size;
       increment *= 2)
    {}

  printf("  Scanning up to %ld MB RAM, starting at offset %ldMB\n",
         max_scan_size_mb, increment >> 20);

  // initialize memory probe points
  for (unsigned long offset = increment;
       offset < max_scan_size;
       offset *= 2)
    *(unsigned long *)(base_addr + offset) = 0;

  // avoid gcc/clang optimization figuring out that base_addr might
  // always be 0 and generating a trap here
  asm("" : "+r" (base_addr));
  // write something at offset 0, does it appear elsewhere?
  *(unsigned long *)base_addr = 0x12345678;
  asm volatile("" : : : "memory");
  for (unsigned long offset = increment;
       offset < max_scan_size;
       offset *= 2)
    if (*(unsigned long *)(base_addr + offset) == 0x12345678)
      return offset >> 20;  // detected a wrap around at offset

  return max_scan_size_mb;
}

void
Platform_single_region_ram::setup_memory_map()
{
  unsigned long ram_size_mb = scan_ram_size(RAM_BASE, RAM_SIZE_MB);
  printf("  Memory size is %ldMB%s (%08lx - %08lx)\n",
         ram_size_mb, ram_size_mb != RAM_SIZE_MB ? " (Limited by Scan)" : "",
         (unsigned long)RAM_BASE, RAM_BASE + (ram_size_mb << 20) - 1);
  mem_manager->ram->add(
      Region::n(RAM_BASE,
                (unsigned long long)RAM_BASE + (ram_size_mb << 20),
                ".ram", Region::Ram));
}
#endif
