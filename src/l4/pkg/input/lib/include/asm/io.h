#ifndef _ASM_IO_H
#define _ASM_IO_H

#ifdef ARCH_arm

#define readb(addr)       (*(volatile unsigned char *)(addr))
#define writeb(val, addr) do { *(volatile unsigned char *)(addr) = (val); } while (0)

#else
#include <l4/util/port_io.h>
#include <l4/util/util.h>
#include <l4/sys/ipc.h>

#define iodelay() do { l4_sleep(1); } while (0)


#define inb(port) l4util_in8(port)
#define outb(value, port) l4util_out8(value, port)

#define inb_p(port) ({ iodelay(); l4util_in8(port); })
#define outb_p(value, port) ({ iodelay(); l4util_out8(value, port); })
#endif

#endif
