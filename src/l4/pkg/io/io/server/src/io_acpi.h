/*
 * (c) 2013 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include "hw_device.h"
#include "irqs.h"
#include "pci.h"

extern "C" {
#include "acpi.h"
#include "accommon.h"
}

template<typename T>
class Acpi_ptr
{
public:
  Acpi_ptr(Acpi_ptr const &) = delete;
  Acpi_ptr &operator = (Acpi_ptr const &) = delete;

  explicit Acpi_ptr(T *p = 0) : _p(p) {}
  ~Acpi_ptr()
  {
    if (!_p)
      return;

    ACPI_FREE(_p);
  }

  T &operator * () const { return *_p; }
  T *operator -> () const { return _p; }

  T **ref() { return &_p; }
  T *get() const { return _p; }
private:
  T *_p;
};

struct Acpi_auto_buffer : ACPI_BUFFER
{
  Acpi_auto_buffer() { Pointer = 0; Length = 0; }
  ~Acpi_auto_buffer()
  {
    if (Pointer)
      ACPI_FREE(Pointer);
  }

  void *release() { void *r = Pointer; Pointer = 0; return r; }
};

template<typename T>
struct Acpi_buffer : ACPI_BUFFER
{
  T value;
  Acpi_buffer()
  {
    Pointer = &value;
    Length  = sizeof(value);
  }
};

struct Acpi_walk
{
  ACPI_WALK_CALLBACK cb;
  void *ctxt;

  template<typename Functor>
  Acpi_walk(Functor f)
  : cb(invoke<Functor>), ctxt(reinterpret_cast<void*>(&f))
  {}

  ACPI_STATUS walk(ACPI_OBJECT_TYPE type, ACPI_HANDLE root, unsigned max_depth)
  {
    return AcpiWalkNamespace(type, root, max_depth, cb, 0, ctxt, 0);
  }

private:

  template< typename Functor >
  static ACPI_STATUS invoke(ACPI_HANDLE obj, l4_uint32_t level, void *ctxt, void **)
  {
    return (*reinterpret_cast<typename cxx::remove_reference<Functor>::type *>(ctxt))(obj, level);
  }
};


namespace Hw {

class Acpi_dev :
  public Hw::Dev_feature
{
public:
  enum { Acpi_event = 0x8 };

  Acpi_dev(ACPI_HANDLE obj)
  : _obj(obj), _have_notification_handler(false) {}

  ACPI_HANDLE handle() const { return _obj; }

  void discover_crs(Hw::Device *host);
  void enable_notifications(Hw::Device *host);
  void disable_notifications(Hw::Device *host);

protected:
  ACPI_HANDLE _obj;
  bool _have_notification_handler;
};

struct Acpi_device_driver
{
  virtual Acpi_dev *probe(Hw::Device *device, ACPI_HANDLE acpi_hdl,
                          ACPI_DEVICE_INFO const *info);
  virtual ~Acpi_device_driver() = 0;

  static bool register_driver(char const *hid, Acpi_device_driver *driver);
  static bool register_driver(unsigned short type, Acpi_device_driver *driver);
  static Acpi_device_driver* find(char const *hid);
  static Acpi_device_driver* find(unsigned short type);
};

inline Acpi_device_driver::~Acpi_device_driver() {}

namespace Acpi {

void register_sci(Io_irq_pin *sci);

}
}

unsigned acpi_get_debug_level();

extern Hw::Pci::Root_bridge *
  (*acpi_create_pci_root_bridge)(int, int busnum, Hw::Device *device);
