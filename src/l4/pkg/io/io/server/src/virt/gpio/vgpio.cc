/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include "debug.h"
#include "gpio"
#include "hw_device.h"
#include "hw_device_client.h"
#include "virt/vdevice.h"
#include "virt/vbus_factory.h"
#include "virt/vbus.h"
#include "virt/vicu.h"

#include <l4/cxx/bitmap>
#include <l4/vbus/vbus_gpio-ops.h>

#include <cerrno>


namespace Vi {
namespace {

class Bitmap : public cxx::Bitmap_base
{
private:
  unsigned _nbits;

protected:
  unsigned words() const
  { return cxx::Bitmap_base::words(_nbits); }

  unsigned bit_buffer_bytes() const
  { return cxx::Bitmap_base::bit_buffer_bytes(_nbits); }

  void _reset(word_type *buffer, unsigned size)
  {
    if (_bits)
      delete [] _bits;
    _bits = buffer;
    _nbits = size;
  }

public:
  Bitmap(Bitmap const &) = delete;
  Bitmap &operator = (Bitmap const &) = delete;

  Bitmap() : cxx::Bitmap_base(0), _nbits(0) {}

  explicit Bitmap(unsigned max_bits)
  : cxx::Bitmap_base(new word_type[cxx::Bitmap_base::words(max_bits)]),
    _nbits(max_bits)
  {
    if (_bits)
      memset(_bits, 0, bit_buffer_bytes());
  }

  ~Bitmap() noexcept { _reset(0, 0); }

  void resize(unsigned sz)
  {
    unsigned nwords = cxx::Bitmap_base::words(sz);
    unsigned owords = cxx::Bitmap_base::words(size());
    unsigned cwords = std::min(nwords, owords);
    word_type *nb = new word_type[nwords];

    if (cwords)
      memcpy(nb, _bits, cwords * sizeof(word_type));

    if (cwords < nwords)
      memset(nb + cwords, 0, (nwords - cwords) * sizeof(word_type));

    _reset(nb, sz);
  }

  unsigned size() const { return _nbits; }

  bool overlaps(Bitmap const &o) const
  {
    unsigned m = std::min(words(), o.words());
    word_type const *a = _bits;
    word_type const *b = o._bits;
    for (unsigned i = 0; i < m; ++i, ++a, ++b)
      if (*a & *b)
        return true;

    return false;
  }

  word_type word_slice(unsigned offset) const
  {
    if (this->size() <= offset)
      return 0;

    word_type res;
    word_type a = _bits[word_index(offset)];
    unsigned ao = bit_index(offset);

    res = a >> ao;

    unsigned bw = word_index(offset + sizeof(word_type) - 1);
    if (bw == word_index(offset) || bw >= words())
      return res;

    a = _bits[bw] << (W_bits - ao);
    return res | a;
  }

  /**
   * \brief Set bits from \a s to (including) \a e to \a val.
   * \param s least significant (first) bit to set to \a val.
   * \param e most significant (last) bit to set to \a val;
   * \param val value (true = 1, or false = 0) to write into bits
   *            \a s to \a e.
   * \return true on success, false on error (out of range access)
   */
  bool set_range(unsigned s, unsigned e, bool val)
  {
    if (s >= size() || e >= size() || s > e)
      return false;

    for (unsigned i = s; i <= e; ++i)
      bit(i, val);

    return true;
  }

};

class Gpio : public Device, public Dev_feature, public Hw::Device_client
{
private:
  typedef std::vector<int> Irqs;

public:
  int dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios);

  explicit Gpio(Hw::Device *d)
  : _hwd(dynamic_cast<Hw::Gpio_chip*>(d))
  {
    add_feature(this);
    d->add_client(this);

    _pins.resize(_hwd->nr_pins());
    _irqs.resize(_hwd->nr_pins(), -1);

    static_assert(-1 != -L4_ENODEV, "...");
  }

