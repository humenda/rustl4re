/*
 * (c) 2010-2020 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/vbus/vbus_interfaces.h>

#include "virt/vdevice.h"
#include <pci-if.h>

namespace Vi {
namespace Pci {
  typedef Hw::Pci::Config Config;

  /**
   * A virtual PCI config space register backed with some 8,
   * 16, or 32 bit memory type.
   *
   * This implementation encapsulates all kinds of smaller than
   * sizeof(T) accesses and allows to specify some read-only bits
   * as argument to the write function.
   */
  template<typename T>
  class Cfg_reg
  {
  public:
    Cfg_reg() = default;

    /**
     * Encapsulates config-space read access.
     *
     * \param  offs  The offset (in bytes) inside this register
     *               (must be < `sizeof(T)`, and aligned to `w`).
     * \param  w     The access width (in terms of 2^`w`).
     */
    l4_uint32_t read(unsigned offs, Hw::Pci::Cfg_width w) const
    {
      if (offs >= sizeof(T))
        return ~0;

      // convert byte offset into bit shift while also aligning the offset
      // according to the access width
      offs >>= w;
      offs <<= w + 3; // now offs is aligned to 'w'
      l4_uint32_t m = 0xffffffffU >> (32 - (8U << w));
      return (_v >> offs) & m;
    }

    /**
     * Encapsulates config-space write accesses.
     *
     * \param  offs  The offset (in bytes) inside this register
     *               (must be < `sizeof(T)`, and aligned to `w`).
     * \param  v     The value to be written (only LSB according to `w`
     *               are used in the operation).
     * \param  w     The access width (in terms of 2^`w`).
     * \param  ro    Bit set in this value are read-only in the register.
     *               The value is relative to the full register.
     */
    void write(unsigned offs, l4_uint32_t v, Hw::Pci::Cfg_width w, T ro = 0)
    {
      if (offs >= sizeof(T))
        return;

      offs >>= w;
      offs <<= w + 3; // now offs is aligned to 'w'
      l4_uint32_t m = 0xffffffffU >> (32 - (8U << w));

      _v = (_v & (ro | ~(m << offs))) | (((v & m) << offs) & ~ro);
    }

    /// Get the raw value
    T get() const
    { return _v; }

    /// Set the raw value (no read-only checking).
    void set(T v)
    { _v = v; }

  private:
    T _v;
  };

  /**
   * Array of virtual PCI config-space Base Address Registers (BARs).
   *
   * \tparam BARS  The number of Base Address Registers.
   */
  template<unsigned BARS = 6>
  class Bar_array
  {
  public:
    using Cfg_width = Hw::Pci::Cfg_width;

    /**
     * Read from the BAR array.
     *
     * \param offs  The offset relative to the beginning of the first BAR.
     * \param w     The access width (in terms of 2^`w`).
     */
    l4_uint32_t read(unsigned offs, Cfg_width w) const
    {
      if (offs < (BARS * 4))
        return _b[offs / 4].read(offs & 3, w);
      else
        return ~0;
    }

    /**
     * Write to the BAR array.
     *
     * \param offs  The offset relative to the beginning of the first BAR.
     * \param v     The value that shall be written to the BAR.
     * \param w     The access width (in terms of 2^`w`).
     *
     * This function handles read-only parts of BAR registers
     * according to BAR size and type.
     */
    void write(unsigned offs, l4_uint32_t v, Cfg_width w)
    {
      if (offs < (BARS * 4))
        _b[offs / 4].write(offs & 3, v, w, ~(~0U << _s[offs / 4]));
    }

    /**
     * Set initial value of a BAR including the size.
     *
     * \param bar    The BAR number (0 .. BARS-1).#
     * \param v      The initial value including BAR type bits
     *               and upper 32bits of address if it is a 64bit BAR.
     * \param order  The size of the BAR in terms of 2^`order`.
     *
     * If `v` denotes a 64bit MMIO bar then the upper 32bit of the
     * address are written into `bar`+1.
     */
    void set(unsigned bar, l4_uint64_t v, unsigned order)
    {
      _b[bar].set(v);
      _s[bar] = order;

      if ((v & 7) == 4) // 64bit MMIO BAR
        {
          _b[bar + 1].set(v >> 32);
          _s[bar + 1] = 0;
        }
    }

    /**
     * Set the given BAR as invalid (empty).
     *
     * \param bar  The number of the BAR (0 .. BARS-1).
     */
    void set_invalid(unsigned bar)
    {
      _b[bar].set(~0U);
      _s[bar] = 4;
    }

    /**
     * Set a BAR for a resource.
     *
     * \tparam RESOURCE  The type of the resource.
     * \param  bar  The BAR number (0 .. BARS-1).
     * \param  r    Pointer to the resource.
     * \returns The number of 32bit registers to skip to find the next
     *          BAR. In particular, for a 64-bit BAR 2 is returned, 1 else.
     *
     * This function set the BAR as invalid if `r == nullptr`,
     * or if `r` is not valid or not compatible with PCI BARs.
     * Otherwise the BAR address is taken from r->start(). The size
     * is calculated from r->alignment(). The type from r->type(),
     * r->is_64bit(), and r->prefetchable().
     */
    template<typename RESOURCE>
    unsigned from_resource(unsigned bar, RESOURCE *r)
    {
      if (!r || !r->valid())
        {
          set_invalid(bar);
          return 1;
        }

      l4_uint32_t type = 0;
      unsigned order = 2;

      switch (r->type())
        {
        case RESOURCE::Io_res:
          type = 1;
          order = 2;
          break;

        case RESOURCE::Mmio_res:
          type = 0;
          if (r->is_64bit())
            type |= 4;

          if (r->prefetchable())
            type |= 8;

          order = 4;
          break;

        default:
          set_invalid(bar);
          return 1;
        }

      l4_uint32_t a = r->alignment();
      for (; order < 32 && (a >> order); ++order)
        ;

      set(bar, type | r->start(), order);
      return r->is_64bit() ? 2 : 1;
    }

  private:
    /**
     * An entry in this array represents a 32bit base address value of an IO or
     * MMIO region. For 64bit MMIO regions two entries are used.
     */
    Cfg_reg<l4_uint32_t> _b[BARS];
    /**
     * An entry in this array stores the size of the corresponding IO or MMIO
     * region.
     */
    l4_uint8_t _s[BARS];
  };

}

