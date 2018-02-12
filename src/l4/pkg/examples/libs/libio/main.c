/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/io/io.h>
#include <l4/irq/irq.h>
#include <l4/util/util.h>
#include <l4/sys/kdebug.h>

#include <stdio.h>

void __libio_dump(void);

static void dump_vbus(void)
{
  l4io_device_handle_t devhandle = l4io_get_root_device();
  l4io_device_t dev;
  l4io_resource_handle_t reshandle;

  while (!l4io_iterate_devices(&devhandle, &dev, &reshandle))
    {
      l4io_resource_t res;
      printf("device: type=%x  name=%s numres=%d flags=%x\n",
             dev.type, dev.name, dev.num_resources, dev.flags);
      while (!l4io_lookup_resource(devhandle, L4IO_RESOURCE_ANY,
                                   &reshandle, &res))
        {
          printf("   resource: %d %x %lx-%lx\n",
                 res.type, res.flags, res.start, res.end);
        }
    }
}

int main(void)
{
  l4_addr_t a1, a2;
  l4irq_t *irq12;

  fprintf(stderr, "libio_test\n");

  dump_vbus();

  if (l4io_request_ioport(0x80, 1))
    return 1;

  fprintf(stderr, "%s %d\n", __func__, __LINE__);

  if (l4io_request_ioport(0x90, 0xf))
    return 1;

  fprintf(stderr, "%s %d\n", __func__, __LINE__);
  fprintf(stderr, "Ports 0x80 and 0x90-0x9e should be there\n");

  if (!(irq12 = l4irq_attach(12)))
    return -1;

  if (l4io_request_iomem(0xfe000000, 0x100000, 0, &a1))
    return 1;

  fprintf(stderr, "0xfe000000 mapped to %lx\n", a1);

  if (l4io_request_iomem(0xfa000000, 0x10000, 1, &a2))
    return 1;

  fprintf(stderr, "0xfa000000 mapped to %lx\n", a2);

  fprintf(stderr, "Check mappings, IRQs and ports in JDB\n");
  //__libio_dump();
  enter_kdebug("check in jdb");

  if (l4io_release_iomem(a1, 0x100000))
    return 1;

  if (l4io_release_ioport(0x90, 0xf))
    return 1;

  if (l4irq_detach(irq12))
    return 1;

  fprintf(stderr, "Check that region at %lx and I/O port range 0x90-0x9e and IRQ12 disappeared\n", a1);
  //__libio_dump();
  enter_kdebug("check in jdb");

  return 0;
}