  int add_filter(cxx::String const &tag, cxx::String const &)
  {
    if (tag != "pins")
      return -L4_ENODEV;
    return -L4_EINVAL;
  }

  int add_filter(cxx::String const &tag, unsigned long long val)
  {
    if (tag != "pins")
      return -L4_ENODEV;
    _pins[val] = true;
    return 0;
  }

  int add_filter(cxx::String const &tag, unsigned long long s, unsigned long long e)
  {
    if (tag != "pins")
      return -L4_ENODEV;
    if (!_pins.set_range(s, e, true))
      return -L4_ERANGE;
    return 0;
  }

  void enable_range(unsigned s, unsigned e)
  {
    if (!_pins.set_range(s, e, true))
      throw -L4_ERANGE;
  }

  char const *hid() const { return "GPIO"; }
  void set_host(Device *d) { _host = d; }
  Device *host() const { return _host; }
  l4_uint32_t interface_type() const { return 1 << L4VBUS_INTERFACE_GPIO; }

  bool match_hw_feature(const Hw::Dev_feature*) const
  { return false; }

  void dump(int indent) const
  {
    printf("%*.s %s.%s\n", indent, " ", hid(), name());
  }

  bool check_conflict(Hw::Device_client const *other) const
  {
    if (Gpio const *g = dynamic_cast<Gpio const *>(other))
      return (g->_hwd == _hwd) && _pins.overlaps(g->_pins);

    return false;
  }

  std::string get_full_name() const { return get_full_path(); }

  void notify(unsigned type, unsigned event, unsigned value)
  { Device::notify(type, event, value, true); }

  unsigned nr_pins() const { return _pins.size(); }

private:
  Device *_host;
  Hw::Gpio_chip *_hwd;

  Bitmap _pins;
  Irqs _irqs;

  int setup(L4::Ipc::Iostream &ios);
  int config_pull(L4::Ipc::Iostream &ios);
  int config_pad(L4::Ipc::Iostream &ios);
  int config_get(L4::Ipc::Iostream &ios);
  int get(L4::Ipc::Iostream &ios);
  int set(L4::Ipc::Iostream &ios);
  int multi_setup(L4::Ipc::Iostream &ios);
  int multi_config_pad(L4::Ipc::Iostream &ios);
  int multi_get(L4::Ipc::Iostream &ios);
  int multi_set(L4::Ipc::Iostream &ios);
  int to_irq(L4::Ipc::Iostream &ios);

  void check(unsigned pin) const
  {
    if (pin >= nr_pins())
      throw -L4_ERANGE;

    if (!_pins[pin])
      throw -L4_ERANGE;
  }

