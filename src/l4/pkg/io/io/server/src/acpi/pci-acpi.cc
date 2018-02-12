#include "debug.h"
#include "io_acpi.h"
#include "pci.h"
#include "__acpi.h"

#include <l4/cxx/list>

namespace {

using namespace Hw;

static void pci_acpi_wake_dev(ACPI_HANDLE, l4_uint32_t event, void *context)
{
  Pci::Dev *pci_dev = static_cast<Pci::Dev*>(context);

  if (event != ACPI_NOTIFY_DEVICE_WAKE || !pci_dev)
    return;

  pci_dev->check_pme_status();
}


struct Acpi_pci_handler : Hw::Feature_manager<Pci::Dev, Acpi_dev>
{
  bool setup(Hw::Device *, Pci::Dev *pci, Acpi_dev *acpi_dev) const
  {
    ACPI_STATUS status;

    status = AcpiInstallNotifyHandler(acpi_dev->handle(),
                                      ACPI_SYSTEM_NOTIFY,
                                      pci_acpi_wake_dev, pci);

    if (ACPI_FAILURE(status))
      d_printf(DBG_ERR, "error: cannot install notification handler "
                        "for ACPI PCI wakeup events: %s\n",
               AcpiFormatException(status));

    return false;
  }
};

static Acpi_pci_handler _acpi_pci_handler;





struct Prt_entry : public cxx::List_item
{
  unsigned slot;
  unsigned char pin;
  acpica_pci_irq irq;
};

class Acpi_pci_irq_router_rs : public Resource_space
{
public:
  Prt_entry *_prt;

public:
  Acpi_pci_irq_router_rs() : _prt(0) {}

  int add_prt_entry(ACPI_HANDLE obj, ACPI_PCI_ROUTING_TABLE *e);
  int find(int device, int pin, struct acpica_pci_irq **irq);
  bool request(Resource *parent, ::Device *, Resource *child, ::Device *cdev);
  bool alloc(Resource *, ::Device *, Resource *, ::Device *, bool)
  { return false; }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Acpi_pci_irq_router_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Acpi_pci_irq_router_rs\n");
    return false;
  }
};


bool
Acpi_pci_irq_router_rs::request(Resource *parent, ::Device *,
                                Resource *child, ::Device *cdev)
{
  if (dlevel(DBG_ALL))
    {
      printf("requesting IRQ resource: ");
      cdev->dump(2);
      child->dump(2);
      printf(" at ACPI IRQ routing resource\n");
    }

  Hw::Device *cd = dynamic_cast<Hw::Device*>(cdev);
  assert (cd);

  struct acpica_pci_irq *irq = 0;

  if (find(cd->adr() >> 16, child->start(), &irq) < 0)
    {
      child->disable();
      if (dlevel(DBG_WARN))
        {
          d_printf(DBG_WARN,
                   "warning: could not allocate PCI IRQ: missing PRT entry: %s\n",
                   cdev->get_full_path().c_str());
          child->dump(2);
          cdev->dump(2);
        }

      return false;
    }

  assert (irq);

  child->del_flags(Resource::F_relative);
  child->start(irq->irq);
  child->del_flags(Resource::Irq_type_mask);
  unsigned flags = Resource::Irq_type_base;
  flags |= (!irq->trigger) * Resource::Irq_type_base * L4_IRQ_F_LEVEL;
  flags |= (!!irq->polarity) * Resource::Irq_type_base * L4_IRQ_F_NEG;
  child->add_flags(flags);

  child->parent(parent);

  return true;
}

static ACPI_STATUS
get_irq_cb(ACPI_RESOURCE *res, void *ctxt)
{
  acpica_pci_irq *irq = (acpica_pci_irq*)ctxt;
  if (!res)
    return AE_OK;

  switch (res->Type)
    {
    case ACPI_RESOURCE_TYPE_IRQ:
      irq->irq = res->Data.Irq.Interrupts[0];
      irq->polarity = res->Data.Irq.Polarity;
      irq->trigger  = res->Data.Irq.Triggering;
      return AE_OK;

    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
      irq->irq = res->Data.ExtendedIrq.Interrupts[0];
      irq->polarity = res->Data.ExtendedIrq.Polarity;
      irq->trigger  = res->Data.ExtendedIrq.Triggering;
      return AE_OK;

    default:
      return AE_OK;
    }
}

