/*
 * (c) 2011 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <cstdio>
#include <cstdlib>

#include "io_acpi.h"
#include "debug.h"
#include "pci.h"
#include "acpi_l4.h"
#include "__acpi.h"
#include "cfg.h"
#include "main.h"
#include "phys_space.h"
#include "hw_root_bus.h"
#include <map>
#include <l4/re/error_helper>
#include <l4/sys/iommu>

extern "C" {
#include "acpi.h"
#include "accommon.h"
#include "acresrc.h"
#include "acnamesp.h"
}

#include <errno.h>
#include <l4/cxx/list>
#include <l4/sys/platform_control>
#include <l4/re/env>
#include <l4/re/util/unique_cap>
#include <l4/re/util/cap_alloc>

#define _COMPONENT		ACPI_BUS_COMPONENT
ACPI_MODULE_NAME("l4main");

// Default ACPICA debug flags. Can be set by user with --acpi-debug-level.
static unsigned _acpi_debug_level =
      ACPI_LV_INIT
      //| ACPI_LV_INFO
      //| ACPI_LV_FUNCTIONS
      //| ACPI_LV_ALL_EXCEPTIONS
      //| ACPI_LV_LOAD
      //| ACPI_LV_INIT_NAMES
        | ACPI_LV_TABLES
      //| ACPI_LV_RESOURCES
      //| ACPI_LV_NAMES
      //| ACPI_LV_VALUES
      //| ACPI_LV_OPREGION
        | ACPI_LV_VERBOSE_INFO
      //| ACPI_LV_PARSE
      //| ACPI_LV_DISPATCH
      //| ACPI_LV_EXEC
      //| ACPI_LV_IO
      ;

static Hw::Pci::Root_bridge *
create_port_bridge(int segment, int busnum, Hw::Device *dev)
{
  return new Hw::Pci::Port_root_bridge(segment, busnum, Hw::Pci::Bus::Pci_bus, dev);
}

Hw::Pci::Root_bridge *(*acpi_create_pci_root_bridge)(int segment,
    int busnum, Hw::Device *device) = create_port_bridge;


void acpi_set_debug_level(unsigned level)
{
  _acpi_debug_level = level;
}

unsigned acpi_get_debug_level()
{
  return _acpi_debug_level;
}

namespace {

struct Acpi_default_driver : Hw::Acpi_device_driver {};

static Io_irq_pin *_sci = 0;

static Hw::Acpi_device_driver *acpi_default_driver()
{
  static Acpi_default_driver d;
  return &d;
}

enum Acpi_irq_model_id {
	ACPI_IRQ_MODEL_PIC = 0,
	ACPI_IRQ_MODEL_IOAPIC,
	ACPI_IRQ_MODEL_IOSAPIC,
	ACPI_IRQ_MODEL_PLATFORM,
	ACPI_IRQ_MODEL_COUNT
};

static int acpi_bus_init_irq(void)
{
  ACPI_STATUS status = AE_OK;
  ACPI_OBJECT arg = { ACPI_TYPE_INTEGER };
  ACPI_OBJECT_LIST arg_list = { 1, &arg };
  char const *message = NULL;


  //int acpi_irq_model = ACPI_IRQ_MODEL_PIC;
  int acpi_irq_model = ACPI_IRQ_MODEL_IOAPIC;
  /*
   * Let the system know what interrupt model we are using by
   * evaluating the \_PIC object, if exists.
   */

  switch (acpi_irq_model) {
    case ACPI_IRQ_MODEL_PIC:
      message = "PIC";
      break;
    case ACPI_IRQ_MODEL_IOAPIC:
      message = "IOAPIC";
      break;
    case ACPI_IRQ_MODEL_IOSAPIC:
      message = "IOSAPIC";
      break;
    case ACPI_IRQ_MODEL_PLATFORM:
      message = "platform specific model";
      break;
    default:
      d_printf(DBG_ERR, "ERROR: Unknown interrupt routing model\n");
      return -1;
  }

  d_printf(DBG_INFO, "Using %s for interrupt routing\n", message);

  arg.Integer.Value = acpi_irq_model;

  status = AcpiEvaluateObject(NULL, (char*)"\\_PIC", &arg_list, NULL);
  if (ACPI_FAILURE(status) && (status != AE_NOT_FOUND)) {
      ACPI_EXCEPTION((AE_INFO, status, "Evaluating _PIC"));
      return -1;
  }

  return 0;
}


struct Discover_ctxt
{
  Hw::Device *last_device;
  Hw::Device *current_bus;
  unsigned level;
};