  void check_mask(Hw::Gpio_chip::Pin_slice const &mask) const
  {
    if ((_pins.word_slice(mask.offset) & mask.mask) != mask.mask)
      throw -L4_ERANGE;
  }
};



int
Gpio::setup(L4::Ipc::Iostream &ios)
{
  unsigned pin, mode;
  int value;
  ios >> pin >> mode >> value;
  check(pin);
  _hwd->setup(pin, mode, value);
  return 0;
}

int
Gpio::config_pull(L4::Ipc::Iostream &ios)
{
  unsigned pin, mode;
  ios >> pin >> mode;
  check(pin);
  _hwd->config_pull(pin, mode);
  return 0;
}

int
Gpio::config_pad(L4::Ipc::Iostream &ios)
{
  unsigned pin, func, value;
  ios >> pin >> func >> value;
  check(pin);
  _hwd->config_pad(pin, func, value);
  return 0;
}

int
Gpio::config_get(L4::Ipc::Iostream &ios)
{
  unsigned pin, func, value;
  ios >> pin >> func;
  check(pin);
  _hwd->config_get(pin, func, &value);
  ios << value;
  return 0;
}

int
Gpio::get(L4::Ipc::Iostream &ios)
{
  unsigned pin;
  ios >> pin;
  check(pin);
  return _hwd->get(pin);
}

int
Gpio::set(L4::Ipc::Iostream &ios)
{
  unsigned pin;
  int value;
  ios >> pin >> value;
  check(pin);
  _hwd->set(pin, value);
  return 0;
}

template<typename T>
T read_from(L4::Ipc::Istream &s)
{
  T tmp;
  s.get(tmp);
  return tmp;
}

int
Gpio::multi_setup(L4::Ipc::Iostream &ios)
{
  Hw::Gpio_chip::Pin_slice mask = read_from<Hw::Gpio_chip::Pin_slice>(ios);
  unsigned mode = read_from<unsigned>(ios);
  unsigned outvalue = read_from<unsigned>(ios);
  check_mask(mask);
  _hwd->multi_setup(mask, mode, outvalue);
  return 0;
}

int
Gpio::multi_config_pad(L4::Ipc::Iostream &ios)
{
  Hw::Gpio_chip::Pin_slice mask = read_from<Hw::Gpio_chip::Pin_slice>(ios);
  unsigned func = read_from<unsigned>(ios);
  unsigned value = read_from<unsigned>(ios);
  check_mask(mask);
  _hwd->multi_config_pad(mask, func, value);
  return 0;
}

int
Gpio::multi_get(L4::Ipc::Iostream &ios)
{
  unsigned offset = read_from<unsigned>(ios);
  unsigned data = _hwd->multi_get(offset);
  unsigned mask = _pins.word_slice(offset);
  ios << (unsigned)(data & mask);
  return 0;
}

int
Gpio::multi_set(L4::Ipc::Iostream &ios)
{
  Hw::Gpio_chip::Pin_slice mask = read_from<Hw::Gpio_chip::Pin_slice>(ios);
  unsigned data = read_from<unsigned>(ios);
  check_mask(mask);
  _hwd->multi_set(mask, data);
  return 0;
}

int
Gpio::to_irq(L4::Ipc::Iostream &ios)
{
  unsigned pin;
  ios >> pin;
  check(pin);

  if (_irqs[pin] == -1)
    {
      // we have to allocate the IRQ...
      // if it fails we mark the IRQ as unavailable (-L4_ENODEV)
      _irqs[pin] = -L4_ENODEV;

      Io_irq_pin *irq = _hwd->get_irq(pin);
      if (!irq)
        return -L4_ENOENT;

      if (System_bus *sb = dynamic_cast<System_bus *>(get_root()))
        {
          int virq = sb->sw_icu()->alloc_irq(Sw_icu::S_allow_set_mode, irq);
	  if (virq < 0)
	    return virq;

	  _irqs[pin] = virq;
	  return virq;
	}
      return -L4_ENODEV;
    }
  return _irqs[pin];
}

int
Gpio::dispatch(l4_umword_t, l4_uint32_t func, L4::Ipc::Iostream &ios)
{
  if (l4vbus_subinterface(func) != L4VBUS_INTERFACE_GPIO)
    return -L4_ENOSYS;

  try
    {
      switch (func)
	{
	case L4VBUS_GPIO_OP_SETUP: return setup(ios);
	case L4VBUS_GPIO_OP_CONFIG_PAD: return config_pad(ios);
	case L4VBUS_GPIO_OP_CONFIG_GET: return config_get(ios);
	case L4VBUS_GPIO_OP_GET: return get(ios);
	case L4VBUS_GPIO_OP_SET: return set(ios);
	case L4VBUS_GPIO_OP_MULTI_SETUP: return multi_setup(ios);
	case L4VBUS_GPIO_OP_MULTI_CONFIG_PAD: return multi_config_pad(ios);
	case L4VBUS_GPIO_OP_MULTI_GET: return multi_get(ios);
	case L4VBUS_GPIO_OP_MULTI_SET: return multi_set(ios);
	case L4VBUS_GPIO_OP_TO_IRQ: return to_irq(ios);
	case L4VBUS_GPIO_OP_CONFIG_PULL: return config_pull(ios);
	default: return -L4_ENOSYS;
	}
    }
  catch (int err)
    {
      return err;
    }
}


static Dev_factory_t<Gpio, Hw::Gpio_device> __gpio_factory;

class Gpio_resource : public Resource
{
public:
  explicit Gpio_resource(::Gpio_resource *hr)
  : Resource(hr->flags(), hr->start(), hr->end()), _hwr(hr), _handle(~0)
  { set_id(hr->id()); }

