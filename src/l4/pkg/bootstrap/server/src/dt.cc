
#include <stdio.h>
#include <assert.h>

extern "C" {
#include <libfdt.h>
}

#include "dt.h"

unsigned long dt_cpu_release_addr(unsigned long fdt_addr)
{
  if (!fdt_addr)
    return -1UL;

  if (fdt_addr & 3)
    return -1UL;

  void *fdt = (void *)fdt_addr;

  if (fdt_check_header(fdt) < 0)
    {
      printf("DT: Invalid FDT\n");
      return -1UL;
    }

  int cpus = fdt_path_offset(fdt, "/cpus");
  if (cpus < 0)
    {
      if (0)
        printf("No 'cpus' node found in DT\n");
      return -1UL;
    }

  int cpu = -1;
  do
    {
      cpu = fdt_node_offset_by_prop_value(fdt, cpu, "device_type",
                                          "cpu", 4);
      if (cpu < 0)
        break;

      if (fdt_node_offset_by_prop_value(fdt, cpu, "enable-method",
                                        "spin-table", 11) < 0)
        continue;

      int len;
      fdt32_t const *cpurel
        = (fdt32_t const *)fdt_getprop(fdt, cpu, "cpu-release-addr", &len);

      unsigned long long cpu_release_addr;

      if (fdt_address_cells(fdt, cpus) == 2)
        {
          assert(len >= 8);
          cpu_release_addr = ((unsigned long long)fdt32_to_cpu(cpurel[0]) << 32)
                             | fdt32_to_cpu(cpurel[1]);
        }
      else
        {
          assert(len >= 4);
          cpu_release_addr = fdt32_to_cpu(cpurel[0]);
        }

      return cpu_release_addr;
    }
  while (1);

  return -1UL;
}
