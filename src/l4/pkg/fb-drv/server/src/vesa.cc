/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/x86emu/int10.h>
#include <l4/vbus/vbus>
#include <l4/vbus/vbus_inhibitor.h>

#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/event>
#include <l4/re/event_enums.h>

#include "fb.h"
#include "splash.h"

#include <cstdio>

class Vesa_fb : public Phys_fb
{
private:
  struct Vbus_handler : L4::Irqep_t<Vbus_handler>
  {
    Vbus_handler(Vesa_fb *f) : fb(f) {}
    Vesa_fb *fb;
    void handle_irq();
  };

  L4Re::Util::Event _vbus_event;
  Vbus_handler _vbus_irq;
  L4::Cap<L4vbus::Vbus> _vbus;
  int _vbemode = 0;

public:
  Vesa_fb() : _vbus_irq(this) {}

  bool setup_drv(Prog_args *pa, L4Re::Util::Object_registry *r);
};

void Vesa_fb::Vbus_handler::handle_irq()
{
  l4util_mb_vbe_ctrl_t vbe;
  l4util_mb_vbe_mode_t vbi;
  L4Re::Event_buffer::Event *e;
  while ((e = fb->_vbus_event.buffer().next()) != NULL)
    {
      if (e->payload.type != L4RE_EV_PM)
        continue;

      switch (e->payload.code)
        {
          case L4VBUS_INHIBITOR_SUSPEND:
            printf("Received suspend event.\n");
            fb->_vbus->acquire(L4VBUS_INHIBITOR_WAKEUP, "restart video on wakeup");
            fb->_vbus->L4Re::Inhibitor::release(L4VBUS_INHIBITOR_SUSPEND);
            break;
          case L4VBUS_INHIBITOR_WAKEUP:
            printf("Received wakeup event.\n");
            fb->_vbus->acquire(L4VBUS_INHIBITOR_SUSPEND, "GFX running");
            fb->_vbus->L4Re::Inhibitor::release(L4VBUS_INHIBITOR_WAKEUP);
            if (fb->_vbemode > 0)
              {
                int res = x86emu_int10_set_vbemode(fb->_vbemode, &vbe, &vbi);
                if (res < 0)
                  printf("error: could not setup VESA mode on resume: %d\n", res);
              }
            break;
          default:
            break;
        }
    }
}


Phys_fb *Phys_fb::probe()
{
  return new Vesa_fb();
}

bool
Vesa_fb::setup_drv(Prog_args *pa, L4Re::Util::Object_registry *r)
{
  l4util_mb_vbe_ctrl_t vbe;
  l4util_mb_vbe_mode_t vbi;
  _vbus = L4Re::chkcap(L4Re::Env::env()->get_cap<L4vbus::Vbus>("vbus"), "request V-BUS cap");
  _vbus_event.init<L4::Irq>(_vbus);
  r->register_obj(&_vbus_irq, L4::cap_cast<L4::Irq>(_vbus_event.irq()));

  _vbus->acquire(L4VBUS_INHIBITOR_SUSPEND, "GFX running");

  int res = x86emu_int10_set_vbemode(pa->vbemode, &vbe, &vbi);
  if (res < 0)
    return false;

  _vbemode = res;

  _vidmem_size = 64*1024*vbe.total_memory;

  _vidmem_start = 0;
  int error;
  error = L4Re::Env::env()->rm()->attach(&_vidmem_start, _vidmem_size,
                                         L4Re::Rm::Search_addr | L4Re::Rm::Eager_map,
                                         _vbus, vbi.phys_base, 20);

  if (error)
    printf("map of gfx mem failed\n");

  _vidmem_end   = _vidmem_start + _vidmem_size;

  _screen_info.width = vbi.x_resolution;
  _screen_info.height = vbi.y_resolution;
  _screen_info.flags = L4Re::Video::Goos::F_auto_refresh;
  _screen_info.pixel_info = L4Re::Video::Pixel_info(&vbi);

  _view_info.buffer_offset = 0; //base_offset;
  _view_info.bytes_per_line = vbi.bytes_per_scanline;

  init_infos();

  printf("Framebuffer memory: phys: %x - %lx\n",
         vbi.phys_base, vbi.phys_base + _vidmem_size);
  printf("                    virt: %lx - %lx\n",
         _vidmem_start, _vidmem_start + _vidmem_size);

  splash_display(&_view_info, _vidmem_start);

  return true;
}
