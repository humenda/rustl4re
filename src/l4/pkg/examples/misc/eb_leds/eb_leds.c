/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/*
 * The ARM EB boards have 8 LEDs which we can play a bit with. This example
 * shows how to get access to the I/O memory...
 */

#include <l4/io/io.h>
#include <stdio.h>
#include <unistd.h>

enum {
  USERSW = 0x4,
  LED    = 0x8,
};

static l4_addr_t sys_base;

static void write_led(int val)
{
  *(volatile unsigned long *)(sys_base + LED) = val & 0xff;
}

static unsigned char read_user_switch(void)
{
  /* only 8 switches there, so char return is enough */
  return *(volatile unsigned long *)(sys_base + USERSW);
}

static void do_fancy(char val)
{
  int mode = val & 0x80;
  int speed = val & 0x7f;
  if (!mode)
    {
      static int pos, delta = 1;
      write_led(1 << pos);
      pos += delta;
      if (pos == 7)
	delta = -1;
      else if (pos == 0)
	delta = 1;
    }
  else
    {
      static int pos, delta = 1;
      write_led((1 << pos) | (1 << (7 - pos)));
      pos += delta;
      if (pos == 3)
	delta = -1;
      else if (pos == 0)
	delta = 1;
    }

  usleep(500000 / (speed + 1));
}

int main(void)
{
  l4io_device_handle_t dh;
  l4io_resource_handle_t hdl;
  char prev = 0;

  /* Look for system controller, the registers we want are there */
  if (l4io_lookup_device("System Control", &dh, 0, &hdl))
    {
      printf("Could not get system controller memory region.\n");
      return 1;
    }

  /* Now get the IO memory from it. We know that it just has one memory
   * region, so we just do this one once: */
  sys_base = l4io_request_resource_iomem(dh, &hdl);
  if (sys_base == 0)
    {
      printf("Could not map system controller region.\n");
      return 1;
    }

  printf("User Switches (toggle them now but don't forget their initial setting!):\n");
  while (1)
    {
      int i;
      char val = read_user_switch();
      if (prev != val)
        {
          for (i = 0; i < 8; ++i)
            printf("%s ", val & (1 << i) ? "ON " : "off");
          printf("\n");
          prev = val;
        }

      do_fancy(val);
    }


  return 0;
}