/**
 * An abstract virtual PCI capability, provided
 * by a virtualized PCI device in the config space.
 */
class Pci_capability
{
  Pci_capability *_next = 0;
  l4_uint8_t _offset; ///< Offset in the PCI config space (in bytes)
  l4_uint8_t _id;     ///< PCI capability ID (as of the PCI spec)
  l4_uint8_t _size;   ///< The size in bytes of this capability

public:
  typedef Hw::Pci::Cfg_width Cfg_width;

  explicit Pci_capability(l4_uint8_t offset)
  : _offset(offset), _size(4) {}

  /// Set the PCI capability ID.
  void set_id(l4_uint8_t id) { _id  = id; }

  /// Set the size of this capability.
  void set_size(l4_uint8_t size) { _size = size; }

  /// Get the config space offset in bytes.
  int offset() const { return _offset; }
  /// Get the size within the config space in bytes.
  int size() const { return _size; }
  /// Get the next PCI capability of the device.
  Pci_capability *next() const { return _next; }
  /// Get a reference to the next pointer.
  Pci_capability *&next() { return _next; }

  /// Check if the given config space offset is inside this PCI capability
  bool is_inside(unsigned offset) const
  { return offset >= _offset && offset < _offset + _size; }

  /**
   * PCI config space read of this capability.
   * \param reg    The config space offset (0x0 based, must be inside this
   *               capability).
   * \param v      Pointer to the buffer receiving the config space value.
   * \param order  The config space access size.
   */
  int cfg_read(int reg, l4_uint32_t *v, Cfg_width order)
  {
    reg &= ~0U << order;
    reg -= _offset;

    l4_uint32_t res;

    if (reg < 2)
      {
        l4_uint32_t header = _id;
        if (_next)
          header |= _next->offset() << 8;

        if ((reg + (1 << order)) > 2)
          {
            l4_uint32_t t;
            cap_read(2, &t, Hw::Pci::Cfg_short);
            header |= t << 16;
          }

        res = header >> (reg * 8);
      }
    else
      cap_read(reg, &res, order);

    if (order < Hw::Pci::Cfg_long)
      res &= (1UL << ((1UL << order) * 8)) - 1;

    *v = res;
    return 0;
  }