static unsigned acpi_adr_t_to_f(unsigned art)
{
  switch (art)
    {
    case ACPI_MEMORY_RANGE: return Resource::Mmio_res;
    case ACPI_IO_RANGE: return Resource::Io_res;
    case ACPI_BUS_NUMBER_RANGE: return Resource::Bus_res;
    default: return ~0;
    }
}

static void
acpi_adr_res(l4_uint32_t id, Hw::Device *host, ACPI_RESOURCE_ADDRESS const *ar,
             l4_uint64_t s, l4_uint64_t l, bool qw)
{
  unsigned flags = acpi_adr_t_to_f(ar->ResourceType);

  if (flags == ~0U)
    return;

  if (qw)
    flags |= Resource::F_width_64bit;

  Resource *r;
  if (ar->ProducerConsumer == ACPI_PRODUCER)
    r = new Resource_provider(flags, s, s + l - 1);
  else
    r = new Resource(flags, s, s + l -1);

  r->set_id(id);
  host->add_resource_rq(r);
}

static ACPI_STATUS
discover_pre_cb(ACPI_HANDLE obj, UINT32 nl, void *ctxt, void **)
{
  Discover_ctxt *c = reinterpret_cast<Discover_ctxt*>(ctxt);

  if (nl > c->level)
    {
      c->current_bus = c->last_device;
      c->level = nl;
    }

  if (nl == 1)
    return AE_OK;

  ACPI_NAMESPACE_NODE *node = AcpiNsValidateHandle(obj);
  if (!node)
    {
      //AcpiUtReleaseMutex(ACPI_MTX_NAMESPACE);
      return AE_OK;
    }

  Acpi_ptr<ACPI_DEVICE_INFO> info;
  if (!ACPI_SUCCESS(AcpiGetObjectInfo(node, info.ref())))
    return AE_OK;

  if (info->Type != ACPI_TYPE_DEVICE && info->Type != ACPI_TYPE_PROCESSOR)
    return AE_OK;

  l4_uint32_t adr = ~0U;

  Hw::Device *nd = 0;
  if (info->Valid & ACPI_VALID_ADR)
    {
      adr = info->Address;
      nd = c->current_bus->get_child_dev_adr(adr, true);
      if (nd->find_feature<Hw::Acpi_dev>())
        nd = 0;
    }

  if (!nd)
    nd = c->current_bus->get_child_dev_uid((l4_umword_t)obj, adr, true);

  c->last_device = nd;

    {
      l4_uint32_t nv = info->Name;
      char n[5];
      for (unsigned i = 0; i < 4; ++i)
        {
          n[i] = nv & 0xff;
          nv >>= 8;
        }
      n[4] = 0;
      nd->set_name(n);
    }

  if (info->Valid & ACPI_VALID_HID)
    nd->set_hid(info->HardwareId.String);

  if (info->Valid & ACPI_VALID_CID)
    for (unsigned i = 0; i < info->CompatibleIdList.Count; ++i)
      nd->add_cid(info->CompatibleIdList.Ids[i].String);

  Hw::Acpi_device_driver *drv = 0;
  if (info->Valid & ACPI_VALID_HID)
    drv = Hw::Acpi_device_driver::find(info->HardwareId.String);

  if (!drv && (info->Valid & ACPI_VALID_CID))
    for (unsigned i = 0; i < info->CompatibleIdList.Count; ++i)
      if ((drv = Hw::Acpi_device_driver::find(info->CompatibleIdList.Ids[i].String)))
        break;

  if (!drv)
    drv =  Hw::Acpi_device_driver::find(info->Type);

  if (!drv)
    drv = acpi_default_driver();


  drv->probe(nd, obj, info.get());

  return AE_OK;
}

static ACPI_STATUS
discover_post_cb(ACPI_HANDLE, UINT32 nl, void *ctxt, void **)
{
  Discover_ctxt *c = reinterpret_cast<Discover_ctxt*>(ctxt);
  if (nl < c->level)
    {
      c->level = nl;
      c->last_device = c->current_bus;
      c->current_bus = c->current_bus->parent();
      if (!c->current_bus)
        c->current_bus = system_bus();
    }
  return AE_OK;
}


