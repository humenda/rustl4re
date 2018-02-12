/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/vbus/vdevice-ops.h>

#include "vicu.h"
#include "vbus_factory.h"
#include "server.h"
#include "main.h"
#include "debug.h"
#include "hw_irqs.h"

#include <l4/re/util/cap_alloc>
#include <l4/re/namespace>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/sys/icu>
#include <l4/sys/irq>
#include <l4/sys/debugger.h>

#include <cassert>
#include <map>

namespace Vi {

using L4Re::Util::Ref_cap;
using L4Re::chksys;
using L4Re::chkcap;

Sw_icu::Sw_icu()
{
  add_feature(this);
  registry->register_obj(this);
}

Sw_icu::~Sw_icu()
{
  registry->unregister_obj(this);
}

Sw_icu::Sw_irq_pin *
Sw_icu::get_msi_pin(unsigned msin)
{
  if (msin >= _num_msis)
    {
      d_printf(DBG_WARN, "use of invalid virtual MSI (number) %u\n", msin);
      return 0;
    }

  Irq_set::Iterator i = _msis.find(msin);
  if (i != _msis.end())
    return &*i;

  Sw_irq_pin *p = new Sw_irq_pin(new Msi_irq_pin(), msin, 0);
  _msis.insert(p);
  return p;
}

int
Sw_icu::op_msi_info(L4::Icu::Rights, l4_umword_t irqnum, l4_uint64_t source,
                    l4_icu_msi_info_t &info)
{
  d_printf(DBG_ALL, "%s: irqnum=%lx...\n", __func__, irqnum);
  if (!(irqnum & L4::Icu::F_msi))
    return -L4_EINVAL;

  Sw_irq_pin *msi = get_msi_pin(irqnum & ~L4::Icu::F_msi);
  if (!msi)
    return -L4_EINVAL;

  d_printf(DBG_ALL, "%s: irqnum=%lx: source=0x%05x\n",
           __func__, irqnum, (unsigned)source);

  Device::Msi_src_info si = source;

  if (!si.svt())
    {
      d_printf(DBG_WARN,
               "warning: MSI %lx (bus: %s) without source ID will be blocked\n",
               irqnum & ~L4::Icu::F_msi, get_root()->name());
      return -L4_ERANGE;
    }

  Io_irq_pin::Msi_src *src = get_root()->find_msi_src(si);
  if (!src)
    {
      d_printf(DBG_WARN,
               "warning: MSI source 0x%05x not found on bus %s\n",
               si.v, get_root()->name());
      return -L4_ENODEV;
    }

  return msi->msi_info(src, &info);
}

int
Sw_icu::bind_irq(unsigned irqn, L4::Ipc::Snd_fpage const &irqc)
{
  if (!irqc.cap_received())
    return -L4_EINVAL;

  d_printf(DBG_ALL, "%s[%p]: bind_irq(%u, ...)\n", name(), this, irqn);

  Sw_irq_pin *irq;
  if (irqn & L4::Icu::F_msi)
    {
      irq = get_msi_pin(irqn & ~L4::Icu::F_msi);
      if (!irq)
        return -L4_ENOENT;
    }
  else
    {
      Irq_set::Iterator i = _irqs.find(irqn);
      if (i == _irqs.end())
        return -L4_ENOENT;
      irq = &*i;
    }

  int err = irq->bind(server_iface()->get_rcv_cap(0));
  if (err < 0)
    d_printf(DBG_ERR, "ERROR: binding irq %u, result is %d (%s)\n", irqn, err, l4sys_errtostr(err));

  return err;
}

int
Sw_icu::unbind_irq(unsigned irqn, L4::Ipc::Snd_fpage const &/*irqc*/)
{
  d_printf(DBG_ALL, "%s[%p]: unbind_irq(%u, ...)\n", name(), this, irqn);

  Irq_set::Iterator i;
  if (irqn & L4::Icu::F_msi)
    {
      i = _msis.find(irqn & ~L4::Icu::F_msi);
      if (i == _msis.end())
        return -L4_ENOENT;
    }
  else
    {
      i = _irqs.find(irqn);
      if (i == _irqs.end())
        return -L4_ENOENT;
    }
  // could check the validity of the cap too, however we just don't care
  return i->unbind();
}

int
Sw_icu::set_mode(unsigned irqn, l4_umword_t mode)
{
  if (irqn & L4::Icu::F_msi)
    {
      d_printf(DBG_WARN, "Setting mode for MSI is useless, ignoring\n");
      return 0;
    }

  Irq_set::Iterator i = _irqs.find(irqn);
  if (i == _irqs.end())
    return -L4_ENOENT;

  return i->set_mode(mode);
}

int
Sw_icu::unmask_irq(unsigned irqn)
{
  Irq_set::Iterator i = _irqs.find(irqn);
  if (i == _irqs.end())
    return -L4_ENOENT;

  if (!i->unmask_via_icu())
    return -L4_EINVAL;

  return i->unmask();
}


bool
Sw_icu::irqs_allocated(Resource const *r)
{
  for (unsigned n = r->start(); n <= r->end(); ++n)
    {
      if (_irqs.find(n) == _irqs.end())
	return false;
    }

  return true;
}

bool
Sw_icu::add_irqs(Resource const *r)
{
  for (unsigned n = r->start(); n <= r->end(); ++n)
    {
      if (_irqs.find(n) != _irqs.end())
	continue;

      Io_irq_pin *ri = Hw::Irqs::real_irq(n);
      if (!ri)
        {
          d_printf(DBG_ERR, "ERROR: No IRQ%d available.\n", n);
          continue;
        }
      Sw_irq_pin *irq = new Sw_irq_pin(ri, n, r->flags());
      _irqs.insert(irq);
    }
  return true;
}

bool
Sw_icu::add_irq(unsigned n, unsigned flags, Io_irq_pin *be)
{
  if (_irqs.find(n) == _irqs.end())
    return false;

  Sw_irq_pin *irq = new Sw_irq_pin(be, n, flags);
  _irqs.insert(irq);
  return true;
}

int
Sw_icu::alloc_irq(unsigned flags, Io_irq_pin *be)
{
  unsigned i;
  for (i = 1; i < 1000; ++i)
    if (_irqs.find(i) == _irqs.end())
      break;

  if (i == 1000)
    return -1;

  Sw_irq_pin *irq = new Sw_irq_pin(be, i, flags);
  _irqs.insert(irq);
  return i;
}

int
Sw_icu::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  if (func != L4vbus_vicu_get_cap)
    return -L4_ENOSYS;