  /**
   * PCI config space write of this capability.
   * \param reg    The config space offset (0x0 based, must be inside this
   *               capability).
   * \param v      Pointer to the value that shall be written.
   * \param order  The config space access size.
   */
  int cfg_write(int reg, l4_uint32_t v, Cfg_width order)
  {
    reg &= ~0U << order;
    reg -= _offset;

    if (reg < 2)
      {
        if ((reg + (1 << order)) <= 2)
          return 0; // first two bytes are RO

        // must be a 4byte write at 0, so ignore the lower two byte
        v >>= 16;
        cap_write(2, v, Hw::Pci::Cfg_short);
        return 0;
      }
    else
      cap_write(reg, v, order);

    return 0;
  }

  /// Abstract read of the contents of this capability.
  virtual int cap_read(int offs, l4_uint32_t *v, Cfg_width) = 0;
  /// Abstract write to the contents of this capability.
  virtual int cap_write(int offs, l4_uint32_t v, Cfg_width) = 0;
};

/**
 * An abstract virtual PCI express extended capability, provided
 * by a virtualized PCI device in the config space.
 */
class Pcie_capability
{
  Pcie_capability *_next = 0;
  l4_uint16_t _offset; ///< Offset in the PCI config space (in bytes)
  l4_uint16_t _id;     ///< Extended PCI capability ID (as of the PCI spec)
  l4_uint8_t _version; ///< Version of PCI extended capability
  l4_uint8_t _size;    ///< The size in bytes of this capability

public:
  typedef Hw::Pci::Cfg_width Cfg_width;

  /// Make an extended PCI capability at given offset.
  explicit Pcie_capability(l4_uint16_t offset)
  : _offset(offset), _size(4) {}

  /// Set ID and version of this capability form the PCI config space value.
  void set_cap(l4_uint32_t cap)
  {
    _id = cap & 0xffff;
    _version = (cap >> 16) & 0xf;
  }

  /// Set the size of this capability in bytes.
  void set_size(l4_uint8_t size) { _size = size; }

  /// Set the offset of this extended PCI capability
  void set_offset(l4_uint16_t offset) { _offset = offset; }
  /// Get the config space offset of this capability.
  int offset() const { return _offset; }
  /// Get the size of the capability in bytes.
  int size() const { return _size; }
  /// Get the next capability of the deivce
  Pcie_capability *next() const { return _next; }
  /// Get a reference to the next pointer
  Pcie_capability *&next() { return _next; }

  /// Check if the given config space offset is inside this capability.
  bool is_inside(unsigned offset) const
  { return offset >= _offset && offset < _offset + _size; }

  /**
   * PCI config space read of this capability.
   * \param reg    The config space offset (0x0 based, must be inside this
   *               capability).
   * \param v      Pointer to the buffer receiving the config space value.
   * \param order  The config space access size.
   */
  int cfg_read(int reg, l4_uint32_t *v, Cfg_width order)
  {
    reg &= ~0U << order;
    reg -= _offset;

    l4_uint32_t res;

    if (reg < 4)
      {
        l4_uint32_t header = _id;
        header |= (l4_uint32_t)_version << 16;

        if (_next)
          header |= (l4_uint32_t)_next->offset() << 20;

        res = header >> (reg * 8);
      }
    else
      cap_read(reg, &res, order);

    if (order < Hw::Pci::Cfg_long)
      res &= (1UL << ((1UL << order) * 8)) - 1;

    *v = res;
    return 0;
  }

  /**
   * PCI config space write of this capability.
   * \param reg    The config space offset (0x0 based, must be inside this
   *               capability).
   * \param v      Pointer to the value that shall be written.
   * \param order  The config space access size.
   */
  int cfg_write(int reg, l4_uint32_t v, Cfg_width order)
  {
    reg &= ~0U << order;
    reg -= _offset;

    if (reg < 4)
      return 0; // first four bytes are RO

    return cap_write(reg, v, order);
  }

  /// Abstract read of the contents of this capability.
  virtual int cap_read(int offs, l4_uint32_t *v, Cfg_width) = 0;
  /// Abstract write to the contents of this capability.
  virtual int cap_write(int offs, l4_uint32_t v, Cfg_width) = 0;
};


/**
 * Proxy PCI capability for PCI capability pass through.
 *
 * The passed-through capability has the identical offset in the config
 * space of the physical PCI device. The capability header is virtualized,
 * however the contents are forwarded to the physical device.
 */
class Pci_proxy_cap : public Pci_capability
{
private:
  Hw::Pci::If *_hwf;

public:

