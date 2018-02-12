/*
 * (c) 2014 Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *
 * This file is licensed under the terms of the Gnu General Public license 2.
 * See file COPYING-GPL-2 for details.
 */
#include "io_acpi.h"
#include "__acpi.h"
#include "debug.h"
#include "event_source.h"
#include <l4/re/event_enums.h>
#include <cassert>

namespace {

using namespace Hw;
using Hw::Device;

/**
 * \brief Setup device as a wake source.
 *
 * \param device This device
 * \param handle ACPI handle
 */
static void acpi_setup_device_wake(Device *device, ACPI_HANDLE handle)
{
  Acpi_auto_buffer buffer;
  buffer.Length = ACPI_ALLOCATE_BUFFER;

  ACPI_STATUS status;
  status = AcpiEvaluateObject(handle, ACPI_STRING("_PRW"), NULL, &buffer);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "error: ACPI %s: could not evaluate _PRW object (%s), "
                        "not enabled as wake source\n",
               device->name(), AcpiFormatException(status));
      return;
    }

  ACPI_OBJECT *prw = (ACPI_OBJECT*)buffer.Pointer;
  if (!prw || prw->Type != ACPI_TYPE_PACKAGE || prw->Package.Count < 2)
    {
      d_printf(DBG_ERR, "error: ACPI %s: has invalid _PRW object, "
                        "not enabled as wake source\n", device->name());
      return;
    }

  ACPI_HANDLE gpe_device = NULL;
  int gpe_number;
  ACPI_OBJECT *gpe_info = &(prw->Package.Elements[0]);
  switch (gpe_info->Type)
    {
    case ACPI_TYPE_PACKAGE:
      if ((gpe_info->Package.Count < 2)
          || (gpe_info->Package.Elements[0].Type != ACPI_TYPE_LOCAL_REFERENCE)
          || (gpe_info->Package.Elements[1].Type != ACPI_TYPE_INTEGER))
          {
            d_printf(DBG_ERR, "error: ACPI %s: invalid GPE info, "
                              "not enabled as wake source\n", device->name());
            return;
          }
      gpe_device = gpe_info->Package.Elements[0].Reference.Handle;
      gpe_number = gpe_info->Package.Elements[1].Integer.Value;
      break;

    case ACPI_TYPE_INTEGER:
      gpe_number = gpe_info->Integer.Value;
      break;

    default:
      d_printf(DBG_ERR, "error: ACPI %s: invalid GPE info, "
                        "not enabled as wake source\n", device->name());
      return;
    }

  status = AcpiSetupGpeForWake(handle, gpe_device, gpe_number);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "error: ACPI %s: AcpiSetupGpeForWake failed: %s\n",
               device->name(), AcpiFormatException(status));
      return;
    }
  status = AcpiSetGpeWakeMask(gpe_device, gpe_number, ACPI_GPE_ENABLE);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "error: ACPI %s: AcpiSetGpeWakeMask failed: %s\n",
               device->name(), AcpiFormatException(status));
      return;
    }
  status = AcpiEnableGpe(gpe_device, gpe_number);
  if (ACPI_FAILURE(status))
    {
      d_printf(DBG_ERR, "error: ACPI %s: could not enable GPE: %s\n",
               device->name(), AcpiFormatException(status));
      return;
    }

  d_printf(DBG_DEBUG, "ACPI %s: configured as wake source\n",
           device->name());

}

class Acpi_lid : public Acpi_dev
{
public:
  Acpi_lid(ACPI_HANDLE hdl, Device *host);

  void enable_notifications(Hw::Device *host);
  void get_and_store_state(bool *closed, bool *duplicate);
  void notify(unsigned type, unsigned event, unsigned value)
  { _host->notify(type, event, value); }

private:
  Hw::Device *_host;
  bool _generate_switch_events;

  static void notification_handler(ACPI_HANDLE handle, UINT32 event,
                                   void *ctxt);
};


Acpi_lid::Acpi_lid(ACPI_HANDLE hdl, Device *host)
: Acpi_dev(hdl), _host(host), _generate_switch_events(false)
{
  Io::Event_source_infos   *source_info = _host->get_event_infos(true);
  L4Re::Event_stream_info  *info  = &source_info->info;
  if (info->get_swbit(L4RE_SW_0))
    {
      d_printf(DBG_ERR, "error: ACPI %s: LID switch 0 already taken, "
                        "no LID events will be generated\n", host->name());
      return;
    }

  info->set_swbit(L4RE_SW_0, true);
  _generate_switch_events = true;

  bool closed, duplicate;
  get_and_store_state(&closed, &duplicate);
  d_printf(DBG_DEBUG, "ACPI %s: initial state: %s\n", host->name(),
           closed ? "closed": "open");
}