  ios << obj_cap();
  return L4_EOK;
}

//static VBus_factory<Sw_icu> __vicu_factory("Vicu");

int
Sw_icu::Sw_irq_pin::trigger() const
{
  return l4_error(_irq->trigger());
}


unsigned
Sw_icu::Sw_irq_pin::l4_type() const
{
  return (type() & S_irq_type_mask) / Resource::Irq_type_base;
}

int
Sw_icu::Sw_irq_pin::set_mode(l4_umword_t mode)
{
  if (!(_state & S_allow_set_mode))
    {
      unsigned t = l4_type();

      mode = (mode & L4_IRQ_F_MASK) | 1;

      if (t != L4_IRQ_F_NONE
          && t != mode)
        d_printf(DBG_WARN, "WARNING: Changing type of IRQ %d from %x to %lx prohibited\n",
                 _irqn, t, mode);
      return 0;
    }

  return _master->set_mode(mode);
}

L4::Cap<L4::Irq_mux>
Sw_icu::Sw_irq_pin::allocate_master_irq()
{
  assert (_master->shared());
  Ref_cap<L4::Irq_mux>::Cap lirq = chkcap(L4Re::Util::cap_alloc.alloc<L4::Irq_mux>(),
      "allocating IRQ capability");
  // printf("IRQ mode = %x -> %x\n", type(), l4_type());
  chksys(L4Re::Env::env()->factory()->create(lirq.get()), "allocating IRQ");
  chksys(_master->bind(lirq, l4_type()), "binding IRQ");
  _master->set_chained(true);
  return lirq.get();
}