  /**
   * Make a pass-through capability.
   * \param hwf     The pysical PCI device.
   * \param offset  The config space offset of the capability.
   *
   * This constructor reads the physical PCI capability and provides
   * an equivalent PCI capability ID.
   */
  Pci_proxy_cap(Hw::Pci::If *hwf, l4_uint8_t offset)
  : Pci_capability(offset), _hwf(hwf)
  {
    set_id(_hwf->config().read<l4_uint8_t>(offset));
  }

  int cap_read(int offs, l4_uint32_t *v, Cfg_width order) override
  { return _hwf->cfg_read(offset() + offs, v, order); }

  int cap_write(int offs, l4_uint32_t v, Cfg_width order) override
  { return _hwf->cfg_write(offset() + offs, v, order); }
};

/**
 * Proxy extended PCI capability.
 */
class Pcie_proxy_cap : public Pcie_capability
{
private:
  Hw::Pci::If *_hwf;
  /// The physical offset in the physical config space.
  l4_uint16_t _phys_offset;

public:
  Pcie_proxy_cap(Hw::Pci::If *hwf, l4_uint32_t header,
                 l4_uint16_t offset, l4_uint16_t phys_offset)
  : Pcie_capability(offset), _hwf(hwf), _phys_offset(phys_offset)
  {
    set_cap(header);
  }

  int cap_read(int offs, l4_uint32_t *v, Cfg_width order) override
  { return _hwf->cfg_read(_phys_offset + offs, v, order); }

  int cap_write(int offs, l4_uint32_t v, Cfg_width order) override
  { return _hwf->cfg_write(_phys_offset + offs, v, order); }
};

/**
 * \brief Generic virtual PCI device.
 * This class provides the basic functionality for a device on a
 * virtual PCI bus.  Implementations may provide proxy access to a real PCI
 * device or a completely virtualized PCI device.
 */
class Pci_dev
{
private:
  Pci_dev &operator = (Pci_dev const &) = delete;
  Pci_dev(Pci_dev const &) = delete;

public:
  typedef Hw::Pci::Cfg_width Cfg_width;
  typedef Io_irq_pin::Msi_src Msi_src;

  struct Irq_info
  {
    int irq;
    unsigned char trigger;
    unsigned char polarity;
  };

  Pci_dev() = default;
  virtual int cfg_read(int reg, l4_uint32_t *v, Cfg_width) = 0;
  virtual int cfg_write(int reg, l4_uint32_t v, Cfg_width) = 0;
  virtual int irq_enable(Irq_info *irq) = 0;
  virtual bool is_same_device(Pci_dev const *o) const = 0;
  virtual Msi_src *msi_src() const = 0;
  virtual ~Pci_dev() = 0;
};

inline
Pci_dev::~Pci_dev()
{}


/**
 * \brief General PCI device providing PCI device functions.
 */
class Pci_dev_feature : public Pci_dev, public Msi_src_feature
{
public:
  l4_uint32_t interface_type() const override
  { return 1 << L4VBUS_INTERFACE_PCIDEV; }
  int dispatch(l4_umword_t, l4_uint32_t, L4::Ipc::Iostream&) override;
};


/**
 * \brief A basic really virtualized PCI device.
 */
class Pci_virtual_dev : public Pci_dev_feature
{
public:
  struct Pci_cfg_header
  {
    l4_uint32_t vendor_device;
    l4_uint16_t cmd;
    l4_uint16_t status;
    l4_uint32_t class_rev;
    l4_uint8_t  cls;
    l4_uint8_t  lat;
    l4_uint8_t  hdr_type;
    l4_uint8_t  bist;
  } __attribute__((packed));

  Pci_cfg_header *cfg_hdr() { return (Pci_cfg_header*)_h; }
  Pci_cfg_header const *cfg_hdr() const { return (Pci_cfg_header const *)_h; }

  int cfg_read(int reg, l4_uint32_t *v, Cfg_width) override;
  int cfg_write(int reg, l4_uint32_t v, Cfg_width) override;

  bool is_same_device(Pci_dev const *o) const override
  { return o == this; }

  Io_irq_pin::Msi_src *msi_src() const override
  { return 0; }

  ~Pci_virtual_dev() = 0;

  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

protected:
  Device *_host;
  unsigned char *_h;
  unsigned _h_len;

};

inline
Pci_virtual_dev::~Pci_virtual_dev() = default;