  ::Gpio_resource *hwr() const { return _hwr; }

  void set_handle(l4vbus_device_handle_t handle)
  { _handle = handle; }

  l4vbus_device_handle_t provider_device_handle() const
  { return _handle; }

private:
  ::Gpio_resource *_hwr;
  l4vbus_device_handle_t _handle;
};

class Root_gpio_rs : public Resource_space
{
public:
  explicit Root_gpio_rs(System_bus *bus) : _bus(bus)
  {}

  bool request(Resource *parent, ::Device *pdev, Resource *child, ::Device *)
  {
    Vi::System_bus *vsb = dynamic_cast<Vi::System_bus *>(pdev);
    if (!vsb || !parent)
      return false;

    Vi::Gpio_resource *r = dynamic_cast<Vi::Gpio_resource*>(child);
    if (!r)
      return false;

    Hw::Gpio_device *gpio = r->hwr()->provider();

    Vi::Device *vbus = vsb;

    for (Hw::Device *bus = system_bus(); bus != gpio; )
      {
        Hw::Device *d = gpio;
        while (d->parent() != bus)
          d = d->parent();

        bus = d;
        if (0)
          printf("BUS: %p:%s\n", bus, bus->name());

        Vi::Device *vd = vbus->find_by_name(bus->name());
        if (!vd)
          {
            if (bus != gpio)
              vd = new Vi::Device();
            else
              vd = new Gpio(gpio);

            vd->name(bus->name());
            vbus->add_child(vd);
          }
        if (0)
          printf("VDEV=%p:%s\n", vd, vd ? vd->name() : "");
        vbus = vd;
      }

    Gpio *vgpio = dynamic_cast<Gpio *>(vbus);

    if (!vgpio)
      {
        d_printf(DBG_ERR, "ERROR: device: %s is not a GPIO device\n", vbus->name());
        return false;
      }

    d_printf(DBG_DEBUG2, "Add GPIO resource to vbus: ");
    if (dlevel(DBG_DEBUG2))
      child->dump();


    unsigned e = r->end();
    unsigned s = r->start();
    unsigned n = vgpio->nr_pins();
    if (e >= n) e = n - 1;
    if (s >= n) s = n - 1;
    vgpio->enable_range(s, e);
    r->set_handle(l4vbus_device_handle_t(vgpio));

    return true;
  }

  bool alloc(Resource *parent, ::Device *, Resource *child, ::Device *, bool)
  {
    d_printf(DBG_DEBUG2, "Allocate virtual GPIO resource ...\n");
    if (dlevel(DBG_DEBUG2))
      child->dump();

    if (!parent)
      return false;
    return false;
  }

  void assign(Resource *, Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot assign to root Root_gpio_rs\n");
  }

  bool adjust_children(Resource *)
  {
    d_printf(DBG_ERR, "internal error: cannot adjust root Root_gpio_rs\n");
    return false;
  }

private:
  Root_gpio_rs(Root_gpio_rs const &);
  void operator = (Root_gpio_rs const &);

  System_bus *_bus;
};

static Vi::Resource_factory_t<Vi::Gpio_resource, ::Gpio_resource> __gpio_res_factory;
static System_bus::Root_resource_factory_t<Resource::Gpio_res, Root_gpio_rs> __rf;

}
}