static int acpi_enter_sleep(int sleepstate = 3 /* s3 */)
{
  // Disable the system control interrupt (SCI) (in line with ACPI spec).
  // Clear all pending SCIs because otherwise they would trigger after resume.
  // Background: The SCI is level triggered. Fiasco receives the IRQ, counts it
  // internally and masks it. As the IRQ count is in software, it survives
  // suspend to RAM (in contrast, hardware interrupts -- like all hardware
  // system state except RAM contents are lost on suspend). Thus, when we later
  // re-enable the SCI, a stored IRQ would immediately be delivered to IO.
  _sci->mask();
  _sci->clear();

  L4::Cap<L4::Platform_control> pf = L4Re::Env::env()->get_cap<L4::Platform_control>("icu");
  if (!pf)
    {
      d_printf(DBG_ERR, "error: no platform control capability found\n");
      return -L4_ENODEV;
    }

  l4_uint8_t sleeptypea, sleeptypeb;

  ACPI_STATUS status = AcpiGetSleepTypeData(sleepstate, &sleeptypea, &sleeptypeb);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "error: cannot determining ACPI sleep type\n");
      return -L4_ENODEV;
    }

  status = AcpiEnterSleepStatePrep(sleepstate);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_WARN, "warning: AcpiEnterSleepStatePrep failed, ignoring\n");
      //ignore... this WILL throw errors on T41p
    }

  /* Clear wake status */
  status = AcpiWriteBitRegister(ACPI_BITREG_WAKE_STATUS, ACPI_CLEAR_STATUS);
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: cannot clear wake status\n");

  /* Clear all fixed and general purpose status bits */
  status = AcpiHwClearAcpiStatus();
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: cannot clear all fixed and GPE status bits.\n");

  /*
   * 1) Disable/Clear all GPEs
   * 2) Enable all wakeup GPEs
   */
  status = AcpiDisableAllGpes();
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: disabling all GPEs.\n");

  AcpiGbl_SystemAwakeAndRunning = FALSE;

  status = AcpiHwEnableAllWakeupGpes();
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: cannot enable all wakeup GPEs\n");

  d_printf(DBG_DEBUG2, "call platform control object for suspend\n");
  int err = 0;
  if ((err = l4_error(pf->system_suspend(sleeptypea | ((unsigned)sleeptypeb << 8)))) < 0)
    d_printf(DBG_ERR, "error: suspend failed: %d\n", err);
  else
    d_printf(DBG_DEBUG2, "resume: wakeup from suspend\n");

  // out of spec, but required on buggy systems
  AcpiWriteBitRegister(ACPI_BITREG_SCI_ENABLE, 1);

  AcpiLeaveSleepStatePrep(sleepstate);

  ACPI_EVENT_STATUS pwr_btn_s;

  AcpiGetEventStatus(ACPI_EVENT_POWER_BUTTON, &pwr_btn_s);

  if (pwr_btn_s & ACPI_EVENT_FLAG_SET)
    {
      AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
      /* FIXME: remember for later */
    }

  status = AcpiDisableAllGpes();
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: disabling all GPEs.\n");

  status = AcpiLeaveSleepState(sleepstate);
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: AcpiLeaveSleepState failed\n");

  status = AcpiEnableAllRuntimeGpes();
  if (ACPI_FAILURE(status))
    d_printf(DBG_WARN, "warning: cannot enable all wakeup GPEs\n");

  // re-enable system control interrupt (SCI)
  _sci->unmask();
  return err;
}

static UINT32
acpi_fixed_device_event_handler(void *context)
{
  Hw::Device *dev = static_cast<Hw::Device*>(context);
  // generate an artificial device notification with value 0x80
  dev->notify(Hw::Acpi_dev::Acpi_event, 0x80, 1);
  trace_event(TRACE_ACPI_EVENT, "Acpi fixed event. Device '%s'.\n",
              dev->name());
  return 0;
}

/**
 * \brief Determine name of given ACPI handle.
 *
 * \param  handle The ACPI handle of be queried.
 * \retval name Name of the ACPI object.
 */
static void acpi_get_name(ACPI_HANDLE handle, char name[5])
{
  Acpi_ptr<ACPI_DEVICE_INFO> info;
  ACPI_STATUS status;

  status = AcpiGetObjectInfo(handle, info.ref());
  if (ACPI_FAILURE(status))
    strncpy(name, "NONE", 5);
  else
    {
      l4_uint32_t nv = info->Name;
      for (unsigned i = 0; i < 4; ++i)
        {
          name[i] = nv & 0xff;
          nv >>= 8;
        }
      name[4] = 0;
    }
}

/**
 * \brief Trace all ACPI events.
 *
 * Global ACPI event handler. This is only called if ACPI event tracing is
 * enabled.
 */
static void
acpi_trace_events(UINT32 type, ACPI_HANDLE handle, UINT32 event, void *)
{
  char name[5];
  acpi_get_name(handle, name);
  d_printf(DBG_INFO, "ACPI Event. Device %s. Type %s. Number 0x%x.\n",
           name, type == ACPI_EVENT_TYPE_GPE ? "GPE" : "FIXED", event);
}

