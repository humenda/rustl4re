#include "support.h"

unsigned long
Memory::find_free_ram(unsigned long size,
                      unsigned long min_addr,
                      unsigned long max_addr)
{
  unsigned long min = min_addr;
  if (min < sizeof(unsigned long long))
    min = sizeof(unsigned long long);
  for (Region *rr = ram->begin(); rr != ram->end(); ++rr)
    {
      if (min >= rr->end())
        continue;

      if (max_addr <= rr->begin())
        continue;

      if (min < rr->begin())
        min = rr->begin();

      unsigned long max = max_addr;
      if (rr->end() < max)
        max = rr->end();

      Region search_area(min, max, "ram for modules");
      unsigned long long to = regions->find_free(search_area, size, L4_PAGESHIFT);
      if (to)
        return to;
    }
  return 0;
}

unsigned long
Memory::find_free_ram_rev(unsigned long size,
                          unsigned long min_addr,
                          unsigned long max_addr)
{
  if (min_addr < sizeof(unsigned long long))
    min_addr = sizeof(unsigned long long);

  unsigned long max = max_addr;
  for (Region *rr = ram->end() - 1; rr >= ram->begin(); --rr)
    {
      if (min_addr >= rr->end())
        continue;

      if (max <= rr->begin())
        continue;

      unsigned long min = min_addr;
      if (min < rr->begin())
        min = rr->begin();

      if (rr->end() < max)
        max = rr->end();

      Region search_area(min, max, "ram for modules");
      unsigned long long to
        = regions->find_free_rev(search_area, size, L4_PAGESHIFT);
      if (to)
        return to;
    }
  return 0;
}


