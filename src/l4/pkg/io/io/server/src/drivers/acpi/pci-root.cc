#include "io_acpi.h"
#include "__acpi.h"
#include "pci.h"
#include "debug.h"
#include <l4/cxx/list>


namespace {

using namespace Hw;
using Hw::Device;

static ACPI_STATUS
acpi_eval_int(ACPI_HANDLE hdl, ACPI_STRING path,
              ACPI_OBJECT_LIST *args, unsigned long long *res)
{
  Acpi_buffer<ACPI_OBJECT> r;
  if (!res)
    return AE_BAD_PARAMETER;

  ACPI_STATUS s = AcpiEvaluateObjectTyped(hdl, path, args, &r,
                                          ACPI_TYPE_INTEGER);
  if (ACPI_FAILURE(s))
    return s;

  *res = r.value.Integer.Value;
  return AE_OK;
}

static ACPI_STATUS
get_bus_range(ACPI_RESOURCE *res, void *ctxt)
{
  int *bbn = (int *)ctxt;
  int mi = 0, len = 0;
  switch (res->Type)
    {
    case ACPI_RESOURCE_TYPE_ADDRESS16:
      mi = res->Data.Address16.Address.Minimum;
      len = res->Data.Address16.Address.AddressLength;
      break;

    case ACPI_RESOURCE_TYPE_ADDRESS32:
      mi = res->Data.Address32.Address.Minimum;
      len = res->Data.Address32.Address.AddressLength;
      break;

    case ACPI_RESOURCE_TYPE_ADDRESS64:
      mi = res->Data.Address64.Address.Minimum;
      len = res->Data.Address64.Address.AddressLength;
      break;

    default:
      return AE_OK;
    }

  if (res->Data.Address.ResourceType != ACPI_BUS_NUMBER_RANGE)
    return AE_OK;

  bbn[0] = mi;
  bbn[1] = len;
  return AE_OK;
}

static ACPI_STATUS
get_bbn(ACPI_HANDLE hdl, int *bbn)
{
  bbn[0] = -1;

  ACPI_STATUS s;
  s = AcpiWalkResources(hdl, ACPI_STRING("_CRS"), get_bus_range, bbn);

  if (ACPI_FAILURE(s))
    return s;

  if (bbn[0] == -1)
    return AE_ERROR;

  return AE_OK;
}


struct Acpi_pci_root_drv : Acpi_device_driver
{
  Acpi_dev *probe(Device *device, ACPI_HANDLE acpi_hdl,
                  ACPI_DEVICE_INFO const *info)
  {
    d_printf(DBG_DEBUG, "Found PCI root bridge...\n");
    // do this first so we have an Acpi_dev feature installed for 'device'
    Acpi_dev *adev = Acpi_device_driver::probe(device, acpi_hdl, info);

    unsigned long long seg = 0;
    ACPI_STATUS s = acpi_eval_int(acpi_hdl, ACPI_STRING("_SEG"), NULL, &seg);
    // _SEG is optional so use segment = 0 if not found
    if (ACPI_FAILURE(s) && s != AE_NOT_FOUND)
      {
        d_printf(DBG_ERR,
                 "error: could not evaluate '_SEG' for PCI root bridge\n");
        return adev;
      }

    Hw::Pci::Root_bridge *rb = Hw::Pci::root_bridge(seg);
    if (!rb)
      {
        d_printf(DBG_ERR, "error: PCI root bridge for segment %d missing\n",
                 (int)seg);
        return adev;
      }

    int bbn[2] = { -1, -1 };
    s = get_bbn(acpi_hdl, bbn);
    if (ACPI_FAILURE(s))
      {
        unsigned long long _bbn = 0;
        s = acpi_eval_int(acpi_hdl, ACPI_STRING("_BBN"), NULL, &_bbn);
        if (ACPI_FAILURE(s) && s != AE_NOT_FOUND)
          {
            d_printf(DBG_ERR,
                     "error: cannot evaluate '_BBN' for PCI root bridge\n");
            return adev;
          }
        bbn[0] = _bbn;
        bbn[1] = 256;
      }

    if (bbn[1] <= 0)
      {
        d_printf(DBG_INFO,
                 "%s: assigned PCI bus range is empty skip root bridge\n",
                 device->name());
        return adev;
      }

    auto *rb2 = Hw::Pci::find_root_bridge(seg, bbn[0]);
    if (rb2 && rb2->host())
      {
        d_printf(DBG_DEBUG, "%s: skip bridge for already existing bus: %d\n",
                 device->name(), bbn[0]);
        return adev;
      }

    d_printf(DBG_DEBUG, "%s: PCI bus range %d-%d\n", device->name(), bbn[0],
             bbn[0] + bbn[1] - 1);

    if (rb->host())
      {
        // we found a second root bridge
        // create a new root bridge instance
        rb = acpi_create_pci_root_bridge(seg, bbn[0], device);
        Hw::Pci::register_root_bridge(rb);
      }
    else
      {
        rb->set_host(device);
        rb->num = bbn[0];
      }

    device->add_feature(rb);
    return adev;
  }
};

static Acpi_pci_root_drv pci_root_drv;

struct Init
{
  Init()
  {
    Acpi_device_driver::register_driver(PCI_ROOT_HID_STRING, &pci_root_drv);
    Acpi_device_driver::register_driver(PCI_EXPRESS_ROOT_HID_STRING, &pci_root_drv);
  }
};

static Init init;

}