/**
 * \brief Trace all ACPI notifications.
 *
 * Global ACPI notification handler. This is only called if ACPI event tracing
 * is enabled.
 */
static void
acpi_trace_notifications(ACPI_HANDLE handle, UINT32 event, void *)
{
  char name[5];
  acpi_get_name(handle, name);
  d_printf(DBG_INFO, "ACPI Notification. Device %s. Event '0x%x'.\n",
           name, event);
}

struct Acpi_pm : Hw::Root_bus::Pm
{
  int suspend()
  {
    int res;
    if ((res = acpi_enter_sleep()) < 0)
      d_printf(DBG_ERR, "error: suspend failed: %d\n", res);

    return res;
  }

  int shutdown()
  {
    int res;
    if ((res = acpi_enter_sleep(5)) < 0)
      d_printf(DBG_ERR, "error: shutdown failed: %d\n", res);

    return res;
  }

  int reboot()
  {
    ACPI_STATUS status = AcpiReset();

    if (ACPI_SUCCESS(status))
      return 0;

    auto pf = L4Re::Env::env()->get_cap<L4::Platform_control>("icu");
    if (!pf)
      {
        d_printf(DBG_WARN, "warning: no platform control capability found\n"
                           "         no fallback for platform reset\n");
        return -L4_ENOENT;
      }

    int err = l4_error(pf->system_shutdown(1));
    if (err < 0)
      {
        d_printf(DBG_ERR, "error: pf->system_shutdown(reset) failed: %d\n",
                 err);
        return err;
      }

    return 0;
  }

};

static Acpi_pm _acpi_pm;

static Hw::Device *find_dev(char const *hid)
{
  for (auto i = Hw::Device::iterator(0, system_bus(), L4VBUS_MAX_DEPTH); i != Hw::Device::iterator(); ++i)
    if ((*i)->match_cid(hid))
      return *i;

  return 0;
}

static void
acpi_install_fixed_button_handler(char const *hid, UINT32 event, char const *name)
{
  Hw::Device *button = find_dev(hid);
  if (!button)
    {
      d_printf(DBG_INFO, "ACPI: %s button not found, create an artificial one\n",
               name);
      button = new Hw::Device();
      button->set_hid(hid);
      system_bus()->add_child(button);
    }

  d_printf(DBG_INFO, "ACPI: %s button is a fixed event on this system\n", name);
  ACPI_STATUS s = AcpiInstallFixedEventHandler(event,
      acpi_fixed_device_event_handler, button);
  if (ACPI_FAILURE(s))
    d_printf(DBG_ERR, "ACPI: could not register power-button handler: %s\n",
             AcpiFormatException(s));
  else if (ACPI_FAILURE(s = AcpiEnableEvent(event, 0)))
    d_printf(DBG_ERR, "ACPI: could not enable power-button event: %s\n",
             AcpiFormatException(s));

}

}

static Hw::Pci::Root_bridge *
create_additional_mmio_bridge(int pci_segment, int bus_num, Hw::Device *)
{
  Hw::Pci::Root_bridge *rb = Hw::Pci::root_bridge(pci_segment);
  if (!rb)
    {
      d_printf(DBG_ERR, "Error: Internal, no root-bridge (segment %d)\n",
                        pci_segment);
      return 0;
    }

  Hw::Pci::Mmio_root_bridge *r = dynamic_cast<Hw::Pci::Mmio_root_bridge *>(rb);
  if (!r)
    {
      d_printf(DBG_ERR, "Error: Internal, not a MMIO root bridge\n");
      return 0;
    }

  // Just copy the existing one, it shall have the right config
  auto *br = new Hw::Pci::Mmio_root_bridge(*r);
  br->num = bus_num;
  br->subordinate = bus_num;
  return br;
}

namespace {
using namespace Hw;

class Dmar_dma_domain : public Dma_domain
{
public:
  Dmar_dma_domain(Pci::Bus const *bus, Hw::Device const *dev)
  : _bus(bus),
    _dev(dev ? const_cast<Hw::Device*>(dev)->find_feature<Hw::Pci::Dev>() : 0)
  {}

  static void init()
  {
    _supports_remapping = true;
  }

  void iommu_bind(L4::Cap<L4::Iommu> iommu, l4_uint64_t src)
  {
    int r = l4_error(iommu->bind(src, _kern_dma_space));
    if (r < 0)
      d_printf(DBG_ERR, "error: setting DMA for device: %d\n", r);
  }