void
Acpi_lid::notification_handler(ACPI_HANDLE handle, UINT32 event,
                               void *ctxt)
{
  Acpi_lid *lid = static_cast<Acpi_lid*>(ctxt);

  (void)handle;
  assert (handle == lid->handle());

  if (event != 0x80)
    {
      // not a LID changed event, pass on
      lid->notify(Acpi_dev::Acpi_event, event, 0);
      return;
    }

  if (!lid->_generate_switch_events)
    return;

  // translate LID status changed into L4Re switch event for switch 0
  bool closed = false, duplicate = false;
  lid->get_and_store_state(&closed, &duplicate);

  d_printf(DBG_DEBUG, "ACPI: LID %s\n", closed ? "closed" : "open");
  if (!duplicate)
    lid->notify(L4RE_EV_SW, L4RE_SW_0, closed ? 0 : 1);
}

void
Acpi_lid::enable_notifications(Hw::Device *d)
{
  d_printf(DBG_DEBUG, "ACPI %s: enable notifications\n", d->name());
  if (_have_notification_handler)
    return;

  ACPI_STATUS s = AcpiInstallNotifyHandler(handle(), ACPI_ALL_NOTIFY,
                                           notification_handler, this);

  if (ACPI_SUCCESS(s))
    _have_notification_handler = true;
  else
    d_printf(DBG_ERR, "error: ACPI %s: cannot install notification handler: %s\n",
             d->name(), AcpiFormatException(s));
}

/**
 * \brief Get the LID state from ACPI and store it.
 *
 * Get the LID state from the ACPI and store it in the Hw::Device
 * L4Re::Event_stream_state struct.
 *
 * \pre _generate_switch_events == true
 * \param handle ACPI handle of this device
 * \retval closed    true if LID is closed, false if LID open
 * \retval duplicate true if stored LID state matches measured LID state
 */
void
Acpi_lid::get_and_store_state(bool *closed, bool *duplicate)
{
  l4_uint64_t value = 1;
  Io::Event_source_infos   *source_info = _host->get_event_infos();
  L4Re::Event_stream_state *state = &source_info->state;

  Acpi_buffer<ACPI_OBJECT> o;
  ACPI_STATUS s = AcpiEvaluateObject(handle(), ACPI_STRING("_LID"), NULL, &o);
  if (!ACPI_SUCCESS(s) || o.value.Type != ACPI_TYPE_INTEGER)
    d_printf(DBG_ERR, "error: ACPI LID: could not read _LID: %s\n",
             AcpiFormatException(s));
  else
    value = o.value.Integer.Value;

  bool lid_closed = (value == 0);
  *closed = lid_closed;
  if (lid_closed != state->get_swbit(L4RE_SW_0))
    {
      d_printf(DBG_DEBUG2, "ACPI LID: duplicate LID event\n");
      *duplicate = true;
      return;
    }

  state->set_swbit(L4RE_SW_0, !lid_closed);
  *duplicate = false;
}

class Acpi_lid_driver : public Acpi_device_driver
{
public:
  Acpi_dev *probe(Device *device, ACPI_HANDLE handle, ACPI_DEVICE_INFO const *)
  {
    Acpi_lid *dev;
    d_printf(DBG_DEBUG, "ACPI: found LID: %s\n", device->name());
    dev = new Acpi_lid(handle, device);

    dev->discover_crs(device);
    device->add_feature(dev);

    // LID and buttons are always wake sources therefore tell ACPICA about it
    acpi_setup_device_wake(device, handle);
    return dev;
  }
};

class Acpi_button_driver : public Acpi_device_driver
{
public:
  Acpi_dev *probe(Device *device, ACPI_HANDLE handle, ACPI_DEVICE_INFO const *)
  {
    Acpi_dev *dev;

    d_printf(DBG_DEBUG, "ACPI: found ACPI button: %s\n", device->name());
    dev = new Acpi_dev(handle);

    dev->discover_crs(device);
    device->add_feature(dev);

    // LID and buttons are always wake sources therefore tell ACPICA about it
    acpi_setup_device_wake(device, handle);
    return dev;
  }
};

static Acpi_lid_driver _acpi_lid_drv;
static Acpi_button_driver _acpi_button_drv;

struct Init
{
  Init()
  {
    Acpi_device_driver::register_driver("PNP0C0C", &_acpi_button_drv);
    Acpi_device_driver::register_driver("PNP0C0D", &_acpi_lid_drv);
    Acpi_device_driver::register_driver("PNP0C0E", &_acpi_button_drv);
  }
};

static Init init;

}

