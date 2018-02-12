#include "of_if.h"
#include <l4/drivers/of_dev.h>

namespace L4_drivers
{
  unsigned long Of_if::detect_ramsize() 
  {
    unsigned long addr_cells, size_cells, ram_size;
    unsigned long reg[100];
    phandle_t mem;

    prom_getprop(get_root(), "#address-cells", &addr_cells, sizeof(&addr_cells));
    prom_getprop(get_root(), "#size-cells", &size_cells, sizeof(&size_cells));
    ram_size = 0;

    mem = get_device("memory");
    unsigned int len = prom_getprop(mem, "reg", &reg, sizeof(reg));
    len /= 4; /* always 32 bit values */

    if(len > sizeof(reg))
      len = sizeof(reg);

    unsigned long base, size;
    for(unsigned i = 0; i < len; i += (addr_cells + size_cells)) {
        base = reg[i + addr_cells - 1];
        size = reg[i + addr_cells + size_cells - 1];
        if((base + size) > ram_size) 
          ram_size = base + size;

        printf("    Memory base: [%08lx; %08lx]  size: %lu KB\n", 
            base, base + size, size / 1024);
    }

    return ram_size;
  }

  unsigned long Of_if::cpu_detect(const char* prop)
  {
    phandle_t cpu;
    unsigned long val;
    cpu = get_device("cpu");

    prom_getprop(cpu, prop, &val, sizeof(&val));
    return val;
  }

  unsigned long Of_if::detect_bus_freq()
  {
    return cpu_detect("bus-frequency");
  }

  unsigned long Of_if::detect_cpu_freq()
  {
    return cpu_detect("clock-frequency");
  }

  unsigned long Of_if::detect_time_freq()
  {
    return cpu_detect("timebase-frequency");
  }

  L4_drivers::Of_if::phandle_t Of_if::get_device(const char *device, const char *prop)
  {
    phandle_t dev;
    char type[32];
    for(dev = 0; prom_next_node(&dev); )
      {
        prom_getprop(dev, prop, type, sizeof(type));

        if (strcmp(type, device))
          continue;

        return dev;
      }
    return 0;
  }

  bool Of_if::detect_devices(unsigned long *start_addr, unsigned long *length)
  {
    static of_device_t dev[MAX_OF_DEVICES];
    char descr[64];

    printf("  Detecting hardware ...\n");
    int i = -1;

    for(phandle_t node = 0; prom_next_node(&node) && i < MAX_OF_DEVICES;)
      {
        if (prom_getprop(node, "device_type", &(dev[i+1].type),
                         sizeof(dev[i].type)) < 0)
          continue;
        i++;

        prom_getprop(node, "name", &dev[i].name, sizeof(dev[i].name));

        if (prom_getprop(node, "reg", &(dev[i].reg), sizeof(dev[i].reg)) < 0)
          dev[i].reg = 0;

        if (!strcmp(dev[i].type, "cpu"))
          {
            dev[i].freq.cpu_freq = detect_cpu_freq();
            dev[i].freq.bus_freq = detect_bus_freq();
            dev[i].freq.time_freq = detect_time_freq();
          }
        else if (prom_getprop(node, "interrupts", (dev[i].interrupts),
                              sizeof(dev[i].interrupts)) < 0)
          dev[i].interrupts[0] = dev[i].interrupts[1] = dev[i].interrupts[2] = ~0UL;


        //just for information
        prom_getprop(node, ".description", descr, sizeof(descr));

        printf("\33[1;36m"
               "    [%s]\33[0;36m (%s)\33[0m\n"
               "      reg: %08lx interrupts: %08lx, %08lx, %08lx\n",
               dev[i].name, descr, dev[i].reg, dev[i].interrupts[0],
               dev[i].interrupts[1], dev[i].interrupts[2]);
      }

    if (i < 0)
      return false;

    *start_addr = (unsigned long)dev;
    *length = (i + 1) * sizeof(of_device_t);

    return true;
  }


  void Of_if::boot_finish() 
  {
    ihandle_t _std;
    //close stdin
    prom_getprop(get_chosen(), "stdin", &_std, sizeof(_std));
    prom_call("close", 1, 0, _std);
    //close stdout
    prom_getprop(get_chosen(), "stdout", &_std, sizeof(_std));
    prom_call("close", 1, 0, _std);
    //finish pending dma requests
    prom_call("quiesce", 0, 0);
  }

  void Of_if::vesa_set_mode(int mode)
  {
    (void)mode;
    unsigned long test_mode = 0x117;
    phandle_t disp = get_device("display", "name");
    int ret = prom_call("vesa-set-mode", 2, 1, disp, test_mode);// &test_mode);
    unsigned long adr = prom_call("vesa-frame-buffer-adr", 1, 1);

    printf("VESA: returned %d fb %08lx\n", ret, adr);
  }
}