  void set_managed_kern_dma_space(L4::Cap<L4::Task> s) override
  {
    Dma_domain::set_managed_kern_dma_space(s);

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    if (!iommu)
      {
        d_printf(DBG_ERR, "no IOMMU capability available\n");
        return;
      }

    if (_dev)
      {
        unsigned phantomfn = _dev->phantomfn_bits();
        unsigned devfn     = _dev->devfn();
        devfn &= (7 >> phantomfn) | 0xf8;
        for (unsigned i = 0; i < (1u << phantomfn); ++i)
          {
            iommu_bind(iommu, 0x40000
                              | (_bus->num << 8)
                              | devfn | (i << (3 - phantomfn)));
          }
      }
    else
      iommu_bind(iommu, 0x80000
                        | (_bus->num << 8) | _bus->num);
  }

  int create_managed_kern_dma_space() override
  {
    assert (!_kern_dma_space);

    auto dma = L4Re::chkcap(L4Re::Util::make_unique_cap<L4::Task>());
    L4Re::chksys(L4Re::Env::env()->factory()->create(dma.get(), L4_PROTO_DMA_SPACE));

    set_managed_kern_dma_space(dma.release());
    return 0;
  }

  int set_dma_task(bool set, L4::Cap<L4::Task> dma_task) override
  {
    if (managed_kern_dma_space())
      return -EBUSY;

    if (set && kern_dma_space())
      return -EBUSY;

    if (!set && !kern_dma_space())
      return -L4_EINVAL;

    static L4::Cap<L4::Task> const &me = L4Re::This_task;
    if (!set && !me->cap_equal(kern_dma_space(), dma_task).label())
      return -L4_EINVAL;

    L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
    if (!iommu)
      {
        d_printf(DBG_ERR, "no IOMMU capability available\n");
        return -L4_EBUSY;
      }

    if (set)
      {
        _kern_dma_space = dma_task;
        int r;
        if (_dev)
          r = l4_error(iommu->bind(0x40000
                                   | (_bus->num << 8)
                                   | (_dev->device_nr() << 3)
                                   | _dev->function_nr(), _kern_dma_space));
        else
          r = l4_error(iommu->bind(0x80000
                                   | (_bus->num << 8) | _bus->num,
                                   _kern_dma_space));

        return r;
      }
    else
      {
        if (!_kern_dma_space)
          return 0;

        int r;
        if (_dev)
          r = l4_error(iommu->unbind(0x40000
                                     | (_bus->num << 8)
                                     | (_dev->device_nr() << 3)
                                     | _dev->function_nr(), _kern_dma_space));
        else
          r = l4_error(iommu->unbind(0x80000
                                     | (_bus->num << 8) | _bus->num,
                                     _kern_dma_space));

        if (r < 0)
          return r;

        _kern_dma_space = L4::Cap<L4::Task>::Invalid;
      }

    return 0;
  }

private:
  Pci::Bus const *_bus = 0;
  Hw::Pci::Dev const *_dev = 0;
};

class Dmar_dma_domain_factory : public Dma_domain_factory
{
public:
  Dmar_dma_domain *create(Hw::Device *bridge, Hw::Device *dev) override
  {
    Pci::Bus *pbus = bridge->find_feature<Pci::Bus>();
    assert (pbus);

    if (!dev)
      // downstream Domain requested, only allowed for non-pcie bus
      assert (pbus->bus_type != Hw::Pci::Bus::Pci_express_bus);

    return new Dmar_dma_domain(pbus, dev);
  }
};

}

static bool
setup_pci_root_mmconfig()
{
  ACPI_TABLE_HEADER *tblhdr;

  if (ACPI_FAILURE(AcpiGetTable(ACPI_STRING(ACPI_SIG_MCFG), 0, &tblhdr)))
    return false;

  if (!tblhdr)
    return false;

  ACPI_TABLE_MCFG *mcfg = (ACPI_TABLE_MCFG *)tblhdr;

  size_t sz = tblhdr->Length - sizeof(ACPI_TABLE_MCFG);
  ACPI_MCFG_ALLOCATION *t = (ACPI_MCFG_ALLOCATION *)(mcfg + 1);
  ACPI_MCFG_ALLOCATION *e = t;
  while (1)
    {
      if (sz < sizeof(ACPI_MCFG_ALLOCATION))
        break;

      d_printf(DBG_INFO, "PCI-MMCONFIG: %llx PciSegment=%d Bus=%d-%d\n",
               (unsigned long long)e->Address,
               e->PciSegment, e->StartBusNumber, e->EndBusNumber);

      // Register for all given buses?
      unsigned num_busses = e->EndBusNumber - e->StartBusNumber + 1;
      Hw::Pci::register_root_bridge(
          new Hw::Pci::Mmio_root_bridge(e->PciSegment, e->StartBusNumber,
                                        Hw::Pci::Bus::Pci_express_bus,
                                        0, e->Address, num_busses));

      acpi_create_pci_root_bridge = create_additional_mmio_bridge;

      e++;
      sz -= sizeof(ACPI_MCFG_ALLOCATION);
    }

  bool ok = e != t;

  if (ok)
    {
      L4::Cap<L4::Iommu> iommu = L4Re::Env::env()->get_cap<L4::Iommu>("iommu");
      if (iommu && iommu.validate().label())
        {
          system_bus()->set_dma_domain_factory(new Dmar_dma_domain_factory());
          Dmar_dma_domain::init();
        }
    }

  return ok;
}