bool
Sw_icu::Sw_irq_pin::bound()
{
  if (!_irq)
    return false;

  // recheck if the bound IRQ object is gone
  if (l4_error(_irq.validate()) == 1)
    return true;

  // could not validate the IRQ capability, assume the IRQ is deleted
  _unbind(true);
  return false;
}

int
Sw_icu::Sw_irq_pin::_direct_bind(Triggerable const &irq)
{
  // the first irq shall be attached to a hw irq
  d_printf(DBG_DEBUG2, "IRQ %d -> client\nIRQ mode = %x -> %x\n",
           irqn(), type(), l4_type());
  int err = _master->bind(irq, l4_type());
  if (err < 0)
    return err;

  _irq = irq;
  _master->inc_sw_irqs();
  if (err == 1)
    _state |= S_unmask_via_icu;

  d_printf(DBG_DEBUG2, "  bound irq %u -> err=%d\n", irqn(), err);
  return err;
}

int
Sw_icu::Sw_irq_pin::bind(L4::Cap<void> rc)
{
  if (bound())
    return -L4_EPERM;

  Triggerable irq =
    chkcap(L4Re::Util::cap_alloc.alloc<L4::Triggerable>(), "allocating IRQ capability");

  irq.get().move(L4::cap_cast<L4::Triggerable>(rc));

  unsigned swis = _master->sw_irqs();
  if (swis == 0)
    return _direct_bind(irq);
  else if (!_master->shared())
    return -L4_EBUSY;
  else if (swis == 1 && !_master->chained())
    {
      // need to switch to IRQ chaining
      Triggerable old_irq = _master->irq();
      if (l4_error(old_irq.validate()) != 1)
        {
          // so the old IRQ object that used to be attached to this IRQ
          // is gone (deleted), remove the last traces and directly bind
          // the new IRQ
          _master->unbind(true);
          return _direct_bind(irq);
        }

      _master->unbind(false);

      L4::Cap<L4::Irq_mux> mux = allocate_master_irq();
      assert (_master->chained());
      d_printf(DBG_DEBUG2, "IRQ %d -> proxy -> %d clients\n", irqn(), swis);
      int err = l4_error(mux->chain(old_irq.get()));
      if (err == -L4_EINVAL)
        {
          // returns -EINVAL when old_irq is somehow invalid, not existent
          // this might happen in the case someone deleted the old IRQ object
          // after the test above
          _unbind(false); // remove the Irq_mux again
          return _direct_bind(irq);
        }
    }

  d_printf(DBG_DEBUG2, "IRQ %d -> proxy -> %d clients\n", irqn(), _master->sw_irqs() + 1);
  L4Re::chksys(L4::cap_cast<L4::Irq_mux>(_master->irq())->chain(irq.get()), "attach");
  _irq = irq;
  _master->inc_sw_irqs();

  return 0;
}

int
Sw_icu::Sw_irq_pin::_unbind(bool deleted)
{
  int err = 0;
  _master->dec_sw_irqs();
  if (_master->sw_irqs() == 0)
    {
      _master->unbind(!_master->chained() && deleted);
      if (_master->chained())
        _master->set_chained(false);
    }

  _irq = L4::Cap<L4::Irq>::Invalid;
  _state &= S_irq_type_mask;
  return err;
}

int
Sw_icu::Sw_irq_pin::unbind()
{
  if (!_master)
    return -L4_EINVAL;

  if (!_master->sw_irqs())
    return -L4_EINVAL;

  if (bound())
    return _unbind(false);

  return 0;
}

}
