#include "irqs.h"
#include "debug.h"
#include "main.h"

#include <cassert>
#include <l4/cxx/bitmap>

int
Kernel_irq_pin::unbind(bool deleted)
{
  if (!irq())
    return -L4_EINVAL;

  int err = 0;
  if (!deleted)
    err = l4_error(system_icu()->icu->unbind(_idx, irq().get()));
  Io_irq_pin::unbind(deleted);
  chg_flags(false, F_shareable);
  return err;
}

int
Kernel_irq_pin::bind(Triggerable const &irq, unsigned mode)
{
  if (this->irq())
    return -L4_EINVAL;

  if (mode != L4_IRQ_F_NONE)
    {
      //printf(" IRQ[%x]: mode=%x ... ", _idx, mode);
      int err = l4_error(system_icu()->icu->set_mode(_idx, mode));
      //printf("result=%d\n", err);

      if (err < 0)
        return err;
    }

  int err = l4_error(system_icu()->icu->bind(_idx, irq.get()));

  // allow sharing if IRQ must be acknowledged via the IRQ object 
  if (err == 0)
    {
      Io_irq_pin::bind(irq, mode);
      chg_flags(true,  F_shareable);
    }

  return err;
}

int
Kernel_irq_pin::unmask()
{
  system_icu()->icu->unmask(_idx);
  return -L4_EINVAL;
}


int
Kernel_irq_pin::set_mode(unsigned mode)
{
  return l4_error(system_icu()->icu->set_mode(_idx, mode));
}

int
Kernel_irq_pin::_msi_info(l4_uint64_t src, l4_icu_msi_info_t *info)
{
  // Avoid warning of usage of potentially uninitialized members,
  // just for the debug printf
  info->msi_addr = 0;
  info->msi_data = 0;
  int res = l4_error(system_icu()->icu->msi_info(_idx, src, info));
  d_printf(DBG_ALL,
           "MSI info for hw IRQ: %x (src=0x%llx). res=%d (addr=%llx data=%x)\n",
           _idx, src, res, info->msi_addr, info->msi_data);
  return res;
}

int
Io_irq_pin::clear()
{
  int cnt = 0;
  while (!l4_error(l4_ipc_receive(irq().cap(), l4_utcb(), L4_IPC_BOTH_TIMEOUT_0)))
    ++cnt;

  return cnt;
}

int
Kernel_irq_pin::mask()
{
  return l4_error(system_icu()->icu->mask(_idx));
}


namespace {
static cxx::Bitmap<256> _msi_allocator;
}

void
Msi_irq_pin::free_msi()
{
  unsigned p = pin();
  if (!p)
    return;

  // if bound we must have a valid MSI number
  assert (p & L4::Icu::F_msi);

  _msi_allocator.clear_bit(p & ~L4::Icu::F_msi);
  d_printf(DBG_ALL, "free global MSI %u\n", p & ~L4::Icu::F_msi);
  // reset the internal IRQ number to 0
  _idx = 0;
}

int
Msi_irq_pin::alloc_msi()
{
  assert (!pin());

  int res = _msi_allocator.scan_zero();
  if (res < 0)
    return -L4_ENOMEM;

  if ((unsigned)res >= system_icu()->info.nr_msis)
    return -L4_ENOMEM;

  _msi_allocator.set_bit(res);
  d_printf(DBG_ALL, "allocate global MSI %d\n", res);
  _idx = res | L4::Icu::F_msi;
  return 0;
}

Msi_irq_pin::~Msi_irq_pin() noexcept
{
  unbind(false);
}

int
Msi_irq_pin::unbind(bool deleted)
{
  int res = Kernel_irq_pin::unbind(deleted);
  free_msi();
  return res;
}

int
Msi_irq_pin::bind(Triggerable const &irq, unsigned mode)
{
  if (this->irq())
    return -L4_EINVAL;

  if (!pin())
    {
      int res = alloc_msi();
      if (res < 0)
        return res;
    }

  return Kernel_irq_pin::bind(irq, mode);
}

int
Msi_irq_pin::reconfig_msi(l4_uint64_t src_info)
{
  if (!pin())
    return -L4_EINVAL;

  // we ignore the 'info' part from the kernel here as we assume the
  // info to be stable for an allocated MSI vector and do not expect
  // a change when we need to reset the source filter for the MSI.
  l4_icu_msi_info_t info;
  return Kernel_irq_pin::_msi_info(src_info, &info);
}

int
Msi_irq_pin::msi_info(Msi_src *src, l4_icu_msi_info_t *info)
{
  if (!pin())
    {
      int res = alloc_msi();
      if (res < 0)
        return res;
    }

  l4_uint64_t si = src->get_src_info(this);
  return Kernel_irq_pin::_msi_info(si, info);
}