int
Acpi_pci_irq_router_rs::add_prt_entry(ACPI_HANDLE obj,
                                      ACPI_PCI_ROUTING_TABLE *e)
{
  if (!e)
    return -EINVAL;

  Prt_entry *ne = new Prt_entry();
  if (!ne)
    return -ENOMEM;

  ne->slot = (e->Address >> 16) & 0xffff;
  ne->pin = e->Pin;

  ne->irq.irq = e->SourceIndex;
  ne->irq.polarity = ACPI_ACTIVE_LOW;
  ne->irq.trigger = ACPI_LEVEL_SENSITIVE;
  if (e->Source[0])
    {
      ACPI_HANDLE link;
      d_printf(DBG_DEBUG, " (dev[%s][%d]) ", e->Source, e->SourceIndex);
      ACPI_STATUS status;
      status = AcpiGetHandle(obj, e->Source, &link);
      if (ACPI_FAILURE(status))
	{
	  d_printf(DBG_WARN, "\nWARNING: Could not find PCI IRQ Link Device...\n");
	  return -ENODEV;
	}

      status = AcpiWalkResources(link, ACPI_STRING("_CRS"), get_irq_cb, &ne->irq);
      if (ACPI_FAILURE(status))
	{
	  d_printf(DBG_WARN, "\nWARNING: Could not evaluate _CRS of PCI IRQ Link Device\n");
	  return -ENODEV;
	}
    }

  _prt = cxx::List_item::push_back(_prt, ne);
  return 0;
}

int
Acpi_pci_irq_router_rs::find(int device, int pin, struct acpica_pci_irq **irq)
{
  Prt_entry::T_iter<Prt_entry> c = _prt;
  while (*c)
    {
      if (c->slot == (unsigned)device && c->pin == pin)
	{
	  *irq = &c->irq;
	  return 0;
	}

      ++c;
    }

  return -ENODEV;
}

static
Resource *discover_prt(Acpi_dev *adev)
{
  ACPI_STATUS status;
  ACPI_HANDLE handle;

  status = AcpiGetHandle(adev->handle(), ACPI_STRING("_PRT"), &handle);

  // no PRT!!
  if (ACPI_FAILURE(status))
    return 0;

  Acpi_auto_buffer buf;
  buf.Length = ACPI_ALLOCATE_BUFFER;

  status = AcpiGetIrqRoutingTable(adev->handle(), &buf);

  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "ERROR: while getting PRT for [%s]\n", "buffer");
      return 0;
    }

  typedef Hw::Pci::Irq_router_res<Acpi_pci_irq_router_rs> Irq_res;
  Irq_res *r = new Irq_res();

  char *p = (char*)buf.Pointer;
  char *e = (char*)buf.Pointer + buf.Length;
  while (1)
    {
      ACPI_PCI_ROUTING_TABLE *prt = (ACPI_PCI_ROUTING_TABLE *)p;
      if (prt->Length == 0)
        break;

      if (p + prt->Length > e)
        break;

      int err = r->provided()->add_prt_entry(adev->handle(), prt);
      if (err < 0)
        {
          d_printf(DBG_ERR, "error: adding PRT entry: %d\n", err);
          return 0;
        }

      p += prt->Length;
    }

  return r;
}

struct Acpi_pci_bridge_handler : Hw::Feature_manager<Hw::Pci::Bus, Acpi_dev>
{
  bool setup(Hw::Device *dev, Hw::Pci::Bus *pci_bus, Acpi_dev *acpi_dev) const
  {
    Resource *router = discover_prt(acpi_dev);
    if (router && !pci_bus->irq_router)
      {
        pci_bus->irq_router = router;
        dev->add_resource_rq(router);
      }
    else if(router)
      {
        d_printf(DBG_WARN, "warning: multiple IRQ routing tables for device: %s\n",
                 dev->get_full_path().c_str());
        delete router;
      }

    return false;
  }
};

static Acpi_pci_bridge_handler _acpi_pci_bridge_handler;

}