/**
 * \brief A virtual PCI proxy for a real PCI device.
 */
class Pci_proxy_dev : public Pci_dev_feature
{
public:
  Pci_proxy_dev(Hw::Pci::If *hwf);

  int cfg_read(int reg, l4_uint32_t *v, Cfg_width) override;
  int cfg_write(int reg, l4_uint32_t v, Cfg_width) override;
  int irq_enable(Irq_info *irq) override;

  l4_uint32_t read_rom() const { return _rom; }
  void write_rom(l4_uint32_t v);

  int vbus_dispatch(l4_umword_t, l4_uint32_t, L4::Ipc::Iostream &)
  { return -L4_ENOSYS; }

  Hw::Pci::If *hwf() const { return _hwf; }
  Msi_src *msi_src() const override;

  void dump() const;
  bool is_same_device(Pci_dev const *o) const override
  {
    if (Pci_proxy_dev const *op = dynamic_cast<Pci_proxy_dev const *>(o))
      return (hwf()->bus_nr() == op->hwf()->bus_nr())
             && (hwf()->device_nr() == op->hwf()->device_nr());
    return false;
  }

  bool match_hw_feature(const Hw::Dev_feature *f) const override
  { return f == _hwf; }

  void set_host(Device *d) override
  { _host = d; }

  Device *host() const override
  { return _host; }

  Pci_capability *find_pci_cap(unsigned offset) const;
  void add_pci_cap(Pci_capability *);
  Pcie_capability *find_pcie_cap(unsigned offset) const;
  void add_pcie_cap(Pcie_capability *);

  bool scan_pci_caps();
  void scan_pcie_caps();

private:
  Device *_host;
  Hw::Pci::If *_hwf;
  Pci_capability *_pci_caps = 0;
  Pcie_capability *_pcie_caps = 0;

  Pci::Bar_array<6> _vbars;
  l4_uint32_t _rom;

  l4_uint16_t _skip_pcie_cap(Hw::Pci::Extended_cap const &cap, unsigned sz);

  int _do_status_cmd_write(l4_uint32_t mask, l4_uint32_t value);
  void _do_cmd_write(unsigned mask,unsigned value);

  int _do_rom_bar_write(l4_uint32_t mask, l4_uint32_t value);
};



/**
 * \brief a basic virtual PCI bridge.
 * This class is the base for virtual Host-to-PCI bridges,
 * for virtual PCI-to-PCI bridges, and also for this such as
 * virtual PCI-to-Cardbus bridges.
 */
class Pci_bridge : public Device
{
public:
  class Dev
  {
  public:
    enum { Fns = 8 };

  private:
    Pci_dev *_fns[Fns];

  public:
    Dev();

    bool empty() const { return !_fns[0]; }
    void add_fn(Pci_dev *f);
    void sort_fns();

    Pci_dev *fn(unsigned f) const { return _fns[f]; }
    void fn(unsigned f, Pci_dev *fn) { _fns[f] = fn; }
    bool cmp(Pci_dev const *od) const
    {
      if (empty())
	return false;

      return _fns[0]->is_same_device(od);
    }
  };

  class Bus
  {
  public:
    enum { Devs = 32 };

  private:
    Dev _devs[Devs];

  public:
    Dev const *dev(unsigned slot) const { return &_devs[slot]; }
    Dev *dev(unsigned slot) { return &_devs[slot]; }

    void add_fn(Pci_dev *d, int slot = -1);
  };

  Pci_bridge() : _free_dev(0), _primary(0), _secondary(0), _subordinate(0) {}

protected:
  Bus _bus;
  unsigned _free_dev;
  union
  {
    struct
    {
      l4_uint32_t _primary:8;
      l4_uint32_t _secondary:8;
      l4_uint32_t _subordinate:8;
      l4_uint32_t _lat:8;
    };
    l4_uint32_t _bus_config;
  };

public:

  void primary(unsigned char v) { _primary = v; }
  void secondary(unsigned char v) { _secondary = v; }
  void subordinate(unsigned char v) { _subordinate = v; }
  Pci_dev *child_dev(unsigned bus, unsigned char dev, unsigned char fn);
  void add_child(Device *d) override;
  void add_child_fixed(Device *d, Pci_dev *vp, unsigned dn, unsigned fn);

  Pci_bridge *find_bridge(unsigned bus);
  void setup_bus();
  void finalize_setup() override;

};

}

