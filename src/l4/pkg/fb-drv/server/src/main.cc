/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/sys/capability>
#include <l4/sys/typeinfo_svr>
#include <l4/re/util/br_manager>

#include <cstdio>
#include <getopt.h>
#include <cstdlib>

#include "fb.h"

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

void
Phys_fb::setup_ds(char const *name)
{
  server.registry()->register_obj(this, name);
  _fb_ds = L4::Cap<L4Re::Dataspace>(obj_cap().cap());
  _ds_start = _vidmem_start;
  _ds_size = _vidmem_size;
  _rw_flags = Writable;
  _cache_flags = L4::Ipc::Gen_fpage<L4::Ipc::Snd_item>::Buffered;
}

int
Phys_fb::map_hook(l4_addr_t offs, unsigned long flags,
                  l4_addr_t min, l4_addr_t max)
{
  // map everything at once, a framebuffer will usually used fully
  (void)offs; (void)flags; (void)min; (void)max;
  if (_map_done)
    return 0;

  int err;
  L4::Cap<L4Re::Dataspace> ds;
  unsigned long sz = 1;
  l4_addr_t off;
  unsigned fl;
  l4_addr_t a = _vidmem_start;

  if ((err = L4Re::Env::env()->rm()->find(&a, &sz, &off, &fl, &ds)) < 0)
    {
      printf("Failed to query video memory: %d\n", err);
      return err;
    }

  if ((err = ds->map_region(off, L4Re::Dataspace::Map_rw,
                            _vidmem_start, _vidmem_end)) < 0)
    {
      printf("Failed to map video memory: %d\n", err);
      return err;
    }

  _map_done = 1;
  return 0;
}

Prog_args::Prog_args(int argc, char *argv[])
 : vbemode(~0), do_dummy(false), config_str(0)
{
  while (1)
    {
      struct option opts[] = {
            { "vbemode", required_argument, 0, 'm' },
            { "config", required_argument, 0, 'c' },
            { "dummy", no_argument, 0, 'D' },
            { 0, 0, 0, 0 },
      };

      int c = getopt_long(argc, argv, "c:m:D", opts, NULL);
      if (c == -1)
        break;

      switch (c)
        {
        case 'c':
          config_str = optarg;
          break;
        case 'm':
          vbemode = strtol(optarg, 0, 0);
          break;
        case 'D':
          do_dummy = true;
          break;
        default:
          printf("Unknown option '%c'\n", c);
          break;
        }
    }
}

int main(int argc, char *argv[])
{
  Prog_args args(argc, argv);
  Phys_fb *fb;
  if (args.do_dummy)
    fb = Phys_fb::get_dummy();
  else
    fb = Phys_fb::probe();

  if (!fb->setup_drv(&args, server.registry()))
    {
      printf("Failed to setup Framebuffer\n");
      return 1;
    }

  fb->setup_ds("fb");

  if (!fb->running())
    {
      printf("Failed to initialize frame buffer; Aborting.\n");
      return 1;
    }

  if (!fb->obj_cap().is_valid())
    {
      printf("Failed to connect.\n");
      return 1;
    }

  printf("Starting server loop\n");
  server.loop();

  return 0;
}