static void setup_pci_root()
{
  if (setup_pci_root_mmconfig())
    return;

  // fall-back to port-based bridge
  Hw::Pci::register_root_bridge(
      new Hw::Pci::Port_root_bridge(0, 0, Hw::Pci::Bus::Pci_bus, 0));
}


int acpica_init()
{
  d_printf(DBG_INFO, "Hello from L4-ACPICA\n");

  AcpiDbgLevel = acpi_get_debug_level();

//0. enable workarounds, see include/acglobals.h
  AcpiGbl_EnableInterpreterSlack = (1==1);
  ACPI_STATUS status;

  status = AcpiInitializeSubsystem();
  if(status!=AE_OK)
    return status;

  status = AcpiInitializeTables(0, 0, TRUE);
  if(status!=AE_OK)
    return status;

  status = AcpiReallocateRootTable();
//  if(status!=AE_OK)
//    return status;

  status = AcpiLoadTables();

  if(ACPI_FAILURE(status))
    return status;

  setup_pci_root();

  d_printf(DBG_DEBUG, "enable ACPI subsystem\n");
  status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "Unable to start the ACPI Interpreter\n");
      exit(status);
    }

  acpi_ecdt_scan();

  d_printf(DBG_DEBUG, "initialize ACPI objects\n");
  status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
  if (ACPI_FAILURE(status)) {
      d_printf(DBG_ERR, "Unable to initialize ACPI objects\n");
      exit(status);
  }

  d_printf(DBG_DEBUG, "Interpreter enabled\n");

  /*
   * Get the system interrupt model and evaluate \_PIC.
   */
  int result = acpi_bus_init_irq();
  if (result)
    {
      d_printf(DBG_ERR, "Could not initialize ACPI IRQ stuff\n");
      exit(1);
    }

  Discover_ctxt c;
  c.last_device = system_bus();
  c.current_bus = system_bus();
  c.level = 1;

  status = AcpiWalkNamespace(ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                             ACPI_UINT32_MAX,
                             discover_pre_cb, discover_post_cb, &c, 0);

  status = AcpiSubsystemStatus();

  if (ACPI_FAILURE(status))
      exit(status);

  d_printf(DBG_INFO, "ACPI subsystem initialized\n");

    {
      Acpi_auto_buffer ret_buffer;
      ret_buffer.Length = ACPI_ALLOCATE_BUFFER;

      status = AcpiGetSystemInfo(&ret_buffer);

      if(ACPI_FAILURE(status))
        exit(status);

      acpi_print_system_info(ret_buffer.Pointer);
    }

  if ((AcpiGbl_FADT.Flags & ACPI_FADT_POWER_BUTTON) == 0)
    acpi_install_fixed_button_handler("PNP0C0C", ACPI_EVENT_POWER_BUTTON, "power");

  if ((AcpiGbl_FADT.Flags & ACPI_FADT_SLEEP_BUTTON) == 0)
    acpi_install_fixed_button_handler("PNP0C0E", ACPI_EVENT_SLEEP_BUTTON, "sleep");

  if (trace_event_enabled(TRACE_ACPI_EVENT))
    {
      status = AcpiInstallGlobalEventHandler(acpi_trace_events, 0);
      if (ACPI_FAILURE(status))
        d_printf(DBG_ERR,
                 "error: could not install global event handler.\n");

      status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_ALL_NOTIFY,
                                        acpi_trace_notifications, 0);
      if (ACPI_FAILURE(status))
        d_printf(DBG_ERR,
                 "error: could not install global device notification handler.\n");
    }

  status = AcpiUpdateAllGpes();
  if(ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "Could not update all GPEs\n");
      exit(status);
    }

  status = AcpiEnableAllRuntimeGpes();
  if(ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "Could not enable all GPEs\n");
      exit(status);
    }

  dynamic_cast<Hw::Root_bus*>(system_bus())->set_pm(&_acpi_pm);

  return 0;
}




