/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "cfg.h"
#include "debug.h"
#include "hw_root_bus.h"
#include "phys_space.h"
#include "resource.h"
#include "pm.h"
#include <l4/sys/platform_control>
#include "server.h"

namespace {

// --- Root address space for IRQs -----------------------------------------
class Root_irq_rs : public Resource_space
{
public:
  bool request(Resource *parent, Device *, Resource *child, Device *)
  {
    child->parent(parent);

    return true;
  };

  bool alloc(Resource *, Device *, Resource *, Device *, bool)
  { return false; }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_irq_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_irq_rs\n");
    return false;
  }
};

// --- Root address space for IO-Ports --------------------------------------
class Root_io_rs : public Resource_space
{
public:
  bool request(Resource *parent, Device *, Resource *child, Device *)
  {
    child->parent(parent);

    return true;
  }

  bool alloc(Resource *parent, Device *, Resource *child, Device *, bool)
  {
    child->parent(parent);

    return true;
  }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_io_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_io_rs\n");
    return false;
  }
};


// --- Root address space for MMIO -----------------------------------------
class Root_mmio_rs : public Resource_space
{
public:
  bool request(Resource *parent, Device *, Resource *child, Device *);
  bool alloc(Resource *parent, Device *, Resource *child, Device *, bool);
  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_mmio_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_mmio_rs\n");
    return false;
  }
};

bool
Root_mmio_rs::request(Resource *parent, Device *, Resource *child, Device *)
{
  //printf("request resource at root level: "); child->dump();
  if (Phys_space::space.alloc(Phys_space::Phys_region(child->start(), child->end())))
    {
      child->parent(parent);
      return true;
    }

  d_printf(DBG_WARN, "WARNING: phys mmio resource allocation failed\n");
  if (dlevel(DBG_WARN))
    child->dump();

  return false;
}


bool
Root_mmio_rs::alloc(Resource *parent, Device *, Resource *child, Device *,
                    bool /*resize*/)
{
  Resource::Size align = cxx::max<Resource::Size>(child->alignment(),  L4_PAGESIZE - 1);
  Phys_space::Phys_region phys = Phys_space::space.alloc(child->size(), align);
  if (!phys.valid())
    {
#if 0
      printf("ERROR: could not reserve physical space for resource\n");
      r->dump();
#endif
      child->disable();
      return false;
    }

  child->start(phys.start());
  child->parent(parent);

  if (dlevel(DBG_DEBUG))
    {
      printf("allocated resource: ");
      child->dump();
    }
  return true;
}

// --- End Root address space for MMIO --------------------------------------
}

namespace {

struct Generic_pm : Hw::Root_bus::Pm
{
  L4::Cap<L4::Platform_control> pfc;

  Generic_pm() : pfc(L4Re::Env::env()->get_cap<L4::Platform_control>("icu"))
  {}

  int suspend()
  {
    if (pfc)
      return l4_error(pfc->system_suspend(0));

    d_printf(DBG_WARN,
             "warning: no platform control capability, cannot suspend\n");
    return 0;
  }

  int shutdown()
  {
    if (pfc)
      return l4_error(pfc->system_shutdown(0));

    d_printf(DBG_WARN,
             "warning: no platform control capability, cannot shutdown\n");
    return 0;
  }

  int reboot()
  {
    if (pfc)
      return l4_error(pfc->system_shutdown(1));

    d_printf(DBG_WARN,
             "warning: no platform control capability, cannot reboot\n");
    return 0;
  }

};

}

namespace Hw {

Root_bus::Root_bus(char const *name)
: Hw::Device(), _pm(new Generic_pm)
{
  set_name(name);

  // add root resource for IRQs
  Root_resource *r = new Root_resource(Resource::Irq_res, new Root_irq_rs());
  add_resource(r);

  Resource_space *rs_mmio = new Root_mmio_rs();
  // add root resource for non-prefetchable MMIO resources
  r = new Root_resource(Resource::Mmio_res, rs_mmio);
  r->add_flags(Resource::F_width_64bit);
  add_resource(r);

  // add root resource for prefetchable MMIO resources
  r = new Root_resource(Resource::Mmio_res | Resource::F_prefetchable, rs_mmio);
  r->add_flags(Resource::F_width_64bit);
  add_resource(r);

  // add root resource for IO ports
  r = new Root_resource(Resource::Io_res, new Root_io_rs());
  add_resource(r);
}

/**
 * \pre supports_pm() must be true
 */
void
Root_bus::suspend()
{
  int res;
  if ((res = ::Pm::pm_suspend_all()) < 0)
    {
      d_printf(DBG_ERR, "error: pm_suspend_all_failed: %d\n", res);
      ::Pm::pm_resume_all();
      return;
    }

  _pm->suspend();

  if ((res = ::Pm::pm_resume_all()) < 0)
    d_printf(DBG_ERR, "error: pm_resume_all failed: %d\n", res);
}

}
