/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is licensed under the terms of the GNU General Public License 2.
 * See file COPYING-GPL-2 for details.
 */
#include "debug.h"
#include "server.h"

#include <stdio.h>
#include <l4/sys/compiler.h>

__BEGIN_DECLS
#include "acpi.h"
#include "acpiosxf.h"
#include "actypes.h"
__END_DECLS

#include <l4/sys/cxx/ipc_epiface>
#include <l4/re/env>
#include "../io_acpi.h"

namespace {

class Acpi_sci :
  public Kernel_irq_pin,
  public L4::Irqep_t<Acpi_sci>
{
public:
  Acpi_sci(ACPI_OSD_HANDLER isr, void *context, int irqnum)
  : Kernel_irq_pin(irqnum), _isr(isr), _context(context) {}

  void handle_irq()
  {
    if (!_isr(_context))
      trace_event(TRACE_ACPI_EVENT, "SCI not handled\n");
    unmask();
  }

private:
  ACPI_OSD_HANDLER _isr;
  void *_context;
};

}

ACPI_STATUS
AcpiOsInstallInterruptHandler (
    UINT32                    interrupt_number,
    ACPI_OSD_HANDLER          service_routine,
    void                      *context)
{
  int err;
  L4::Cap<L4::Icu> icu = L4Re::Env::env()->get_cap<L4::Icu>("icu");
  if (!icu)
    {
      d_printf(DBG_ERR, "error: could not find ICU capability.\n");
      return AE_BAD_PARAMETER;
    }

  Acpi_sci *sci = new Acpi_sci(service_routine, context, interrupt_number);
  L4::Cap<L4::Irq> irq = irq_queue()->register_irq_obj(sci);
  if (!irq.is_valid())
    {
      d_printf(DBG_ERR, "error: could not register ACPI event server\n");
      return AE_BAD_PARAMETER;
    }

  if ((err = sci->bind(irq, L4_IRQ_F_NONE)) < 0)
    {
      d_printf(DBG_ERR, "error: irq bind failed: %d\n", err);
      return AE_BAD_PARAMETER;
    }

  d_printf(DBG_INFO, "created ACPI event server and attached it to irq %d\n",
           interrupt_number);

  sci->unmask();

  Hw::Acpi::register_sci(sci);

  return AE_OK;
};

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
    UINT32                   interrupt_number,
    ACPI_OSD_HANDLER         service_routine)
{
  printf("%s:%d:%s(%d, %p): UNINPLEMENTED\n",
         __FILE__, __LINE__, __func__,
         interrupt_number, service_routine);
  return AE_OK;
}