namespace Hw {

namespace {
  typedef std::map<l4_uint64_t, Acpi_device_driver *> Drv_list;

  static Drv_list &drv_list()
  {
    static Drv_list l;
    return l;
  }

  static l4_uint64_t acpi_get_key(char const *hid)
  {
    l4_uint64_t k = 0;
    for (unsigned i = 0; i < 8 && hid[i]; ++i)
      k = (k << 8) | hid[i];
    return k;
  }

  static l4_uint64_t acpi_get_key(unsigned short type)
  {
    l4_uint64_t k = ((l4_uint64_t)type << 16) | 0x0100;
    return k;
  }

  static Resource *res(l4_uint32_t id, unsigned long flags, l4_uint64_t start,
                       l4_uint64_t end)
  {
    Resource *r = new Resource(flags, start, end);
    r->set_id(id);
    return r;
  }
}

namespace Acpi {
void register_sci(Io_irq_pin *sci)
{
  ::_sci = sci;
}
}

void
Acpi_dev::discover_crs(Hw::Device *host)
{
  Acpi_auto_buffer buf;
  buf.Length = ACPI_ALLOCATE_BUFFER;

  if (ACPI_FAILURE(AcpiGetCurrentResources(this->handle(), &buf)))
    return;

  char const *p = (char const *)buf.Pointer;
  l4_uint32_t res_id = 0x00504341; // ACPx
  while (p)
    {
      ACPI_RESOURCE const *r = (ACPI_RESOURCE const *)p;
      ACPI_RESOURCE_DATA const *d = &r->Data;
      unsigned flags = 0;

      switch (r->Type)
	{
	case ACPI_RESOURCE_TYPE_END_TAG:
	  return;

	case ACPI_RESOURCE_TYPE_IRQ:
	  flags = Resource::Irq_res | Resource::Irq_type_base;
	  flags |= (!d->Irq.Triggering) * Resource::Irq_type_base * L4_IRQ_F_LEVEL;
	  flags |= (!!d->Irq.Polarity) * Resource::Irq_type_base * L4_IRQ_F_NEG;
	  for (unsigned c = 0; c < d->Irq.InterruptCount; ++c)
	    host->add_resource_rq(res(res_id++, flags, d->Irq.Interrupts[c],
	                              d->Irq.Interrupts[c]));
	  break;

        case ACPI_RESOURCE_TYPE_DMA:
          // ignore this legacy type to avoid warnings about unknown types
          break;

	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
	  flags = Resource::Irq_res | Resource::Irq_type_base;
	  flags |= (!d->ExtendedIrq.Triggering) * Resource::Irq_type_base * L4_IRQ_F_LEVEL;
	  flags |= (!!d->ExtendedIrq.Polarity) * Resource::Irq_type_base * L4_IRQ_F_NEG;
	  if (d->ExtendedIrq.ResourceSource.StringPtr)
	    {
	      d_printf(DBG_DEBUG2, "hoo indirect IRQ resource found src=%s idx=%d\n",
		       d->ExtendedIrq.ResourceSource.StringPtr,
		       d->ExtendedIrq.ResourceSource.Index);
	    }
	  else
	    {
	      for (unsigned c = 0; c < d->ExtendedIrq.InterruptCount; ++c)
		host->add_resource_rq(res(res_id++, flags, d->ExtendedIrq.Interrupts[c],
		                          d->ExtendedIrq.Interrupts[c]));
	    }
	  break;

	case ACPI_RESOURCE_TYPE_IO:
          if (d->Io.AddressLength == 0)
            break;
	  flags = Resource::Io_res;
	  host->add_resource_rq(res(res_id++, flags, d->Io.Minimum,
		                    d->Io.Minimum + d->Io.AddressLength - 1));
	  break;

	case ACPI_RESOURCE_TYPE_FIXED_IO:
          if (d->FixedIo.AddressLength == 0)
            break;
	  flags = Resource::Io_res;
	  host->add_resource_rq(res(res_id++, flags, d->FixedIo.Address,
		                    d->FixedIo.Address + d->FixedIo.AddressLength - 1));
	  break;

	case ACPI_RESOURCE_TYPE_MEMORY24:
          if (d->Memory24.AddressLength == 0)
            break;
	  flags = Resource::Mmio_res;
	  host->add_resource_rq(res(res_id++, flags, d->Memory24.Minimum,
		                    d->Memory24.Minimum + d->Memory24.AddressLength - 1));
	  break;

	case ACPI_RESOURCE_TYPE_MEMORY32:
          if (d->Memory32.AddressLength == 0)
            break;
	  flags = Resource::Mmio_res;
	  host->add_resource_rq(res(res_id++, flags, d->Memory32.Minimum,
		                    d->Memory32.Minimum + d->Memory32.AddressLength - 1));
	  break;

	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
          if (d->FixedMemory32.AddressLength == 0)
            break;
	  flags = Resource::Mmio_res;
	  host->add_resource_rq(res(res_id++, flags, d->FixedMemory32.Address,
		                    d->FixedMemory32.Address + d->FixedMemory32.AddressLength - 1));
	  break;

	case ACPI_RESOURCE_TYPE_ADDRESS16:
          if (d->Address16.Address.AddressLength == 0)
            break;
	  acpi_adr_res(res_id++, host, &d->Address, d->Address16.Address.Minimum, d->Address16.Address.AddressLength, 0);
	  break;

	case ACPI_RESOURCE_TYPE_ADDRESS32:
          if (d->Address32.Address.AddressLength == 0)
            break;
	  acpi_adr_res(res_id++, host, &d->Address, d->Address32.Address.Minimum, d->Address32.Address.AddressLength, 0);
	  break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:
          if (d->Address64.Address.AddressLength == 0)
            break;
	  acpi_adr_res(res_id++, host, &d->Address, d->Address64.Address.Minimum, d->Address64.Address.AddressLength, 1);
	  break;

	default:
	  d_printf(DBG_WARN, "WARNING: ignoring ACPI resource (unknown type: %d)\n", r->Type);
	  break;


	}

      p += r->Length;
    }
}

static void acpi_dev_notification_handler(ACPI_HANDLE, UINT32 event, void *ctxt)
{
  Hw::Device *device = static_cast<Hw::Device*>(ctxt);
  trace_event(TRACE_ACPI_EVENT, "ACPI device notification. Device = %s, Event = 0x%x\n",
              device->name(), event);
  device->notify(Acpi_dev::Acpi_event, event, 1);
}

void
Acpi_dev::enable_notifications(Hw::Device *host)
{
  if (_have_notification_handler)
    return;

  ACPI_STATUS s = AcpiInstallNotifyHandler(handle(), ACPI_ALL_NOTIFY, acpi_dev_notification_handler, host);

  if (ACPI_SUCCESS(s))
    _have_notification_handler = true;
  else
    d_printf(DBG_ERR,
             "error: cannot install notification handler for ACPI device: %s\n",
             AcpiFormatException(s));
}

void
Acpi_dev::disable_notifications(Hw::Device *)
{
  if (!_have_notification_handler)
    return;

  ACPI_STATUS s = AcpiRemoveNotifyHandler(handle(), ACPI_ALL_NOTIFY, acpi_dev_notification_handler);
  if (ACPI_FAILURE(s))
    d_printf(DBG_ERR,
             "error: cannot remove notification handler for ACPI device: %s\n",
             AcpiFormatException(s));

  _have_notification_handler = false;
}

bool
Acpi_device_driver::register_driver(char const *hid, Acpi_device_driver *driver)
{
  drv_list()[acpi_get_key(hid)] = driver;
  return true;
}

bool
Acpi_device_driver::register_driver(unsigned short type, Acpi_device_driver *driver)
{
  drv_list()[acpi_get_key(type)] = driver;
  return true;
}

Acpi_device_driver *
Acpi_device_driver::find(char const *hid)
{
  Drv_list const &l = drv_list();
  Drv_list::const_iterator r = l.find(acpi_get_key(hid));
  if (r != l.end())
    return (*r).second;
  return 0;
}

Acpi_device_driver *
Acpi_device_driver::find(unsigned short type)
{
  Drv_list const &l = drv_list();
  Drv_list::const_iterator r = l.find(acpi_get_key(type));
  if (r != l.end())
    return (*r).second;
  return 0;
}

Acpi_dev *
Acpi_device_driver::probe(Hw::Device *device, ACPI_HANDLE acpi_hdl,
                          ACPI_DEVICE_INFO const *info)
{
  Acpi_dev *adev = new Acpi_dev(acpi_hdl);
  if ((info->Valid & ACPI_VALID_STA)
      && (info->CurrentStatus & ACPI_STA_DEVICE_ENABLED))
    adev->discover_crs(device);
  device->add_feature(adev);
  return adev;
}

}
