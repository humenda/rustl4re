#include "io_acpi.h"
#include "__acpi.h"
#include "debug.h"
#include <l4/cxx/bitfield>

#include <l4/util/port_io.h>
#include <l4/util/util.h>

#include <map>

namespace {

using namespace Hw;
using Hw::Device;

struct Acpi_ec : Acpi_dev
{
  struct Flags
  {
    l4_uint32_t raw;
    CXX_BITFIELD_MEMBER( 0, 0, as_handler_installed, raw);
    CXX_BITFIELD_MEMBER( 1, 1, gpe_handler_installed, raw);
    CXX_BITFIELD_MEMBER( 0, 1, handlers_installed, raw);

    Flags() = default;
    explicit Flags(l4_uint32_t v) : raw(v) {}
  };

  /**
   * \brief The EC status register.
   */
  struct Status
  {
    l4_uint8_t raw;
    CXX_BITFIELD_MEMBER_UNSHIFTED(0, 0, out_buffer_full, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(1, 1, in_buffer_full, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(3, 3, command_issued, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(4, 4, burst_mode, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(5, 5, sci_pending, raw);
    CXX_BITFIELD_MEMBER_UNSHIFTED(6, 6, smi_pending, raw);

    Status() = default;
    Status(l4_uint8_t v) : raw(v) {}
  };

  // EC Commands
  enum Ec_command : l4_uint8_t
  {
    Ec_read          = 0x80,
    Ec_write         = 0x81,
    Ec_burst_enable  = 0x82,
    Ec_burst_disable = 0x83,
    Ec_query         = 0x84
  };

  Acpi_ec(ACPI_HANDLE hdl) : Acpi_dev(hdl), _flags(0), _data(0) {}


  static bool discover_ecdt()
  {
    ACPI_TABLE_ECDT *ecdt;
    ACPI_STATUS status = AcpiGetTable(ACPI_STRING(ACPI_SIG_ECDT), 1, (ACPI_TABLE_HEADER**)&ecdt);
    if (ACPI_FAILURE(status))
      return false;

    d_printf(DBG_DEBUG, "ACPI: EC via ECDT found\n");

    Acpi_ec *ec = new Acpi_ec(ACPI_ROOT_OBJECT);
    ec->_data = ecdt->Data.Address;
    ec->_control = ecdt->Control.Address;
    ec->_gpe = ecdt->Gpe;
    ec->_need_lock = 0;
    ec->_uid = ecdt->Uid;
    AcpiGetHandle(ACPI_ROOT_OBJECT, ACPI_STRING(ecdt->Id), &ec->_obj);

    _ecdt_ec = ec;
    ec->install_handlers();
    return true;
  }


  void setup_ec(Hw::Device *host)
  {
    Resource_list const &r = *host->resources();
    if (r.size() < 2)
      {
        d_printf(DBG_ERR, "ERROR: EC: error missing resources\n");
        return;
      }

    if (r[0]->type() != Resource::Io_res || r[1]->type() != Resource::Io_res)
      {
        d_printf(DBG_ERR, "ERROR: EC: resource type mismatch: types=%d,%d\n",
                 r[0]->type(), r[1]->type());
        return;
      }

    if (!r[0]->start() || !r[1]->start())
      {
        d_printf(DBG_ERR, "ERROR: EC: invalid ec ports=%x,%x\n",
                 (unsigned)r[0]->start(), (unsigned)r[1]->start());
        return;
      }

      {
        // handle ECs _GLK object
        Acpi_buffer<ACPI_OBJECT> glk;
        ACPI_STATUS status = AcpiEvaluateObject(handle(), ACPI_STRING("_GLK"), NULL, &glk);
        if (ACPI_FAILURE(status) || glk.value.Type != ACPI_TYPE_INTEGER)
          _need_lock = 0;
        else
          _need_lock = glk.value.Integer.Value;
      }

    // if we are already initialized (may be via ECDT) we must skip the rest
    if (_data)
      {
        return;
      }

      {
        // handle the _GPE object
        ACPI_STATUS status;
        Acpi_buffer<ACPI_OBJECT> gpe;
        status = AcpiEvaluateObject(handle(), ACPI_STRING("_GPE"), NULL, &gpe);

        // never seen the package return value here...
        if (ACPI_FAILURE(status) || gpe.value.Type != ACPI_TYPE_INTEGER)
          {
            d_printf(DBG_ERR, "ERROR: EC: invalid result from _GPE: %d\n", status);
            return;
          }

        _gpe = gpe.value.Integer.Value;
      }

    _data = r[0]->start();
    _control = r[1]->start();

    install_handlers();

    Acpi_walk find_qr = [this](ACPI_HANDLE hdl, int)
    {
      Acpi_buffer<char [5]> name;
      ACPI_STATUS s = AcpiGetName(hdl, ACPI_SINGLE_NAME, &name);
      unsigned qval;
      if (ACPI_SUCCESS(s) && sscanf(name.value, "_Q%x", &qval) == 1)
        _query_handlers[qval] = hdl;

      return AE_OK;
    };

    find_qr.walk(ACPI_TYPE_METHOD, handle(), 1);

    AcpiEnableGpe(0, _gpe);
  }

private:
  friend struct Acpi_ec_drv;
  typedef std::map<l4_uint8_t, ACPI_HANDLE> Handler_map;

  Flags _flags;
  Handler_map _query_handlers;

  l4_uint16_t _data;
  l4_uint16_t _control;

  l4_uint32_t _gpe;
  l4_uint32_t _uid;

  bool _need_lock : 1;

  static Acpi_ec *_ecdt_ec;


  void write_cmd(l4_uint8_t cmd)
  { l4util_out8(cmd, _control); }

  void write_data(l4_uint8_t data)
  { l4util_out8(data, _data); }

  l4_uint8_t read_data()
  { return l4util_in8(_data); }

  void wait_read()
  {
    while (!status().out_buffer_full())
      ;
  }

  void wait_write()
  {
    while (status().in_buffer_full())
      ;
  }

  ACPI_STATUS write(l4_uint8_t address, l4_uint8_t data)
  {
    if (!_data)
      return AE_NOT_FOUND;

    AcpiDisableGpe(NULL, _gpe);
    write_cmd(Ec_write);
    wait_write();
    write_data(address);
    wait_write();
    write_data(data);
    l4_sleep(1);
    AcpiEnableGpe(NULL, _gpe);
    return AE_OK;
  }

  ACPI_STATUS read(l4_uint8_t address, l4_uint8_t *data)
  {
    if (!_data)
      return AE_NOT_FOUND;

    AcpiDisableGpe(NULL, _gpe);
    write_cmd(Ec_read);
    wait_write();
    write_data(address);
    wait_read();
    *data = read_data();
    l4_sleep(1);
    AcpiEnableGpe(NULL, _gpe);
    return AE_OK;
  }


  Status status() const
  { return l4util_in8(_control); }


  ACPI_STATUS as_handler(l4_uint32_t func, ACPI_PHYSICAL_ADDRESS addr,
                         l4_uint32_t width, UINT64 *_value)
  {
    ACPI_STATUS result = AE_OK;
    unsigned bytes = width / 8;

    l4_uint8_t *value = reinterpret_cast<l4_uint8_t*>(_value);

    if ((addr > 0xFF) || !value)
      return AE_BAD_PARAMETER;

    if (func != ACPI_READ && func != ACPI_WRITE)
      return AE_BAD_PARAMETER;

    for (unsigned i = 0; i < bytes; ++i, ++addr, ++value)
      result = (func == ACPI_READ)
               ? read(addr, value)
               : write(addr, *value);

    return result;
  }

  ACPI_STATUS gpe_handler(ACPI_HANDLE, l4_uint32_t)
  {
    if (status().sci_pending())
      AcpiOsExecute(OSL_NOTIFY_HANDLER, _gpe_query, this);

    return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
  }

  void gpe_query()
  {
    AcpiDisableGpe(NULL, _gpe);
    write_cmd(Ec_query);
    wait_read();
    l4_uint8_t q = read_data();
    AcpiEnableGpe(NULL, _gpe);

    Handler_map::const_iterator i = _query_handlers.find(q);
    if (i == _query_handlers.end())
      return;

    AcpiEvaluateObject(i->second, 0, 0, 0);
  }

  static void _gpe_query(void *ec)
  {
    static_cast<Acpi_ec*>(ec)->gpe_query();
  }

  static ACPI_STATUS _as_handler(l4_uint32_t func, ACPI_PHYSICAL_ADDRESS addr,
                                 l4_uint32_t width, UINT64 *value,
                                 void *ctxt, void *)
  {
    return static_cast<Acpi_ec*>(ctxt)->as_handler(func, addr, width, value);
  }

  static ACPI_STATUS _gpe_handler(ACPI_HANDLE handle, l4_uint32_t gpe, void *ctxt)
  {
    return static_cast<Acpi_ec*>(ctxt)->gpe_handler(handle, gpe);
  }

  void install_handlers()
  {
    if (_flags.handlers_installed())
      return;

    ACPI_STATUS status;
    status = AcpiInstallAddressSpaceHandler(handle(), ACPI_ADR_SPACE_EC,
                                            &_as_handler, 0, this);
    if (ACPI_FAILURE(status))
      {
        d_printf(DBG_ERR, "error: failed to install EC address-space handler: %s\n",
                 AcpiFormatException(status));
        return;
      }

    _flags.as_handler_installed() = true;

    status = AcpiInstallGpeHandler(NULL, _gpe, ACPI_GPE_EDGE_TRIGGERED,
                                   &_gpe_handler, this);
    if (ACPI_FAILURE(status))
      {
        d_printf(DBG_ERR, "error: failed to install EC GPE handler: %s\n",
                 AcpiFormatException(status));
        return;
      }

    _flags.gpe_handler_installed() = true;
  }
};


Acpi_ec *Acpi_ec::_ecdt_ec;


struct Acpi_ec_drv : Acpi_device_driver
{
  Acpi_dev *probe(Device *device, ACPI_HANDLE acpi_hdl,
                  ACPI_DEVICE_INFO const *)
  {
    d_printf(DBG_DEBUG, "Found ACPI EC\n");
    Acpi_ec *ec = 0;
    if (   Acpi_ec::_ecdt_ec
        && (   Acpi_ec::_ecdt_ec->handle() == acpi_hdl
            || Acpi_ec::_ecdt_ec->handle() == ACPI_ROOT_OBJECT))
      {
        d_printf(DBG_DEBUG, "ACPI: Use EC from ECDT\n");
        ec = Acpi_ec::_ecdt_ec;
        Acpi_ec::_ecdt_ec = 0;
      }
    else
      ec = new Acpi_ec(acpi_hdl);

    ec->discover_crs(device);
    ec->setup_ec(device);
    device->add_feature(ec);

    return ec;
  };
};

static Acpi_ec_drv _acpi_ec_drv;

struct Init
{
  Init()
  {
    Acpi_device_driver::register_driver("PNP0C09", &_acpi_ec_drv);
  }
};

static Init init;

}

int acpi_ecdt_scan()
{
  Acpi_ec::discover_ecdt();
  return 0;
}

